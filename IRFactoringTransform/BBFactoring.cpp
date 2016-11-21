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


// TODO: ? place BB comparing in this file
// TODO: tests tests tests
// TODO: llvm code style: http://llvm.org/docs/CodingStandards.html



using namespace llvm;

namespace {

class BBNode {
  //mutable AssertingVH<BasicBlock> BB;
  mutable BasicBlock *BB;
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

  BBFactoring() : ModulePass(ID)
  {}

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

  auto BBTree = std::map<BBNode, size_t, BBNodeCmp>(BBNodeCmp(&GlobalNumbers));

  for (auto it = HashedBBs.begin(), itEnd = HashedBBs.end(); it != itEnd; ++it) {
    auto Element = BBTree.insert(std::make_pair(BBNode(it->second, it->first), IdenticalBlocksContainer.size()));
    if (Element.second) {
      IdenticalBlocksContainer.push_back(std::vector<BasicBlock *>(1, it->second));
    } else {
      IdenticalBlocksContainer[Element.first->second].push_back(it->second);
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

/// isInstUsedOutsideOfBB - Return true if there are any uses of Inst outside of the
/// specified block. This function doesn't consider Phi nodes as a special case,
/// but deals with TerminatorInst's like instructions in other block.
bool isValUsedOutsideOfBB(const Value *InstOriginal, const BasicBlock *BB) {

  for (const Use &U : InstOriginal->uses()) {
    const Instruction *I = cast<Instruction>(U.getUser());
    if (I->getParent() != BB || isa<TerminatorInst>(I))
      return true;
  }
  return false;
}

SmallVector<size_t, 8> getOutput(const BasicBlock *BB) {
  SmallVector<size_t, 8> result;
  size_t current = 0;
  auto IE = isa<TerminatorInst>(BB->back()) ? --BB->end() : BB->end();
  for (auto I = BB->begin(); I != IE; ++I, ++current) {
    if (isValUsedOutsideOfBB(&*I, BB)) {
      result.push_back(current);
    }
  }
  return result;
}

SmallVector<Value *, 8> convertOutput(BasicBlock *BB,
                                      const SmallVectorImpl<size_t> &numInstr) {
  SmallVector<Value *, 8> Result;
  Result.reserve(numInstr.size());
  if (numInstr.empty())
    return Result;
  auto It = BB->begin();
  std::advance(It, numInstr.front());
  Result.push_back(&*It);
  for (size_t i = 1; i < numInstr.size(); ++i) {
    std::advance(It, numInstr[i] - numInstr[i-1]);
    Result.push_back(&*It);
  }
  return Result;
}

Value *convertOutput(BasicBlock *BB, const size_t numInstr) {
  if (BB->size() <= numInstr)
    return nullptr;
  auto It = BB->begin();
  std::advance(It, numInstr);
  return &*It;
}


size_t getFunctionRetValId(const ArrayRef<Value *> Outputs) {
  return find_if(Outputs.rbegin(), Outputs.rend(), [](Value *V) { return V->getType()->isFirstClassType(); }) - Outputs.rbegin();
}

Function *createFuncFromBB(BasicBlock *BB, const SmallVectorImpl<Value *> &Input,
                           const SmallVectorImpl<Value *> &Output, const Value *ReturnValue) {
  Module *M = BB->getModule();
  const DataLayout& Layout = M->getDataLayout();
  // create Function
  SmallVector<Type *, 8> Params;
  Params.reserve(Input.size() + Output.size());

  std::transform(Input.begin(), Input.end(), std::back_inserter(Params),
                 [](const Value *V) { return V->getType(); });

  Type *FunctionReturnT = ReturnValue ?
                          ReturnValue->getType() :
                          llvm::Type::getVoidTy(M->getContext());

  transform(Output.begin(), Output.end(), std::back_inserter(Params),
            [](const Value *V) {return PointerType::get(V->getType(), 0); });

  llvm::FunctionType *FType = FunctionType::get(FunctionReturnT, Params, false);
  llvm::Function *F = Function::Create(FType, llvm::GlobalValue::LinkageTypes::PrivateLinkage, "", M);

  // add some attributes
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::Naked);
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::MinSize);
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::OptimizeForSize);

