//===-- BBFactoring.cpp - Merge identical basic blocks -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This pass looks for equivalent basic blocks that are mergable and folds
/// them.
/// Bacic blocks comparison is the same as in MergeFunctions.cpp
/// After finding identity BBs, pass creates new function, that consists of
/// whole BB without terminator instruction and replaces these BBs with
/// tail call to the newly created function.
/// Pass is working with well-formed basic blocks, but terminator
/// instruction is not required to be stated at the end of the block.
///
//===----------------------------------------------------------------------===//

#include "../external/merging.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

#define DEBUG_TYPE "bbfactor"

// TODO: ? place BB comparing in this file

// Future work:
// 1) If function with 1 basic block already exists and all arguments are
// the same as in the function, we want to create, use existed one to be called.
// Example:
// F0 { BB0, BB1, BB2 }
// F1 { BB3, BB0 }
// F2 { BB0 }
// Where all BB0 are supposed to be identity BBs
// turns into
// F0 { call F2, BB1, BB2 }
// F1 { BB2, call F2 }
// F2 { BB0 }
//
// 2) Elaborate Basic Block replacing.
//   a) If identical basic blocks have 2 subsets of output arguments and
// power of each of 2 subsets is large enough, create 2 different functions.
// Try to expand this logic on N (where N >= 2) subsets.
// Check, whether this assumption is more profitable in size compaction,
// than creating just 1 function.
//   b) Filter some subset of identical BBs, which has too many
// output parameters. Now pass discards all basic block if at least
// one BB has too many output parameters.

using namespace llvm;

namespace {
/// Auxiliary class, that holds basic block and it's hash.
/// Used BB for comparison
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
  /// If profitable, creates function with body of BB and replaces BBs
  /// with a call to new function
  /// \param BBs - Vector of identity BBs
  /// \returns whether BBs were replaced with a function call
  bool replace(const std::vector<BasicBlock *> &BBs);

  /// Comparator for BBNode
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

  // calculate hashes for all basic blocks in every function
  for (auto &F : M.functions()) {
    if (!F.isDeclaration() && !F.hasAvailableExternallyLinkage()) {
      for (auto &BB : F.getBasicBlockList())
        HashedBBs.push_back({BBComparator::basicBlockHash(BB, false), &BB});
    }
  }

  std::vector<std::vector<BasicBlock *>> IdenticalBlocksContainer;
  auto BBTree = std::map<BBNode, size_t, BBNodeCmp>(BBNodeCmp(&GlobalNumbers));

