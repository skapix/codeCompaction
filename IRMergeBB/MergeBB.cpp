//===-- MergeBB.cpp - Merge identical basic blocks -------*- C++ -*-===//
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
/// because these instructions are not part of our merged BB.
/// After finding identity BBs, pass creates new function (if necessary)
/// and replaces the merged BB with tail call to the appropriate function.
/// Pass works only with well-formed basic blocks.
/// Definition: Merged BB is the part of Basic Block, which can be replaced
/// with this pass. Merged BB consists of the whole BB without Phi nodes and
/// terminator instructions.
///
//===----------------------------------------------------------------------===//

#include "CompareBB.h"
#include "FunctionCompiler.h"
#include "Utilities.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Transforms/Utils/Cloning.h"

// TODO: add partial replacing (several replaced, others not)
// TODO: create cache for sized functions
// TODO: solve issues with function allignment

#define DEBUG_TYPE "mergebb"

STATISTIC(MergeCounter, "Number of merged basic blocks");
STATISTIC(FunctionCounter, "Amount of created functions");

using namespace llvm;
using namespace llvm::utilities;

static cl::opt<bool>
    ForceMerge("mergebb-force", cl::Hidden, cl::init(false),
               cl::desc("Force folding basic blocks, when it is unprofitable"));

static cl::opt<std::string> MergeSpecialFunction(
    "mergebb-function", cl::Hidden,
    cl::desc("Merge group of identical BBs,"
             "if at least one BB from this set has specified parent"));

static cl::opt<std::string>
    MergeSpecialBB("mergebb-bb", cl::Hidden,
                   cl::desc("Merge group of identical BBs,"
                            "if at least one BB name equals to specified"));

namespace {

/// MergeBB finds basic blocks which will generate identical machine code
/// Once identified, MergeBB will fold them by replacing these basic blocks
/// with a call to a function.
class MergeBB : public ModulePass {
public:
  static char ID;

  MergeBB() : ModulePass(ID) {}

  virtual void getAnalysisUsage(AnalysisUsage &Info) const override;

  virtual bool runOnModule(Module &M) override;

private:
  /// If profitable, creates function with body of BB and replaces BBs
  /// with a call to new function
  /// \param BBs - Vector of identity BBs
  /// \returns whether BBs were replaced with a function call
  bool replace(const SmallVectorImpl<BasicBlock *> &BBs);

  std::unique_ptr<FunctionNameCreator> FNamer;
  GlobalNumberState GlobalNumbers;
  std::map<std::string, size_t> CostHash;
  std::unique_ptr<FunctionCompiler> Cost;
};

} // end anonymous namespace

char MergeBB::ID = 0;
static RegisterPass<MergeBB> X("mergebb", "Merge basic blocks", false, false);

void MergeBB::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetTransformInfoWrapperPass>();
}

static bool skipFromMerging(const BasicBlock *BB);

bool MergeBB::runOnModule(Module &M) {
  if (skipModule(M))
    return false;

  DEBUG(dbgs() << "Module name: ");
  DEBUG(dbgs().write_escaped(M.getName()) << '\n');

  Cost = std::make_unique<FunctionCompiler>(M);
  FNamer = std::make_unique<FunctionNameCreator>(M);
  if (!Cost->isInitialized())
    return false;

  std::vector<BBNode> HashedBBs;

  // calculate hashes for all basic blocks in every function
  for (auto &F : M.functions()) {
    if (!F.isDeclaration() && !F.hasAvailableExternallyLinkage()) {
      for (auto &BB : F.getBasicBlockList())
        if (!skipFromMerging(&BB))
          HashedBBs.emplace_back(&BB);
    }
  }

  using VectorOfBBs = SmallVector<BasicBlock *, 16>;
  auto BBTree =
      std::map<BBNode, VectorOfBBs, BBNodeCmp>(BBNodeCmp(&GlobalNumbers));

  // merge hashed values into map
  for (const auto &Node : HashedBBs) {
    auto InsertedBBNode =
        BBTree.insert(std::make_pair(Node, VectorOfBBs({Node.getBB()})));
    if (!InsertedBBNode.second)
      InsertedBBNode.first->second.push_back(Node.getBB());
  }

  auto RemoveIf = [&BBTree](const std::function<bool(const BasicBlock *)> &F) {
    for (auto It = BBTree.begin(), EIt = BBTree.end(); It != EIt;) {
      bool Exists = any_of(It->second, F);
      if (Exists)
        ++It;
      else {
        It = BBTree.erase(It);
        EIt = BBTree.end();
      }
    }
  };

  if (!MergeSpecialFunction.empty()) {
    RemoveIf([](const BasicBlock *BB) {
      return BB->getParent()->getName() == MergeSpecialFunction;
    });
  }

  if (!MergeSpecialBB.empty()) {
    // BB's names are not saved during loading IR module.
    // Names are concatenated with some number and hence, only
    // the first symbols of names should be matched.
    RemoveIf([](const BasicBlock *BB) {
      return BB->getName().take_front(MergeSpecialBB.size()) == MergeSpecialBB;
    });
  }

  bool Changed = false;

  for (auto &IdenticalBlocks : BBTree) {
    if (IdenticalBlocks.second.size() >= 2) {
      Changed |= replace(IdenticalBlocks.second);
    }
  }

  return Changed;
}

