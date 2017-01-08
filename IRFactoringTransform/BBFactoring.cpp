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
/// Bacic blocks comparison is almost the same as in MergeFunctions.cpp, but
/// commutativity check added
/// After finding identity BBs, pass creates new function, that consists of
/// whole BB without phi nodes and terminator instruction
/// and replaces these BBs with tail call to the newly created function.
/// Pass works only with well-formed basic blocks, but terminator
/// instruction is not required to be stated at the end of the block.
///
//===----------------------------------------------------------------------===//

#include "../external/merging.h"
#include <llvm/ADT/Statistic.h>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <set>

#define DEBUG_TYPE "bbfactor"

STATISTIC(MergeCounter, "Counts total number of merged basic blocks");
STATISTIC(FunctionCounter, "Counts amount of created functions");

// TODO: Compare without Phi Nodes
// TODO: perfomance issue: on replacing, remove just redundant instructions
//       instead of substituting the whole BB
// TODO: pick appropriate constants for filtering bad BB factorizing
//       (# of outputs, # of inputs, # instructions in function)
// TODO: ? restrict pass to work only with well-formed BBs
// TODO: ? place BB comparing in this file


// 1) Elaborate Basic Block replacing.
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

/// Retrieves end iterator
/// Terminator instrtuction is not of our interest
static inline BasicBlock::iterator getEndIt(BasicBlock *BB) {
  return isa<TerminatorInst>(BB->back()) ? std::prev(BB->end()) : BB->end();
}

static inline BasicBlock::const_iterator getEndIt(const BasicBlock *BB) {
  return isa<TerminatorInst>(BB->back()) ? std::prev(BB->end()) : BB->end();
}

static void debugPrint(const BasicBlock *BB, const StringRef Str) {
  DEBUG(dbgs() << Str << ". Block: " << BB->getName()
               << ". Function: " << BB->getParent()->getName() << '\n');
}

