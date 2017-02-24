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

  virtual size_t getOriginalBlockWeight() const;

  virtual size_t getNewBlockWeight(const size_t InputArgs,
                                   const size_t OutputArgs) const override;

  virtual size_t getFunctionCreationWeight(const size_t InputArgs,
                                           const size_t OutputArgs) const override;


  virtual ~DecisionMaker_x86_64() {}

private:
  virtual bool isTiny() const override;

private:

  // Handle last ICmp instruction in a special way, because x86 perform
  // jumps according to flags. Since then, if instruction returns boolean value,
  // processor has to sef flags one more time
  bool IsLastCmp;

  // usually in x86_64 all allocas are combined in 1 alloca instruction
  // (add rsp, ?). Also in the end of routine substracts previously
  // added value.
  bool HasAlloca;
};


void DecisionMaker_x86_64::init(
  const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) {
  assert(!Insts.empty() && "Should not reach here");
  BlockWeight = 0;
  HasAlloca = false;
  size_t HalfWeight = 0;
  for (auto I : Insts) {
    if (isCommonlySkippedInstruction(I))
      continue;
    switch (I->getOpcode()) {
      case llvm::Instruction::Alloca:
        HasAlloca = true;
        break;
      //case llvm::Instruction::GetElementPtr:
        // GEP is usually not used in MC, because of relative addressing in x86-64
     //   break;

        // Heuristics
      case llvm::Instruction::Load:
      case llvm::Instruction::Store:
        ++HalfWeight;
        break;
      default:
        ++BlockWeight;
    }
  }

  BlockWeight += (HalfWeight + 1) / 2;
  IsLastCmp = Insts.back()->getOpcode() == llvm::Instruction::ICmp;
}


bool DecisionMaker_x86_64::isTiny() const {
  return BlockWeight <= 2 + static_cast<size_t>(IsLastCmp);
}

size_t DecisionMaker_x86_64::getNewBlockWeight(const size_t InputArgs,
                                               const size_t OutputArgs) const {
  return CommonDecisionMaker::getNewBlockWeight(InputArgs, OutputArgs) + IsLastCmp;
}

size_t DecisionMaker_x86_64::getOriginalBlockWeight() const {
  return CommonDecisionMaker::getOriginalBlockWeight() + HasAlloca * 2;
}

size_t DecisionMaker_x86_64::getFunctionCreationWeight(const size_t InputArgs, const size_t OutputArgs) const {
  // isLastCmp is added because of comparing results is necessary
  // to store in register.
  // It is used because usually additional instruction is added: sete %al
  return CommonDecisionMaker::getFunctionCreationWeight(InputArgs, OutputArgs)
         + HasAlloca * 2 + IsLastCmp;
}


#endif // LLVMTRANSFORM_DECISIONMAKER_X86_64_H
