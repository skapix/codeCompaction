//===-- IDecisionMaker.cpp - DM Factory--------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "IProceduralAbstractionCost.h"
#include "TargetDependent/CommonPAC.h"
#include "TargetDependent/PAC_arm.h"
#include "TargetDependent/PAC_x86_64.h"
#include <llvm/Support/Debug.h>

using namespace llvm;

std::unique_ptr<IProceduralAbstractionCost>
IProceduralAbstractionCost::Create(const StringRef Arch) {
  // TODO: if is far not the best option here. Fix it
  if (Arch == "")
    return std::make_unique<CommonPAC>();
  if (Arch == "x86_64")
    return std::make_unique<PAC_x86_64>();
  if (Arch.size() >= 3 && Arch.substr(0, 3) == "arm")
    return std::make_unique<PAC_arm>();

  dbgs() << "Warning! Unknown architecture: " << Arch << "\n";
  dbgs() << "  For greater pass impact of code compaction, please, write a "
            "function"
            "for this arch, which calculates an Instruction weight.";

  return std::make_unique<CommonPAC>();
}

void IProceduralAbstractionCost::setTail(const bool IsReallyTail) {
  IsTail = IsReallyTail;
}
