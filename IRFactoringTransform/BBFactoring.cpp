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
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <set>

#define DEBUG_TYPE "bbfactor"

STATISTIC(MergeCounter, "Counts total number of merged basic blocks");
STATISTIC(FunctionCounter, "Counts amount of created functions");

using namespace llvm;

// used for testing
static cl::opt<bool>
    ForceMerge("bbfactor-force-merging", cl::Hidden,
               cl::desc("Force folding basic blocks, when it is unprofitable"));

// TODO: ? decide what to do with extra value of lifetime start/end and
// similar intrinsics, that create variables, but don't produce any code.
// The main issue is that these variables are used as input/output arguments for
// created functions
// Possible solutions:
// a) remove the entries, which are the part of our block [easier to implement]
// b) don't factor out these instructions into new function [preferable]
// TODO: ? place BB comparing in this file

// TODO: ? Elaborate function searching
//   a) If suitable function exists, but arguments order is not the same,
// try to permute bb's input/output arguments to be able to call the function.

namespace {
/// Auxiliary class, that holds basic block and it's hash.
/// Used BB for comparison
class BBNode {
  mutable BasicBlock *BB;
  BBComparator::BasicBlockHash Hash;

public:
  // Note the hash is recalculated potentially multiple times, but it is cheap.
  BBNode(BasicBlock *BB)
      : BB(BB), Hash(BBComparator::basicBlockHash(*BB, false, false)) {}

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
        HashedBBs.push_back({BBComparator::basicBlockHash(BB, false, false), &BB});
    }
  }

  std::vector<std::vector<BasicBlock *>> IdenticalBlocksContainer;
  auto BBTree = std::map<BBNode, size_t, BBNodeCmp>(BBNodeCmp(&GlobalNumbers));

  // merge hashed values into map
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

/// The way of representing output and skipped instructions of basic blocks
using BBInstIds = SmallVector<size_t, 8>;


// TODO: ? make template with order. Need reverse order, Instruction * values.
/// Class is used for smart searching of skipped instructions
/// Class is useful when we need to traverse [getBeginIt, getEndIt) basic block.
class BBSkippedIds
{
public:
  void checkBegin() const { assert(Cur == SkippedIds.begin() &&
                                   "Cur should point to the beginning of the array"); }
  void push_back(const size_t InstId) {
    assert((SkippedIds.empty() || SkippedIds.back() < InstId) &&
           "SkippedIds should be sorted");
    SkippedIds.push_back(InstId);
  }

  void resetIt() const {
    Cur = SkippedIds.begin();
  }
  const BBInstIds &get() const { return SkippedIds;}

  /// \param InstId number of instruction
  /// \return true if \p InstId is number of instruction
  /// should be skipped from factoring out
  bool contains(size_t InstId) const;

private:
  /// Ascendingly sorted vector of instruction numbers
  BBInstIds SkippedIds;
  mutable BBInstIds::const_iterator Cur;
};

bool BBSkippedIds::contains(size_t InstId) const {
  if (Cur == SkippedIds.end()) // no skipped elements
    return false;
  if (*Cur != InstId)
    return false;
  Cur = (Cur == std::prev(SkippedIds.end())) ? SkippedIds.begin() : Cur + 1;
  return true;
}

