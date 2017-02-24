//===-- TargetDependent/CommonDecisionMaker.cpp - Common utils---*- C++ -*-===//
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
/// factoring out BBs. These decision maker (DM) is used, when no appropriate DM
/// was found or module contains no information about its target
///
//===----------------------------------------------------------------------===//
#include "CommonDecisionMaker.h"
#include <cstddef>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/Debug.h>
#include <set>

using namespace llvm;

void CommonDecisionMaker::init(const SmallVectorImpl<Instruction *> &Insts) {
  BlockWeight = 0;
  for (Instruction *I : Insts) {
    if (isCommonlySkippedInstruction(I))
      continue;
    ++BlockWeight;
  }
}

bool CommonDecisionMaker::isTiny() const { return BlockWeight <= 2; }

bool CommonDecisionMaker::replaceNoFunction(const size_t InputArgs,
                                            const size_t OutputArgs) const {
  return getNewBlockWeight(InputArgs, OutputArgs) < getOriginalBlockWeight();
}

bool CommonDecisionMaker::replaceWithFunction(const size_t BBAmount,
                                              const size_t InputArgs,
                                              const size_t OutputArgs) const {
  assert(BBAmount >= 2);
  assert(replaceNoFunction(InputArgs, OutputArgs) &&
         "BBs with failed precheck of profitability shouldn't reach here");

  const size_t OldCost = getOriginalBlockWeight();
  const size_t NewBlockCost = getNewBlockWeight(InputArgs, OutputArgs);
  const size_t InstsProfitBy1Replacement =  OldCost - NewBlockCost;
  const size_t FunctionCreationCost = getFunctionCreationWeight(InputArgs,
                                                                OutputArgs);

  // check if we don't lose created a function, and it's costs are lower,
  // than total gain of function replacement
  return BBAmount * InstsProfitBy1Replacement > FunctionCreationCost;
}

CommonDecisionMaker::~CommonDecisionMaker() {}


bool CommonDecisionMaker::isCommonlySkippedInstruction(
  const llvm::Instruction *I) {
  if (isa<BitCastInst>(I))
    return true;

  const IntrinsicInst *Intr = dyn_cast<IntrinsicInst>(I);
  if (Intr == nullptr)
    return false;

  using Intrinsic::ID;
  static const std::set<Intrinsic::ID> NoCodeProduction = {
      ID::lifetime_start,
      ID::lifetime_end,
      ID::donothing,
      ID::invariant_start,
      ID::invariant_end,
      ID::invariant_group_barrier,
      ID::var_annotation,
      ID::ptr_annotation,
      ID::annotation,
      ID::dbg_declare,
      ID::dbg_value,
      ID::assume,
      ID::instrprof_increment,
      ID::instrprof_increment_step,
      ID::instrprof_value_profile,
      ID::pcmarker};

  return NoCodeProduction.count(Intr->getIntrinsicID()) > 0;
}


size_t CommonDecisionMaker::getNewBlockWeight(const size_t InputArgs,
                                              const size_t OutputArgs) const {
  const size_t AllocaOutputs = OutputArgs > 0 ? OutputArgs - 1 : 0;
  const bool HasAlloca = AllocaOutputs > 0;
  // call cost is calculated in the following way:
  // 1 for the call function, store input args in register(stack),
  // ContainsOutput for allocating space (consider the worst case)
  // AllocaOutputs multiplied by 2, because:
  //  1) move address to appropriate register (or push)
  //  2) after call extract value from allocated space
  return 1 + HasAlloca + InputArgs + 2 * AllocaOutputs;
}

size_t CommonDecisionMaker::getOriginalBlockWeight() const {
  return BlockWeight;
}

size_t CommonDecisionMaker::getFunctionCreationWeight(const size_t InputArgs,
                                                      const size_t OutputArgs) const {
  const size_t OutputStores = OutputArgs > 0 ? OutputArgs - 1 : 0;
  // count storing values for each output value and assume the worst case when
  // we move values into convenient registers
  // don't forget about return instruction
  return BlockWeight + OutputStores + 1;
}
