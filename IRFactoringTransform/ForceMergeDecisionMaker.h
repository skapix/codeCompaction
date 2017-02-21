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
/// This file contains Decision Maker(DM), that always allows to continue BB
/// to factor out. It is used for BBFactor transform testing/debugging, i.e.
/// when we need to merge all blocks, independently of the profitability.
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_FORCEMERGEDECISIONMAKER_H
#define LLVMTRANSFORM_FORCEMERGEDECISIONMAKER_H

#include "IDecisionMaker.h"

/// DM is used when flag --bbfactor-force-merging is activated
class ForceMergeDecisionMaker : public IDecisionMaker {
public:
  virtual void
  init(const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) override {}
  virtual bool isTiny() const override { return false; }
  virtual bool replaceNoFunction(const size_t InputArgs,
                                 const size_t OutputArgs) const override {
    return true;
  }
  virtual bool replaceWithFunction(const size_t BBAmount,
                                   const size_t InputArgs,
                                   const size_t OutputArgs) const override {
    return true;
  }
  virtual ~ForceMergeDecisionMaker() {}
};

#endif // LLVMTRANSFORM_FORCEMERGEDECISIONMAKER_H
