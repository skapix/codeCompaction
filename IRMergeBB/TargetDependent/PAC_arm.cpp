//===-- TargetDependent/PAC_arm.h - Arm Procedural analysis cost-----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PAC_arm.h"

using namespace llvm;


void PAC_arm::init(const TargetTransformInfo &TTI,
                   const InstructionLocation &IL,
                   const BasicBlock::const_iterator &Begin,
                   const BasicBlock::const_iterator &End) {
  CommonPAC::init(TTI, IL, Begin, End);

  size_t LoadInsts = 0;
  for (auto It = Begin; It != End; ++It) {
    if (isa<LoadInst>(It))
      ++LoadInsts;
  }
  // Load insts usually are not loaded and it is convinient to use it
  // as a heuristic penalty
  const size_t Penalty = LoadInsts;
  OriginalBlockWeight = OriginalBlockWeight  < Penalty ? 0 : OriginalBlockWeight - Penalty;

  // because of arm flag register we need to add one more instruction
  const Instruction *PCI = &*getLastFuncInst(IL, Begin, End);
  if (isa<CmpInst>(PCI))
    ++NewBlockAddWeight;
}

size_t PAC_arm::getNewBlockWeight(const size_t InputArgs,
                                  const size_t OutputArgs) const {
  // output calculation:
  // 1st output is free because it is usually returned by a function
  // 2nd output costs 3:
  //  1) Get variable address
  //  2) Set register to allocated memory
  //  3) Store parameter from stack pointer into register
  const size_t ParamOutputs = OutputArgs > 0 ? OutputArgs - 1 : 0;
  const size_t Penalty = OutputArgs > 0 ? OutputArgs - 1 : 0;
  const size_t Result = NewBlockAddWeight + 1 + InputArgs + 3 * ParamOutputs + Penalty;
  if (AddBlockWeight < 0 && Result < static_cast<size_t>(-AddBlockWeight))
    return 0;
  return static_cast<size_t>(Result + AddBlockWeight);
}