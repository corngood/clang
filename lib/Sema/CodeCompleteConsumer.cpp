//===---- CodeCompleteConsumer.h - Code Completion Interface ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the CodeCompleteConsumer class.
//
//===----------------------------------------------------------------------===//
#include "clang/Sema/CodeCompleteConsumer.h"
#include "clang/Lex/Preprocessor.h"
#include "Sema.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <string.h>
using namespace clang;

CodeCompleteConsumer::CodeCompleteConsumer(Sema &S) : SemaRef(S) {
  SemaRef.setCodeCompleteConsumer(this);
}

CodeCompleteConsumer::~CodeCompleteConsumer() {
  SemaRef.setCodeCompleteConsumer(0);
}

void 
CodeCompleteConsumer::CodeCompleteMemberReferenceExpr(Scope *S, 
                                                      QualType BaseType,
                                                      bool IsArrow) {
  if (IsArrow) {
    if (const PointerType *Ptr = BaseType->getAs<PointerType>())
      BaseType = Ptr->getPointeeType();
    else if (BaseType->isObjCObjectPointerType())
    /*Do nothing*/ ;
    else
      return;
  }
  
  ResultSet Results;
  unsigned NextRank = 0;
  
  if (const RecordType *Record = BaseType->getAs<RecordType>()) {
    NextRank = CollectMemberResults(Record->getDecl(), NextRank, Results);

    if (getSema().getLangOptions().CPlusPlus) {
      if (!Results.empty())
        // The "template" keyword can follow "->" or "." in the grammar.
        Results.MaybeAddResult(Result("template", NextRank++));

      // FIXME: For C++, we also need to look into the current scope, since
      // we could have the start of a nested-name-specifier.
    }

    // Hand off the results found for code completion.
    ProcessCodeCompleteResults(Results.data(), Results.size());
    
    // We're done!
    return;
  }
}

void 
CodeCompleteConsumer::CodeCompleteQualifiedId(Scope *S, 
                                              NestedNameSpecifier *NNS,
                                              bool EnteringContext) {
  CXXScopeSpec SS;
  SS.setScopeRep(NNS);
  DeclContext *Ctx = getSema().computeDeclContext(SS, EnteringContext);
  if (!Ctx)
    return;
  
  ResultSet Results;
  unsigned NextRank = CollectMemberResults(Ctx, 0, Results);
  
  // The "template" keyword can follow "::" in the grammar
  if (!Results.empty())
    Results.MaybeAddResult(Result("template", NextRank));
  
  ProcessCodeCompleteResults(Results.data(), Results.size());
}

void CodeCompleteConsumer::ResultSet::MaybeAddResult(Result R) {
  if (R.Kind != Result::RK_Declaration) {
    // For non-declaration results, just add the result.
    Results.push_back(R);
    return;
  }
  
  // FIXME: Using declarations
  
  Decl *CanonDecl = R.Declaration->getCanonicalDecl();
  unsigned IDNS = CanonDecl->getIdentifierNamespace();

  // Friend declarations and declarations introduced due to friends are never
  // added as results.
  if (isa<FriendDecl>(CanonDecl) || 
      (IDNS & (Decl::IDNS_OrdinaryFriend | Decl::IDNS_TagFriend)))
    return;

  ShadowMap &SMap = ShadowMaps.back();
  ShadowMap::iterator I, IEnd;
  for (llvm::tie(I, IEnd) = SMap.equal_range(R.Declaration->getDeclName());
       I != IEnd; ++I) {
    NamedDecl *ND = I->second.first;
    unsigned Index = I->second.second;
    if (ND->getCanonicalDecl() == CanonDecl) {
      // This is a redeclaration. Always pick the newer declaration.
      I->second.first = R.Declaration;
      Results[Index].Declaration = R.Declaration;
      
      // Pick the best rank of the two.
      Results[Index].Rank = std::min(Results[Index].Rank, R.Rank);
      
      // We're done.
      return;
    }
  }
  
  // This is a new declaration in this scope. However, check whether this
  // declaration name is hidden by a similarly-named declaration in an outer
  // scope.
  std::list<ShadowMap>::iterator SM, SMEnd = ShadowMaps.end();
  --SMEnd;
  for (SM = ShadowMaps.begin(); SM != SMEnd; ++SM) {
    for (llvm::tie(I, IEnd) = SM->equal_range(R.Declaration->getDeclName());
         I != IEnd; ++I) {
      // A tag declaration does not hide a non-tag declaration.
      if (I->second.first->getIdentifierNamespace() == Decl::IDNS_Tag &&
          (IDNS & (Decl::IDNS_Member | Decl::IDNS_Ordinary | 
                   Decl::IDNS_ObjCProtocol)))
        continue;
      
      // Protocols are in distinct namespaces from everything else.
      if (((I->second.first->getIdentifierNamespace() & Decl::IDNS_ObjCProtocol)
           || (IDNS & Decl::IDNS_ObjCProtocol)) &&
          I->second.first->getIdentifierNamespace() != IDNS)
        continue;
      
      // The newly-added result is hidden by an entry in the shadow map.
      R.Hidden = true;
      break;
    }
  }
  
  // Insert this result into the set of results and into the current shadow
  // map.
  SMap.insert(std::make_pair(R.Declaration->getDeclName(),
                             std::make_pair(R.Declaration, Results.size())));
  Results.push_back(R);
}

