//===- BBFactoring.cpp - Merge identical basic blocks ---------------------===//
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

#include <set>
#include <map>

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

  BBNode(BasicBlock *BB, const BBComparator::BasicBlockHash Hash)
      : BB(BB), Hash(Hash) {}

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

  BBFactoring() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;

private:
  bool replace(const std::vector<BasicBlock *> BBs);


  class BBNodeCmp {
    GlobalNumberState *GlobalNumbers;
  public:
    BBNodeCmp(GlobalNumberState *GN) : GlobalNumbers(GN) {}

    bool operator()(const BBNode &LHS, const BBNode &RHS) const {
      // Order first by hashes, then full function comparison.
      if (LHS.getHash() != RHS.getHash())
        return LHS.getHash() < RHS.getHash();
      BBComparator BBCmp(LHS.getBB(), RHS.getBB(), GlobalNumbers);
      return BBCmp.compare() == -1;
    }
  };

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


  std::vector<std::vector<BasicBlock *>> IdenticalBlocksContainer;

  // avoid MSVS compiler bug
  auto BBTree = std::map<BBNode, std::vector<BasicBlock *>::size_type,
      BBNodeCmp>(BBNodeCmp(&GlobalNumbers));

  for (auto it = HashedBBs.begin(), itEnd = HashedBBs.end(); it != itEnd; ++it) {
    auto Element = BBTree.insert(std::make_pair(BBNode(it->second, it->first), IdenticalBlocksContainer.size()));
    if (Element.second) {
      IdenticalBlocksContainer.push_back(std::vector<BasicBlock *>(1, Element.first->first.getBB()));
    } else {
      IdenticalBlocksContainer[Element.first->second].push_back(Element.first->first.getBB());
    }
  }

  bool Changed = false;

  for (auto &IdenticalBlocks : IdenticalBlocksContainer) {
    if (IdenticalBlocks.size() >= 2) {
      Changed |= replace(IdenticalBlocks);
    }
  }

  return Changed;

}


bool BBFactoring::replace(const std::vector<BasicBlock *> BBs) {
  if (BBs.front()->size() <= 4) {
    DEBUG(dbgs() << "Block " << BBs.front()->getName() << " in function "
                 << BBs.front()->getParent()->getName() << " is to small to bother merging\n");
    return false;
  }


  // merge not implemented yet

  return false;
}


//bool Instruction::isUsedOutsideOfBlock