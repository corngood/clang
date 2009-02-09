//==- ProgramPoint.h - Program Points for Path-Sensitive Analysis --*- C++ -*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the interface ProgramPoint, which identifies a
//  distinct location in a function.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_PROGRAM_POINT
#define LLVM_CLANG_ANALYSIS_PROGRAM_POINT

#include "clang/AST/CFG.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/FoldingSet.h"
#include <cassert>
#include <utility>

namespace clang {
    
class ProgramPoint {
public:
  enum Kind { BlockEdgeKind = 0x0,
              BlockEntranceKind = 0x1,
              BlockExitKind = 0x2, 
              // Keep the following four together and in this order.
              PostStmtKind = 0x3,
              PostLocationChecksSucceedKind = 0x4,
              PostOutOfBoundsCheckFailedKind = 0x5,
              PostNullCheckFailedKind = 0x6,
              PostUndefLocationCheckFailedKind = 0x7,
              PostLoadKind = 0x8,
              PostStoreKind = 0x9,
              PostPurgeDeadSymbolsKind = 0x10,
              PostStmtCustomKind = 0x11,
              MinPostStmtKind = PostStmtKind,
              MaxPostStmtKind = PostStmtCustomKind };

private:
  enum { TwoPointers = 0x1, Custom = 0x2, Mask = 0x3 };
  
  std::pair<uintptr_t,uintptr_t> Data;
  
protected:
  ProgramPoint(const void* P, Kind k)
    : Data(reinterpret_cast<uintptr_t>(P),
           (uintptr_t) k) {}
    
  ProgramPoint(const void* P1, const void* P2)
    : Data(reinterpret_cast<uintptr_t>(P1) | TwoPointers,
           reinterpret_cast<uintptr_t>(P2)) {}

  ProgramPoint(const void* P1, const void* P2, bool)
    : Data(reinterpret_cast<uintptr_t>(P1) | Custom,
           reinterpret_cast<uintptr_t>(P2)) {}

protected:
  void* getData1NoMask() const {
    Kind k = getKind(); k = k;
    assert(k == BlockEntranceKind || k == BlockExitKind);
    return reinterpret_cast<void*>(Data.first);
  }
  
  void* getData1() const {
    Kind k = getKind(); k = k;
    assert(k == BlockEdgeKind || (k >= MinPostStmtKind && k < MaxPostStmtKind));
    return reinterpret_cast<void*>(Data.first & ~Mask);
  }

  void* getData2() const { 
    Kind k = getKind(); k = k;
    assert(k == BlockEdgeKind || k == PostStmtCustomKind);
    return reinterpret_cast<void*>(Data.second);
  }
    
public:    
  Kind getKind() const {
    switch (Data.first & Mask) {
      case TwoPointers: return BlockEdgeKind;
      case Custom: return PostStmtCustomKind;
      default: return (Kind) Data.second;
    }
  }

  // For use with DenseMap.
  unsigned getHashValue() const {
    std::pair<void*,void*> P(reinterpret_cast<void*>(Data.first),
                             reinterpret_cast<void*>(Data.second));
    return llvm::DenseMapInfo<std::pair<void*,void*> >::getHashValue(P);
  }
  
  static bool classof(const ProgramPoint*) { return true; }

  bool operator==(const ProgramPoint & RHS) const {
    return Data == RHS.Data;
  }

  bool operator!=(const ProgramPoint& RHS) const {
    return Data != RHS.Data;
  }
    
  bool operator<(const ProgramPoint& RHS) const {
    return Data < RHS.Data;
  }
  
  void Profile(llvm::FoldingSetNodeID& ID) const {
    ID.AddPointer(reinterpret_cast<void*>(Data.first));
    ID.AddPointer(reinterpret_cast<void*>(Data.second));
  }    
};
               
class BlockEntrance : public ProgramPoint {
public:
  BlockEntrance(const CFGBlock* B) : ProgramPoint(B, BlockEntranceKind) {}
    
  CFGBlock* getBlock() const {
    return reinterpret_cast<CFGBlock*>(getData1NoMask());
  }
  
  Stmt* getFirstStmt() const {
    CFGBlock* B = getBlock();
    return B->empty() ? NULL : B->front();
  }

  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == BlockEntranceKind;
  }
};

class BlockExit : public ProgramPoint {
public:
  BlockExit(const CFGBlock* B) : ProgramPoint(B, BlockExitKind) {}
  
  CFGBlock* getBlock() const {
    return reinterpret_cast<CFGBlock*>(getData1NoMask());
  }

  Stmt* getLastStmt() const {
    CFGBlock* B = getBlock();
    return B->empty() ? NULL : B->back();
  }
  
