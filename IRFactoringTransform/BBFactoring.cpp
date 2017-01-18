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
/// with commutativity check and without checking phi values and term insts,
/// because they are not part of our factored BB.
/// After finding identity BBs, pass creates new function (if necessary)
/// and replaces the factored BB with tail call to the appropriate function.
/// Pass works only with well-formed basic blocks.
/// Definition: Factored BB is the part of Basic Block, which can be replaced
/// with this pass. Factored BB consists of the whole BB without Phi nodes and
/// terminator instructions.
///
//===----------------------------------------------------------------------===//

#include "../external/merging.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <set>

#define DEBUG_TYPE "bbfactor"

STATISTIC(MergeCounter, "Counts total number of merged basic blocks");
STATISTIC(FunctionCounter, "Counts amount of created functions");

// TODO: ? skip LandingPad Instructions
// TODO: pick appropriate constants for filtering bad BB factorizing
//       (# of outputs, # of inputs, # instructions in function)
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


/// \return end iterator of the factored part of \p BB
static inline BasicBlock::iterator getEndIt(BasicBlock *BB) {
  assert(isa<TerminatorInst>(BB->back()));
  return std::prev(BB->end());
}

static inline BasicBlock::const_iterator getEndIt(const BasicBlock *BB) {
  assert(isa<TerminatorInst>(BB->back()));
  return std::prev(BB->end());
}


/// \return begin iterator of the factored part of \p BB
static inline BasicBlock::iterator getBeginIt(BasicBlock *BB) {
  auto It = BB->begin();
  while (isa<PHINode>(It))
    ++It;
  return It;
}

static inline BasicBlock::const_iterator getBeginIt(const BasicBlock *BB) {
  auto It = BB->begin();
  while (isa<PHINode>(It))
    ++It;
  return It;
}

static void debugPrint(const BasicBlock *BB, const StringRef Str) {
  DEBUG(dbgs() << Str << ". Block: " << BB->getName()
               << ". Function: " << BB->getParent()->getName() << '\n');
}

