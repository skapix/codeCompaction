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
/// because these instructions are not part of our factored BB.
/// After finding identity BBs, pass creates new function (if necessary)
/// and replaces the factored BB with tail call to the appropriate function.
/// Pass works only with well-formed basic blocks.
/// Definition: Factored BB is the part of Basic Block, which can be replaced
/// with this pass. Factored BB consists of the whole BB without Phi nodes and
/// terminator instructions.
///
//===----------------------------------------------------------------------===//

#include "../external/merging.h"
#include "ForceMergePAC.h"
#include "TargetDependent/CommonPAC.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/TargetTransformInfo.h"
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

static cl::opt<bool>
    ForceMerge("bbfactor-force-merging", cl::Hidden, cl::init(false),
               cl::desc("Force folding basic blocks, when it is unprofitable"));

// TODO: clone free instructions for sweeping them from output
// TODO: ? place BB comparing in this file

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

  virtual void getAnalysisUsage(AnalysisUsage &Info) const override;

  virtual bool runOnModule(Module &M) override;

private:
  /// If profitable, creates function with body of BB and replaces BBs
  /// with a call to new function
  /// \param BBs - Vector of identity BBs
  /// \param DM - target-dependent subroutines
  /// \returns whether BBs were replaced with a function call
  bool replace(const SmallVectorImpl<BasicBlock *> &BBs,
               IProceduralAbstractionCost *DM);

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

