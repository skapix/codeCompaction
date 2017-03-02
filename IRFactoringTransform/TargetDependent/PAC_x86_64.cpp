//===-- TargetDependent/PAC_x86_64.cpp - x86_64 Procedural analysis cost---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PAC_x86_64.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

bool checkLoadStore(SmallVectorImpl<Instruction *>::const_iterator Start,
                    SmallVectorImpl<Instruction *>::const_iterator End) {
  if (std::distance(Start, End) < 3)
    return false;
  if (!isa<LoadInst>(*Start) || !isa<StoreInst>(*(Start + 2)))
    return false;

  const LoadInst *L = cast<LoadInst>(*Start);
  const StoreInst *S = cast<StoreInst>(*(Start + 2));
  if (L->getPointerOperand() != S->getPointerOperand())
    return false;

  // TODO: investigate cases, when load, *, store code produce 1 instruction
  // now it handles just cases, like:
  // add i64 %val, 1
  // sub i32 %val, 1

  if (!isa<BinaryOperator>(*(Start + 1)))
    return false;
  const BinaryOperator *BinOp = cast<BinaryOperator>(*(Start + 1));
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

size_t
getCombinedCost(SmallVectorImpl<Instruction *>::const_iterator &Beg,
                const SmallVectorImpl<Instruction *>::const_iterator &End) {
  if (checkLoadStore(Beg, End)) {
    Beg += 3 - 1;
    return 1;
  }
  return 0;
}

void PAC_x86_64::init(const TargetTransformInfo &TTI,
                      const SmallVectorImpl<Instruction *> &Insts) {
  assert(!Insts.empty() && "Should not reach here");
  this->TTI = &TTI;

  BlockWeight = 0;
  HasAlloca = false;
  size_t HalfWeight = 0;
  for (auto It = Insts.begin(), EIt = Insts.end(); It != EIt; ++It) {
    size_t Cost = getCombinedCost(It, EIt);
    if (Cost) {
      BlockWeight += Cost;
      continue;
    }

    const Instruction *I = *It;
    if (isSkippedInstruction(TTI, I))
      continue;
    switch (I->getOpcode()) {
    case Instruction::Alloca:
      HasAlloca = true;
      break;
    case Instruction::Call:
      BlockWeight += getFunctionCallWeight(cast<CallInst>(*I));
    // case Instruction::GetElementPtr:
    // GEP is usually not used in MC, because of relative addressing in x86-64
    //   break;

    // Heuristics
    // case Instruction::Load:
    // case Instruction::Store:
    //  ++HalfWeight;
    // break;
    default:
      ++BlockWeight;
    }
  }

  BlockWeight += (HalfWeight + 1) / 2;
  IsLastCmp = Insts.back()->getOpcode() == Instruction::ICmp;
}

bool PAC_x86_64::isTiny() const {
  return BlockWeight <= 2 + static_cast<size_t>(IsLastCmp);
}

size_t PAC_x86_64::getNewBlockWeight(const size_t InputArgs,
                                     const size_t OutputArgs) const {
  return CommonPAC::getNewBlockWeight(InputArgs, OutputArgs) + IsLastCmp;
}

size_t PAC_x86_64::getOriginalBlockWeight() const {
  return CommonPAC::getOriginalBlockWeight() + HasAlloca * 2;
}

size_t PAC_x86_64::getFunctionCreationWeight(const size_t InputArgs,
                                             const size_t OutputArgs) const {
  // isLastCmp is added because of comparing results is necessary
  // to store in register.
  // It is used because usually additional instruction is added: sete %al
  return CommonPAC::getFunctionCreationWeight(InputArgs, OutputArgs) +
         HasAlloca * 2 + IsLastCmp;
}
