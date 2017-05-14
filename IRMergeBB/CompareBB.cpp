//===- BBComparing.cpp - Comparing basic blocks ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CompareBB.h"
#include "Utilities.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

using namespace llvm;

#define DEBUG_TYPE "compareBB"

// Test whether two basic blocks have equivalent behaviour,
// except termination instruction
int BBComparator::compareBasicBlocks(const BasicBlock *BBL,
                                     const BasicBlock *BBR) const {
  assert(BBL->size() && BBR->size() && "Sizes can't be equal to 0");
  // minimum BB size equals to 1 and if so, this instruction is TermInst,
  // that is not checked
  if (BBL->size() == 1 || BBR->size() == 1)
    return BBL->size() > BBR->size() ? 1 : BBL->size() < BBR->size() ? -1 : 0;

  auto InstL = utilities::getBeginIt(BBL), InstLE = utilities::getEndIt(BBL);
  auto InstR = utilities::getBeginIt(BBR), InstRE = utilities::getEndIt(BBR);

  while (InstL != InstLE && InstR != InstRE) {
    bool needToCmpOperands = true;
    if (int Res = cmpOperations(&*InstL, &*InstR, needToCmpOperands))
      return Res;
    if (needToCmpOperands) {
      if (int Res = compareInstOperands(&*InstL, &*InstR))
        return Res;
    }

    ++InstL;
    ++InstR;
  }

  if (InstL != InstLE && InstR == InstRE)
    return 1;
  if (InstL == InstLE && InstR != InstRE)
    return -1;
  return 0;
}

int BBComparator::compareInstOperands(const Instruction *InstL,
                                      const Instruction *InstR) const {
  assert(InstL->getNumOperands() == InstR->getNumOperands());

  int Res = 0;
  for (unsigned i = 0, e = InstL->getNumOperands(); i != e; ++i) {
    Value *OpL = InstL->getOperand(i);
    Value *OpR = InstR->getOperand(i);
    if ((Res = cmpValues(OpL, OpR)))
      break;
    // cmpValues should ensure this is true.
    assert(cmpTypes(OpL->getType(), OpR->getType()) == 0);
  }
  if (Res == 0 || !InstL->isCommutative())
    return Res;

  // op(x1,y1); op(x2,y2)
  assert(InstL->isCommutative() == InstR->isCommutative());
  assert(InstL->getNumOperands() == 2);
  if (!cmpValues(InstL->getOperand(0), InstR->getOperand(1)) &&
      !cmpValues(InstL->getOperand(1), InstR->getOperand(0)))
    return 0;

  return Res;
}

namespace {
// Accumulate the hash of a sequence of 64-bit integers. This is similar to a
// hash of a sequence of 64bit ints, but the entire input does not need to be
// available at once. This interface is necessary for functionHash because it
// needs to accumulate the hash as the structure of the function is traversed
// without saving these values to an intermediate buffer. This form of hashing
// is not often needed, as usually the object to hash is just read from a
// buffer.
class HashAccumulator64 {
  uint64_t Hash;

public:
  // Initialize to random constant, so the state isn't zero.
  HashAccumulator64() { Hash = 0x6acaa36bef8325c5ULL; }
  void add(uint64_t V) { Hash = llvm::hashing::detail::hash_16_bytes(Hash, V); }
  // No finishing is required, because the entire hash value is used.
  uint64_t getHash() { return Hash; }
};
} // end anonymous namespace

// A function hash is calculated by considering only the number of arguments and
// whether a function is varargs, the order of basic blocks (given by the
// successors of each basic block in depth first order), and the order of
// opcodes of each instruction within each of these basic blocks. This mirrors
// the strategy compare() uses to compare functions by walking the BBs in depth
// first order and comparing each instruction in sequence. Because this hash
// does not look at the operands, it is insensitive to things such as the
// target of calls and the constants used in the function, which makes it useful
// when possibly merging functions which are the same modulo constants and call
// targets.
BBComparator::BasicBlockHash
BBComparator::basicBlockHash(const BasicBlock &BB) {
  HashAccumulator64 H;
  auto IE = std::prev(BB.end());
  auto I = BB.begin();
  for (; isa<PHINode>(I); ++I)
    ;

  for (; I != IE; ++I)
    H.add(I->getOpcode());
  return H.getHash();
}

static int cmpSpecialFnAttrs(const AttributeSet LF, const AttributeSet RF) {
  Attribute::AttrKind Attributes[] = {
      Attribute::MinSize, Attribute::NoImplicitFloat, Attribute::OptimizeNone,
      Attribute::OptimizeForSize};

  for (auto A : Attributes) {
    bool L = LF.hasFnAttribute(A);
    bool R = RF.hasFnAttribute(A);
    if (L < R)
      return -1;
    if (R < L)
      return 1;
  }

  StringRef StringAttributes[] = {"target-cpu",
                                  "target-features",
                                  "correctly-rounded-divide-sqrt-fp-math",
                                  "less-precise-fpmad",
                                  "no-infs-fp-math",
                                  "no-nans-fp-math",
                                  "no-signed-zeros-fp-math",
                                  "no-trapping-math"};

  for (auto A : StringAttributes) {
    bool L = LF.hasFnAttribute(A);
    bool R = RF.hasFnAttribute(A);
    if (L < R)
      return -1;
    if (R < L)
      return 1;
  }
  return 0;
}

int BBComparator::compareBB(const BasicBlock *BBL, const BasicBlock *BBR) {
  beginCompare();
  FnL = BBL->getParent();
  FnR = BBR->getParent();
  if (int R = compareSignatures())
    return R;
  return compareBasicBlocks(BBL, BBR);
}

int BBComparator::compareSignatures() const {
  if (int Res = cmpSpecialFnAttrs(FnL->getAttributes(), FnR->getAttributes()))
    return Res;

  if (int Res = cmpNumbers(FnL->hasGC(), FnR->hasGC()))
    return Res;

  if (FnL->hasGC()) {
    if (int Res = cmpMem(FnL->getGC(), FnR->getGC()))
      return Res;
  }

  if (int Res = cmpNumbers(FnL->hasSection(), FnR->hasSection()))
    return Res;

  if (FnL->hasSection()) {
    if (int Res = cmpMem(FnL->getSection(), FnR->getSection()))
      return Res;
  }
  return 0;
}
