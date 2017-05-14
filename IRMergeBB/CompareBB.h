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

#include "llvm/Transforms/Utils/FunctionComparator.h"

namespace llvm {

class BBComparator : private FunctionComparator {
public:
  BBComparator(GlobalNumberState *GN)
      : FunctionComparator(nullptr, nullptr, GN) {}
  int compareBB(const BasicBlock *BBL, const BasicBlock *BBR);

  typedef uint64_t BasicBlockHash;

  static BasicBlockHash basicBlockHash(const BasicBlock &);

private:
  int compareSignatures() const;
  int compareBasicBlocks(const BasicBlock *BBL, const BasicBlock *BBR) const;
  int compareInstOperands(const Instruction *IL, const Instruction *IR) const;
};

} // namespace llvm

#endif // LLVMTRANSFORM_BBCOMPARING_H
