//===- BBComparing.h - Comparing basic blocks -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_BBCOMPARING_H
#define LLVMTRANSFORM_BBCOMPARING_H

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/ValueMap.h"

namespace llvm {

class GlobalValue;

class GlobalNumberState {
  struct Config : ValueMapConfig<GlobalValue *> {
    enum { FollowRAUW = false };
  };
  // Each GlobalValue is mapped to an identifier. The Config ensures when RAUW
  // occurs, the mapping does not change. Tracking changes is unnecessary, and
  // also problematic for weak symbols (which may be overwritten).
  typedef ValueMap<GlobalValue *, uint64_t, Config> ValueNumberMap;
  ValueNumberMap GlobalNumbers;
  // The next unused serial number to assign to a global.
  uint64_t NextNumber;

public:
  GlobalNumberState() : GlobalNumbers(), NextNumber(0) {}

  uint64_t getNumber(GlobalValue *Global) {
    ValueNumberMap::iterator MapIter;
    bool Inserted;
    std::tie(MapIter, Inserted) = GlobalNumbers.insert({Global, NextNumber});
    if (Inserted)
      ++NextNumber;
    return MapIter->second;
  }

  void clear() { GlobalNumbers.clear(); }
};

class BBComparator {
public:
  BBComparator(GlobalNumberState *GN, const DataLayout &DL)
      : GlobalNumbers(GN), DL(DL) {}

  /// Test whether the two basic blocks have equivalent behaviour.
  int compare(const BasicBlock *BBL, const BasicBlock *BBR) const;

  /// Hash a function. Equivalent functions will have the same hash, and unequal
  /// functions will have different hashes with high probability.
  typedef uint64_t BasicBlockHash;

  static BasicBlockHash basicBlockHash(const BasicBlock &);

private:
  /// Test whether two basic blocks have equivalent behaviour.
  int cmpBasicBlocks(const BasicBlock *BBL, const BasicBlock *BBR) const;

  int cmpInstOperands(const Instruction *IL, const Instruction *IR) const;
  int cmpConstants(const Constant *L, const Constant *R) const;
  int cmpGlobalValues(GlobalValue *L, GlobalValue *R) const;
  int cmpValues(const Value *L, const Value *R) const;
  int cmpOperations(const Instruction *L, const Instruction *R) const;
  int cmpGEPs(const GEPOperator *GEPL, const GEPOperator *GEPR) const;

  int cmpGEPs(const GetElementPtrInst *GEPL,
              const GetElementPtrInst *GEPR) const {
    return cmpGEPs(cast<GEPOperator>(GEPL), cast<GEPOperator>(GEPR));
  }

  int cmpTypes(Type *TyL, Type *TyR) const;
  int cmpNumbers(uint64_t L, uint64_t R) const;
  int cmpOrderings(AtomicOrdering L, AtomicOrdering R) const;
  int cmpAPInts(const APInt &L, const APInt &R) const;
  int cmpAPFloats(const APFloat &L, const APFloat &R) const;
  int cmpInlineAsm(const InlineAsm *L, const InlineAsm *R) const;
  int cmpMem(StringRef L, StringRef R) const;
  int cmpAttrs(const AttributeList L, const AttributeList R) const;
  int cmpRangeMetadata(const MDNode *L, const MDNode *R) const;
  int cmpOperandBundlesSchema(const Instruction *L, const Instruction *R) const;

  mutable DenseMap<const Value *, int> sn_mapL, sn_mapR;

  // The global state we will use
  GlobalNumberState *GlobalNumbers;
  const DataLayout &DL;
};

} // namespace llvm

#endif // LLVMTRANSFORM_BBCOMPARING_H