static void debugPrint(const BasicBlock *BB, const StringRef Str = "",
                       bool NewLine = true) {
  DEBUG(dbgs() << Str << (Str != "" ? ". " : "") << "Block: " << BB->getName()
               << ". Function: " << BB->getParent()->getName()
               << (NewLine ? '\n' : ' '));
}

static bool skipFromMerging(const BasicBlock *BB) {
  if (BB->size() <= 3)
    return true;

  if (BB->isLandingPad()) {
    debugPrint(BB, "Block family is a landing pad. Skip it");
    return true;
  }

  long BBsSize = std::distance(getBeginIt(BB), getEndIt(BB));
  if (BBsSize <= 2) {
    debugPrint(BB, "Block family is too small to bother merging");
    return true;
  }

  // we don't create function with variadic arguments (VA) because
  // we use fastcc calling convention and we don't create VA functions
  static Intrinsic::ID BadIntrinsics[] = {
      Intrinsic::ID::vastart, Intrinsic::ID::vaend, Intrinsic::ID::vacopy};

  for (auto &It : *BB) {
    auto II = dyn_cast<IntrinsicInst>(&It);
    if (II == nullptr)
      continue;
    auto ID = II->getIntrinsicID();
    if (any_of(BadIntrinsics, [ID](Intrinsic::ID Bad) { return ID == Bad; }))
      return true;
  }

  return false;
}

/// The way of representing output and skipped instructions of basic blocks
using BBInstIds = SmallVector<size_t, 8>;
using BBInstIdsImpl = SmallVectorImpl<size_t>;

using SmartSortedSetInstIds = SmartSortedSet<size_t>;

/// This function is like isInstUsedOutsideOfBB, but does
/// consider Phi nodes and TerminatorInsts as a special case, because
/// they are not part of the created function.
/// \return if the value \p V is used outside it's parent function
static bool isInstUsedOutsideParent(const Instruction *I) {
  const BasicBlock *BB = I->getParent();
  for (const Use &U : I->uses()) {
    const Instruction *UI = cast<Instruction>(U.getUser());
    if (UI->getParent() != BB || isa<TerminatorInst>(UI) || isa<PHINode>(UI))
      return true;
  }
  return false;
}