void BBFactoring::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetTransformInfoWrapperPass>();
}

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
        HashedBBs.push_back(
            {BBComparator::basicBlockHash(BB, false, false), &BB});
    }
  }

  std::vector<SmallVector<BasicBlock *, 16>> IdenticalBlocksContainer;
  auto BBTree = std::map<BBNode, size_t, BBNodeCmp>(BBNodeCmp(&GlobalNumbers));

  // merge hashed values into map
  for (auto It = HashedBBs.begin(), Ite = HashedBBs.end(); It != Ite; ++It) {
    auto InsertedBBNode = BBTree.insert(std::make_pair(
        BBNode(It->second, It->first), IdenticalBlocksContainer.size()));
    if (InsertedBBNode.second) {
      IdenticalBlocksContainer.emplace_back(1, It->second);
    } else {
      IdenticalBlocksContainer[InsertedBBNode.first->second].push_back(
          It->second);
    }
  }

  bool Changed = false;

  const StringRef Arch =
      M.getTargetTriple().substr(0, M.getTargetTriple().find('-'));
  auto DM = ForceMerge ? make_unique<ForceMergePAC>()
                       : IProceduralAbstractionCost::Create(Arch);
  assert(DM.get() && "DM was not created properly");

  for (auto &IdenticalBlocks : IdenticalBlocksContainer) {
    if (IdenticalBlocks.size() >= 2) {
      Changed |= replace(IdenticalBlocks, DM.get());
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

/// Class is used to find values in sorted array.
/// Values, that should be found must be ordered.
/// Class is useful when we need to traverse all
/// BB insts [getBeginIt, getEndIt) and find particular instructions
class SmartSortedSet {
public:
  SmartSortedSet() { resetIt(); }

  void checkBegin() const {
    assert(Cur == InstIds.begin() &&
           "Cur should point to the beginning of the array");
  }
  void push_back(const size_t InstId) {
    assert((InstIds.empty() || InstIds.back() < InstId) &&
           "InstIds should be sorted");
    InstIds.push_back(InstId);
  }

  void resetIt() const { Cur = InstIds.begin(); }
  const BBInstIds &get() const { return InstIds; }

  /// \param InstId number of instruction
  /// \return true if \p InstId is number of instruction
  /// should be skipped from factoring out
  bool contains(size_t InstId) const;

private:
  /// Ascendingly sorted vector of instruction numbers
  BBInstIds InstIds;
  mutable BBInstIds::const_iterator Cur;
};

bool SmartSortedSet::contains(size_t InstId) const {
  if (Cur == InstIds.end()) // no skipped elements
    return false;
  if (*Cur != InstId)
    return false;
  Cur = (Cur == std::prev(InstIds.end())) ? InstIds.begin() : Cur + 1;
  return true;
}

/// \return Values, that were created outside of the factored \p BB
static SmallVector<Value *, 8> getInput(BasicBlock *BB,
                                        const SmartSortedSet &SkipIds) {
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
static SmallVector<Instruction *, 8>
convertInstIds(BasicBlock *BB, const BBInstIds &NumsInstr) {
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

/// Class, that keeps all important information about each basic block,
/// that is going to be factor out. Evaluates values as lazy as possible
/// It is created for every BB individually
class BBInfo {
public:
  BBInfo(BasicBlock *BB, const SmartSortedSet &SkippedIds,
         const BBInstIds &OutputIds)
      : BB(BB), SkippedIds(SkippedIds), OutputIds(OutputIds) {}
  /// Because of const arguments, we have to override the assignment operator.
  /// All these constant references are equal for every BB of set of equal BBs
  BBInfo &operator=(const BBInfo &Other);

  BasicBlock *getBB() const { return BB; }

  const SmallVector<Value *, 8> &getInputs() const;
  const SmallVector<Instruction *, 8> &getOutputs() const;

  /// permutates Inputs according to Permut
  /// i.e:
  /// Inputs: [0, 2, 3]
  /// Permut: [2, 0, 1]
  /// In result Input will be equal [3, 0, 2]
  void permutateInputs(const SmallVectorImpl<size_t> &Permut);
  Value *getReturnValue() const { return ReturnValue; }
  void extractReturnValue(const size_t ResultId);

private:
  BasicBlock *BB;

  const SmartSortedSet &SkippedIds;
  const BBInstIds &OutputIds;
  mutable Optional<SmallVector<Value *, 8>> Inputs;
  mutable SmallVector<Instruction *, 8> Outputs;
  mutable Value *ReturnValue = nullptr;
};

} // end anonymous namespace

const SmallVector<Value *, 8> &BBInfo::getInputs() const {
  if (!Inputs)
    Inputs = getInput(BB, SkippedIds);
  return *Inputs;
}

const SmallVector<Instruction *, 8> &BBInfo::getOutputs() const {
  if (!ReturnValue && !OutputIds.empty() && Outputs.empty())
    Outputs = convertInstIds(BB, OutputIds);
  return Outputs;
}

/// \return vector of \p Inputs, permutated with \p Permuts
static SmallVector<Value *, 8>
applyPermutation(const SmallVectorImpl<Value *> &Inputs,
                 const SmallVectorImpl<size_t> &Permuts) {
  SmallVector<Value *, 8> Result;
  Result.reserve(Inputs.size());
  for (size_t Perm : Permuts) {
    Result.push_back(Inputs[Perm]);
  }
  return Result;
}

void BBInfo::permutateInputs(const SmallVectorImpl<size_t> &Permut) {
  Inputs = applyPermutation(getInputs(), Permut);
}
void BBInfo::extractReturnValue(const size_t ResultId) {

  assert(ReturnValue == nullptr && "Return value should be set once");
  if (getOutputs().size() == ResultId) {
    return;
  }
  assert(ResultId < Outputs.size() && "Should be index of Outputs");

  ReturnValue = Outputs[ResultId];
  Outputs[ResultId] = Outputs.back();
  Outputs.pop_back();
}

BBInfo &BBInfo::operator=(const BBInfo &Other) {
  if (this != &Other) {
    BB = Other.BB;
    Inputs = Other.Inputs;
    Outputs = Other.Outputs;
    ReturnValue = Other.ReturnValue;
    // no need in copying references since they are the same for each BBInfo
  }
  return *this;
}

namespace {

/// Common information about Basic Blocks
/// Creates for every equal set of BBs
class BBsCommonInfo {
public:
  BBsCommonInfo(ArrayRef<BasicBlock *> BBs);

  const BBInstIds &getOutputIds() const { return OutputIds; }
  const SmartSortedSet &getSkippedInsts() const { return SkippedInsts; }

private:
  /// merges exsistent output OutputIds with \p Ids
  /// e.g.
  /// OutputIds: [1, 3, 5]
  /// \p Ids: [1, 2, 5 ]
  /// in result OutputIds = [1, 2, 3, 5]
  ///
  void mergeOutput(const BBInstIds &Ids);

  /// Sets SkippedInsts, using instructions from \p BB
  void setSkippedInsts(BasicBlock *BB);

private:
  BBInstIds OutputIds;
  SmartSortedSet SkippedInsts;
};

} // end anonymous namespace

BBsCommonInfo::BBsCommonInfo(ArrayRef<BasicBlock *> BBs)
    : OutputIds(getOutput(BBs.front())) {
  std::for_each(BBs.begin() + 1, BBs.end(), [this](const BasicBlock *BB) {
    this->mergeOutput(getOutput(BB));
  });

  setSkippedInsts(BBs.front());
}

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
    } else if (Other < This) {
      It = OutputIds.insert(It, Other); // It points to *IdsIt
      EIt = OutputIds.end();
      ++IdsIt;
    }
    // else (Other > This) continue;
  }
  if (IdsIt != IdsEIt)
    OutputIds.insert(OutputIds.end(), IdsIt, IdsEIt);
}

