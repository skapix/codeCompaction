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
#include <llvm/IR/IRBuilder.h>

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

namespace {

std::vector<const Value*> getInput(const BasicBlock *BB) {
  std::set<const Value*> values;
  std::vector<const Value*> result;
  for (auto &I : BB->getInstList()) {
    values.insert(&I);
    if (isa<TerminatorInst>(I))
      continue;

    for (auto& Ops : I.operands()) {
      if (isa<Constant>(Ops.get()))
        continue;
      if (values.find(Ops.get()) == values.end())
      {
        result.push_back(Ops.get());
        values.insert(Ops.get());
      }
    }
  }
  return result;
}

std::vector<const Value *> getOutput(const BasicBlock *BB) {
  std::vector<const Value*> result;
  for (auto &I : BB->getInstList()) {
    if (isa<TerminatorInst>(&I)) {
      for (auto &Ops : I.operands()) {
        if (!isa<BasicBlock>(Ops.get())) {
          result.push_back(Ops.get());
        }
      }
      continue;
    }
    if (I.isUsedOutsideOfBlock(BB)) {
      result.push_back(&I);
    }
  }
  return result;
}

// TODO: set output params
Function *createFuncFromBB(BasicBlock *BB, const std::vector<const Value*>& Input) {
  Module *M = BB->getModule();
  // create Function
  std::vector<Type*> Params;
  Params.reserve(Input.size());

  std::transform(Input.cbegin(), Input.cend(), back_inserter(Params), [](const Value *V) { return V->getType(); });
  llvm::FunctionType *ft = FunctionType::get(llvm::Type::getVoidTy(M->getContext()), Params, false);
  llvm::Function * f = Function::Create(ft, llvm::GlobalValue::LinkageTypes::InternalLinkage, "", M);
  // add some attributes ?

  // create auxiliary Map to params
  DenseMap<const Value*, Value*> InputToArgs;
  {
    auto ArgIt = f->arg_begin();

    for (auto It = Input.cbegin(); It != Input.cend(); ++It) {
      InputToArgs.insert(std::make_pair(*It, &*ArgIt++));
    }
  }
  // start filling function
  BasicBlock *NewBB = BasicBlock::Create(M->getContext(), "Entry", f);
  IRBuilder<> Builder(NewBB);
  for (auto &I : BB->getInstList()) {
    if (isa<TerminatorInst>(I)) {
      continue;
    }
    Instruction *Inserted = Builder.Insert(I.clone(), "");
    InputToArgs.insert(std::make_pair(&I, Inserted));
    for (auto &Op : Inserted->operands()) {
      auto it = InputToArgs.find(Op.get());
      if (it != InputToArgs.end()) {
        Op.set(it->second);
      }
    }
  }
  Builder.CreateRetVoid();
  //
  return f;
}


} // namespace

bool BBFactoring::replace(const std::vector<BasicBlock *> BBs) {
  if (BBs.front()->size() <= 4) {
    DEBUG(dbgs() << "Block " << BBs.front()->getName() << " in function "
                 << BBs.front()->getParent()->getName() << " is to small to bother merging\n");
    return false;
  }

  for (auto BB : BBs) {

  }
  // merge not implemented yet

  return false;
}


//bool Instruction::isUsedOutsideOfBlock