  for (size_t i = Input.size() + 1; i < Params.size() + 1; ++i) {
    F->addAttribute(static_cast<unsigned>(i),
                    llvm::Attribute::get(M->getContext(), llvm::Attribute::AttrKind::Dereferenceable,
                      Layout.getTypeStoreSize(Params[i-1]->getSequentialElementType())));
    F->addAttribute(static_cast<unsigned>(i),
                    llvm::Attribute::get(M->getContext(), llvm::Attribute::AttrKind::NoAlias));
  }

  // create auxiliary Map from Input to function arguments
  DenseMap<const Value *, Value *> InputToArgs;
  // create auxiliary Map from Output to function arguments
  DenseMap<const Value *, Value *> OutputToArgs;
  {
    auto ArgIt = F->arg_begin();

    for (auto It = Input.begin(); It != Input.end(); ++It) {
      InputToArgs.insert(std::make_pair(*It, &*ArgIt++));
    }
    for (auto It = Output.begin(); It != Output.end(); ++It) {
        OutputToArgs.insert(std::make_pair(*It, &*ArgIt++));
    }
    assert(ArgIt == F->arg_end());
  }
  // start filling function
  BasicBlock *NewBB = BasicBlock::Create(M->getContext(), "Entry", F);
  IRBuilder<> Builder(NewBB);
  Value *ReturnValueF = nullptr;
  for (auto &I : BB->getInstList()) {
    if (isa<TerminatorInst>(I)) {
      assert(&I == &BB->back());
      break;
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
      Builder.CreateStore(Inserted, found->second);
    }
    else if (&I == ReturnValue) {
      assert(ReturnValueF == nullptr);
      ReturnValueF = Inserted;
    }
  }
  if (ReturnValueF)
    Builder.CreateRet(ReturnValueF);
  else
    Builder.CreateRetVoid();

