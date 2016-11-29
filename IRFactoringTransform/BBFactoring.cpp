//===- BBFactoring.cpp - Merge identical basic blocks ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "../external/merging.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

#define DEBUG_TYPE "bbfactor"

// TODO: ? place BB comparing in this file
// TODO: tests tests tests
// TODO: llvm code style: http://llvm.org/docs/CodingStandards.html

using namespace llvm;

namespace {

class BBNode {
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
};

/// BBFactoring finds basic blocks which will generate identical machine code
/// Once identified, BBFactoring will fold them by replacing these basic blocks
/// with a call to a function.
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

} // end anonymous namespace

char BBFactoring::ID = 0;
static RegisterPass<BBFactoring> X("bbfactor", "BBFactoring Pass", false,
                                   false);

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

  for (auto it = HashedBBs.begin(), itEnd = HashedBBs.end(); it != itEnd;
       ++it) {
    auto Element = BBTree.insert(std::make_pair(
        BBNode(it->second, it->first), IdenticalBlocksContainer.size()));
    if (Element.second) {
      IdenticalBlocksContainer.push_back(
          std::vector<BasicBlock *>(1, it->second));
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

/// Get values, that were created outside of the basic block \p BB
static SmallVector<Value *, 8> getInput(const BasicBlock *BB) {
  std::set<const Value *> Values;
  SmallVector<Value *, 8> Result;
  for (auto &I : BB->getInstList()) {
    Values.insert(&I);
    if (isa<TerminatorInst>(I))
      continue;

    for (auto &Ops : I.operands()) {
      if (isa<Constant>(Ops.get()))
        continue;
      if (Values.find(Ops.get()) == Values.end()) {
        Result.push_back(Ops.get());
        Values.insert(Ops.get());
      }
    }
  }
  return Result;
}

/// This function is like isInstUsedOutsideOfBB, but doesn't
/// consider Phi nodes as a special case, It also deals with TerminatorInst's
/// like instructions in other block.
static bool isValUsedOutsideOfBB(const Value *InstOriginal,
                                 const BasicBlock *BB) {
  for (const Use &U : InstOriginal->uses()) {
    const Instruction *I = cast<Instruction>(U.getUser());
    if (I->getParent() != BB || isa<TerminatorInst>(I))
      return true;
  }
  return false;
}

/// Get values, that are used outside of the basic block \p BB
static SmallVector<size_t, 8> getOutput(const BasicBlock *BB) {
  SmallVector<size_t, 8> result;
  size_t current = 0;
  auto IE = isa<TerminatorInst>(BB->back()) ? std::prev(BB->end()) : BB->end();
  for (auto I = BB->begin(); I != IE; ++I, ++current) {
    if (isValUsedOutsideOfBB(&*I, BB)) {
      result.push_back(current);
    }
  }
  return result;
}

/// Converts instruction numbers in basic block \p BB
/// into Values * \p NumsInstr
static SmallVector<Value *, 8>
convertOutput(BasicBlock *BB, const SmallVectorImpl<size_t> &NumsInstr) {
  SmallVector<Value *, 8> Result;
  Result.reserve(NumsInstr.size());
  if (NumsInstr.empty())
    return Result;
  auto It = BB->begin();
  std::advance(It, NumsInstr.front());
  Result.push_back(&*It);
  for (size_t i = 1, isz = NumsInstr.size(); i < isz; ++i) {
    std::advance(It, NumsInstr[i] - NumsInstr[i - 1]);
    Result.push_back(&*It);
  }
  return Result;
}

static Value *convertOutput(BasicBlock *BB, const size_t NumInstr) {
  if (BB->size() <= NumInstr)
    return nullptr;
  auto It = BB->begin();
  std::advance(It, NumInstr);
  return &*It;
}

/// Select an out operand to be a function return value
static size_t getFunctionRetValId(const ArrayRef<Value *> Outputs) {
  return find_if(Outputs.rbegin(), Outputs.rend(),
                 [](Value *V) { return V->getType()->isFirstClassType(); }) -
         Outputs.rbegin();
}

/// Creates new function, that consists of Basic block \p BB
/// \param Input - values, which types are input function arguments.
/// \param Output - values, which types with added pointer are
/// output function arguments. They follow right after Input params.
/// \param ReturnValue - value, which type is function's return value.
static Function *createFuncFromBB(BasicBlock *BB,
                                  const ArrayRef<Value *> &Input,
                                  const ArrayRef<Value *> &Output,
                                  const Value *ReturnValue) {
  Module *M = BB->getModule();
  const DataLayout &Layout = M->getDataLayout();
  // create Function
  SmallVector<Type *, 8> Params;
  Params.reserve(Input.size() + Output.size());

  std::transform(Input.begin(), Input.end(), std::back_inserter(Params),
                 [](const Value *V) { return V->getType(); });

  Type *FunctionReturnT = ReturnValue ? ReturnValue->getType()
                                      : llvm::Type::getVoidTy(M->getContext());

  transform(Output.begin(), Output.end(), std::back_inserter(Params),
            [](const Value *V) { return PointerType::get(V->getType(), 0); });

  llvm::FunctionType *FType = FunctionType::get(FunctionReturnT, Params, false);
  llvm::Function *F = Function::Create(
      FType, llvm::GlobalValue::LinkageTypes::PrivateLinkage, "", M);

  // add some attributes
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::Naked);
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::MinSize);
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::OptimizeForSize);

  for (size_t i = Input.size() + 1, ie = Params.size() + 1; i < ie; ++i) {
    F->addAttribute(
        static_cast<unsigned>(i),
        llvm::Attribute::get(M->getContext(),
                             llvm::Attribute::AttrKind::Dereferenceable,
                             Layout.getTypeStoreSize(
                                 Params[i - 1]->getSequentialElementType())));
    F->addAttribute(static_cast<unsigned>(i),
                    llvm::Attribute::get(M->getContext(),
                                         llvm::Attribute::AttrKind::NoAlias));
  }

  // create auxiliary Map from Input to function arguments
  DenseMap<const Value *, Value *> InputToArgs;
  // create auxiliary Map from Output to function arguments
  DenseMap<const Value *, Value *> OutputToArgs;
  {
    auto ArgIt = F->arg_begin();

    for (auto It = Input.begin(), EIt = Input.end(); It != EIt; ++It) {
      InputToArgs.insert(std::make_pair(*It, &*ArgIt++));
    }
    for (auto It = Output.begin(), EIt = Output.end(); It != EIt; ++It) {
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
    auto Found = OutputToArgs.find(&I);
    if (Found != OutputToArgs.end()) {
      Builder.CreateStore(Inserted, Found->second);
    } else if (&I == ReturnValue) {
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

static SmallVector<Instruction *, 8>
createAllocaInstructions(IRBuilder<> &Builder,
                         const Function::arg_iterator ItBegin,
                         const Function::arg_iterator ItEnd) {
  SmallVector<Instruction *, 8> result;
  for (auto It = ItBegin; It != ItEnd; ++It) {
    result.push_back(
        Builder.CreateAlloca(It->getType()->getPointerElementType()));
  }
  return result;
}

static bool replaceBBWithFunctionCall(BasicBlock *BB, Function *F,
                                      const ArrayRef<Value *> &Input,
                                      const ArrayRef<Value *> &Output,
                                      Value *Result) {

  // create New Basic Block

  std::string NewName = BB->getName();
  BasicBlock *NewBB =
      BasicBlock::Create(BB->getContext(), "", BB->getParent(), BB);
  IRBuilder<> builder(NewBB);
  SmallVector<Value *, 8> Args;
  assert(F->arg_size() == Input.size() + Output.size());
  Args.reserve(F->arg_size());
  copy(Input.begin(), Input.end(), std::back_inserter(Args));
  auto OutputArgStart = F->arg_begin();
  std::advance(OutputArgStart, Input.size());
  auto outputAllocas =
      createAllocaInstructions(builder, OutputArgStart, F->arg_end());
  copy(outputAllocas.begin(), outputAllocas.end(), std::back_inserter(Args));

  Instruction *CallFunc = builder.CreateCall(F, Args);
  if (Result)
    Result->replaceAllUsesWith(CallFunc);

  // remove basic block, replacing all Uses

  for (size_t i = 0, esz = Output.size(); i < esz; ++i) {
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

  NewBB->setName(NewName);
  return true;
}

using SetOfVectors = std::set<SmallVector<size_t, 8>>;

/// Merges set of sorted vectors into sorted vector of unique values
static SmallVector<size_t, 8> combineOutputs(const SetOfVectors &Outputs) {
  SmallVector<size_t, 8> Result;
  if (Outputs.size() == 0) {
    return Result;
  }
  if (Outputs.size() == 1) {
    return *Outputs.begin();
  }
  // iterator pair begin/const end
  using ItCurrentEnd = std::pair<SmallVectorImpl<size_t>::const_iterator,
                                 const SmallVectorImpl<size_t>::const_iterator>;
  SmallVector<ItCurrentEnd, 8> Its;

  std::transform(Outputs.begin(), Outputs.end(), std::back_inserter(Its),
                 [](const SmallVectorImpl<size_t> &Impl) {
                   return std::make_pair(Impl.begin(), Impl.end());
                 });

  size_t CurMin = std::numeric_limits<size_t>::max();
  while (true) {
    size_t NextMin = std::numeric_limits<size_t>::max();
    bool Done = true;
    for (auto &It : Its) {
      if (It.first == It.second)
        continue;
      if (*It.first == CurMin)
        ++It.first;
      else if (*It.first < NextMin) {
        NextMin = *It.first;
        Done = false;
      }
    } // for
    if (Done)
      break;
    Result.push_back(NextMin);
    CurMin = NextMin;
  }

  return Result;
}

bool BBFactoring::replace(const std::vector<BasicBlock *> BBs) {
  if (BBs.front()->size() <= 4) {
    DEBUG(dbgs() << "Block family is too small to bother merging. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }

  std::vector<SmallVector<Value *, 8>> Inputs;
  Inputs.reserve(BBs.size());
  SetOfVectors OutputsStorage;
  std::vector<SetOfVectors::const_iterator> Outputs;
  Outputs.reserve(BBs.size());
  for (auto BB : BBs) {
    Inputs.emplace_back(getInput(BB));
    auto Result = OutputsStorage.insert(getOutput(BB));
    Outputs.emplace_back(Result.first);
  }
  // Get function Output Values
  SmallVector<size_t, 8> FunctionOutputIds = combineOutputs(OutputsStorage);

  // Input Validation
  assert(std::equal(std::next(Inputs.begin()), Inputs.end(), Inputs.begin(),
                    [](const SmallVectorImpl<Value *> &Val1,
                       const SmallVectorImpl<Value *> &Val2) {
                      if (Val1.size() != Val2.size())
                        return false;

                      return std::equal(Val1.begin(), Val1.end(), Val2.begin(),
                                        Val2.end(), [](Value *V1, Value *V2) {
                                          return V1->getType() == V2->getType();
                                        });
                    }) &&
         "Types of argument of equal BB must be equal");

  if (FunctionOutputIds.size() > 4) {
    DEBUG(dbgs() << "Block family has many output parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }
  if (FunctionOutputIds.size() + Inputs.size() > 5) {
    DEBUG(dbgs() << "Block family has many parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }

  // Change output values according to function's return
  auto FunctionOutput = convertOutput(BBs.front(), FunctionOutputIds);
  size_t ResultInstId = std::numeric_limits<size_t>::max();
  Value *ReturnF = nullptr;
  {
    size_t ReturnIdF = getFunctionRetValId(FunctionOutput);
    if (ReturnIdF != FunctionOutput.size()) {
      ReturnF = FunctionOutput[ReturnIdF];
      FunctionOutput.erase(FunctionOutput.begin() + ReturnIdF);
      ResultInstId = FunctionOutputIds[ReturnIdF];
      FunctionOutputIds.erase(FunctionOutputIds.begin() + ReturnIdF);
    }
  }
  Function *F =
      createFuncFromBB(BBs.front(), Inputs.front(), FunctionOutput, ReturnF);

  DEBUG(dbgs() << "Function created:");
  DEBUG(F->print(dbgs()));
  DEBUG(dbgs() << '\n');

  SmallVector<Value *, 8> Out;
  SmallVector<size_t, 8> OutPoses;
  for (size_t i = 0, sz = BBs.size(); i < sz; ++i) {
    Out = convertOutput(BBs[i], FunctionOutputIds);
    ReturnF = convertOutput(BBs[i], ResultInstId);
    replaceBBWithFunctionCall(BBs[i], F, Inputs[i], Out, ReturnF);
  }

  return true;
}