/// \return Values, that were created outside of the factored \p BB
static SmallVector<Value *, 8> getInput(BasicBlock *BB) {
  // Values, created by factored BB or inserted into Result as Input
  SmallPtrSet<const Value *, 8> Values;
  SmallVector<Value *, 8> Result;

  auto I = getBeginIt(BB);
  auto IE = getEndIt(BB);
  for (; I != IE; ++I) {
    Values.insert(&*I);
    assert(!isa<TerminatorInst>(I) && !isa<PHINode>(I) && "Malformed BB");

    for (auto &Ops : I->operands()) {
      if (isa<Constant>(Ops.get()))
        continue;
      // TODO: check global variable
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
/// and PhiNodes like instructions in other block because
/// they are not part of the created function.
/// \return if the value \p V is used outside the function, created from \p BB
static bool isValUsedOutsideOfBB(const Value *V, const BasicBlock *BB) {
  for (const Use &U : V->uses()) {
    const Instruction *I = cast<Instruction>(U.getUser());
    if (I->getParent() != BB || isa<TerminatorInst>(I) || isa<PHINode>(I))
      return true;
  }
  return false;
}

/// Get values, that are created inside of \p BB and
/// are used outside of it.
/// Result is written as a vector of instruction's numerical order in BB
static SmallVector<size_t, 8> getOutput(const BasicBlock *BB) {
  SmallVector<size_t, 8> Result;
  size_t Current = 0;
  auto I = getBeginIt(BB);
  auto IE = getEndIt(BB);

  for (; I != IE; ++I, ++Current) {
    if (isValUsedOutsideOfBB(&*I, BB)) {
      Result.push_back(Current);
    }
  }
  return Result;
}

/// Converts instruction numbers of \p BB
/// into Values * \p NumsInstr
static SmallVector<Value *, 8> convertOutput(BasicBlock *BB,
                                             const ArrayRef<size_t> NumsInstr) {
  SmallVector<Value *, 8> Result;
  Result.reserve(NumsInstr.size());
  if (NumsInstr.empty())
    return Result;
  auto It = getBeginIt(BB);
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
using BBOutputIdsRef = std::reference_wrapper<const BBOutputIds>;
using BBOutputStorage = std::set<BBOutputIds>;

namespace {

struct BBInfo {
  BBInfo(BasicBlock *BB, BBOutputStorage &OutputStorage)
      : BB(BB), Inputs(getInput(BB)),
        OutputsIds(*OutputStorage.emplace(getOutput(BB)).first),
        Outputs(convertOutput(BB, OutputsIds.get())) {}

  BasicBlock *BB;
  SmallVector<Value *, 8> Inputs;
  BBOutputIdsRef OutputsIds;
  SmallVector<Value *, 8> Outputs;

  Value *ReturnVal = nullptr;
  bool WasReplaced = false;
};

} // end anonymous namespace

using BBInfoRef = std::reference_wrapper<BBInfo>;
using BBEqualInfoOutput = SmallVector<BBInfoRef, 8>;

/// Select a function return value from array of operands
static size_t getFunctionRetValId(const ArrayRef<Value *> Outputs) {
  return find_if(Outputs.rbegin(), Outputs.rend(),
                 [](Value *V) { return V->getType()->isFirstClassType(); }) -
         Outputs.rbegin();
}


/// \return whether \p BB is able to throw
static bool canThrow(const BasicBlock *BB) {
  auto It = getBeginIt(BB);
  auto EIt = getEndIt(BB);

  for (; It != EIt; ++It) {
    if (auto CallI = dyn_cast<CallInst>(It)) {
      // we can definetly say that function with no unwind can't throw,
      // otherwise undefined behaviour
      if (!CallI->getFunction()->hasFnAttribute(Attribute::NoUnwind))
        return true;
    }
  }
  return false;
}


/// \param Model - the Basic block, which helps us to create a Function
/// \param Sizes - amount of merging basic blocks
/// \return whether code size will reduce if factored with creating new function
static bool shouldCreateFunction(const BBInfo &Model, const size_t Sizes) {
  assert(Sizes >= 2);
  BasicBlock *BB = Model.BB;

  auto It = getBeginIt(BB), EIt = getEndIt(BB);
  // approximately amount of instructions for the backend
  // assume general case, sizes of all instructions are equal
  size_t Points = 0;
  while (It != EIt && isa<PHINode>(It))
    ++It;
  for (; It != EIt; ++It) {
    // TODO: add more insts/operators, llvm intrinsics
    if (isa<BitCastInst>(It) ||
        isa<BitCastOperator>(It)) // TODO: difference between 2 of them
      continue;

    ++Points;
  }

  size_t CallCost = 1 + 2 * Model.Outputs.size() + Model.Inputs.size();
  if (Points <= CallCost)
    return false;

  size_t InstsProfitBy1Replacement = Points - CallCost;
  // check if we don't lose created a function, and it's costs are lower,
  // than total gain of function replacement
  return Sizes * InstsProfitBy1Replacement >= Points;
}

// TODO: set input attributes from created BB
/// \param Info - Information about model basic block
/// \return new function, that consists of Basic block \p Info BB
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
  auto I = getBeginIt(BB);
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


/// \param Info - Basic block, which is going to be replaced with function call to \p F
/// \return true if \p BB was replaced with \p F, false otherwise
static bool replaceBBWithFunctionCall(const BBInfo &Info, Function *F) {
  // check if this BB was already replaced
  if (Info.WasReplaced)
    return false;

  BasicBlock *BB = Info.BB;
  auto &Input = Info.Inputs;
  auto &Output = Info.Outputs;
  Value *Result = Info.ReturnVal;

  auto Beg = getBeginIt(BB);

  IRBuilder<> Builder(cast<Instruction>(Beg));
  auto GetValueForArgs = [&Builder](Value *V, Type *T) {
    return V->getType() == T ? V : Builder.CreateBitCast(V, T);
  };

  SmallVector<Value *, 8> Args;
  assert(F->arg_size() == Input.size() + Output.size() &&
         "Argument sizes not match");
  Args.reserve(F->arg_size());

  auto CurArg = F->arg_begin();
  // 1) Append Input arguments into call argument list
  for (auto I = Input.begin(), IE = Input.end(); I != IE; ++I, ++CurArg) {
    Args.push_back(GetValueForArgs(*I, CurArg->getType()));
  }

  // 2) Allocate space for output pointers, that are Output function arguments
  for (auto IE = F->arg_end(); CurArg != IE; ++CurArg) {
    Args.push_back(
        Builder.CreateAlloca(CurArg->getType()->getPointerElementType()));
  }

  // 3) Create a call
  Instruction *LastBBInst = Builder.CreateCall(F, Args);
  if (Result) {
    Value *ResultReplace = GetValueForArgs(LastBBInst, Result->getType());
    Result->replaceAllUsesWith(ResultReplace);
  }

  // 4) Save and Replace all Output values
  auto AllocaIt = Args.begin() + Input.size();
  for (auto It = Output.begin(), EIt = Output.end(); It != EIt;
       ++It, ++AllocaIt) {
    Value *CurrentValue = *It;
    if (!isValUsedOutsideOfBB(CurrentValue, BB))
      continue;

    LastBBInst = Builder.CreateLoad(*AllocaIt);
    auto BitCasted = GetValueForArgs(LastBBInst, CurrentValue->getType());
    CurrentValue->replaceAllUsesWith(BitCasted);
  }

  // 5) perform cleanup from the end because in opposite llvm will
  // complain that value is still in use
  auto LastToDelete = getEndIt(BB);
  while (&*--LastToDelete != LastBBInst)
    LastToDelete = LastToDelete->eraseFromParent();

  ++MergeCounter;
  return true;
}

/// Merges set of sorted vectors into sorted vector of unique values
/// All values of this map are unused, use this map as set of output values
/// Example. combineOutputs : {{1, 2, 3}, {1, 2}, {1, 6}} => {1, 2, 3, 6}
static BBOutputIds combineOutputs(const BBOutputStorage &Outputs) {

  if (Outputs.size() == 1) {
    return *Outputs.begin();
  }

  BBOutputIds Result;
  if (Outputs.size() == 0) {
    return Result;
  }

  // ItCurrentEnd::first points to the least element, that is
  // still not in the resultant set
  // ItCurrentEnd::second always points to the last element
  // used as a right border
  using ItCurrentEnd =
      std::pair<BBOutputIds::const_iterator, const BBOutputIds::const_iterator>;
  // vector of iterator pair, that steps from begin to end
  SmallVector<ItCurrentEnd, 8> Its;
  Its.reserve(Outputs.size());
  for (auto &Output : Outputs) {
    if (!Output.empty())
      Its.push_back(std::make_pair(Output.begin(), Output.end()));
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
      if (*It.first == CurMin)
        ++It.first;
      if (It.first == It.second)
        continue;

      if (*It.first < NextMin) {
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


/// \param F - possible function to be merged with args \p Inputs, \p Outputs
/// \returns true, if the Function F, that consists of 1 basic block can be used
/// as a callee of other basic blocks
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

static void extractReturnValue(BBInfo &Info, size_t InstNum) {
  size_t Index =
      find(Info.OutputsIds.get(), InstNum) - Info.OutputsIds.get().begin();
  // don't erase Index in OutputsIds because we will not use it anymore
  Info.ReturnVal = Info.Outputs[Index];
  Info.Outputs.erase(Info.Outputs.begin() + Index);
}

static void findAndExtractReturnValue(const BBInfo &Model,
                                      std::vector<BBInfo> &Infos) {
  size_t ReturnIdF = getFunctionRetValId(Model.Outputs);
  if (ReturnIdF == Model.Outputs.size()) {
    // return type void ~ nullptr
    return;
  }

  ReturnIdF = Model.OutputsIds.get()[ReturnIdF];
  for (BBInfo &Info : Infos) {
    if (Info.WasReplaced)
      continue;
    extractReturnValue(Info, ReturnIdF);
  }
}

/// \param Info - the only basic block of its parent
/// \return Id in output values of return value
static size_t getFunctionReturnId(const BBInfo &Info) {
  Function *F = Info.BB->getParent();
  assert(F->size() == 1);
  Value *V = cast<ReturnInst>(F->front().back()).getReturnValue();
  size_t Idx = find(Info.Outputs, V) - Info.Outputs.begin();
  assert(Idx != Info.Outputs.size());
  return Info.OutputsIds.get()[Idx];
}

/// \param BBs array of identical basic blocks with equal Input and Output
/// \return false if no basic blocks were replaced, true otherwise
static bool tryMergeWithExistingF(SmallVectorImpl<BBInfoRef> &BBs) {
  // Try to find appropriate function
  size_t i = 0, ei = BBs.size();
  for (; i < ei; ++i) {
    assert(!BBs[i].get().WasReplaced && "BBs should be filtered");
    Function *F = BBs[i].get().BB->getParent();
    if (F->size() == 1) {
      if (isRightOrder(F, BBs[i].get().Inputs, BBs[i].get().Outputs))
        break;
    }
  }
  if (i == ei)
    return false;
  debugPrint(BBs[i].get().BB, "Found suitable function with 1 Basic Block");

  Function *F = BBs[i].get().BB->getParent();
  size_t Idx = getFunctionReturnId(BBs[i]);

  for (size_t j = 0, sz = BBs.size(); j < sz; ++j) {
    if (i == j)
      continue;
    extractReturnValue(BBs[j], Idx);
    replaceBBWithFunctionCall(BBs[j], F);
    BBs[j].get().WasReplaced = true;
  }

  return true;
}

/// \param BBInfos array of all basic blocks
/// \return Infos, grouped by equal output
static SmallVector<BBEqualInfoOutput, 8>
combineByOutputsIds(std::vector<BBInfo> &BBInfos) {
  std::deque<BBInfoRef> BBRefInfos(BBInfos.begin(),
                                   BBInfos.end()); // list or deque
  // Most likely all BBs have the same set of output args
  SmallVector<BBEqualInfoOutput, 8> Result;
  while (!BBRefInfos.empty()) {
    Result.push_back(BBEqualInfoOutput());
    const BBInfo &Current = BBRefInfos.front();
    // remove all elements, which are equal to Current.OutputsIds
    // from BBRefInfos and insert into Result
    for (auto It = BBRefInfos.begin(); It != BBRefInfos.end();) {
      if (It->get().OutputsIds.get() == Current.OutputsIds.get()) {
        Result.back().push_back(*It);
        It = BBRefInfos.erase(It);
        continue;
      }
      ++It;
    }
  }
  return Result;
};

/// Common steps of replacing equal basic blocks
/// 1) Separate basic blocks by output values into sets of basic blocks
/// and try to merge each of these sets with existing basic block
/// 2) Combine all outputs and watch if there is a function (with 1 this block)
/// which can replace all others
/// 3) If there are some basic blocks left, create function and merge all the
/// rest basic blocks, if it is profitable to replace them with newly-created
/// function
/// \param BBs array of equal basic blocks
/// \return true if any BB was changed
bool BBFactoring::replace(const std::vector<BasicBlock *> &BBs) {
  assert(BBs.size() >= 2 && "No sence in merging");

  if (BBs.front()->size() <= 3) {
    debugPrint(BBs.front(), "Block family is too small to bother merging");
    return false;
  }
  bool Changed = false;

  BBOutputStorage OutputStorage;
  std::vector<BBInfo> BBInfos;
  BBInfos.reserve(BBs.size());
  std::transform(
      BBs.begin(), BBs.end(), std::back_inserter(BBInfos),
      [&OutputStorage](BasicBlock *BB) { return BBInfo(BB, OutputStorage); });

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
  // but functions should have the same output
  for (auto &Outputs : EqualOutputIds) {
    Changed |= tryMergeWithExistingF(Outputs);
  }
  auto IfNotReplaced = [](const BBInfo &I) { return !I.WasReplaced; };
  size_t NotReplaced =
      (size_t)std::count_if(BBInfos.begin(), BBInfos.end(), IfNotReplaced);
  assert(NotReplaced > 0 && "At least one should not be replaced");
  if (NotReplaced == 1) {
    // 1 is our function with solo body. All Basic blocks were replaced with
    // call to the function. There is no sence in creating new one.
    return true;
  }

  // create common outputs and modify outputValues
  {
    auto OutputsIds =
        OutputStorage.emplace(combineOutputs(OutputStorage)).first;
    for (auto &Info : BBInfos) {
      // if their sizes are not equal => common output differs from Info's
      // if was already replaces => meaningless to replace sth. there
      //   because this value won't be used
      if (!Info.WasReplaced && &*OutputsIds != &Info.OutputsIds.get()) {
        Info.OutputsIds = *OutputsIds; // copies just pointer
        Info.Outputs = convertOutput(Info.BB, *OutputsIds);
      }
    }
  }
  // try again to replace, not creating function, but with common OutputsIds
  {
    BBEqualInfoOutput ActiveBBs;
    std::copy_if(BBInfos.begin(), BBInfos.end(), std::back_inserter(ActiveBBs),
                 IfNotReplaced);
    Changed |= tryMergeWithExistingF(ActiveBBs);
    NotReplaced =
        (size_t)std::count_if(BBInfos.begin(), BBInfos.end(), IfNotReplaced);
    if (NotReplaced == 1) {
      return true;
    }
  }

  // preparation to create function
  auto Model = find_if(BBInfos, IfNotReplaced);
  assert(Model != BBInfos.end());

  findAndExtractReturnValue(*Model, BBInfos);

   if (!shouldCreateFunction(*Model, NotReplaced)) {
    debugPrint(BBs.front(), "Unprofitable replacement");
    return false;
  }

  Function *F = createFuncFromBB(*Model);

  for (size_t i = 0, sz = BBs.size(); i < sz; ++i) {
    Changed |= replaceBBWithFunctionCall(BBInfos[i], F);
  }

  DEBUG(dbgs() << BBs.size()
               << " basic blocks were replaced with just created function "
               << F->getName() << "\n");

  return true;
}