  return F;
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
                               Value *Result) {

  // create New Basic Block

  std::string newName = BB->getName();
  BasicBlock *NewBB = BasicBlock::Create(BB->getContext(), "", BB->getParent(), BB);
  IRBuilder<> builder(NewBB);
  SmallVector<Value *, 8> Args;
  assert(F->arg_size() == Input.size() + Output.size());
  Args.reserve(F->arg_size());
  copy(Input.begin(), Input.end(), std::back_inserter(Args));
  auto OutputArgStart = F->arg_begin();
  std::advance(OutputArgStart, Input.size());
  auto outputAllocas = createAllocaInstructions(builder,
    OutputArgStart, F->arg_end());
  copy(outputAllocas.begin(), outputAllocas.end(), std::back_inserter(Args));

  Instruction *CallFunc = builder.CreateCall(F, Args);
  if (Result)
    Result->replaceAllUsesWith(CallFunc);

  // remove basic block, replacing all Uses
  
  for (size_t i = 0; i < Output.size(); ++i) {
    if (isValUsedOutsideOfBB(Output[i], BB)) {
      auto Inst = builder.CreateLoad(outputAllocas[i]);
      Output[i]->replaceAllUsesWith(Inst);
    }
  }

  Instruction *TermInst = &BB->back();
  if (isa<TerminatorInst>(TermInst)) {
    auto newTerm = builder.Insert(TermInst->clone());
    TermInst->replaceAllUsesWith(newTerm);
  }

  BB->replaceAllUsesWith(NewBB);
  BB->eraseFromParent();

  NewBB->setName(newName);
  return true;
}

template <typename T>
struct SmallVectorImplComp
{
  bool operator()(const SmallVectorImpl<T>& v1, const SmallVectorImpl<T>& v2) const noexcept {
    if (v1.size() != v2.size())
      return v1.size() < v2.size();
    for (auto i1 = v1.begin(), i2 = v2.begin(), e1 = v1.end(); i1 != e1; ++i1, ++i2) {
      if (*i1 != *i2) {
        return *i1 < *i2;
      }
    }
    return false;
  }
};

using SetOfVectors = std::set<SmallVector<size_t, 8>, SmallVectorImplComp<size_t>>;

/// Merges set of sorted vectors in sorted vector of unique values
SmallVector<size_t, 8> combineOutputs(const SetOfVectors &Outputs) {
  SmallVector<size_t, 8> Result;
  if (Outputs.size() == 0) {
    return Result;
  }
  if (Outputs.size() == 1) {
    return *Outputs.begin();
  }

  using ItCurrentEnd = std::pair<
      SmallVectorImpl<size_t>::const_iterator,
      const SmallVectorImpl<size_t>::const_iterator
  >;
  SmallVector<ItCurrentEnd, 8> Its;

  std::transform(Outputs.begin(), Outputs.end(), std::back_inserter(Its),
                 [](const SmallVectorImpl<size_t> &Impl){
                   return std::make_pair(Impl.begin(), Impl.end());
                 });

  size_t CurMin = std::numeric_limits<size_t>::max();
  while (true) {
    size_t NextMin = std::numeric_limits<size_t>::max();
    bool Done = true;
    for (auto It : Its) {
      if (It.first == It.second)
        continue;
      if (*It.first == CurMin)
        ++It.first;
      else if (*It.first < NextMin)
      {
        NextMin = *It.first;
        Done = false;
      }
    } // for
    if (Done)
      break;
    Result.push_back(NextMin);
    CurMin = NextMin;
  }

  return  Result;
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
  SetOfVectors OutputsStorage;
  std::vector<SetOfVectors::const_iterator> Outputs;
  Outputs.reserve(BBs.size());
  for (auto BB : BBs) {
    Inputs.emplace_back(getInput(BB));
    auto result = OutputsStorage.insert(getOutput(BB));
    Outputs.emplace_back(result.first);
  }
  // Get function Output Values
  SmallVector<size_t, 8> functionOutputIds = combineOutputs(OutputsStorage);

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

  if (functionOutputIds.size() > 4) {
    DEBUG(dbgs() << "Block family has many output parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }
  if (functionOutputIds.size() + Inputs.size() > 5) {
    DEBUG(dbgs() << "Block family has many parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }

  // Change output values according to function's return
  auto FunctionOutput = convertOutput(BBs.front(), functionOutputIds);
  size_t ResultInstId = std::numeric_limits<size_t>::max();
  Value *ReturnF = nullptr;
  {
    size_t ReturnIdF = getFunctionRetValId(FunctionOutput);
    if (ReturnIdF != FunctionOutput.size()) {
      ReturnF = FunctionOutput[ReturnIdF];
      FunctionOutput.erase(FunctionOutput.begin() + ReturnIdF);
      ResultInstId = functionOutputIds[ReturnIdF];
      functionOutputIds.erase(functionOutputIds.begin() + ReturnIdF);
    }
  }
  Function *F = createFuncFromBB(BBs.front(), Inputs.front(),
                                 FunctionOutput, ReturnF);

  DEBUG(dbgs() << "Function created:");
  DEBUG(F->print(dbgs()));
  DEBUG(dbgs() << '\n');

  SmallVector<Value *, 8> Out;
  SmallVector<size_t, 8> OutPoses;
  for (size_t i = 0; i < BBs.size(); ++i) {
    Out = convertOutput(BBs[i], functionOutputIds);
    ReturnF = convertOutput(BBs[i], ResultInstId);
    replaceBBWithFunctionCall(BBs[i], F, Inputs[i], Out, ReturnF);
  }

  return true;
}