/// \param I instruction, that is decided whether to be skipped or not
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
      return find(AlreadySkipped, BBBitCast->getOperand(0)) !=
             AlreadySkipped.end();
    }
  } // find(Outputs, I) != Outputs.end()

  if (auto BBIntr = dyn_cast<IntrinsicInst>(I)) {
    auto ID = BBIntr->getIntrinsicID();
    if (ID ==
        Intrinsic::ID::lifetime_start) { // llvm.lifetime.start(size, pointer)
      return find(AlreadySkipped, BBIntr->getArgOperand(1)) !=
             AlreadySkipped.end();
    }
    if (ID == Intrinsic::lifetime_end) { // llvm.lifetime.end(size, pointer)
      Value *Op = BBIntr->getArgOperand(1);
      auto Bitcast = cast<Instruction>(Op);
      return Bitcast->getParent() != BB;
    }
    return false;
  }

  // Following code handles AllocaCast's that are not in the outputs in the
  // special way: It looks at uses of this alloca. If any of these uses is going
  // to be an output => alloca instruction should be factored out
  if (auto *BBAlloca = dyn_cast<AllocaInst>(I)) {
    for (auto BBAllocaUser : BBAlloca->users()) {
      const Instruction *BBAllocaInst = dyn_cast<Instruction>(BBAllocaUser);
      if (!BBAllocaInst)
        continue;

      if (BBAllocaInst->getParent() == BB &&
          (find(Outputs, BBAllocaUser) != Outputs.end())) {
        return true;
      }
    }
    return false;
  }

  return false;
}

