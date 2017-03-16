//===-- TargetDependent/PAC_x86_64.cpp - x86_64 Procedural analysis cost---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PAC_x86_64.h"
#include "llvm/Analysis/TargetTransformInfo.h"

using namespace llvm;

bool checkLoadStore(BasicBlock::const_iterator Start,
                    BasicBlock::const_iterator End) {
  if (std::distance(Start, End) < 3)
    return false;
  const Instruction *PLoad = &*Start++;
  const Instruction *POp = &*Start++;
  const Instruction *PStore = &*Start;

  if (!isa<LoadInst>(PLoad) || !isa<StoreInst>(PStore))
    return false;

  const LoadInst *L = cast<LoadInst>(PLoad);
  const StoreInst *S = cast<StoreInst>(PStore);
  if (L->getPointerOperand() != S->getPointerOperand())
    return false;

  // TODO: investigate cases, when load, *, store code produce 1 instruction
  // now it handles just cases, like:
  // add i64 %val, 1
  // sub i32 %val, 1

  if (!isa<BinaryOperator>(POp))
    return false;
  const BinaryOperator *BinOp = cast<BinaryOperator>(POp);
  if (BinOp->getOpcode() == BinaryOperator::BinaryOps::Add ||
      BinOp->getOpcode() == BinaryOperator::BinaryOps::Sub) {
    // check for increment and decrement of the value
    // this case does not handle instuctions like: 1 + %value
    auto SecondOp = dyn_cast<ConstantInt>(BinOp->getOperand(1));
    if (BinOp->getOperand(0) == L && SecondOp) {
      return SecondOp->isOne() || SecondOp->isMinusOne();
    }
  }

  return false;
}

size_t getCombinedCost(BasicBlock::const_iterator &Beg,
                       const BasicBlock::const_iterator &End) {
  if (checkLoadStore(Beg, End)) {
    std::advance(Beg, 3 - 1);
    return 1;
  }
  return 0;
}

void PAC_x86_64::init(const llvm::TargetTransformInfo &TTI,
                      const InstructionLocation &IL,
                      const llvm::BasicBlock::const_iterator &Begin,
                      const llvm::BasicBlock::const_iterator &End) {
  assert(std::distance(Begin, End) >= 1 && "Should not reach here");
  this->TTI = &TTI;
  OriginalBlockWeight = 0;
  NewBlockAddWeight = 0;
  FunctionWeight = 0;

  // usually in x86_64 all allocas are combined in 1 alloca instruction
  // (add rsp, ?). Also in the end of routine substracts previously
  // added value.
  bool HasAllocaInFunc = false;
  bool HasAllocaOutside = false;

  size_t i = 0;
  for (auto It = Begin; It != End; ++It, ++i) {
    size_t Cost = getCombinedCost(It, End);
    if (Cost) {
      addWeight(IL, Cost, i);
      continue;
    }

    const Instruction *I = &*It;
    if (isSkippedInstruction(TTI, I))
      continue;

    Cost = 0;

    switch (I->getOpcode()) {
    case Instruction::Alloca:
      HasAllocaInFunc |= IL.isUsedInsideFunction(i);
      HasAllocaOutside |= IL.isUsedOutsideFunction(i);
      break;
    case Instruction::Call:
      Cost = getFunctionCallWeight(*cast<CallInst>(I));
      break;
    default:
      Cost = 1;
    }
    if (Cost)
      addWeight(IL, Cost, i);

  } // for

  if (HasAllocaInFunc)
    FunctionWeight += 2;
  if (HasAllocaOutside)
    NewBlockAddWeight += 2;
  if (HasAllocaInFunc || HasAllocaOutside)
    OriginalBlockWeight += 2;

  // Handle last ICmp instruction in a special way, because x86 perform
  // jumps according to flags. Since then, if instruction returns boolean value:
  // For new BB: processor has to set flags one more time
  // For function: move from flag registers into ax: sete %al
  const Instruction *PCMP = &*getLastFuncInst(IL, Begin, End);
  bool IsLastCmp = PCMP->getOpcode() == Instruction::ICmp;

  if (IsLastCmp) {
    ++FunctionWeight;
    ++NewBlockAddWeight;
  }
}

size_t PAC_x86_64::getNewBlockWeight(const size_t InputArgs,
                                     const size_t OutputArgs) const {
  const bool HasAlloca = OutputArgs > 1;
  const size_t AmountArgs = InputArgs + OutputArgs - HasAlloca;
  const size_t Penalty = AmountArgs <= 4 ? 0 : AmountArgs - 4;

  return CommonPAC::getNewBlockWeight(InputArgs, OutputArgs) + HasAlloca +
         2 * Penalty;
}

size_t PAC_x86_64::getFunctionCallWeight(const llvm::CallInst &Inst) {
  auto Sz = TTI->getUserCost(&Inst);
  if (Sz == TargetTransformInfo::TCC_Free)
    return 0;
  else if (Sz == TargetTransformInfo::TCC_Basic)
    return 1;

  size_t OperandStores = 0;

  for (auto &Op : Inst.arg_operands()) {
    auto I = dyn_cast<Instruction>(Op.get());
    if (!I) {
      ++OperandStores;
      continue;
    }

    // skip these because of lea instruction was already counted
    if (isa<LoadInst>(I) && I->getParent() == Inst.getParent())
      continue;
    ++OperandStores;
  }

  return 1 + OperandStores;
}
