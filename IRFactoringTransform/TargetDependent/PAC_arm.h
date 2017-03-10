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

// TODO: edit it
#ifndef LLVMTRANSFORM_DECISIONMAKER_ARM_H
#define LLVMTRANSFORM_DECISIONMAKER_ARM_H

#include "CommonPAC.h"

class PAC_arm : public CommonPAC {
public:
  virtual void init(const llvm::TargetTransformInfo &TTI,
                    const InstructionLocation &I,
                    const llvm::BasicBlock::const_iterator &Begin,
                    const llvm::BasicBlock::const_iterator &End) override;
  virtual ~PAC_arm() {}
};

// TODO: implement
void PAC_arm::init(const llvm::TargetTransformInfo &TTI,
                   const InstructionLocation &I,
                   const llvm::BasicBlock::const_iterator &Begin,
                   const llvm::BasicBlock::const_iterator &End) {
  CommonPAC::init(TTI, I, Begin, End);
}

#endif // LLVMTRANSFORM_DECISIONMAKER_ARM_H
