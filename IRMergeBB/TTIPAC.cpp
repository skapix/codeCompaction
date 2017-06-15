//
// Created by skap on 6/16/17.
//

#include <llvm/Analysis/TargetTransformInfo.h>
#include "IProceduralAbstractionCost.h"
#include "TTIPAC.h"

using namespace llvm;

void TTIPAC::init(const TargetTransformInfo &TTI, const InstructionLocation &IL,
                  const BasicBlock::const_iterator &Begin,
                  const BasicBlock::const_iterator &End) {
  this->TTI = &TTI;
  OriginalBlockWeight = 0;
  NewBlockAddWeight = 0;
  FunctionWeight = 0;

  size_t i = 0;
  for (auto It = Begin; It != End; ++It, ++i) {
    const Instruction *I = &*It;
    const size_t InstWeight = static_cast<size_t>(TTI.getUserCost(I));
    if (InstWeight)
      addWeight(IL, InstWeight, i);
  }
}