/// \return Values, that were created outside of the factored \p BB
static SmallVector<Value *, 8> getInput(BasicBlock *BB, const BBSkippedIds &SkipIds) {
  // Values, created by factored BB or inserted into Result as Input
  SmallPtrSet<const Value *, 8> Values;
  SmallVector<Value *, 8> Result;

  size_t InstNum = 0;
  SkipIds.checkBegin();
  for (auto I = getBeginIt(BB), IE = getEndIt(BB); I != IE; ++I, ++InstNum) {
    if (SkipIds.contains(InstNum)) {
      continue;
    }
    Values.insert(&*I);
    assert(!isa<TerminatorInst>(I) && !isa<PHINode>(I) && "Malformed BB");

    for (auto &Ops : I->operands()) {
      // global value is treated like constant
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

/// This function is like isInstUsedOutsideOfBB, but does
/// consider Phi nodes and TerminatorInsts as a special case, because
/// they are not part of the created function.
/// \return if the value \p V is used outside it's parent function
static bool isInstUsedOutsideParent(const Instruction *V) {
  const BasicBlock *BB = V->getParent();
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
    if (isInstUsedOutsideParent(&*I)) {
      Result.push_back(Current);
    }
  }
  return Result;
}

/// Converts instruction numbers of \p BB
/// into Values * \p NumsInstr
static SmallVector<Instruction *, 8> convertInstIds(BasicBlock *BB,
                                                    const BBInstIds &NumsInstr) {
  SmallVector<Instruction *, 8> Result;
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

namespace {

/// Structure, that keeps all important information about basic blocks,
/// that are going to be factor out
struct BBInfo {
  BBInfo(BasicBlock *BB, const BBSkippedIds &Ids)
    : BB(BB), Inputs(getInput(BB, Ids))
  {}

  BasicBlock *BB;
  SmallVector<Value *, 8> Inputs;

  SmallVector<Instruction *, 8> Outputs;
  Value *ReturnVal = nullptr;

  void extractReturnValue(const size_t ResultId);
};

} // end anonymous namespace

void BBInfo::extractReturnValue(const size_t ResultId) {
  assert(ReturnVal == nullptr && "Return value should be set once");
  assert(ResultId != Outputs.size() && "Should be index of Outputs");

  std::swap(Outputs[ResultId], Outputs.back());
  ReturnVal = Outputs.back();
  Outputs.pop_back();
}

namespace {

struct BBsCommonInfo {
  size_t Weight;
  BBInstIds OutputIds;
  BBSkippedIds SkippedInsts;

  void mergeOutput(const BBInstIds& Ids);
  void setSkippedInsts(BasicBlock *BB);
};

} // end anonymous namespace

void BBsCommonInfo::mergeOutput(const BBInstIds &Ids) {
  assert(std::is_sorted(Ids.begin(), Ids.end()) &&
         "Output values must be sorted into ascending order");
  auto IdsIt = Ids.begin(), IdsEIt = Ids.end();
  for (auto It = OutputIds.begin(), EIt = OutputIds.end();
       It != EIt && IdsIt != IdsEIt; ++It) {
    size_t This = *It;
    size_t Other = *IdsIt;

    if (Other == This) {
      ++IdsIt;
    }
    else if (Other < This) {
      It = OutputIds.insert(It, Other); // It points to *IdsIt
      EIt = OutputIds.end();
      ++IdsIt;
    }
    // else (Other > This) continue;

  }
  if (IdsIt != IdsEIt)
    OutputIds.insert(OutputIds.end(), IdsIt, IdsEIt);
}

/// \param I instruction, that is decided whether to be skipped
/// \param AlreadySkipped array of already skipped instructions
/// \param Outputs output instructions
/// \return true if instruction will not be factored out in separate function
static bool skipInst(const Instruction *I,
                     const SmallVectorImpl<Instruction *> &AlreadySkipped,
                     const SmallVectorImpl<Instruction *> &Outputs) {
  const BasicBlock *BB = I->getParent();
  if (find(Outputs, I) != Outputs.end()) {
    if (isa<AllocaInst>(I))
      return true;

    if (auto GEP = dyn_cast<GetElementPtrInst>(I)) {
      auto Ptr = GEP->getPointerOperand();
      if (const Instruction *PtrInst = dyn_cast<Instruction>(Ptr)) {
        return PtrInst->getParent() == BB &&
               (find(AlreadySkipped, PtrInst) != AlreadySkipped.end());
      }
      return false;
    }
    if (auto BBBitCast = dyn_cast<BitCastInst>(I)) {
      return find(AlreadySkipped, BBBitCast->getOperand(0)) != AlreadySkipped.end();
    }
  } // find(Outputs, I) != Outputs.end()

  if (auto BBIntr = dyn_cast<IntrinsicInst>(I)) {
    auto ID = BBIntr->getIntrinsicID();
    if (ID == Intrinsic::ID::lifetime_start) { // llvm.lifetime.start(size, pointer)
      return find(AlreadySkipped, BBIntr->getArgOperand(1)) != AlreadySkipped.end();
    }
    if (ID == Intrinsic::lifetime_end) { // llvm.lifetime.end(size, pointer)
      Value *Op = BBIntr->getArgOperand(1);
      auto Bitcast = cast<Instruction>(Op);
      return Bitcast->getParent() != BB;
    }
    return false;
  }

  // Following code handles AllocaCast's that are not in the outputs in the special way
  // It looks at uses of this alloca. If any of these uses is going to be an output =>
  // alloca instruction should be factored out
  if (auto *BBAlloca = dyn_cast<AllocaInst>(I)) {
    for (auto BBAllocaUser : BBAlloca->users()) {
      if (!isa<Instruction>(BBAllocaUser))
        continue;

      const Instruction *I = cast<Instruction>(BBAllocaUser);
      if (I->getParent() == BB && (find(Outputs, BBAllocaUser) != Outputs.end())) {
        return true;
      }
    }
    return false;
  }


  return false;
}

/// Function should be used only after setting OutputIds
void BBsCommonInfo::setSkippedInsts(BasicBlock *BB) {
  const auto Outputs = convertInstIds(BB, OutputIds);
  SmallVector<Instruction *, 8> CurBBSkippedInsts;

  auto AddSkippedInst = [&](size_t InstNum, Instruction *Inst)
  {
    CurBBSkippedInsts.push_back(Inst);
    SkippedInsts.push_back(InstNum);

    auto It = find(OutputIds, InstNum);
    if (It == OutputIds.end())
      return;
    OutputIds.erase(It);
  };

  size_t i = 0;
  for (auto It = getBeginIt(BB), EIt = getEndIt(BB); It != EIt; ++It, ++i) {
    if (skipInst(&*It, CurBBSkippedInsts, Outputs)) {
      AddSkippedInst(i, &*It);
    }

  }
  SkippedInsts.resetIt();
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

// TODO: ? skip GetElementPtrInst with x86 arch
/// Calculates Weight of basic block \p BB for further decisions
static size_t getBBWeight(const BasicBlock *BB) {

  // approximate amount of instructions for the backend
  // assume general case, sizes of all instructions are equal

  size_t Points = 0;
  size_t StoresAndLoads = 0;
  for (auto It = getBeginIt(BB), EIt = getEndIt(BB); It != EIt; ++It) {
    // skip instructions, that can produce no code
    if (isa<BitCastInst>(It) ||  isa<ICmpInst>(It))
      continue;
    if (isa<LoadInst>(It) || isa<StoreInst>(It)) {
      ++StoresAndLoads;
      continue;
    }
    if (const IntrinsicInst *Intr = dyn_cast<IntrinsicInst>(It)) {
      using Intrinsic::ID;
      static const std::set<Intrinsic::ID> NoCodeProduction = {
          ID::lifetime_start,
          ID::lifetime_end,
          ID::donothing,
          ID::invariant_start,
          ID::invariant_end,
          ID::invariant_group_barrier,
          ID::var_annotation,
          ID::ptr_annotation,
          ID::annotation,
          ID::dbg_declare,
          ID::dbg_value,
          ID::assume,
          ID::instrprof_increment,
          ID::instrprof_increment_step,
          ID::instrprof_value_profile,
          ID::pcmarker};

      if (NoCodeProduction.count(Intr->getIntrinsicID()))
        continue;
    }

    ++Points;
  }
  return Points + StoresAndLoads/2 + StoresAndLoads %2;
}

/// Decides, whether it is profitable to factor out without creating
/// any functions
/// \param Weight - basic block weight, returned by getBBWeight(const BasicBlock
/// *)
/// \param InputArgs - amount of input basic block arguments
/// \param OutputArgs - amount of output basic block arguments
/// \return true if profitable
static bool isProfitableReplacement(const size_t Weight, const size_t InputArgs,
                                    const size_t OutputArgs) {
  const size_t AllocaOutputs = OutputArgs < 1 ? 0 : OutputArgs - 1;
  if (ForceMerge)
    return true;
  size_t CallCost = 1 + 2 * AllocaOutputs + InputArgs;
  return Weight > CallCost;
}

/// Decides, whether it is profitable to factor out BB, creating a function
/// \param Weight - basic block weight, returned by getBBWeight(const BasicBlock
/// *)
/// \param BBAmount - amount of merging basic blocks
/// \param InputArgs - amount of input basic block arguments
/// \param OutputArgs - amount of output basic block arguments
/// \return whether code size will reduce if factored with creating new function
static bool shouldCreateFunction(const size_t Weight, const size_t BBAmount,
                                 const size_t InputArgs,
                                 const size_t OutputArgs) {
  assert(BBAmount >= 2);
  assert(isProfitableReplacement(Weight, InputArgs, OutputArgs) &&
         "BBs with failed precheck of profitability shouldn't reach here");

  if (ForceMerge)
    return true;

  const size_t CallCost = 1 + 2 * OutputArgs + InputArgs;
  const size_t InstsProfitBy1Replacement = Weight - CallCost;
  // count storing values for each output value and assume the worst case when
  // we move values into convenient registers
  const size_t FunctionCreationCost = Weight + 2 * OutputArgs + InputArgs;
  // check if we don't lose created a function, and it's costs are lower,
  // than total gain of function replacement
  return BBAmount * InstsProfitBy1Replacement - FunctionCreationCost >= 0;
}

// TODO: set input attributes from created BB
/// \param Info - Information about model basic block
/// \return new function, that consists of Basic block \p Info BB
static Function *createFuncFromBB(const BBInfo &Info, const BBSkippedIds &SkippedInsts) {
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

  F->setCallingConv(CallingConv::Fast);
  // add some attributes
  F->addFnAttr(Attribute::Naked);
  F->addFnAttr(Attribute::MinSize);
  F->addFnAttr(Attribute::OptimizeForSize);
  // newly created function can't call itself
  F->addFnAttr(Attribute::NoRecurse);
  bool Throws = canThrow(BB);
  if (!Throws)
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

  size_t i = 0;
  SkippedInsts.checkBegin();
  for (auto I = getBeginIt(BB), IE = getEndIt(BB); I != IE; ++I, ++i) {
    if (SkippedInsts.contains(i)) {
      continue;
    }
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

  debugPrint(BB, "Function created");
  DEBUG(F->print(dbgs()));

  ++FunctionCounter;
  return F;
}

/// \param Info - Basic block, which is going to be replaced with function call
/// to \p F
/// \return true if \p BB was replaced with \p F, false otherwise
static bool replaceBBWithFunctionCall(const BBInfo &Info, Function *F,
                                      const BBSkippedIds &SkippedInsts) {
  // check if this BB was already replaced
  BasicBlock *BB = Info.BB;
  auto &Input = Info.Inputs;
  auto &Output = Info.Outputs;
  Value *Result = Info.ReturnVal;

  auto LastIt = getEndIt(BB);
  IRBuilder<> Builder(&*LastIt);
  // save value for future removing old part of basic block
  --LastIt;

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
  CallInst *TailCallInst = Builder.CreateCall(F, Args);
  TailCallInst->setTailCallKind(CallInst::TailCallKind::TCK_Tail);
  TailCallInst->setCallingConv(F->getCallingConv());
  if (Result) {
    Value *ResultReplace = GetValueForArgs(TailCallInst, Result->getType());
    Result->replaceAllUsesWith(ResultReplace);
  }

  // 4) Save and Replace all Output values
  auto AllocaIt = Args.begin() + Input.size();
  for (auto It = Output.begin(), EIt = Output.end(); It != EIt;
       ++It, ++AllocaIt) {
    Instruction *CurrentInst = *It;
    if (!isInstUsedOutsideParent(CurrentInst))
      continue;

    auto BBLoadInst = Builder.CreateLoad(*AllocaIt);
    auto BitCasted = GetValueForArgs(BBLoadInst, CurrentInst->getType());
    CurrentInst->replaceAllUsesWith(BitCasted);
  }

  // 5) perform cleanup from the end because in opposite llvm will
  // complain that value is still in use

  auto InsertBeforeInsts = convertInstIds(BB, SkippedInsts.get());
  SmallPtrSet<Instruction *, 8> SkipInstsSet(InsertBeforeInsts.begin(),
                                           InsertBeforeInsts.end());
  auto FirstInst = getBeginIt(BB);
  while (SkipInstsSet.count(&*FirstInst))
    ++FirstInst;

  while (true) {
    if (SkipInstsSet.count(&*LastIt)) {
      --LastIt;
      continue;
    }
    if (FirstInst == LastIt)
    {
      LastIt->eraseFromParent();
      break;
    }
    LastIt = LastIt->eraseFromParent();
    --LastIt;
  }


  ++MergeCounter;
  return true;
}

/// Select a function return value from array of operands
static size_t getFunctionRetValId(const ArrayRef<Instruction *> Outputs) {
  for (auto It = Outputs.rbegin(), EIt = Outputs.rend(); It != EIt; ++It) {
    Value *V = *It;
    if (V->getType()->isFirstClassType()) {
      Instruction * I = cast<Instruction>(V);
      if (!llvm::isa<AllocaInst>(I))
        return It - Outputs.rbegin();
    }
  }
  return Outputs.size();
}

/// \param F - possible function to be merged with args \p Inputs, \p Outputs
/// \returns true, if the Function F, that consists of 1 basic block can be used
/// as a callee of other basic blocks
static bool hasRightOrder(const BBInfo &Info) {
  const Function *F = Info.BB->getParent();
  const SmallVectorImpl<Value *> &Inputs = Info.Inputs;
  const SmallVectorImpl<Instruction *> &Outputs = Info.Outputs;

  assert(F->size() == 1);
  if (F->isVarArg())
    return false;
  assert(!isa<PHINode>(F->front().front()) &&
         "Functions with solo basic block can't contain any phi nodes");

  bool BBNotVoid = Info.ReturnVal != nullptr;
  bool NotVoid = F->getReturnType() != Type::getVoidTy(F->getContext());

  if (BBNotVoid != NotVoid) {
    return false;
  }

  if (Inputs.size() + Outputs.size() < F->arg_size()) {
    // don't use functions with unused arguments
    return false;
  }
  assert(Inputs.size() + Outputs.size() == F->arg_size());

  Value *ReturnVal = nullptr;
  if (NotVoid) {
    ReturnVal = cast<ReturnInst>(F->front().back()).getReturnValue();
    if (Info.ReturnVal != ReturnVal)
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
    if (static_cast<const Value *>(&*It++) != Output)
      return false;
  }

  return true;
}

/// \param CommonInfo used for setting \p Infos output and return values
/// \param Infos [in, out] set Outputs and return value, found in Model into \p Infos
static void prepareForBBMerging(const BBsCommonInfo &CommonInfo, SmallVectorImpl<BBInfo> &Infos) {

  for (BBInfo &Info : Infos) {
    Info.Outputs = convertInstIds(Info.BB, CommonInfo.OutputIds);
  }

  const BBInfo &Model = Infos.front();
  size_t ReturnIdF = getFunctionRetValId(Model.Outputs);
  if (ReturnIdF == Model.Outputs.size()) {
    // return type void ~ nullptr
    return;
  }

  for (BBInfo &Info : Infos) {
    Info.extractReturnValue(ReturnIdF);
  }
}

/// Tries to find appropriate function for factoring out
/// \param BBs basic blocks, which parents are observed
/// \return basic block, which function is suitable for merging
/// or size of \p BBs if function was not found
static size_t findAppropriateBBsId(const ArrayRef<BBInfo> BBs) {
  size_t i = 0;
  for (size_t ei = BBs.size(); i < ei; ++i) {
    const Function *F = BBs[i].BB->getParent();
    if (F->size() == 1) {
      // check the order of passed arguments
      if (hasRightOrder(BBs[i]))
        return i;
    }
  }
  return i;
}

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
  if (BBs.front()->isLandingPad()) {
    debugPrint(BBs.front(), "Block family is a landing pad. Skip it");
    return false;
  }

  // Fill common basic blocks info

  BBsCommonInfo CommonInfo;
  CommonInfo.Weight = getBBWeight(BBs.front());

  if (CommonInfo.Weight <= 2) {
    debugPrint(BBs.front(), "Block family is not worth merging");
    return false;
  }

  CommonInfo.OutputIds = getOutput(BBs.front());
  std::for_each(BBs.begin()+1, BBs.end(),
    [&CommonInfo](const BasicBlock *BB) { CommonInfo.mergeOutput(getOutput(BB)); });

  CommonInfo.setSkippedInsts(BBs.front());

  SmallVector<BBInfo, 8> BBInfos;
  BBInfos.emplace_back(BBs.front(), CommonInfo.SkippedInsts);

  if (!isProfitableReplacement(CommonInfo.Weight, BBInfos.front().Inputs.size(),
               CommonInfo.OutputIds.size())) {
    return false;
  }
  std::for_each(BBs.begin() + 1, BBs.end(),
    [&BBInfos, &CommonInfo](BasicBlock *BB) { BBInfos.emplace_back(BB, CommonInfo.SkippedInsts); });

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

  // Try to find suitable function for merging

  prepareForBBMerging(CommonInfo, BBInfos);

  size_t Id = findAppropriateBBsId(BBInfos);
  if (Id != BBInfos.size()) {
    Function *F = BBInfos[Id].BB->getParent();

    // remove BBInfos[Id] from replacing
    std::swap(BBInfos[Id], BBInfos.back());
    BBInfos.pop_back();

    for (auto &Info : BBInfos) {
      replaceBBWithFunctionCall(Info, F, CommonInfo.SkippedInsts);
    }

    DEBUG(dbgs() << BBInfos.size()
                 << " basic blocks were replaced with existed function "
                 << F->getName() << "\n");
    return true;
  }


  if (!shouldCreateFunction(CommonInfo.Weight, BBInfos.size(), BBInfos.front().Inputs.size(),
                            CommonInfo.OutputIds.size())) {
    debugPrint(BBs.front(), "Unprofitable replacement");
    return false;
  }


  Function *F = createFuncFromBB(BBInfos.front(), CommonInfo.SkippedInsts);

  for (auto &Info : BBInfos) {
    replaceBBWithFunctionCall(Info, F, CommonInfo.SkippedInsts);
  }

  DEBUG(dbgs() << BBInfos.size()
               << " basic blocks were replaced with just created function "
               << F->getName() << "\n");

  return true;
}