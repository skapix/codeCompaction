//===-- TargetDependent/CommonDecisionMaker.h - Common BBFactor utils------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains PAC implementation, that always allows to continue BB
/// to factor out. It is used for BBFactor transform testing/debugging, i.e.
/// when we need to merge all blocks, independently of the profitability.
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_FORCEMERGEDECISIONMAKER_H
#define LLVMTRANSFORM_FORCEMERGEDECISIONMAKER_H

#include "IProceduralAbstractionCost.h"

/// PAC implementation is used when flag --bbfactor-force-merging is activated
class ForceMergePAC : public IProceduralAbstractionCost {
public:
  virtual void
  init(const llvm::TargetTransformInfo &TTI,
       const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) override {}
  virtual bool isTiny() const override { return false; }
  virtual bool replaceWithCall(const size_t InputArgs,
                               const size_t OutputArgs) const override {
    return true;
  }
  virtual bool replaceWithCall(const size_t BBAmount, const size_t InputArgs,
                               const size_t OutputArgs) const override {
    return true;
  }
  virtual ~ForceMergePAC() {}
};

#endif // LLVMTRANSFORM_FORCEMERGEDECISIONMAKER_H
