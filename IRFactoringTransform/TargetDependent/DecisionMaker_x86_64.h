//===-- TargetDependent/DecisionMaker_x86_64.h - x86_64 DM-----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file is included just once in IDecisionMaker.cpp, since then we can
/// implement all methods in header and don't bother about
/// executable size growth
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_DECISIONMAKER_X86_64_H
#define LLVMTRANSFORM_DECISIONMAKER_X86_64_H

#include "CommonDecisionMaker.h"
#include "llvm/IR/Instruction.h"

class DecisionMaker_x86_64 : public CommonDecisionMaker {
public:
  virtual void
  init(const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) override;
  virtual ~DecisionMaker_x86_64() {}
};

// TODO: investigate cases with binary size growth and edit
void DecisionMaker_x86_64::init(
    const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) {
  BlockWeight = 0;
  bool HasAlloca = false;
  for (llvm::Instruction *I : Insts) {
    if (isCommonlySkippedIntruction(I))
      continue;
    switch (I->getOpcode()) {
    case llvm::Instruction::Alloca:
      HasAlloca = true;
      break;
    default:
      ++BlockWeight;
    }
  }
  // usually in x86_64 all allocas are combined in 1 alloca instruction
  // (add rsp, ?). Also in the end of routine substracts previously
  // added value.
  BlockWeight += HasAlloca * 2;
}

#endif // LLVMTRANSFORM_DECISIONMAKER_X86_64_H
