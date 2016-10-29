//===- BBFactoring.cpp - Merge identical functions ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include "../external/merging.h"

#include <forward_list>
#include <set>

#define DEBUG_TYPE "bbfactor"

// TODO: add merging
// TODO: tests tests tests
// TODO: llvm code style: http://llvm.org/docs/CodingStandards.html



using namespace llvm;

namespace {

class BBNode {
  mutable AssertingVH<BasicBlock> BB;
  BBComparator::BasicBlockHash Hash;
public:
  // Note the hash is recalculated potentially multiple times, but it is cheap.
  BBNode(BasicBlock *BB)
      : BB(BB), Hash(BBComparator::basicBlockHash(*BB, false)) {}

  BasicBlock *getBB() const { return BB; }

  BBComparator::BasicBlockHash getHash() const { return Hash; }

  /// Replace the reference to the function F by the function G, assuming their
  /// implementations are equal.
  void replaceBy(BasicBlock *G) const {
    BB = G;
  }

  void release() { BB = nullptr; }
};

} // namespace

namespace {

void printUses(const BasicBlock &block) {
  if (!block.hasNUsesOrMore(1)) {
    errs() << "No Uses\n";
    return;
  }
  errs() << "Uses:\n";
  int i = 0;
  for (auto &U : block.uses()) {
    errs() << "Use " << i++ << "\n";
    U.getUser()->print(errs());
    errs() << '\n';
  }

}

void printFunctionBlocks(const Function &function) {
  if (function.isDeclaration())
    return;
  errs() << "Function: ";
  errs().write_escaped(function.getName());
  errs() << '\n';
  int i = 0;
  for (auto &BB: function.getBasicBlockList()) {
    printUses(BB);
    errs() << "Block " << i++ << "\n";
    BB.print(errs());
    errs() << '\n';
  }
}

void printBlocks(const SmallVectorImpl<const BasicBlock *> &BBVector) {
  int i = 0;
  for (auto &BB: BBVector) {
    errs() << "Block " << i++ << "\n";
    BB->print(errs());
    errs() << '\n';
  }
}


class BBFactoring : public ModulePass {
public:
  static char ID;

  BBFactoring() : ModulePass(ID), FnTree(BBNodeCmp(&GlobalNumbers)) {}

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const {
    //AU.setPreservesCFG();
    //AU.addRequired<LoopInfoWrapperPass>();
    // preserve nothing
  }

private:

  bool insert(BasicBlock *NewBB);
  //void remove(BasicBlock *F);
  //void replaceBBInTree(const BBNode &BBN, BasicBlock *G);

  class BBNodeCmp {
    GlobalNumberState* GlobalNumbers;
  public:
    BBNodeCmp (GlobalNumberState* GN) : GlobalNumbers(GN) {}
    bool operator()(const BBNode &LHS, const BBNode &RHS) const {
      // Order first by hashes, then full function comparison.
      if (LHS.getHash() != RHS.getHash())
        return LHS.getHash() < RHS.getHash();
      BBComparator BBCmp(LHS.getBB(), RHS.getBB(), GlobalNumbers);
      return BBCmp.compare() == -1;
    }
  };

  typedef std::set<BBNode, BBNodeCmp> FnTreeType;

  FnTreeType FnTree;

  ValueMap<BasicBlock*, FnTreeType::iterator> BNodesInTree;

  GlobalNumberState GlobalNumbers;

};

} // namespace

char BBFactoring::ID = 0;
static RegisterPass<BBFactoring> X("bbfactor", "BBFactoring Pass", false, false);

bool BBFactoring::runOnModule(Module &M) {
  if (skipModule(M))
    return false;

  DEBUG(dbgs() << "Module name: ");
  DEBUG(dbgs().write_escaped(M.getName()) << '\n');
  std::vector<std::pair<BBComparator::BasicBlockHash, BasicBlock *>> HashedBBs;


  // calculate fingerprints for all basic blocks in every function
  for (auto &F : M.functions()) {
    if (!F.isDeclaration() && !F.hasAvailableExternallyLinkage()) {
      for (auto &BB : F.getBasicBlockList())
        HashedBBs.push_back({BBComparator::basicBlockHash(BB, false), &BB});
    }
  }

  std::stable_sort(
      HashedBBs.begin(), HashedBBs.end(),
      [](const std::pair<BBComparator::BasicBlockHash, BasicBlock *> &a,
         const std::pair<BBComparator::BasicBlockHash, BasicBlock *> &b) {
        return a.first < b.first;
      });

  std::vector<WeakVH> Deferred;

  auto S = HashedBBs.begin();
  for (auto I = HashedBBs.begin(), IE = HashedBBs.end(); I != IE; ++I) {
    // If the hash value matches the previous value or the next one, we must
    // consider merging it. Otherwise it is dropped and never considered again.
    if ((I != S && std::prev(I)->first == I->first) ||
        (std::next(I) != IE && std::next(I)->first == I->first)) {
      Deferred.push_back(WeakVH(I->second));
    }
  }


  bool Changed = false;

  for (WeakVH &I : Deferred) {
    if (!I)
      continue;
    BasicBlock *BB = cast<BasicBlock>(I);
    Changed |= insert(BB);
  }

  return Changed;

}

bool BBFactoring::insert(BasicBlock *NewBB) {
  std::pair<FnTreeType::iterator, bool> Result =
      FnTree.insert(BBNode(NewBB));

  if (Result.second) {
    assert(BNodesInTree.count(NewBB) == 0);
    BNodesInTree.insert({NewBB, Result.first});
    DEBUG(dbgs() << "Inserting as unique: " << NewBB->getName() << '\n');
    return false;
  }

  // have matched basic blocks
  // merge them

  const BBNode &OldBB = *Result.first;

  // Don't merge tiny basic blocks, since it can just end up making code
  // larger.
    if (NewBB->size() <= 3) {
      DEBUG(dbgs() << NewBB->getName()
                   << " is to small to bother merging\n");
      return false;
    }

  DEBUG(dbgs() << "Matched blocks: \n");
  DEBUG(NewBB->print(dbgs()));
  DEBUG(dbgs() << "\n");
  DEBUG(OldBB.getBB()->print(dbgs()));

  // merging not implemented

  return false;
}

//bool Instruction::isUsedOutsideOfBlock