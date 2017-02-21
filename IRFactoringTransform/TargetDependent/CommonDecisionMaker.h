//===-- TargetDependent/CommonDecisionMaker.h - Common BBFactor utils------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_WEIGHT_H
#define LLVMTRANSFORM_WEIGHT_H
#include "../IDecisionMaker.h"

class CommonDecisionMaker : public IDecisionMaker {
public:
  virtual void
  init(const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) override;
  virtual bool isTiny() const override;
  virtual bool replaceNoFunction(const size_t InputArgs,
                                 const size_t OutputArgs) const override;
  virtual bool replaceWithFunction(const size_t BBAmount,
                                   const size_t InputArgs,
                                   const size_t OutputArgs) const override;
  virtual ~CommonDecisionMaker();

protected:
  /// Provides us with information, when Instruction does not
  /// produce machine code in architecture (e.g BitCastInst)
  /// \param I instruction
  /// \return true, if this instruction has a mapping to machine code
  static bool isCommonlySkippedIntruction(const llvm::Instruction *I);

protected:
  size_t BlockWeight;
};

#endif // LLVMTRANSFORM_WEIGHT_H
