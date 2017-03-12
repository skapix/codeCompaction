//===-- TargetDependent/PAC_arm.h - Arm Procedural analysis cost-----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file describes size of machine Arm instructions
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_PAC_ARM_H
#define LLVMTRANSFORM_PAC_ARM_H

#include "CommonPAC.h"

class PAC_arm : public CommonPAC {
public:
  virtual void init(const llvm::TargetTransformInfo &TTI,
                    const InstructionLocation &I,
                    const llvm::BasicBlock::const_iterator &Begin,
                    const llvm::BasicBlock::const_iterator &End) override;

  virtual size_t getNewBlockWeight(const size_t InputArgs,
                                   const size_t OutputArgs) const override;
  virtual ~PAC_arm() {}
};

// TODO: implement
void PAC_arm::init(const llvm::TargetTransformInfo &TTI,
                   const InstructionLocation &IL,
                   const llvm::BasicBlock::const_iterator &Begin,
                   const llvm::BasicBlock::const_iterator &End) {
  CommonPAC::init(TTI, IL, Begin, End);
  // because of arm flag register we need to add one more instruction
  // TODO: create common instruction, checking the last instruction
  auto PCI = std::prev(End);
  size_t PCIIndex = IL.amountInsts() - 1;
  while (!IL.isUsedInsideFunction(PCIIndex)) {
    --PCI;
    --PCIIndex;
  }
  if (llvm::isa<llvm::CmpInst>(PCI))
    ++NewBlockAddWeight;
}

size_t PAC_arm::getNewBlockWeight(const size_t InputArgs,
                                  const size_t OutputArgs) const {
  // output calculation:
  // 1st output is free because it is usually returned by a function
  // 2nd output costs 3:
  //  1) Get variable address
  //  2) Set register to allocated memory
  //  3) Store parameter from stack pointer into register
  const size_t ParamOutputs = OutputArgs > 0 ? OutputArgs - 1 : 0;
  const size_t Penalty = OutputArgs > 0 ? OutputArgs - 1 : 0;
  return NewBlockAddWeight + 1 + InputArgs + 3 * ParamOutputs + Penalty;
}

#endif // LLVMTRANSFORM_PAC_ARM_H
