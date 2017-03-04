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

#ifndef LLVMTRANSFORM_DECISIONMAKER_X86_64_H
#define LLVMTRANSFORM_DECISIONMAKER_X86_64_H

#include "CommonPAC.h"
#include "llvm/IR/Instruction.h"

class PAC_x86_64 : public CommonPAC {
public:
  virtual void
  init(const llvm::TargetTransformInfo &TTI,
       const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) override;

  virtual size_t getOriginalBlockWeight() const;

  virtual size_t getNewBlockWeight(const size_t InputArgs,
                                   const size_t OutputArgs) const override;

  virtual size_t
  getFunctionCreationWeight(const size_t InputArgs,
                            const size_t OutputArgs) const override;

  virtual ~PAC_x86_64() {}

private:
  size_t getFunctionCallWeight(const llvm::CallInst &Inst);
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

#endif // LLVMTRANSFORM_DECISIONMAKER_X86_64_H
