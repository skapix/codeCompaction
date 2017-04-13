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

#endif // LLVMTRANSFORM_PAC_ARM_H