  // merge
  for (auto It = HashedBBs.begin(), Ite = HashedBBs.end(); It != Ite; ++It) {
    auto InsertedBBNode = BBTree.insert(std::make_pair(
        BBNode(It->second, It->first), IdenticalBlocksContainer.size()));
    if (InsertedBBNode.second) {
      IdenticalBlocksContainer.push_back(
          std::vector<BasicBlock *>(1, It->second));
    } else {
      IdenticalBlocksContainer[InsertedBBNode.first->second].push_back(
          It->second);
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

/// Get values, that were created outside of the basic block \p BB and
/// are used inside of it.
static SmallVector<Value *, 8> getInput(const BasicBlock *BB) {
  std::set<const Value *> Values;
  SmallVector<Value *, 8> Result;
  auto IE = isa<TerminatorInst>(BB->back()) ? std::prev(BB->end()) : BB->end();
  for (auto I = BB->begin(); I != IE; ++I) {
    Values.insert(&*I);
    assert(!isa<TerminatorInst>(&*I) && "Malformed BB");

    for (auto &Ops : I->operands()) {
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

/// Get values, that are created inside this basic block \p BB and
/// are used outside of it. Result is written as a vector
/// of instruction's numerical order in Basic block.
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

/// The same operation, but for one value
static Value *convertOutput(BasicBlock *BB, const size_t NumInstr) {
  if (BB->size() <= NumInstr)
    return nullptr;
  auto It = BB->begin();
  std::advance(It, NumInstr);
  return &*It;
}

/// Select a function return value from array of operands
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
  LLVMContext &Context = M->getContext();
  const DataLayout &Layout = M->getDataLayout();
  // create Function
  SmallVector<Type *, 8> Params;
  Params.reserve(Input.size() + Output.size());

  std::transform(Input.begin(), Input.end(), std::back_inserter(Params),
                 [](const Value *V) { return V->getType(); });

  Type *FunctionReturnT =
      ReturnValue ? ReturnValue->getType() : llvm::Type::getVoidTy(Context);

  transform(Output.begin(), Output.end(), std::back_inserter(Params),
            [](const Value *V) { return PointerType::get(V->getType(), 0); });

  llvm::FunctionType *FType = FunctionType::get(FunctionReturnT, Params, false);
  llvm::Function *F = Function::Create(
      FType, llvm::GlobalValue::LinkageTypes::PrivateLinkage, "", M);

  // add some attributes
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::Naked);
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::MinSize);
  F->addAttribute(AttributeSet::FunctionIndex, Attribute::OptimizeForSize);

  // set attributes to all output params
  for (size_t i = Input.size() + 1, ie = Params.size() + 1; i < ie; ++i) {
    F->addAttribute(static_cast<unsigned>(i),
                    llvm::Attribute::get(
                        Context, llvm::Attribute::AttrKind::Dereferenceable,
                        Layout.getTypeStoreSize(
                            Params[i - 1]->getSequentialElementType())));
    F->addAttribute(
        static_cast<unsigned>(i),
        llvm::Attribute::get(Context, llvm::Attribute::AttrKind::NoAlias));
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
  // Fill function, using BB with some changes:
  // Replace all input values with Function arguments
  // Store all output values to Function arguments
  BasicBlock *NewBB = BasicBlock::Create(Context, "Entry", F);
  IRBuilder<> Builder(NewBB);
  Value *ReturnValueF = nullptr;
  auto Ie = isa<TerminatorInst>(BB->back()) ? std::prev(BB->end()) : BB->end();
  for (auto I = BB->begin(); I != Ie; ++I) {
    Instruction *NewI = Builder.Insert(I->clone());
    InputToArgs.insert(std::make_pair(&*I, NewI));

    for (auto &Op : NewI->operands()) {
      auto it = InputToArgs.find(Op.get());
      if (it != InputToArgs.end()) {
        Op.set(it->second);
      }
    }

    auto Found = OutputToArgs.find(&*I);
    if (Found != OutputToArgs.end()) {
      Builder.CreateStore(NewI, Found->second);
    } else if (&*I == ReturnValue) {
      assert(ReturnValueF == nullptr &&
             "Function return value is already assigned");
      ReturnValueF = NewI;
    }
  }

  if (ReturnValueF)
    Builder.CreateRet(ReturnValueF);
  else
    Builder.CreateRetVoid();

  return F;
}

/// Function creates alloca instructions from \p ItBegin till \p ItEnd
/// \returns Instructions, that are created by \p Builder
static SmallVector<Instruction *, 8>
createAllocaInstructions(IRBuilder<> &Builder,
                         const Function::arg_iterator ItBegin,
                         const Function::arg_iterator ItEnd) {
  SmallVector<Instruction *, 8> Result;
  for (auto It = ItBegin; It != ItEnd; ++It) {
    Result.push_back(
        Builder.CreateAlloca(It->getType()->getPointerElementType()));
  }
  return Result;
}

/// \param Input - \p BB operands to be used as an input arguments to \p F
/// \param Output - \p BB operands to be used as an output arguments to \p F
/// \param Result - \p BB instruction to be used as a result of \p F
/// \returns true if \p BB was replaced with \p F, false otherwise
static bool replaceBBWithFunctionCall(BasicBlock *BB, Function *F,
                                      const ArrayRef<Value *> &Input,
                                      const ArrayRef<Value *> &Output,
                                      Value *Result) {

  // create New Basic Block

  std::string NewName = BB->getName();
  BasicBlock *NewBB =
      BasicBlock::Create(BB->getContext(), "", BB->getParent(), BB);
  IRBuilder<> Builder(NewBB);
  SmallVector<Value *, 8> Args;
  assert(F->arg_size() == Input.size() + Output.size() &&
         "Argument sizes not match");
  Args.reserve(F->arg_size());
  copy(Input.begin(), Input.end(), std::back_inserter(Args));
  auto OutputArgStart = F->arg_begin();
  std::advance(OutputArgStart, Input.size());
  // Alloca instruction for Output function arguments
  auto OutputAllocas =
      createAllocaInstructions(Builder, OutputArgStart, F->arg_end());
  copy(OutputAllocas.begin(), OutputAllocas.end(), std::back_inserter(Args));

  Instruction *CallFunc = Builder.CreateCall(F, Args);
  if (Result)
    Result->replaceAllUsesWith(CallFunc);

  // remove basic block, replacing all Uses
  for (size_t i = 0, esz = Output.size(); i < esz; ++i) {
    if (isValUsedOutsideOfBB(Output[i], BB)) {
      auto Inst = Builder.CreateLoad(OutputAllocas[i]);
      Output[i]->replaceAllUsesWith(Inst);
    }
  }

  Instruction *TermInst = &BB->back();
  if (isa<TerminatorInst>(TermInst)) {
    auto NewTermInst = Builder.Insert(TermInst->clone());
    TermInst->replaceAllUsesWith(NewTermInst);
  }

  BB->replaceAllUsesWith(NewBB);
  BB->eraseFromParent();

  NewBB->setName(NewName);
  return true;
}

using SetOfVectors = std::set<SmallVector<size_t, 8>>;

/// Merges set of sorted vectors into sorted vector of unique values
/// Example. combineOutputs : {{1, 2, 3}, {1, 2}, {1, 6}} => {1, 2, 3, 6}
static SmallVector<size_t, 8> combineOutputs(const SetOfVectors &Outputs) {
  SmallVector<size_t, 8> Result;
  if (Outputs.size() == 0) {
    return Result;
  }
  if (Outputs.size() == 1) {
    return *Outputs.begin();
  }
  // ItCurrentEnd::first points to the least element, that is
  // still not in the resultant set
  // ItCurrentEnd::second always points to the last element
  // used as a right border
  using ItCurrentEnd = std::pair<SmallVectorImpl<size_t>::const_iterator,
                                 const SmallVectorImpl<size_t>::const_iterator>;
  // vector of iterator pair, that steps from begin to end
  SmallVector<ItCurrentEnd, 8> Its;

  std::transform(Outputs.begin(), Outputs.end(), std::back_inserter(Its),
                 [](const SmallVectorImpl<size_t> &Impl) {
                   return std::make_pair(Impl.begin(), Impl.end());
                 });

  // Algorithm finds min element A through all iterators
  // and pushes it in the resultant set.
  // Then it increments all iterators, that point to A and repeat steps,
  // untill all iterators reached theirs right border.
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

/// Selects an operand from \p BBOutputsIds to be function's return value and
/// if found, removes it from \p BBOutputsIds.
/// \param [in,out] BBOutputsIds - Values, that are created in \p BB and used
/// either in terminator inst or in other basic block.
/// \param [out] ReturnValueId - instruction number in \p BB, that is used as
/// return value for the function
static Function *
createFunctionWithReturnValue(BasicBlock *BB, const ArrayRef<Value *> &BBInput,
                              SmallVectorImpl<size_t> &BBOutputsIds,
                              size_t &ReturnValueId) {
  auto FunctionOutput = convertOutput(BB, BBOutputsIds);
  ReturnValueId = std::numeric_limits<size_t>::max();
  Value *ReturnF = nullptr;
  // Find and remove (if found) return value from output list
  size_t ReturnIdF = getFunctionRetValId(FunctionOutput);
  if (ReturnIdF != FunctionOutput.size()) {
    ReturnF = FunctionOutput[ReturnIdF];
    FunctionOutput.erase(FunctionOutput.begin() + ReturnIdF);
    ReturnValueId = BBOutputsIds[ReturnIdF];
    BBOutputsIds.erase(BBOutputsIds.begin() + ReturnIdF);
  }
  Function *F = createFuncFromBB(BB, BBInput, FunctionOutput, ReturnF);

  DEBUG(dbgs() << "Function created:");
  DEBUG(F->print(dbgs()));
  DEBUG(dbgs() << '\n');

  return F;
}

bool BBFactoring::replace(const std::vector<BasicBlock *> &BBs) {
  if (BBs.front()->size() <= 4) {
    DEBUG(dbgs() << "Block family is too small to bother merging. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }

  std::vector<SmallVector<Value *, 8>> BBInputs;
  BBInputs.reserve(BBs.size());
  SetOfVectors OutputsStorage;

  // Most likely all BBs have the same set of input/output args
  for (auto BB : BBs) {
    BBInputs.emplace_back(getInput(BB));
    OutputsStorage.insert(getOutput(BB));
  }
  // Get function Output Values
  SmallVector<size_t, 8> BBOutputsIds = combineOutputs(OutputsStorage);

  // Input Validation
  assert(std::equal(std::next(BBInputs.begin()), BBInputs.end(),
                    BBInputs.begin(),
                    [](const SmallVectorImpl<Value *> &Val1,
                       const SmallVectorImpl<Value *> &Val2) {
                      return Val1.size() == Val2.size() &&
                             std::equal(Val1.begin(), Val1.end(), Val2.begin(),
                                        Val2.end(), [](Value *V1, Value *V2) {
                                          return V1->getType() == V2->getType();
                                        });

                    }) &&
         "Amount and types of argument of identity BBs must be equal");

  if (BBOutputsIds.size() > 4) {
    DEBUG(dbgs() << "Block family has many output parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }
  if (BBOutputsIds.size() + BBInputs.size() > 5) {
    DEBUG(dbgs() << "Block family has many parameters. Block: "
                 << BBs.front()->getName() << ". Function: "
                 << BBs.front()->getParent()->getName() << '\n');
    return false;
  }

  // Change output values according to function's return
  size_t ResultInstId;
  Function *F = createFunctionWithReturnValue(BBs.front(), BBInputs.front(),
                                              BBOutputsIds, ResultInstId);

  // Values, that are passed as an output arguments to the function
  SmallVector<Value *, 8> BBOutputs;
  for (size_t i = 0, sz = BBs.size(); i < sz; ++i) {
    BBOutputs = convertOutput(BBs[i], BBOutputsIds);
    Value *ReturnF = convertOutput(BBs[i], ResultInstId);
    replaceBBWithFunctionCall(BBs[i], F, BBInputs[i], BBOutputs, ReturnF);
  }

  return true;
}