/// \brief Enter into a new scope.
void CodeCompleteConsumer::ResultSet::EnterNewScope() {
  ShadowMaps.push_back(ShadowMap());
}

/// \brief Exit from the current scope.
void CodeCompleteConsumer::ResultSet::ExitScope() {
  ShadowMaps.pop_back();
}

/// \brief Collect the results of searching for members within the given
/// declaration context.
///
/// \param Ctx the declaration context from which we will gather results.
///
/// \param InitialRank the initial rank given to results in this tag
/// declaration. Larger rank values will be used for, e.g., members found
/// in base classes.
///
/// \param Results the result set that will be extended with any results
/// found within this declaration context (and, for a C++ class, its bases).
///
/// \returns the next higher rank value, after considering all of the
/// names within this declaration context.
unsigned CodeCompleteConsumer::CollectMemberResults(DeclContext *Ctx, 
                                                    unsigned InitialRank,
                                                    ResultSet &Results) {
  // Enumerate all of the results in this context.
  Results.EnterNewScope();
  for (DeclContext *CurCtx = Ctx->getPrimaryContext(); CurCtx; 
       CurCtx = CurCtx->getNextContext()) {
    for (DeclContext::decl_iterator D = CurCtx->decls_begin(), 
                                 DEnd = CurCtx->decls_end();
         D != DEnd; ++D) {
      if (NamedDecl *ND = dyn_cast<NamedDecl>(*D)) {
        // FIXME: Apply a filter to the results
        Results.MaybeAddResult(Result(ND, InitialRank));
      }
    }
  }
  
  // Traverse the contexts of inherited classes.
  unsigned NextRank = InitialRank;
  if (CXXRecordDecl *Record = dyn_cast<CXXRecordDecl>(Ctx)) {
    for (CXXRecordDecl::base_class_iterator B = Record->bases_begin(),
                                         BEnd = Record->bases_end();
         B != BEnd; ++B) {
      QualType BaseType = B->getType();
      
      // Don't look into dependent bases, because name lookup can't look
      // there anyway.
      if (BaseType->isDependentType())
        continue;
      
      const RecordType *Record = BaseType->getAs<RecordType>();
      if (!Record)
        continue;
      
      // FIXME: We should keep track of the virtual bases we visit, so 
      // that we don't visit them more than once.
      
      // FIXME: It would be nice to be able to determine whether referencing
      // a particular member would be ambiguous. For example, given
      //
      //   struct A { int member; };
      //   struct B { int member; };
      //   struct C : A, B { };
      //
      //   void f(C *c) { c->### }
      // accessing 'member' would result in an ambiguity. However, code
      // completion could be smart enough to qualify the member with the
      // base class, e.g.,
      //
      //   c->B::member
      //
      // or
      //
      //   c->A::member
      
      // Collect results from this base class (and its bases).
      NextRank = std::max(NextRank, 
                          CollectMemberResults(Record->getDecl(), 
                                               InitialRank + 1, 
                                               Results));
    }
  }
  
  // FIXME: Look into base classes in Objective-C!

  Results.ExitScope();
  return NextRank;
}

namespace {
  struct VISIBILITY_HIDDEN SortCodeCompleteResult {
    typedef CodeCompleteConsumer::Result Result;
    
    bool operator()(const Result &X, const Result &Y) const {
      // Sort first by rank.
      if (X.Rank < Y.Rank)
        return true;
      else if (X.Rank > Y.Rank)
        return false;
      
      // Result kinds are ordered by decreasing importance.
      if (X.Kind < Y.Kind)
        return true;
      else if (X.Kind > Y.Kind)
        return false;

      // Non-hidden names precede hidden names.
      if (X.Hidden != Y.Hidden)
        return !X.Hidden;
      
      // Ordering depends on the kind of result.
      switch (X.Kind) {
      case Result::RK_Declaration:
        // Order based on the declaration names.
        return X.Declaration->getDeclName() < Y.Declaration->getDeclName();
          
      case Result::RK_Keyword:
        return strcmp(X.Keyword, Y.Keyword) == -1;
      }
      
      // If only our C++ compiler did control-flow warnings properly.
      return false;
    }
  };
}

void 
PrintingCodeCompleteConsumer::ProcessCodeCompleteResults(Result *Results, 
                                                         unsigned NumResults) {
  // Sort the results by rank/kind/etc.
  std::stable_sort(Results, Results + NumResults, SortCodeCompleteResult());
  
  // Print the results.
  for (unsigned I = 0; I != NumResults; ++I) {
    switch (Results[I].Kind) {
    case Result::RK_Declaration:
      OS << Results[I].Declaration->getNameAsString() << " : " 
         << Results[I].Rank;
      if (Results[I].Hidden)
        OS << " (Hidden)";
      OS << '\n';
      break;
      
    case Result::RK_Keyword:
      OS << Results[I].Keyword << " : " << Results[I].Rank << '\n';
      break;
    }
  }
  
  // Once we've printed the code-completion results, suppress remaining
  // diagnostics.
  // FIXME: Move this somewhere else!
  getSema().PP.getDiagnostics().setSuppressAllDiagnostics();
}