/// Returns vector of values:
/// a) Phi nodes of the \p BB
/// b) created outside of the \p BB and used inside of it
static SmallVector<Value *, 8> getInput(BasicBlock *BB) {
  SmallPtrSet<const Value *, 8> Values;
  SmallVector<Value *, 8> Result;

  auto I = BB->begin();
  auto IE = getEndIt(BB);
  for (; I != IE && isa<PHINode>(I); ++I) {
    Values.insert(&*I);
    Result.push_back(&*I);
  }

  for (; I != IE; ++I) {
    Values.insert(&*I);
    assert(!isa<TerminatorInst>(&*I) && "Malformed BB");

    for (auto &Ops : I->operands()) {
      if (isa<Constant>(Ops.get()))
        continue;

      if (Values.count(Ops.get()) == 0) {
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
/// are used outside of it and operands of PHI nodes of this BB
/// Result is written as a vector of instruction's numerical order in BB
static SmallVector<size_t, 8> getOutput(const BasicBlock *BB) {
  SmallPtrSet<const Value *, 8> PhiOperands;
  SmallVector<size_t, 8> Result;
  size_t Current = 0;
  auto I = BB->begin();
  auto IE = getEndIt(BB);

  for (; I != IE && isa<PHINode>(I); ++I, ++Current) {
    auto PhiNode = cast<PHINode>(I);
    for (auto &POp : PhiNode->operands()) {
      if (Instruction *Inst = dyn_cast<Instruction>(POp)) {
        if (Inst->getParent() == BB && !isa<PHINode>(Inst)) {
          PhiOperands.insert(Inst);
        }
      }
    }
  }

  for (; I != IE; ++I, ++Current) {
    if (isValUsedOutsideOfBB(&*I, BB) || PhiOperands.count(&*I)) {
      Result.push_back(Current);
    }
  }
  return Result;
}

/// Converts instruction numbers in basic block \p BB
/// into Values * \p NumsInstr
static SmallVector<Value *, 8>
convertOutput(BasicBlock *BB, const ArrayRef<size_t> &NumsInstr) {
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

// The way of representing output parameters of basic blocks
// Indicies of BB's instructions
using BBOutputIds = SmallVector<size_t, 8>;
using BBOutputIdsRef = std::reference_wrapper<BBOutputIds>;

namespace {

struct BBInfo {
  BBInfo(BasicBlock *BB)
      : BB(BB), Inputs(getInput(BB)), OutputsIds(getOutput(BB)),
        Outputs(convertOutput(BB, OutputsIds)) {}

  BasicBlock *BB;
  SmallVector<Value *, 8> Inputs;
  BBOutputIds OutputsIds;
  SmallVector<Value *, 8> Outputs;

  Value *ReturnVal = nullptr;
  bool WasReplaced = false;
};

} // end anonymous namespace

using BBInfoRef = std::reference_wrapper<BBInfo>;
using BBEqualInfoOutput = SmallVector<BBInfoRef, 8>;

// TODO: test structure as return type
/// Select a function return value from array of operands
static size_t getFunctionRetValId(const ArrayRef<Value *> Outputs) {
  return find_if(Outputs.rbegin(), Outputs.rend(),
                 [](Value *V) { return V->getType()->isFirstClassType(); }) -
         Outputs.rbegin();
}

static bool canThrow(const BasicBlock *BB) {
  auto It = BB->begin();
  auto EIt = getEndIt(BB);
  // skip all phi nodes
  for (; isa<PHINode>(It) && It != EIt; ++It)
    ;

  for (; It != EIt; ++It) {
    if (auto CallI = dyn_cast<CallInst>(It)) {
      if (!CallI->getFunction()->hasFnAttribute(Attribute::NoUnwind))
        return true;
    }
  }
  return false;
}

// TODO: return nullptr if created function is too small
// TODO: set input attributes from created BB
/// Creates new function, that consists of Basic block \p BB
/// \param Input - values, which types are input function arguments
/// \param Output - values, which types with added pointer are
/// output function arguments. They follow right after Input params.
/// \param ReturnValue - value, which type is function's return value.
static Function *createFuncFromBB(const BBInfo &Info) {
  BasicBlock *BB = Info.BB;
  auto &Input = Info.Inputs;
  auto &Output = Info.Outputs;
  const Value *ReturnValue = Info.ReturnVal;

  Module *M = BB->getModule();
  LLVMContext &Context = M->getContext();
  const DataLayout &Layout = M->getDataLayout();
  // create Function
  SmallVector<Type *, 8> Params;
  Params.reserve(Input.size() + Output.size());

  std::transform(Input.begin(), Input.end(), std::back_inserter(Params),
                 [](const Value *V) { return V->getType(); });

  Type *FunctionReturnT =
      ReturnValue ? ReturnValue->getType() : Type::getVoidTy(Context);

  transform(Output.begin(), Output.end(), std::back_inserter(Params),
            [](const Value *V) { return PointerType::get(V->getType(), 0); });

  FunctionType *FType = FunctionType::get(FunctionReturnT, Params, false);
  Function *F =
      Function::Create(FType, GlobalValue::LinkageTypes::PrivateLinkage, "", M);

  // add some attributes
  F->addFnAttr(Attribute::Naked);
  F->addFnAttr(Attribute::MinSize);
  F->addFnAttr(Attribute::OptimizeForSize);
  // newly created function can't call itself
  F->addFnAttr(Attribute::NoRecurse);
  bool throws = canThrow(BB);
  if (!throws)
    F->addFnAttr(Attribute::NoUnwind);

  // set attributes to all output params
  for (size_t i = Input.size() + 1, ie = Params.size() + 1; i < ie; ++i) {
    F->addAttribute(
        static_cast<unsigned>(i),
        Attribute::get(
            Context, Attribute::AttrKind::Dereferenceable,
            Layout.getTypeStoreSize(Params[i - 1]->getPointerElementType())));
    F->addAttribute(static_cast<unsigned>(i),
                    Attribute::get(Context, Attribute::AttrKind::NoAlias));
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
  auto I = BB->begin();
  // phi nodes should be kept in removed BB
  for (; isa<PHINode>(&*I); ++I)
    ;

  auto Ie = getEndIt(BB);
  for (; I != Ie; ++I) {
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

  DEBUG(dbgs() << "Function created:");
  DEBUG(F->print(dbgs()));
  DEBUG(dbgs() << '\n');
  ++FunctionCounter;
  return F;
}

/// \param Input - \p BB operands to be used as an input arguments to \p F
/// \param Output - \p BB operands to be used as an output arguments to \p F
/// \param Result - \p BB instruction to be used as a result of \p F
/// \returns true if \p BB was replaced with \p F, false otherwise
static bool replaceBBWithFunctionCall(const BBInfo &Info, Function *F) {
  // check if this BB was already replaced
  if (Info.WasReplaced)
    return false;

  BasicBlock *BB = Info.BB;
  auto &Input = Info.Inputs;
  auto &Output = Info.Outputs;
  Value *Result = Info.ReturnVal;

  std::string NewName = BB->getName();
  BasicBlock *NewBB =
      BasicBlock::Create(BB->getContext(), "", BB->getParent(), BB);
  IRBuilder<> Builder(NewBB);
  SmallVector<Value *, 8> Args;
  auto GetValueForArgs = [&Builder](Value *V, Type *T) {
    return V->getType() == T ? V : Builder.CreateBitCast(V, T);
  };

  assert(F->arg_size() == Input.size() + Output.size() &&
         "Argument sizes not match");
  Args.reserve(F->arg_size());

  // Construct our function argumemnts Args
  // 1) Phi nodes that should be at the beginning of new BB
  // and first arguments of our new function
  auto CurArg = F->arg_begin();
  for (auto I = BB->begin(); isa<PHINode>(&*I); ++I, ++CurArg) {
    assert(I != std::prev(BB->end()) &&
           "Blocks that consist of only phi nodes shouldn't reach here");
    auto newPhi = Builder.Insert(I->clone(), I->getName());

    Args.push_back(GetValueForArgs(&*newPhi, CurArg->getType()));
    I->replaceAllUsesWith(newPhi);
  }

  assert(Args.size() <= Input.size() &&
         "All Phi nodes should be included in Input list");

  // 2) Append rest of Input arguments, that were created in other BB
  for (auto I = Input.begin() + Args.size(), IE = Input.end(); I != IE;
       ++I, ++CurArg) {
    Args.push_back(GetValueForArgs(*I, CurArg->getType()));
  }

  // 3) Allocate space for output pointers, that are Output function arguments
  for (auto IE = F->arg_end(); CurArg != IE; ++CurArg) {
    Args.push_back(
        Builder.CreateAlloca(CurArg->getType()->getPointerElementType()));
  }

  Instruction *CallFunc = Builder.CreateCall(F, Args);
  if (Result) {
    Value *ResultReplace = GetValueForArgs(CallFunc, Result->getType());
    Result->replaceAllUsesWith(ResultReplace);
  }

  // Save and Replace all Output values
  auto AllocaIt = Args.begin() + Input.size();
  for (auto I : Output) {
    if (isValUsedOutsideOfBB(I, BB)) {
      auto Inst = Builder.CreateLoad(*AllocaIt++);
      auto BitCasted = GetValueForArgs(Inst, I->getType());
      I->replaceAllUsesWith(BitCasted);
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

  ++MergeCounter;
  return true;
}

/// Merges set of sorted vectors into sorted vector of unique values
/// All values of this map are unused, use this map as set of output values
/// Example. combineOutputs : {{1, 2, 3}, {1, 2}, {1, 6}} => {1, 2, 3, 6}
static BBOutputIds combineOutputs(const ArrayRef<BBEqualInfoOutput> Outputs) {

  auto ExtractBBOutputIds = [&Outputs](size_t i) -> BBOutputIds & {
    assert(i < Outputs.size());
    return Outputs[i][0].get().OutputsIds;
  };
  if (Outputs.size() == 1) {
    return ExtractBBOutputIds(0);
  }

  BBOutputIds Result;
  if (Outputs.size() == 0) {
    return Result;
  }

  // ItCurrentEnd::first points to the least element, that is
  // still not in the resultant set
  // ItCurrentEnd::second always points to the last element
  // used as a right border
  using ItCurrentEnd = std::pair<SmallVectorImpl<size_t>::const_iterator,
                                 const SmallVectorImpl<size_t>::const_iterator>;
  // vector of iterator pair, that steps from begin to end
  SmallVector<ItCurrentEnd, 8> Its;
  Its.reserve(Outputs.size());
  for (size_t i = 0, ei = Outputs.size(); i < ei; ++i) {
    auto &Out = ExtractBBOutputIds(i);
    Its.push_back(std::make_pair(Out.begin(), Out.end()));
  }

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

/// Checks if the Function F, that consists of 1 basic block can be used
/// as a factor of other basic blocks
static bool isRightOrder(const Function *F, const ArrayRef<Value *> Inputs,
                         const ArrayRef<Value *> Outputs) {
  assert(F->size() == 1);
  if (F->isVarArg())
    return false;
  assert(!isa<PHINode>(F->front().front()));

  bool NotVoid = F->getReturnType() != Type::getVoidTy(F->getContext());
  assert(Inputs.size() + Outputs.size() == F->arg_size() + NotVoid);

  Value *ReturnVal = nullptr;
  if (NotVoid) {
    ReturnVal = cast<ReturnInst>(F->front().back()).getReturnValue();
    Value *OurRetVal = Outputs[getFunctionRetValId(Outputs)];
    if (OurRetVal != ReturnVal)
      return false;
  }

  auto It = F->arg_begin();
  for (auto Input : Inputs) {
    if (&*It++ != Input)
      return false;
  }

  for (auto Output : Outputs) {
    if (Output == ReturnVal)
      continue;
    if (&*It++ != Output)
      return false;
  }

  return true;
}

static void selectSoloReturnValue(BBInfo &Info, size_t InstNum) {
  size_t Index = find(Info.OutputsIds, InstNum) - Info.OutputsIds.begin();
  // don't erase Index in OutputsIds because we will not use it anymore
  Info.ReturnVal = Info.Outputs[Index];
  Info.Outputs.erase(Info.Outputs.begin() + Index);
}

static void selectReturnValue(std::vector<BBInfo> &Infos) {
  assert(!Infos.empty());
  size_t ReturnIdF = getFunctionRetValId(Infos.front().Outputs);
  if (ReturnIdF == Infos.front().Outputs.size()) {
    // return type void ~ nullptr
    return;
  }
  ReturnIdF = Infos.front().OutputsIds[ReturnIdF];
  for (auto &Info : Infos) {
    if (Info.WasReplaced)
      continue;
    selectSoloReturnValue(Info, ReturnIdF);
  }
}

/// \param BBs
/// \return nullptr if no basic blocks replaced, or basic block, which replaced
/// all others otherwise
static bool tryMergeWithExistingF(SmallVectorImpl<BBInfoRef> &BBs) {
  if (BBs.size() == 1)
    return false;
  // Try to find appropriate function
  size_t i = 0, ei = BBs.size();
  for (; i < ei; ++i) {
    Function *F = BBs[i].get().BB->getParent();
    if (F->size() == 1) {
      if (isRightOrder(F, BBs[i].get().Inputs, BBs[i].get().Outputs))
        break;
    }
  }
  if (i == ei)
    return false;
  debugPrint(BBs[i].get().BB, "Suitable function found");
  Function *F = BBs[i].get().BB->getParent();
  Value *V = cast<ReturnInst>(F->front().back()).getReturnValue();
  size_t Idx = find(BBs[i].get().Outputs, V) - BBs[i].get().Outputs.begin();
  assert(Idx != BBs[i].get().Outputs.size());
  Idx = BBs[i].get().OutputsIds[Idx];

  // calculate Idx of RetVal
  for (size_t j = 0, sz = BBs.size(); j < sz; ++j) {
    if (i == j)
      continue;
    selectSoloReturnValue(BBs[j], Idx);
    replaceBBWithFunctionCall(BBs[j], F);
    BBs[j].get().WasReplaced = true;
  }

  return true;
}

/// \param BBInfos
/// \return
static SmallVector<BBEqualInfoOutput, 8>
combineByOutputsIds(std::vector<BBInfo> &BBInfos) {
  std::deque<BBInfoRef> BBRefInfos(BBInfos.begin(),
                                   BBInfos.end()); // list or deque
  // Most likely all BBs have the same set of output args
  SmallVector<BBEqualInfoOutput, 8> Result;
  while (!BBRefInfos.empty()) {
    Result.push_back(BBEqualInfoOutput());
    const BBInfo &Current = BBRefInfos.front();
    // remove from BBRefInfos and insert into Result
    for (auto It = BBRefInfos.begin(); It != BBRefInfos.end();) {
      if (It->get().OutputsIds == Current.OutputsIds) {
        Result.back().push_back(*It);
        It = BBRefInfos.erase(It);
        continue;
      }
      ++It;
    }
  }
  return Result;
};

bool BBFactoring::replace(const std::vector<BasicBlock *> &BBs) {
  assert(BBs.size() >= 2 && "No sence in merging");

  if (BBs.front()->size() <= 3) {
    debugPrint(BBs.front(), "Block family is too small to bother merging");
    return false;
  }
  bool Changed = false;

  std::vector<BBInfo> BBInfos(BBs.begin(), BBs.end());

  // Input Validation
  assert(std::equal(std::next(BBInfos.begin()), BBInfos.end(), BBInfos.begin(),
                    [](const BBInfo &Val1, const BBInfo &Val2) {
                      return Val1.Inputs.size() == Val2.Inputs.size() &&
                             std::equal(
                                 Val1.Inputs.begin(), Val1.Inputs.end(),
                                 Val2.Inputs.begin(), Val2.Inputs.end(),
                                 [](Value *V1, Value *V2) {
                                   return V1->getType()->canLosslesslyBitCastTo(
                                       V2->getType());
                                 });
                    }) &&
         "Input argument types of identity BBs must be equal");

  SmallVector<BBEqualInfoOutput, 8> EqualOutputIds =
      combineByOutputsIds(BBInfos);

  // at first we try to replace basic blocks without creating a function
  for (auto &Outputs : EqualOutputIds) {
    Changed |= tryMergeWithExistingF(Outputs);
  }
  long C = std::count_if(BBInfos.begin(), BBInfos.end(),
                         [](const BBInfo &I) { return !I.WasReplaced; });
  assert(C != 0);
  if (C == 1) {
    // 1 is our function with solo body. All Basic blocks were replaced with
    // call to the function. There is no sence in creating new one.
    return Changed;
  }

  // create common
  BBOutputIds BBOutputsIds = combineOutputs(EqualOutputIds);

  selectReturnValue(BBInfos);

  Function *F = createFuncFromBB(BBInfos.front());

  if (F == nullptr) {
    debugPrint(BBs.front(), "Unprofitable replacement");
    return false;
  }

  for (size_t i = 0, sz = BBs.size(); i < sz; ++i) {
    Changed |= replaceBBWithFunctionCall(BBInfos[i], F);
  }

  DEBUG(dbgs() << BBs.size()
               << " basic blocks were replaced with just created function "
               << F->getName() << "\n");

  return true;
}