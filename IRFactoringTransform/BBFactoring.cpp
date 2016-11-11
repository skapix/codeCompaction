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


// TODO: Fix assert with uses
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

SmallVector<Value *, 8> getInput(const BasicBlock *BB) {
  std::set<const Value *> values;
  SmallVector<Value *, 8> result;
  for (auto &I : BB->getInstList()) {
    values.insert(&I);
    if (isa<TerminatorInst>(I))
      continue;

    for (auto &Ops : I.operands()) {
      if (isa<Constant>(Ops.get()))
        continue;
      if (values.find(Ops.get()) == values.end()) {
        result.push_back(Ops.get());
        values.insert(Ops.get());
      }
    }
  }
  return result;
}

SmallVector<size_t, 8> getOutput(const BasicBlock *BB) {
  SmallVector<size_t, 8> result;
  std::map<const Value *, const size_t> auxiliary;
  size_t current = 0;
  for (auto I = BB->begin(); I != BB->end(); ++I, ++current) {
    auxiliary.insert(std::make_pair(&*I, current));
    if (isa<TerminatorInst>(&*I)) {
      for (auto &Ops : I->operands()) {
        auto found = auxiliary.find(Ops.get());
        if (found != auxiliary.end()) {
          result.push_back(found->second);
        }
      }
      continue;
    }
    if (I->isUsedOutsideOfBlock(BB)) {
      result.push_back(current);
    }
  }
  return result;
}

/// Function finds Positions of 'smaller' elements in 'greater' array
SmallVector<size_t, 8> findPoses(const ArrayRef<size_t>& smaller,
                                 const ArrayRef<size_t>& greater) {
  SmallVector<size_t, 8> result;
  result.reserve(smaller.size());
  size_t currentPos = 0;
  for (auto SIt = smaller.begin(), GIt = greater.begin(); SIt != smaller.end(); ++GIt, ++currentPos) {
    if (*GIt == *SIt) {
      result.push_back(currentPos);
      ++SIt;
    }
  }
  return result;
}

SmallVector<Value *, 8> convertOutput(BasicBlock *BB, const SmallVectorImpl<size_t> &numInstr) {
  size_t current = 0;
  auto currentNum = numInstr.begin();
  SmallVector<Value *, 8> result;
  for (auto I = BB->begin(); I != BB->end() && currentNum != numInstr.end(); ++I, ++current) {
    if (*currentNum == current) {
      result.push_back(&*I);
      ++currentNum;
    }
  }
  return result;
}

// TODO: Add return value if 1 output argument of first class type
Function *createFuncFromBB(BasicBlock *BB, const SmallVectorImpl<Value *> &Input,
                           const SmallVectorImpl<Value *> &Output) {
  Module *M = BB->getModule();
  // create Function
  SmallVector<Type *, 16> Params;
  Params.reserve(Input.size() + Output.size());

  std::transform(Input.begin(), Input.end(), std::back_inserter(Params),
                 [](const Value *V) { return V->getType(); });
  std::transform(Output.begin(), Output.end(), std::back_inserter(Params),
                 [](const Value *V) { return PointerType::get(V->getType(), 0); });
  llvm::FunctionType *ft = FunctionType::get(llvm::Type::getVoidTy(M->getContext()), Params, false);
  llvm::Function *f = Function::Create(ft, llvm::GlobalValue::LinkageTypes::InternalLinkage, "", M);
  // add some attributes
  for (size_t i = Input.size() + 1; i < Params.size() + 1; ++i) {
    f->addAttribute(static_cast<unsigned>(i),
                    llvm::Attribute::get(M->getContext(), llvm::Attribute::AttrKind::NonNull));
  }

  // create auxiliary Map from Input to function arguments
  DenseMap<const Value *, Value *> InputToArgs;
  // create auxiliary Map from Output to function arguments
  DenseMap<const Value *, Value *> OutputToArgs;
  {
    auto ArgIt = f->arg_begin();

    for (auto It = Input.begin(); It != Input.end(); ++It) {
      InputToArgs.insert(std::make_pair(*It, &*ArgIt++));
    }
    for (auto It = Output.begin(); It != Output.end(); ++It) {
      OutputToArgs.insert(std::make_pair(*It, &*ArgIt++));
    }
    assert(ArgIt == f->arg_end());
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
    auto found = OutputToArgs.find(&I);
    if (found != OutputToArgs.end()) {
      Builder.CreateStore(Inserted, found->second); // TODO: decide what to do with volatile
    }
  }
  Builder.CreateRetVoid();
  return f;
}

SmallVector<Instruction *, 8> createAllocaInstructions(IRBuilder<> &builder,
                                                       const ArrayRef<Value *> &Stores) {
  SmallVector<Instruction *, 8> result;
  result.reserve(Stores.size());
  for (auto It : Stores) {
    result.push_back(builder.CreateAlloca(It->getType()));
  }
  return result;
}