void BBsCommonInfo::setSkippedInsts(BasicBlock *BB) {
  const auto Outputs = convertInstIds(BB, OutputIds);
  SmallVector<Instruction *, 8> CurBBSkippedInsts;

  auto AddSkippedInst = [&](size_t InstNum, Instruction *Inst) {
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

// TODO: ? set input attributes from created BB
/// \param Info - Information about model basic block
/// \return new function, that consists of Basic block \p Info BB
static Function *createFuncFromBB(const SmallVectorImpl<Instruction *> &Insts,
                                  const BBInfo &Info) {
  assert(!Insts.empty() && "Should not reach here");
  assert(Insts.front()->getParent() == Info.getBB() &&
         "Basic blocks not match");

  BasicBlock *BB = Info.getBB();
  auto &Input = Info.getInputs();
  auto &Output = Info.getOutputs();
  const Value *ReturnValue = Info.getReturnValue();

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

  for (auto I : Insts) {
    Instruction *NewI = Builder.Insert(I->clone());
    InputToArgs.insert(std::make_pair(I, NewI));

    for (auto &Op : NewI->operands()) {
      auto it = InputToArgs.find(Op.get());
      if (it != InputToArgs.end()) {
        Op.set(it->second);
      }
    }

    auto Found = OutputToArgs.find(I);
    if (Found != OutputToArgs.end()) {
      Builder.CreateStore(NewI, Found->second);
    } else if (&*I == ReturnValue) {
      assert(ReturnValueF == nullptr &&
             "Function return value is already assigned");
      ReturnValueF = NewI;
    }
  }

  // create return instruction
  assert((ReturnValue == nullptr) == (ReturnValueF == nullptr) &&
         "Return value in basic block should be found, but it wasn't");
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
                                      const SmartSortedSet &SkippedInsts) {
  // check if this BB was already replaced
  BasicBlock *BB = Info.getBB();
  auto &Input = Info.getInputs();
  auto &Output = Info.getOutputs();
  Value *Result = Info.getReturnValue();

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
    if (FirstInst == LastIt) {
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
    Instruction *I = *It;
    assert(!llvm::isa<AllocaInst>(I) && "Alloca Can't be return value");
    if (I->getType()->isFirstClassType()) {
      return Outputs.rend() - It - 1;
    }
  }
  return Outputs.size();
}

/// \param Info BB, which parent F can be used as a callee for other BBs
/// \param Permut - result of permutation of input to function
/// arguments. \p Permut sets only if isMergable returns true \return true, if
/// the Function F, that consists of 1 basic block can be used as a callee for
/// other basic blocks
static bool isMergeable(const BBInfo &Info, SmallVectorImpl<size_t> &Permut) {
  const Function *F = Info.getBB()->getParent();
  const SmallVectorImpl<Value *> &Inputs = Info.getInputs();

  assert(F->size() == 1);
  if (F->isVarArg())
    return false;
  assert(!isa<PHINode>(F->front().front()) &&
         "Functions with solo basic block can't contain any phi nodes");

  assert(Info.getReturnValue() == nullptr && "Return value should not be set");

  if (Inputs.size() != F->arg_size()) {
    // don't use functions with unused arguments
    return false;
  }

  Permut.clear();
  Permut.reserve(Inputs.size());
  for (auto &Arg : F->args()) {
    size_t Id = find(Inputs, &Arg) - Inputs.begin();
    assert(Id != Inputs.size() &&
           "Function argument not found. Check correctness of getting inputs");
    Permut.push_back(Id);
  }

  return true;
}

/// Tries to find appropriate function for factoring out
/// \param BBs basic blocks, which parents are observed
/// \param Permuts Input Permutation if appropriate function was found
/// \return basic block, which function is suitable for merging
/// or size of \p BBs if function was not found
static size_t findAppropriateBBsId(const ArrayRef<BBInfo> BBs,
                                   SmallVectorImpl<size_t> &Permuts) {
  size_t i = 0;
  for (size_t ei = BBs.size(); i < ei; ++i) {
    const Function *F = BBs[i].getBB()->getParent();
    if (F->size() == 1) {
      if (isMergeable(BBs[i], Permuts))
        break;
    }
  }
  return i;
}

bool beforeReturnBaseBlock(const BasicBlock *BB, const Value *OutputVal) {
  auto CheckReturn = [OutputVal](const ReturnInst *RI) {
    return OutputVal == RI->getReturnValue() ||
           RI->getReturnValue()->getType()->getTypeID() == Type::VoidTyID;
  };
  // codegen generates better code,
  // when the call is in tail position (ret immediately follows call
  // and ret uses value of call or is void).
  if (auto ImmediateRI = dyn_cast<ReturnInst>(&BB->back()))
    return CheckReturn(ImmediateRI);

  auto Br = dyn_cast<BranchInst>(&BB->back());
  if (!Br || !Br->isUnconditional())
    return false;
  auto NextBB = Br->getSuccessor(0);
  auto Ret = dyn_cast<ReturnInst>(&NextBB->front());
  return Ret && CheckReturn(Ret);
}

/// \return vector of instructions from \p BB
/// with skipped \p Skipped instructions
SmallVector<Instruction *, 16>
extractActualInsts(BasicBlock *BB, const SmartSortedSet &Skipped) {
  SmallVector<Instruction *, 16> Result;
  auto It = getBeginIt(BB);
  auto EIt = getEndIt(BB);
  long BBSize = std::distance(It, EIt);
  assert(BBSize > 0 && "Should not reach here");
  assert(static_cast<size_t>(BBSize) >= Skipped.get().size() &&
         "Bad Basic block should not reach here");
  Result.reserve(static_cast<size_t>(BBSize) - Skipped.get().size());

  Skipped.checkBegin();
  for (size_t i = 0; It != EIt; ++It, ++i) {
    if (Skipped.contains(i))
      continue;
    Result.push_back(&*It);
  }
  return Result;
};

/// Common steps of replacing equal basic blocks
/// 1) Separate basic blocks by output values into sets of basic blocks
/// and try to merge each of these sets with existing basic block
/// 2) Combine all outputs and watch if there is an appripriate
/// function (with 1 this block), which can be used as a callee
/// 3) Create function and merge all the rest basic blocks,
/// if it is profitable to replace them with newly-created function
/// \param BBs array of equal basic blocks
/// \return true if any BB was changed
bool BBFactoring::replace(const SmallVectorImpl<BasicBlock *> &BBs,
                          IProceduralAbstractionCost *DM) {
  assert(BBs.size() >= 2 && "No sence in merging");
  if (BBs.front()->size() <= 3)
    return false;

  if (BBs.front()->isLandingPad()) {
    debugPrint(BBs.front(), "Block family is a landing pad. Skip it");
    return false;
  }

  long BBsSize = std::distance(getBeginIt(BBs.front()), getEndIt(BBs.front()));
  if (BBsSize <= 2) {
    debugPrint(BBs.front(), "Block family is too small to bother merging");
    return false;
  }

  BBsCommonInfo CommonInfo(BBs);
  auto ExtractedBlock =
      extractActualInsts(BBs.front(), CommonInfo.getSkippedInsts());
  if (ExtractedBlock.size() <= 2) {
    debugPrint(BBs.front(), "Block family is unprofitable to be factored out");
    return false;
  }

  auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(
      *BBs.front()->getParent());

  DM->init(TTI, ExtractedBlock);

  if (DM->isTiny()) {
    debugPrint(BBs.front(), "Block family is not worth merging");
    return false;
  }

  SmallVector<BBInfo, 8> BBInfos;
  transform(BBs, std::back_inserter(BBInfos),
            [&BBInfos, &CommonInfo](BasicBlock *BB) {
              return BBInfo(BB, CommonInfo.getSkippedInsts(),
                            CommonInfo.getOutputIds());
            });

  // check for tail call
  bool IsReallyTail =
      CommonInfo.getOutputIds().size() <= 1 &&
      all_of(BBInfos, [](const BBInfo &Info) {
        const Value *OutputValue =
            Info.getOutputs().size() == 1 ? Info.getOutputs().front() : nullptr;
        return beforeReturnBaseBlock(Info.getBB(), OutputValue);
      });
  DM->setTail(IsReallyTail);

  if (!DM->replaceWithCall(BBInfos.front().getInputs().size(),
                           CommonInfo.getOutputIds().size())) {
    debugPrint(BBs.front(), "BB factoring out won't decrease the code size");
    return false;
  }

  // Try to find suitable for merging function
  // If basic block has more, than 1 output, function can not be found
  // because llvm doesn't support multiple return values
  if (CommonInfo.getOutputIds().size() <= 1) {
    SmallVector<size_t, 8> Permuts;
    size_t Id = findAppropriateBBsId(BBInfos, Permuts);
    if (Id != BBInfos.size()) {
      Function *F = BBInfos[Id].getBB()->getParent();

      // remove BBInfos[Id] from replacing
      BBInfos[Id] = BBInfos.back();
      BBInfos.pop_back();

      for (auto &Info : BBInfos) {
        Info.permutateInputs(Permuts);
        Info.extractReturnValue(0);
        replaceBBWithFunctionCall(Info, F, CommonInfo.getSkippedInsts());
      }

      DEBUG(dbgs() << BBInfos.size()
                   << " basic blocks were replaced with existed function "
                   << F->getName() << "\n");
      return true;
    }
  }

  if (!DM->replaceWithCall(BBInfos.size(), BBInfos.front().getInputs().size(),
                           CommonInfo.getOutputIds().size())) {
    debugPrint(BBs.front(), "Unprofitable to factor out, creating a function");
    return false;
  }

  auto &Model = BBInfos.front();
  size_t ReturnIdF = getFunctionRetValId(Model.getOutputs());
  Model.extractReturnValue(ReturnIdF);
  Function *F = createFuncFromBB(ExtractedBlock, Model);

  for (auto &Info : BBInfos) {
    // Model return value is already set
    if (&Info != &Model)
      Info.extractReturnValue(ReturnIdF);
    replaceBBWithFunctionCall(Info, F, CommonInfo.getSkippedInsts());
  }

  DEBUG(dbgs() << BBInfos.size()
               << " basic blocks were replaced with just created function "
               << F->getName() << "\n");

  return true;
}
