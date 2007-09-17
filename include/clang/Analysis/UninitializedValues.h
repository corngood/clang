//===- UninitializedValues.h - unintialized values analysis ----*- C++ --*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by Ted Kremenek and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the interface for the Unintialized Values analysis,
// a flow-sensitive analysis that detects when variable values are unintialized.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITVALS_H
#define LLVM_CLANG_UNITVALS_H

#include "llvm/ADT/BitVector.h"
#include "clang/Analysis/DataflowValues.h"

namespace clang {

  class VarDecl;
  class Expr;
  class DeclRefExpr;

/// UninitializedValues_ValueTypes - Utility class to wrap type declarations
///   for dataflow values and dataflow analysis state for the
///   Unitialized Values analysis.
class UninitializedValues_ValueTypes {
public:

  //===--------------------------------------------------------------------===//
  // ValTy - Dataflow value.
  //===--------------------------------------------------------------------===//
  
  struct ValTy {
    llvm::BitVector DeclBV;
    llvm::BitVector ExprBV;

    // Used by the solver.
    void resetValues() {
      DeclBV.reset();
      ExprBV.reset();
    }
    
    bool equal(ValTy& RHS) const { 
      return DeclBV == RHS.DeclBV && ExprBV == RHS.ExprBV; 
    }
    
    void copyValues(ValTy& RHS) {
      DeclBV = RHS.DeclBV;
      ExprBV = RHS.ExprBV;
    }    
  };
  
  //===--------------------------------------------------------------------===//
  // AnalysisDataTy - Whole-function meta data used by the transfer function
  //  logic.
  //===--------------------------------------------------------------------===//
  
  struct ObserverTy;
  
  struct AnalysisDataTy {
    llvm::DenseMap<const VarDecl*, unsigned > VMap;
    llvm::DenseMap<const Expr*, unsigned > EMap;
    unsigned NumDecls;
    unsigned NumBlockExprs;
    ObserverTy* Observer;
    
    AnalysisDataTy() : NumDecls(0), NumBlockExprs(0), Observer(NULL) {}
  };
  
  //===--------------------------------------------------------------------===//
  // ObserverTy - Observer for querying DeclRefExprs that use an uninitalized
  //   value.
  //===--------------------------------------------------------------------===//
  
  struct ObserverTy {
    virtual ~ObserverTy();
    virtual void ObserveDeclRefExpr(ValTy& Val, AnalysisDataTy& AD, 
                                    DeclRefExpr* DR, VarDecl* VD) = 0;
  };  
};

/// UninitializedValues - Objects of this class encapsulate dataflow analysis
///  information regarding what variable declarations in a function are
///  potentially unintialized.
class UninitializedValues : 
  public DataflowValues<UninitializedValues_ValueTypes> {  
public:
  typedef UninitializedValues_ValueTypes::ObserverTy ObserverTy;

  UninitializedValues() {}
  
  /// IntializeValues - Create initial dataflow values and meta data for
  ///  a given CFG.  This is intended to be called by the dataflow solver.
  void InitializeValues(const CFG& cfg);
};

} // end namespace clang
#endif
