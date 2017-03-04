//===-- TargetDependent/CommonPAC.cpp - Common size utils---*- C++ -*------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains auxiliary target-independent information about
/// factoring out BBs. This PAC is used, when no appropriate PAC
/// was found or module contains no information about its target
///
//===----------------------------------------------------------------------===//
#include "CommonPAC.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include <set>

using namespace llvm;

// TODO: handle call insts in a special way
void CommonPAC::init(const TargetTransformInfo &TTI,
                     const SmallVectorImpl<Instruction *> &Insts) {
  this->TTI = &TTI;
  BlockWeight = 0;
  for (Instruction *I : Insts) {
    if (isSkippedInstruction(TTI, I))
      continue;
    if (auto CI = dyn_cast<CallInst>(I)) {
      BlockWeight += getFunctionCallWeight(*CI);
    }
    ++BlockWeight;
  }
}

bool CommonPAC::isTiny() const { return BlockWeight <= 2; }

bool CommonPAC::replaceWithCall(const size_t InputArgs,
                                const size_t OutputArgs) const {
  auto OriginalBlockWeight = getOriginalBlockWeight();
  return (IsTail && 1 < OriginalBlockWeight) ||
         (getNewBlockWeight(InputArgs, OutputArgs) < OriginalBlockWeight);
}

bool CommonPAC::replaceWithCall(const size_t BBAmount, const size_t InputArgs,
                                const size_t OutputArgs) const {
  assert(BBAmount >= 2);
  assert(replaceWithCall(InputArgs, OutputArgs) &&
         "BBs with failed precheck of profitability shouldn't reach here");
  if (IsTail) {
    return true;
  }

  const size_t OldCost = getOriginalBlockWeight();
  const size_t NewBlockCost = getNewBlockWeight(InputArgs, OutputArgs);
  const size_t InstsProfitBy1Replacement = OldCost - NewBlockCost;
  const size_t FunctionCreationCost =
      getFunctionCreationWeight(InputArgs, OutputArgs);

  // check if we don't lose created a function, and it's costs are lower,
  // than total gain of function replacement
  return BBAmount * InstsProfitBy1Replacement > FunctionCreationCost;
}

CommonPAC::~CommonPAC() {}

bool CommonPAC::isSkippedInstruction(const TargetTransformInfo &TTI,
                                     const llvm::Instruction *I) {

  if (TTI.getUserCost(I) == TargetTransformInfo::TCC_Free)
    return true;

  if (isa<BitCastInst>(I))
    return true;

  const IntrinsicInst *Intr = dyn_cast<IntrinsicInst>(I);
  if (Intr == nullptr)
    return false;

  using Intrinsic::ID;
  static const std::set<Intrinsic::ID> NoCodeProduction = {
      ID::donothing,
      ID::invariant_group_barrier,
      ID::instrprof_increment,
      ID::instrprof_increment_step,
      ID::instrprof_value_profile,
      ID::pcmarker};

  return NoCodeProduction.count(Intr->getIntrinsicID()) > 0;
}

size_t CommonPAC::getNewBlockWeight(const size_t InputArgs,
                                    const size_t OutputArgs) const {
  const size_t AllocaOutputs = OutputArgs > 0 ? OutputArgs - 1 : 0;
  // call cost is calculated in the following way:
  // 1 for the call function, N for storing input args in register(stack),
  // AllocaOutputs is multiplied by 2, because:
  //  1) move address to appropriate register (or push)
  //  2) after call extract value from allocated space
  return 1 + InputArgs + 2 * AllocaOutputs;
}

size_t CommonPAC::getOriginalBlockWeight() const { return BlockWeight; }

size_t CommonPAC::getFunctionCreationWeight(const size_t InputArgs,
                                            const size_t OutputArgs) const {
  const size_t OutputStores = OutputArgs > 0 ? OutputArgs - 1 : 0;
  // count storing values for each output value and assume the worst case when
  // we move values into convenient registers
  // don't forget about return instruction
  return BlockWeight + OutputStores + 1;
}

size_t CommonPAC::getFunctionCallWeight(const llvm::CallInst &Inst) {
  return 1 + Inst.getNumArgOperands();
}
