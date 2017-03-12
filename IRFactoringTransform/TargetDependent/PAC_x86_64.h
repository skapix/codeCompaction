//===-- TargetDependent/PAC_x86_64.h - x86_64 Procedural analysis cost-----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file is used for the x86-64 size calculating.
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_PAC_X86_64_H
#define LLVMTRANSFORM_PAC_X86_64_H

#include "CommonPAC.h"
#include "llvm/IR/Instruction.h"

class PAC_x86_64 : public CommonPAC {
public:
  virtual void init(const llvm::TargetTransformInfo &TTI,
                    const InstructionLocation &I,
                    const llvm::BasicBlock::const_iterator &Begin,
                    const llvm::BasicBlock::const_iterator &End) override;

  virtual size_t getNewBlockWeight(const size_t InputArgs,
                                   const size_t OutputArgs) const override;

  size_t getFunctionCallWeight(const llvm::CallInst &Inst);

  virtual ~PAC_x86_64() {}
};

#endif // LLVMTRANSFORM_PAC_X86_64_H
