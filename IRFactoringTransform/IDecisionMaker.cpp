//===-- IDecisionMaker.cpp - DM Factory--------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "IDecisionMaker.h"
#include "TargetDependent/CommonDecisionMaker.h"
#include "TargetDependent/DecisionMaker_arm.h"
#include "TargetDependent/DecisionMaker_x86_64.h"
#include <llvm/Support/Debug.h>

using namespace llvm;

std::unique_ptr<IDecisionMaker> IDecisionMaker::Create(const StringRef Arch) {
  // TODO: if is far not the best option here. Fix it
  if (Arch == "")
    return std::make_unique<CommonDecisionMaker>();
  if (Arch == "x86_64")
    return std::make_unique<DecisionMaker_x86_64>();
  if (Arch == "arm")
    return std::unique_ptr<DecisionMaker_arm>();

  dbgs() << "Warning! Unknown architecture: " << Arch << "\n";
  dbgs() << "  For greater pass impact of code compaction, please, write a "
            "function"
            "for this arch, which calculates an Instruction weight.";

  return std::make_unique<CommonDecisionMaker>();
}