static bool isValUsedByInsts(const Value *V,
                             const SmallVectorImpl<Instruction *> &Insts) {
  for (auto I : Insts) {
    for (auto &Op : I->operands()) {
      if (V == Op.get())
        return true;
    }
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
convertInstIds(BasicBlock *BB, const BBInstIdsImpl &NumsInstr) {
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

////////// Common Basic Block Info //////////

namespace {

/// Common information about Basic Blocks
/// Creates for every equal set of BBs
class BBsCommonInfo {
public:
  BBsCommonInfo(ArrayRef<BasicBlock *> BBs, const TargetTransformInfo &TTI);

  const BBInstIdsImpl &getOutputIds() const { return OutputIds; }
  const InstructionLocation &getSpecialInsts() const { return SpecialInsts; }

  size_t getReturnValueId() const { return ReturnValueOutputId; }

private:
  /// merges exsistent output OutputIds with \p Ids
  /// e.g.
  /// OutputIds: [1, 3, 5]
  /// \p Ids: [1, 2, 5 ]
  /// in result OutputIds = [1, 2, 3, 5]
  void mergeOutput(const BBInstIds &Ids);

  /// Function sets values:
  /// 1) skippedInsts - instructions, that are skipped from factoring out and
  /// inserted before call
  /// 2) clonedInsts - instructions, that are factored out and cloned after
  /// the function call in order to reduce amount of
  /// input/output function arguments
  /// Function should be called after input/output initialization
  void setSpecialInsts(const TargetTransformInfo &TTI, BasicBlock *BB);

  /// Select a function return value from array of operands
  void setFunctionRetValId(const ArrayRef<Instruction *> Outputs);

private:
  BBInstIds OutputIds;
  // ReturnValueOutputId is an Id of OutputIds if return value exists,
  // otherwise it is greater or equal than OutputIds' size
  size_t ReturnValueOutputId;

  InstructionLocation SpecialInsts;
};

} // end anonymous namespace

BBsCommonInfo::BBsCommonInfo(ArrayRef<BasicBlock *> BBs,
                             const TargetTransformInfo &TTI)
    : OutputIds(getOutput(BBs.front())) {
  std::for_each(BBs.begin() + 1, BBs.end(), [this](const BasicBlock *BB) {
    this->mergeOutput(getOutput(BB));
  });
  setSpecialInsts(TTI, BBs.front());

  setFunctionRetValId(convertInstIds(BBs.front(), OutputIds));
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

// Auxiliary routine, that decides, where to copy instruction
// by looking at instruction operands.
/// \return  None if some of operands is not outside the
// function => instruction will not be factored out.
static Optional<InstructionLocation::Type>
InstOutPos(const Instruction *I,
           const SmallPtrSetImpl<const Value *> &UsedBefore,
           const SmallPtrSetImpl<const Value *> &UsedAfter,
           const SmallVectorImpl<Instruction *> &Outputs,
           const InstructionLocation::Type InitialPos =
               InstructionLocation::Type::CopyBefore) {
  assert((InitialPos == InstructionLocation::Type::CopyBefore ||
          InitialPos == InstructionLocation::Type::CopyAfter) &&
         "Check Param");
  const BasicBlock *BB = I->getParent();
  InstructionLocation::Type Result = InitialPos;
  for (auto Op : I->operand_values()) {
    auto IOp = dyn_cast<Instruction>(Op);
    if (!IOp)
      continue;
    if (IOp->getParent() != BB || isa<PHINode>(IOp) || isa<TerminatorInst>(IOp))
      continue;
    if (UsedBefore.find(IOp) != UsedBefore.end())
      continue;

    if (UsedAfter.find(IOp) != UsedAfter.end() ||
        find(Outputs, IOp) != Outputs.end()) {
      Result = InstructionLocation::Type::CopyAfter;
      continue;
    }
    // if any of parameters is used only in basic block =>
    // we can't factor instruction out
    return None;
  }
  return Result;
}

/// Main purpose of this function is to reduce amount of Output values
/// \return None if \p I stays in factored out BB
static Optional<InstructionLocation::Type>
setTypeIfOutput(const Instruction *I,
                const SmallPtrSetImpl<const Value *> &ValuesBefore,
                const SmallPtrSetImpl<const Value *> &ValuesAfter,
                const SmallVectorImpl<Instruction *> &Outputs,
                const TargetTransformInfo &TTI) {
  switch (I->getOpcode()) {
  case Instruction::Alloca: {
    // TODO: what if alloca size is not constant and calculated inside the BB?
    assert(InstOutPos(I, ValuesBefore, ValuesAfter, Outputs) != None &&
           "Alloca operand is created in BB, but alloca must stay in original "
           "function");
    return InstructionLocation::Type::MoveBefore;
  }
  case Instruction::BitCast:
    // bitcast is an output => decide how to take it out from output values:
    // copy or move
    return InstOutPos(I, ValuesBefore, ValuesAfter, Outputs);
  case Instruction::GetElementPtr:
    return InstOutPos(I, ValuesBefore, ValuesAfter, Outputs,
                      InstructionLocation::Type::CopyAfter);
  default: {
    if (TTI.getUserCost(I) == TargetTransformInfo::TCC_Free)
      return InstOutPos(I, ValuesBefore, ValuesAfter, Outputs);
    return None;
  }
  }
}

/// The same, as setTypeIfOutput, but handles all instructions and is used,
/// if setTypeIfOutput can't handle \p I
static Optional<InstructionLocation::Type>
setTypeCommonCase(const Instruction *I,
                  const SmallPtrSetImpl<const Value *> &ValuesBefore,
                  const SmallPtrSetImpl<const Value *> &ValuesAfter,
                  const SmallVectorImpl<Instruction *> &Outputs,
                  const TargetTransformInfo &TTI) {
  // try handle intrinsic
  auto Intr = dyn_cast<IntrinsicInst>(I);
  if (Intr) {
    switch (Intr->getIntrinsicID()) {
    case Intrinsic::ID::lifetime_start:
    case Intrinsic::ID::lifetime_end:
      return InstOutPos(I, ValuesBefore, ValuesAfter, Outputs);
    default:
      return None;
    }
  }

  switch (I->getOpcode()) {
  case Instruction::Alloca: {
    // Following code handles AllocaCast's that are not in the outputs in the
    // special way: It looks at uses of this alloca. If any of these uses is
    // going to be an output => alloca instruction should be factored out
    const AllocaInst *AI = cast<AllocaInst>(I);
    for (auto AU : AI->users()) {
      const Instruction *AUI = dyn_cast<Instruction>(AU);
      assert(AUI && "Can't be used in value; Investigate");
      assert(AUI->getParent() == I->getParent() &&
             "Should be set as skipped passed earlier");
      if (find(Outputs, AUI) != Outputs.end()) {
        assert(InstOutPos(I, ValuesBefore, ValuesAfter, Outputs) != None &&
               "Alloca operand is created in BB, but alloca must stay in "
               "original function");
        return InstructionLocation::Type::MoveBefore;
      }
    }
    return None;
  }
  default:
    return None;
  }
}

/// All special-handled instructions are connected with reducing
/// input-output values. Exceptions are lifetime insts
void BBsCommonInfo::setSpecialInsts(const TargetTransformInfo &TTI,
                                    BasicBlock *BB) {
  const SmartSortedSet<Instruction *> OutputsSet(convertInstIds(BB, OutputIds));
  SmallPtrSet<const Value *, 8> BBSpecialBefore;
  SmallPtrSet<const Value *, 8> BBSpecialAfter;
  //  SmallPtrSet<const Value *, 8> BBSkippedInstsOut;

  auto RemoveOutput = [&](size_t InstNum, Instruction *Inst) {
    auto It = find(OutputIds, InstNum);
    assert(It != OutputIds.end());
    OutputIds.erase(It);
  };

  OutputsSet.checkBegin();
  size_t i = 0;
  // Every cycle should insert a value into SpecialInsts
  for (auto It = getBeginIt(BB), EIt = getEndIt(BB); It != EIt; ++It, ++i) {
    Instruction *I = &*It;
    Optional<InstructionLocation::Type> ResultInstType;
    if (OutputsSet.contains(I)) {
      ResultInstType = setTypeIfOutput(I, BBSpecialBefore, BBSpecialAfter,
                                       OutputsSet.get(), TTI);
    }

    if (ResultInstType)
      RemoveOutput(i, I);
    else
      ResultInstType = setTypeCommonCase(I, BBSpecialBefore, BBSpecialAfter,
                                         OutputsSet.get(), TTI);

    SpecialInsts.push_back(ResultInstType ? ResultInstType.getValue()
                                          : InstructionLocation::Type::Usual);

    if (SpecialInsts.isUsedBeforeFunction(i))
      BBSpecialBefore.insert(I);
    else if (SpecialInsts.isUsedBeforeFunction(i))
      BBSpecialAfter.insert(I);
  } // for

  // remove reduntant Instructions from function, i.e
  // if it is possible to move away instruction from function, move it.
  DenseSet<Value *> UsedValues;

  auto RIt = getEndIt(BB), REIt = getBeginIt(BB);
  // insert return value from TerminatorInst
  for (auto Op : RIt->operand_values()) {
    UsedValues.insert(Op);
  }

  while (RIt != REIt) {
    --RIt;
    --i;

    if (!SpecialInsts.isUsedInsideFunction(i))
      continue;

    if (UsedValues.find(&*RIt) == UsedValues.end()) {
      if (SpecialInsts[i] == InstructionLocation::Type::CopyBefore) {
        SpecialInsts[i] = InstructionLocation::Type::MoveBefore;
        continue;
      }
      if (SpecialInsts[i] == InstructionLocation::Type::CopyAfter) {
        SpecialInsts[i] = InstructionLocation::Type::MoveAfter;
        continue;
      }
    }

    for (auto Op : RIt->operand_values()) {
      UsedValues.insert(Op);
    }
  }
}

/// Select a function return value from array of operands
void BBsCommonInfo::setFunctionRetValId(const ArrayRef<Instruction *> Outputs) {
  ReturnValueOutputId = Outputs.empty() ? 0 : Outputs.size() - 1;

  DEBUG(
  for (auto It = Outputs.begin(), EIt = Outputs.end(); It != EIt; ++It) {
    assert(!isa<AllocaInst>(*It) && "Alloca Can't be return value");
    assert((*It)->getType()->isFirstClassType() &&
             "Output instruction can be only the first class");
  }
  );
}

////////// Common Basic Block Info End //////////

////////// Basic Block Info //////////

namespace {

/// Class, that keeps all important information about each basic block,
/// that is going to be factor out. Evaluates values as lazy as possible
/// It is created for every BB individually
class BBInfo {

public:
  BBInfo(BasicBlock *BB, const BBsCommonInfo &CommonInfo)
      : BB(BB), CommonInfo(CommonInfo) {}

  /// Because of the constant member, we have to overwrite the assignment
  /// operator. The constant reference is the same for every BB in the set of
  /// equal BBs
  BBInfo(const BBInfo &Other) : CommonInfo(Other.CommonInfo) { *this = Other; }
  BBInfo &operator=(const BBInfo &Other);

  void setBB(BasicBlock *BB) {
    this->BB = BB;
    Inputs.reset();
    Outputs.clear();
    ReturnValue = nullptr;
  }
  BasicBlock *getBB() const { return BB; }

  const SmallVector<Value *, 8> &getInputs() const;
  const SmallVector<Instruction *, 8> &getOutputs() const;

  const InstructionLocation &getSpecial() const {
    return CommonInfo.getSpecialInsts();
  };

  // SmartSortedSet<Instruction *>;
  /// permutates Inputs according to Permut
  /// i.e:
  /// Inputs: [0, 2, 3]
  /// Permut: [2, 0, 1]
  /// In result Input will be equal [3, 0, 2]
  void permutateInputs(const SmallVectorImpl<size_t> &Permut);
  Value *getReturnValue() const;

private:
  void extractReturnValue(const size_t ResultId) const;

private:
  BasicBlock *BB;

  const BBsCommonInfo &CommonInfo;

  mutable Optional<SmallVector<Value *, 8>> Inputs;
  mutable SmallVector<Instruction *, 8> Outputs;

  mutable Value *ReturnValue = nullptr;
};

} // end anonymous namespace

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

/// \return Values, that were created outside of the merged \p BB
static SmallVector<Value *, 8>
getInput(BasicBlock *BB, const InstructionLocation &SpecialInsts) {
  // Values, created by merged BB or inserted into Result as Input
  DenseSet<const Value *> Values;
  SmallVector<Value *, 8> Result;

  size_t InstNum = 0;
  for (auto I = getBeginIt(BB), IE = getEndIt(BB); I != IE; ++I, ++InstNum) {
    if (!SpecialInsts.isUsedInsideFunction(InstNum)) {
      continue;
    }

    Values.insert(&*I);
    assert(!isa<TerminatorInst>(I) && !isa<PHINode>(I) && "Malformed BB");
    for (auto &Ops : I->operands()) {
      // global value is treated like constant
      if (isa<Constant>(Ops.get()))
        continue;
      if (isa<InlineAsm>(Ops.get()))
        continue;

      if (Values.count(Ops.get()) == 0) {
        Result.push_back(Ops.get());
        Values.insert(Ops.get());
      }
    }
  }
  return Result;
}

const SmallVector<Value *, 8> &BBInfo::getInputs() const {
  if (!Inputs)
    Inputs = getInput(BB, CommonInfo.getSpecialInsts());
  return *Inputs;
}

const SmallVector<Instruction *, 8> &BBInfo::getOutputs() const {
  const auto &OutputIds = CommonInfo.getOutputIds();
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

void BBInfo::extractReturnValue(const size_t ResultId) const {
  assert(ReturnValue == nullptr && "Return value should be set once");
  if (getOutputs().size() == ResultId) {
    return;
  }
  assert(ResultId < Outputs.size() && "Should be index of Outputs");

  ReturnValue = Outputs[ResultId];
  Outputs[ResultId] = Outputs.back();
  Outputs.pop_back();
}

Value *BBInfo::getReturnValue() const {
  size_t RId = CommonInfo.getReturnValueId();
  if (ReturnValue == nullptr && RId < CommonInfo.getOutputIds().size()) {
    extractReturnValue(RId);
  }
  // check that the extracted return value has not been changed since our first
  // extraction
  DEBUG(if (RId < CommonInfo.getOutputIds().size()) {
    size_t BBRId = CommonInfo.getOutputIds()[RId];
    auto It = getBeginIt(BB);
    std::advance(It, BBRId);
    assert(&*It == ReturnValue);
  } else { assert(ReturnValue == nullptr); });

  return ReturnValue;
}

////////// Basic Block Info End //////////

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
static Function *createFuncFromBB(const BBInfo &Info) {
  BasicBlock *BB = Info.getBB();
  auto &Input = Info.getInputs();
  auto &Output = Info.getOutputs();
  const Value *ReturnValue = Info.getReturnValue();
  auto &SpecialInsts = Info.getSpecial();

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
  for (auto It = getBeginIt(BB), EIt = getEndIt(BB); It != EIt; ++It, ++i) {
    Instruction *I = &*It;
    if (!SpecialInsts.isUsedInsideFunction(i))
      continue;

    Instruction *NewI = Builder.Insert(I->clone());
    InputToArgs.insert(std::make_pair(I, NewI));

    for (auto &Op : NewI->operands()) {
      auto FoundIter = InputToArgs.find(Op.get());
      if (FoundIter != InputToArgs.end()) {
        Op.set(FoundIter->second);
      }
    }

    auto Found = OutputToArgs.find(I);
    if (Found != OutputToArgs.end()) {
      Builder.CreateStore(NewI, Found->second);
    } else if (I == ReturnValue) {
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

  ++FunctionCounter;
  return F;
}

/// \param Info - Basic block, which is going to be replaced with function call
/// to \p F
static void replaceBBWithCall(BBInfo &Info, Function *F) {
  BasicBlock *BB = Info.getBB();
  auto &Input = Info.getInputs();
  auto &Output = Info.getOutputs();
  Value *Result = Info.getReturnValue();
  SmallVector<Instruction *, 8> UsedBefore;
  SmallVector<Instruction *, 8> UsedAfter;
  const auto ItBeg = getBeginIt(BB);
  const auto ItEnd = getEndIt(BB);

  // creating Used* variables
  {
    const InstructionLocation &SpecialInsts = Info.getSpecial();
    size_t i = 0;
    for (auto It = ItBeg; It != ItEnd; ++It, ++i) {
      assert((!SpecialInsts.isUsedBeforeFunction(i) ||
              !SpecialInsts.isUsedAfterFunction(i)) &&
             "Instruction can't be used before and after function call");
      if (Info.getSpecial().isUsedBeforeFunction(i))
        UsedBefore.push_back(&*It);
      else if (Info.getSpecial().isUsedAfterFunction(i))
        UsedAfter.push_back(&*It);
    }
  }

  // 0) Prepare auxiliary utils

  auto NewBB = BasicBlock::Create(BB->getContext(), "", BB->getParent(), BB);
  IRBuilder<> Builder(NewBB);

  auto GetValueForArgs = [&Builder](Value *V, Type *T) {
    if (V->getType() == T)
      return V;
    if (llvm::BitCastInst::isBitCastable(V->getType(), T))
      return Builder.CreateBitCast(V, T);
    // TODO: create common function with least specified arguments
    if (V->getType()->isPointerTy() && !T->isPointerTy()) {
      return Builder.CreatePtrToInt(V, T);
    }
    Value *PI;
    if (!V->getType()->isPointerTy() && T->isPointerTy()) {
      PI = Builder.CreateIntToPtr(V, llvm::Type::getInt8PtrTy(V->getContext()));
      return Builder.CreateBitCast(PI, T);
    }
    llvm_unreachable("Bad BB comparison or wrong Type convertion");
  };

  auto MoveInst = [&](Instruction *I) {
    I->removeFromParent();
    Builder.Insert(I, I->getName());

  };

  // 1) Move all pre-function Insts (Phi-nodes)
  for (; BB->begin() != ItBeg;) {
    MoveInst(&*BB->begin());
  }

  // 2) Create all moved/copied before functions
  for (auto It : UsedBefore) {
    MoveInst(&*It);
  }

  // 3) Create Argument list for function call
  SmallVector<Value *, 8> Args;
  assert(F->arg_size() == Input.size() + Output.size() &&
         "Argument sizes not match");
  Args.reserve(F->arg_size());

  auto CurArg = F->arg_begin();
  // 3a) Append Input arguments into call argument list
  for (auto I = Input.begin(), IE = Input.end(); I != IE; ++I, ++CurArg) {
    Args.push_back(GetValueForArgs(*I, CurArg->getType()));
  }
  // 3b) Allocate space for output pointers, that are Output function arguments
  for (auto IE = F->arg_end(); CurArg != IE; ++CurArg) {
    Args.push_back(
        Builder.CreateAlloca(CurArg->getType()->getPointerElementType()));
  }

  // 4) Create a call
  CallInst *TailCallInst = Builder.CreateCall(F, Args);
  TailCallInst->setTailCallKind(CallInst::TailCallKind::TCK_Tail);
  TailCallInst->setCallingConv(F->getCallingConv());
  if (Result) {
    Value *ResultReplace = GetValueForArgs(TailCallInst, Result->getType());
    ResultReplace->takeName(Result);
    Result->replaceAllUsesWith(ResultReplace);
  }

  // 5) Save and Replace all Output values
  auto AllocaIt = Args.begin() + Input.size();
  for (auto It = Output.begin(), EIt = Output.end(); It != EIt;
       ++It, ++AllocaIt) {
    Instruction *CurrentInst = *It;
    if (!isInstUsedOutsideParent(CurrentInst) &&
        !isValUsedByInsts(CurrentInst, UsedAfter))
      continue;

    auto BBLoadInst = Builder.CreateLoad(*AllocaIt);
    auto BitCasted = GetValueForArgs(BBLoadInst, CurrentInst->getType());
    BitCasted->takeName(CurrentInst);
    CurrentInst->replaceAllUsesWith(BitCasted);
  }

  // 6) Store all Insts, used after function call
  for (auto I : UsedAfter) {
    MoveInst(I);
  }

  // 7) Finish BB with TerminatorInst
  BasicBlock::iterator It;
  for (auto It = ItEnd; It != BB->end(); ++It) {
    // we should copy instructions because RAVW
    // won't work with instructions without TerminatorInst
    auto I = Builder.Insert(It->clone(), It->getName());
    It->replaceAllUsesWith(I);
  }

  NewBB->takeName(BB);
  BB->replaceAllUsesWith(NewBB);
  BB->removeFromParent();
  Info.setBB(NewBB);

  delete BB;
  ++MergeCounter;
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

  auto FRetVal = cast<ReturnInst>(&F->front().back())->getReturnValue();
  (void)FRetVal;
  assert(
      (Info.getReturnValue() == nullptr || Info.getReturnValue() == FRetVal) &&
      "BBs are not equal");

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

///
/// \param F ~ Function to be called
/// \param OtherInfo ~ Info, going to be cloned and used for factoring out
/// \param BB ~ Basic block that is going to be factored out
static void replaceBBInOtherFunction(Function *F, const BBInfo &OtherInfo,
                                     BasicBlock *BB) {
  BBInfo M2BBInfo = OtherInfo;
  M2BBInfo.setBB(BB);
  replaceBBWithCall(M2BBInfo, F);
}

// \p F is our merged function, \p MBBInfos are going to make a call to it.
// Procedure replace basic block in function anyway
static Function *addReplacedFunction(FunctionCompiler &FC, Function *F,
                                     ArrayRef<BBInfo> MBBInfos) {
  const BBInfo &MBBInfo = MBBInfos.front();
  BasicBlock *ClonedBB = MBBInfo.getBB();
  // M2 stands for other module, that is in Function Cost
  Function *CommonFunction =
      FC.cloneFunctionToInnerModule(*MBBInfo.getBB()->getParent(), &ClonedBB);
  assert(ClonedBB != MBBInfo.getBB() && "Basic block must have been replaced");
  assert(ClonedBB->getModule() != MBBInfo.getBB()->getModule() &&
         "Basic block must have different modules");
  Function *NewCommonFunction =
      FC.cloneInnerFunction(*CommonFunction, ClonedBB,
                            std::string(CommonFunction->getName()) + ".new");

  replaceBBInOtherFunction(F, MBBInfo, ClonedBB);

  for (size_t i = 1, ei = MBBInfos.size(); i < ei; ++i) {
    const BBInfo &I = MBBInfos[i];
    BasicBlock *BB =
        getMappedBBofIdenticalFunctions(I.getBB(), NewCommonFunction);
    replaceBBInOtherFunction(F, MBBInfo, BB);
  }

  return NewCommonFunction;
}

// TODO: collapse shouldReplaceCommonChoice and shouldReplacePreciseChoice

static bool shouldReplaceCommonChoice(bool FuncCreated, Function *F,
                                      const SmallVector<BBInfo, 8> &BBInfos,
                                      FunctionCompiler &Cost) {
  SmallVector<StringRef, 16> Funcs;

  if (FuncCreated)
    Funcs.push_back(F->getName());

  Function *M2F = Cost.cloneFunctionToInnerModule(*F);
  for (auto It = BBInfos.begin(), EIt = BBInfos.end(); It != EIt;) {
    // We are going to solve a case, when identical basic blocks
    // reside in the same function. In this case we need to replace all
    // these BBs and insert our function into comparing list just once
    ArrayRef<BBInfo> InSameFunction =
        ArrayRef<BBInfo>(It, EIt).take_while([It](const BBInfo &I) -> bool {
          return I.getBB()->getParent() == It->getBB()->getParent();
        });

    Function *M2MergedF = addReplacedFunction(Cost, M2F, InSameFunction);
    Funcs.push_back(It->getBB()->getParent()->getName());
    Funcs.push_back(M2MergedF->getName());
    It += InSameFunction.size();
  }

  if (!Cost.compile()) {
    DEBUG(dbgs() << "Can't determine module size\n");
    Cost.clearModule();
    return false;
  }

  auto Results = getFunctionSizes(Cost.getObject(), Funcs);
  Cost.clearModule();

  int SizeProfit = FuncCreated ? -Results.front() : 0;
  for (size_t i = static_cast<size_t>(FuncCreated); i < Results.size();
       i += 2) {
    SizeProfit += Results[i] - Results[i + 1];
  }

  return SizeProfit > 0;
}

static bool shouldReplacePreciseChoice(bool FuncCreated, Function *Common,
                                       const SmallVector<BBInfo, 8> &BBInfos,
                                       FunctionCompiler &Cost) {
  // get current size of functions
  SmallVector<StringRef, 16> Funcs;
  for (auto It = BBInfos.begin(), EIt = BBInfos.end(); It != EIt;) {
    Function *LastF = It->getBB()->getParent();
    Funcs.push_back(LastF->getName());
    Cost.cloneFunctionToInnerModule(*LastF);
    do {
      ++It;
    } while (It != EIt && LastF == It->getBB()->getParent());
  }

  if (!Cost.compile()) {
    DEBUG(dbgs() << "Can't determine module size\n");
    Cost.clearModule();
    return false;
  }

  auto OldSizes = getFunctionSizes(Cost.getObject(), Funcs);

  size_t EHOldSize = getEHSize(Cost.getObject());
  // we can't reuse the same functions because they are modified, when compiled
  // some instructions might be added
  Cost.clearModule();

  // compute new size of functions

  Function *NewCommon = nullptr;
  if (FuncCreated) {
    Funcs.push_back(Common->getName());
    NewCommon = Cost.cloneFunctionToInnerModule(*Common);
  } else {
    // create a declaration
    NewCommon = cast<Function>(Cost.getInnerModuleValue(*Common));
  }

  // clone and modify functions
  for (auto It = BBInfos.begin(), EIt = BBInfos.end(); It != EIt;) {
    ArrayRef<BBInfo> InSameFunction =
        ArrayRef<BBInfo>(It, EIt).take_while([It](const BBInfo &I) -> bool {
          return I.getBB()->getParent() == It->getBB()->getParent();
        });
    Function *NewF = Cost.cloneFunctionToInnerModule(*It->getBB()->getParent());
    for (auto &Info : InSameFunction) {
      BasicBlock *BB = Info.getBB();
      BB = getMappedBBofIdenticalFunctions(BB, NewF);
      replaceBBInOtherFunction(NewCommon, Info, BB);
      ++It;
    }
  }

  if (!Cost.compile()) {
    DEBUG(dbgs() << "Can't determine module size\n");
    Cost.clearModule();
    return false;
  }
  auto NewSizes = getFunctionSizes(Cost.getObject(), Funcs);

  size_t EHNewSize = getEHSize(Cost.getObject());
  Cost.clearModule();

  int SizeProfit = FuncCreated ? -NewSizes.back() : 0;
  for (size_t i = 0, ei = OldSizes.size(); i < ei; ++i) {
    SizeProfit += OldSizes[i] - NewSizes[i];
  }
  SizeProfit += EHOldSize - EHNewSize;
  return SizeProfit > 0;
}

static bool shouldReplace(bool FuncCreated, Function *F,
                          const SmallVector<BBInfo, 8> &BBInfos,
                          FunctionCompiler &Cost) {
  AttributeSet FnAttr = F->getAttributes().getFnAttributes();
  // TODO: understand NoUnwind attribute
  if (FnAttr.hasFnAttribute(Attribute::NoUnwind))
    return shouldReplaceCommonChoice(FuncCreated, F, BBInfos, Cost);
  else
    return shouldReplacePreciseChoice(FuncCreated, F, BBInfos, Cost);
}

/// Common steps of replacing equal basic blocks
/// 1) Get common basic block info(inputs, outputs, ...)
/// 2) Find suitable for merging function, or create if not found
/// 3) Replace Basic blocks with factored out function.
/// \param BBs array of equal basic blocks
/// \return true if any BB was changed
bool MergeBB::replace(const SmallVectorImpl<BasicBlock *> &BBs) {
  assert(BBs.size() >= 2 && "No sence in merging");
  assert(!skipFromMerging(BBs.front()) && "BB shouldn't be merged");

  auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(
      *BBs.front()->getParent());

  BBsCommonInfo CommonInfo(BBs, TTI);

  SmallVector<BBInfo, 8> BBInfos;
  std::for_each(BBs.begin(), BBs.end(),
                [&CommonInfo, &BBInfos](BasicBlock *BB) {
                  BBInfos.emplace_back(BB, CommonInfo);
                });
  // we need to sort it to identify BBs, sharing the same functions
  std::sort(BBInfos.begin(), BBInfos.end(),
            [](const BBInfo &BBL, const BBInfo &BBR) {
              return BBL.getBB()->getParent() < BBR.getBB()->getParent();
            });

  Function *F = nullptr;
  StringRef CreatedInfo;
  DEBUG(CreatedInfo = "existed");

  // Try to find suitable for merging function
  // If basic block has more, than 1 output, function can not be found
  // because llvm doesn't support multiple return values
  if (CommonInfo.getOutputIds().size() <= 1) {
    SmallVector<size_t, 8> Permuts;
    size_t Id = findAppropriateBBsId(BBInfos, Permuts);
    if (Id != BBInfos.size()) {
      F = BBInfos[Id].getBB()->getParent();

      // remove BBInfos[Id] from replacing
      BBInfos[Id] = std::move(BBInfos.back());
      BBInfos.pop_back();

      for (auto &Info : BBInfos) {
        Info.permutateInputs(Permuts);
      }
    }
  }
  bool FunctionCreated = F == nullptr;
  // if function was not found, create it
  if (FunctionCreated) {
    auto &Model = BBInfos.front();

    F = createFuncFromBB(Model);
    F->setName(FNamer->getName());
    DEBUG(CreatedInfo = "created");
  }
  assert(F != nullptr && "Should not be reached");

  if (!ForceMerge && !shouldReplace(FunctionCreated, F, BBInfos, *Cost)) {
    if (FunctionCreated)
      F->eraseFromParent();
    return false;
  }

  for (auto &Info : BBInfos) {
    replaceBBWithCall(Info, F);
  }

  DEBUG(dbgs() << "Number of basic blocks, replaced with " << CreatedInfo
               << " function " << F->getName() << ": " << BBInfos.size()
               << "\n");
  debugPrint(BBInfos.front().getBB(), "", false);
  DEBUG(F->print(dbgs()));
  DEBUG(dbgs() << "\n");

  return true;
}