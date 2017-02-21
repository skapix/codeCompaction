//===-- TargetDependent/DecisionMaker_arm.h - Arm DM-----------------------===//
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

#ifndef LLVMTRANSFORM_DECISIONMAKER_ARM_H
#define LLVMTRANSFORM_DECISIONMAKER_ARM_H

#include "CommonDecisionMaker.h"

class DecisionMaker_arm : public CommonDecisionMaker {
public:
  virtual void
  init(const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) override;
  virtual ~DecisionMaker_arm() {}
};

// TODO: implement
void DecisionMaker_arm::init(
    const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) {
  CommonDecisionMaker::init(Insts);
}

#endif // LLVMTRANSFORM_DECISIONMAKER_ARM_H