  Stmt* getTerminator() const {
    return getBlock()->getTerminator();
  }
    
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == BlockExitKind;
  }
};


class PostStmt : public ProgramPoint {
protected:
  PostStmt(const Stmt* S, Kind k) : ProgramPoint(S, k) {}
  PostStmt(const Stmt* S, const void* data) : ProgramPoint(S, data, true) {}
  
public:
  PostStmt(const Stmt* S) : ProgramPoint(S, PostStmtKind) {}
      
  Stmt* getStmt() const { return (Stmt*) getData1(); }

  static bool classof(const ProgramPoint* Location) {
    unsigned k = Location->getKind();
    return k >= MinPostStmtKind && k <= MaxPostStmtKind;
  }
};

class PostLocationChecksSucceed : public PostStmt {
public:
  PostLocationChecksSucceed(const Stmt* S)
    : PostStmt(S, PostLocationChecksSucceedKind) {}
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostLocationChecksSucceedKind;
  }
};
  
class PostStmtCustom : public PostStmt {
  PostStmtCustom(const Stmt* S,
                 const std::pair<const void*, const void*>* TaggedData)
    : PostStmt(S, TaggedData) {
    assert(getKind() == PostStmtCustomKind);
  }

  const std::pair<const void*, const void*>& getTaggedPair() const {
    return *reinterpret_cast<std::pair<const void*, const void*>*>(getData2());
  }
  
  const void* getTag() const { return getTaggedPair().first; }
  
  const void* getTaggedData() const { return getTaggedPair().second; }
    
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostStmtCustomKind;
  }
};
  
class PostOutOfBoundsCheckFailed : public PostStmt {
public:
  PostOutOfBoundsCheckFailed(const Stmt* S)
  : PostStmt(S, PostOutOfBoundsCheckFailedKind) {}
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostOutOfBoundsCheckFailedKind;
  }
};

class PostUndefLocationCheckFailed : public PostStmt {
public:
  PostUndefLocationCheckFailed(const Stmt* S)
  : PostStmt(S, PostUndefLocationCheckFailedKind) {}
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostUndefLocationCheckFailedKind;
  }
};
  
class PostNullCheckFailed : public PostStmt {
public:
  PostNullCheckFailed(const Stmt* S)
  : PostStmt(S, PostNullCheckFailedKind) {}
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostNullCheckFailedKind;
  }
};
  
class PostLoad : public PostStmt {
public:
  PostLoad(const Stmt* S) : PostStmt(S, PostLoadKind) {}
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostLoadKind;
  }
};
  
class PostStore : public PostStmt {
public:
  PostStore(const Stmt* S) : PostStmt(S, PostStoreKind) {}
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostStoreKind;
  }
};
  
class PostPurgeDeadSymbols : public PostStmt {
public:
  PostPurgeDeadSymbols(const Stmt* S) : PostStmt(S, PostPurgeDeadSymbolsKind) {}
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == PostPurgeDeadSymbolsKind;
  }
};
  
class BlockEdge : public ProgramPoint {
public:
  BlockEdge(const CFGBlock* B1, const CFGBlock* B2)
    : ProgramPoint(B1, B2) {}
    
  CFGBlock* getSrc() const {
    return static_cast<CFGBlock*>(getData1());
  }
    
  CFGBlock* getDst() const {
    return static_cast<CFGBlock*>(getData2());
  }
  
  static bool classof(const ProgramPoint* Location) {
    return Location->getKind() == BlockEdgeKind;
  }
};

  
} // end namespace clang


namespace llvm { // Traits specialization for DenseMap 
  
template <> struct DenseMapInfo<clang::ProgramPoint> {

static inline clang::ProgramPoint getEmptyKey() {
  uintptr_t x =
   reinterpret_cast<uintptr_t>(DenseMapInfo<void*>::getEmptyKey()) & ~0x7;    
  return clang::BlockEntrance(reinterpret_cast<clang::CFGBlock*>(x));
}

static inline clang::ProgramPoint getTombstoneKey() {
  uintptr_t x =
   reinterpret_cast<uintptr_t>(DenseMapInfo<void*>::getTombstoneKey()) & ~0x7;    
  return clang::BlockEntrance(reinterpret_cast<clang::CFGBlock*>(x));
}

static unsigned getHashValue(const clang::ProgramPoint& Loc) {
  return Loc.getHashValue();
}

static bool isEqual(const clang::ProgramPoint& L,
                    const clang::ProgramPoint& R) {
  return L == R;
}

static bool isPod() {
  return true;
}
};
} // end namespace llvm

#endif
