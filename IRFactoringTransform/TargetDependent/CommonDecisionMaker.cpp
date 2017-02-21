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
    if (isCommonlySkippedIntruction(I))
      continue;
    ++BlockWeight;
  }
}

bool CommonDecisionMaker::isTiny() const { return BlockWeight <= 2; }

bool CommonDecisionMaker::replaceNoFunction(const size_t InputArgs,
                                            const size_t OutputArgs) const {
  const size_t AllocaOutputs = OutputArgs < 1 ? 0 : OutputArgs - 1;
  size_t CallCost = 1 + 2 * AllocaOutputs + InputArgs;
  return BlockWeight > CallCost;
}

bool CommonDecisionMaker::replaceWithFunction(const size_t BBAmount,
                                              const size_t InputArgs,
                                              const size_t OutputArgs) const {
  assert(BBAmount >= 2);
  assert(replaceNoFunction(InputArgs, OutputArgs) &&
         "BBs with failed precheck of profitability shouldn't reach here");

  const size_t CallCost = 1 + 2 * OutputArgs + InputArgs;
  const size_t InstsProfitBy1Replacement = BlockWeight - CallCost;
  // count storing values for each output value and assume the worst case when
  // we move values into convenient registers
  const size_t FunctionCreationCost = BlockWeight + 2 * OutputArgs + InputArgs;
  // check if we don't lose created a function, and it's costs are lower,
  // than total gain of function replacement
  return BBAmount * InstsProfitBy1Replacement - FunctionCreationCost > 0;
}

CommonDecisionMaker::~CommonDecisionMaker() {}

bool CommonDecisionMaker::isCommonlySkippedIntruction(
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