SmallVector<Instruction *, 8> createAllocaInstructions(IRBuilder<> &builder,
                                                       const Function::arg_iterator ItBegin,
                                                       const Function::arg_iterator ItEnd) {
  SmallVector<Instruction *, 8> result;
  for (auto It = ItBegin; It != ItEnd; ++It) {
    result.push_back(builder.CreateAlloca(It->getType()->getPointerElementType()));
  }
  return result;
}

bool replaceBBWithFunctionCall(BasicBlock *BB, Function *F,
                               const ArrayRef<Value *> &Input,
                               const ArrayRef<Value *> &Output,
                               const ArrayRef<size_t> &OutputPoses) {
  assert(Output.size() == OutputPoses.size());
  // create New Basic Block
  std::string newName = BB->getName();
  BasicBlock *NewBB = BasicBlock::Create(BB->getContext(), "", BB->getParent(), BB);
  IRBuilder<> builder(NewBB);
  std::vector<Value *> arguments;
  assert(F->arg_size() >= Input.size() + Output.size());
  arguments.reserve(F->arg_size());
  copy(Input.begin(), Input.end(), back_inserter(arguments));
  auto OutputArgStart = F->arg_begin();
  std::advance(OutputArgStart, Input.size());
  auto outputAllocas = createAllocaInstructions(builder,
                                                OutputArgStart, F->arg_end()/*Output*/);
  copy(outputAllocas.begin(), outputAllocas.end(), back_inserter(arguments));

  builder.CreateCall(F, arguments);

  // remove basic block, replacing all Uses
  {
    // i iterates over function arguments
    // j iterates over OutputPoses, which points to position in function arguments
    size_t j = 0;
    for (size_t i = 0; /*i < outputAllocas.size() && */j < OutputPoses.size(); ++i) {
      if (i == OutputPoses[j]) {
        auto Inst = builder.CreateLoad(outputAllocas[i]);
        Output[j]->replaceAllUsesWith(Inst);
        ++j;
      }
    }
  }

  Instruction *TermInst = &*BB->rbegin();
  if (isa<TerminatorInst>(TermInst)) {
    builder.Insert(TermInst->clone());
  }

  BB->replaceAllUsesWith(NewBB);
  BB->eraseFromParent();

  NewBB->setName(newName);
  return true;
}

} // namespace

bool BBFactoring::replace(const std::vector<BasicBlock *> BBs) {
  if (BBs.front()->size() <= 4) {
    DEBUG(dbgs() << "Block family is too small to bother merging. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }

  std::vector<SmallVector<Value*, 8>> Inputs;
  Inputs.reserve(BBs.size());
  std::vector<SmallVector<size_t, 8>> Outputs;
  Outputs.reserve(BBs.size());
  for (auto BB : BBs) {
    Inputs.emplace_back(getInput(BB));
    Outputs.emplace_back(getOutput(BB));
  }

  SmallVector<size_t, 8> functionOutput;
  // TODO: optimize getting function output
  for (auto& Output : Outputs) {
    SmallVector<size_t, 8> Tmp;
    std::set_union(Output.begin(), Output.end(),
                   functionOutput.begin(), functionOutput.end(),
                   std::back_inserter(Tmp));
    functionOutput.clear();
    std::copy(Tmp.begin(), Tmp.end(), std::back_inserter(functionOutput));
  }

  // Input Validation
  assert(std::equal(std::next(Inputs.begin()), Inputs.end(),
                       Inputs.begin(),
                    [](const SmallVectorImpl<Value *> &Val1,
                       const SmallVectorImpl<Value *> &Val2) {
                      bool Result = Val1.size() == Val2.size();

                      Result = Result && std::equal(Val1.begin(), Val1.end(),
                                                    Val2.begin(), Val2.end(),
                                                    [](Value *V1, Value *V2) {
                                                      return V1->getType() == V2->getType();
                                                    });
                      return Result;
                    }
  ));

  if (functionOutput.size() > 4) {
    DEBUG(dbgs() << "Block family has many output parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }
  if (functionOutput.size() + Inputs.size() > 5) {
    DEBUG(dbgs() << "Block family has many parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }

  Function *F = createFuncFromBB(BBs.front(), Inputs.front(),
                                 convertOutput(BBs.front(), functionOutput));

  SmallVector<Value *, 8> Out;
  SmallVector<size_t, 8> OutPoses;
  for (size_t i = 0; i < BBs.size(); ++i) {
    Out = convertOutput(BBs[i], Outputs[i]);
    OutPoses = findPoses(Outputs[i], functionOutput);
    replaceBBWithFunctionCall(BBs[i], F, Inputs[i], Out, OutPoses);
  }

  return true;
}