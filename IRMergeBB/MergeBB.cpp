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
#include "Utilities.h"
#include "ForceMergePAC.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/IRBuilder.h"
#include <deque>


#define DEBUG_TYPE "mergebb"

STATISTIC(MergeCounter, "Number of merged basic blocks");
STATISTIC(FunctionCounter, "Amount of created functions");

using namespace llvm;
using namespace utilities;

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

static cl::opt<uint32_t> MinActualBlockSize(
    "mergebb-threshold", cl::Hidden, cl::init(2),
    cl::desc("Sizes of basic block, that are equal or less the specified"
             "will be skipped from merging"));

static cl::opt<int>
    AddBlockWeight("mergebb-addWeight", cl::Hidden, cl::init(0),
                   cl::desc("Additional weight to the created block"));

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
  /// \param PAC - target-dependent subroutines
  /// \returns whether BBs were replaced with a function call
  bool replace(const SmallVectorImpl<BasicBlock *> &BBs,
               IProceduralAbstractionCost *PAC);

  GlobalNumberState GlobalNumbers;
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

  const StringRef Arch =
      StringRef(M.getTargetTriple()).take_front(M.getTargetTriple().find('-'));
  auto DM = ForceMerge
                ? make_unique<ForceMergePAC>()
                : IProceduralAbstractionCost::Create(Arch, AddBlockWeight);
  assert(DM.get() && "DM was not created properly");

  for (auto &IdenticalBlocks : BBTree) {
    if (IdenticalBlocks.second.size() >= 2) {
      Changed |= replace(IdenticalBlocks.second, DM.get());
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
  if (BB->size() <= MinActualBlockSize + 1)
    return true;

  if (BB->isLandingPad()) {
    debugPrint(BB, "Block family is a landing pad. Skip it");
    return true;
  }

  long BBsSize = std::distance(getBeginIt(BB), getEndIt(BB));
  if (BBsSize <= MinActualBlockSize) {
    debugPrint(BB, "Block family is too small to bother merging");
    return true;
  }
  return false;
}

/// The way of representing output and skipped instructions of basic blocks
using BBInstIds = SmallVector<size_t, 8>;
using BBInstIdsImpl = SmallVectorImpl<size_t>;

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

/// Class intends to handle special instruction indicies that are not going to
/// be factored out
class SpecialInstsIds : public InstructionLocation {
public:
  enum class Type : char {
    Usual,
    CopyBefore,
    MoveBefore,
    CopyAfter,
    MoveAfter
  };

  void push_back(const Type T) { SpecialInsts.push_back(T); }

  bool isUsual(const size_t Id) const {
    return SpecialInsts[Id] == Type::Usual;
  }

  bool isUsedBeforeFunction(const size_t Id) const {
    return SpecialInsts[Id] == Type::CopyBefore ||
           SpecialInsts[Id] == Type::MoveBefore;
  }

  virtual bool isUsedInsideFunction(const size_t Id) const override {
    return SpecialInsts[Id] == Type::Usual ||
           SpecialInsts[Id] == Type::CopyBefore ||
           SpecialInsts[Id] == Type::CopyAfter;
  }

  bool isUsedAfterFunction(const size_t Id) const {
    return SpecialInsts[Id] == Type::CopyAfter ||
           SpecialInsts[Id] == Type::MoveAfter;
  }
  virtual bool isUsedOutsideFunction(const size_t Id) const override {
    return !isUsual(Id);
  }

  const Type &operator[](const size_t i) const { return SpecialInsts[i]; }
  Type &operator[](const size_t i) { return SpecialInsts[i]; }

  virtual size_t amountInsts() const override { return SpecialInsts.size(); }

private:
  SmallVector<Type, 64> SpecialInsts;
};

////////// Common Basic Block Info //////////

namespace {

/// Common information about Basic Blocks
/// Creates for every equal set of BBs
class BBsCommonInfo {
public:
  BBsCommonInfo(ArrayRef<BasicBlock *> BBs, const TargetTransformInfo &TTI);

  const BBInstIdsImpl &getOutputIds() const { return OutputIds; }
  const SpecialInstsIds &getSpecialInsts() const { return SpecialInsts; }

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

  SpecialInstsIds SpecialInsts;
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
static Optional<SpecialInstsIds::Type>
InstOutPos(const Instruction *I,
           const SmallPtrSetImpl<const Value *> &UsedBefore,
           const SmallPtrSetImpl<const Value *> &UsedAfter,
           const SmallVectorImpl<Instruction *> &Outputs,
           const SpecialInstsIds::Type InitialPos =
               SpecialInstsIds::Type::CopyBefore) {
  assert((InitialPos == SpecialInstsIds::Type::CopyBefore ||
          InitialPos == SpecialInstsIds::Type::CopyAfter) &&
         "Check Param");
  const BasicBlock *BB = I->getParent();
  SpecialInstsIds::Type Result = InitialPos;
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
      Result = SpecialInstsIds::Type::CopyAfter;
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
static Optional<SpecialInstsIds::Type>
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
    return SpecialInstsIds::Type::MoveBefore;
  }
  case Instruction::BitCast:
    // bitcast is an output => decide how to take it out from output values:
    // copy or move
    return InstOutPos(I, ValuesBefore, ValuesAfter, Outputs);
  case Instruction::GetElementPtr:
    return InstOutPos(I, ValuesBefore, ValuesAfter, Outputs,
                      SpecialInstsIds::Type::CopyAfter);
  default: {
    if (TTI.getUserCost(I) == TargetTransformInfo::TCC_Free)
      return InstOutPos(I, ValuesBefore, ValuesAfter, Outputs);
    return None;
  }
  }
}

/// The same, as setTypeIfOutput, but handles all instructions and is used,
/// if setTypeIfOutput can't handle \p I
static Optional<SpecialInstsIds::Type>
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
        return SpecialInstsIds::Type::MoveBefore;
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
    Optional<SpecialInstsIds::Type> ResultInstType;
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
                                          : SpecialInstsIds::Type::Usual);

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
      if (SpecialInsts[i] == SpecialInstsIds::Type::CopyBefore) {
        SpecialInsts[i] = SpecialInstsIds::Type::MoveBefore;
        continue;
      }
      if (SpecialInsts[i] == SpecialInstsIds::Type::CopyAfter) {
        SpecialInsts[i] = SpecialInstsIds::Type::MoveAfter;
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
  for (auto It = Outputs.rbegin(), EIt = Outputs.rend(); It != EIt; ++It) {
    Instruction *I = *It;
    assert(!llvm::isa<AllocaInst>(I) && "Alloca Can't be return value");
    if (I->getType()->isFirstClassType()) {
      ReturnValueOutputId = Outputs.rend() - It - 1;
      return;
    }
  }
  ReturnValueOutputId = Outputs.size();
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

  void setBB(BasicBlock *BB) { this->BB = BB; }
  BasicBlock *getBB() const { return BB; }

  const SmallVector<Value *, 8> &getInputs() const;
  const SmallVector<Instruction *, 8> &getOutputs() const;

  const SpecialInstsIds &getSpecial() const {
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
static SmallVector<Value *, 8> getInput(BasicBlock *BB,
                                        const SpecialInstsIds &SpecialInsts) {
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
/// \param [out] Replaces - pair of instructions, from old and new basic blocks,
/// which are equivalent. Used for replacing old basic block with a new one.
static BasicBlock *
createBBWithCall(const BBInfo &Info, Function *F,
                 SmallVectorImpl<std::pair<Value *, Value *>> &Replaces) {
  BasicBlock *BB = Info.getBB();
  auto &Input = Info.getInputs();
  auto &Output = Info.getOutputs();
  Value *Result = Info.getReturnValue();
  auto &SpecialInsts = Info.getSpecial();

  auto NewBB = BasicBlock::Create(BB->getContext());
  IRBuilder<> Builder(NewBB);
  Replaces.clear();

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

  auto InsertInst = [&](Instruction *I) {
    Instruction *NewI = Builder.Insert(I->clone());
    Replaces.emplace_back(I, NewI);
  };

  // 1) Restore all pre-function Insts (Phi-nodes)
  auto ItBeg = getBeginIt(BB);
  auto ItEnd = getEndIt(BB);
  for (auto It = BB->begin(); It != ItBeg; ++It)
    InsertInst(&*It);

  // 2) Create all moved/copied before functions
  size_t i = 0;
  for (auto It = ItBeg; It != ItEnd; ++It, ++i) {
    if (SpecialInsts.isUsedBeforeFunction(i))
      InsertInst(&*It);
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
    Replaces.emplace_back(Result, ResultReplace);
  }

  // 5) Save and Replace all Output values
  SmallVector<Instruction *, 8> UsedAfterFunction;
  i = 0;
  for (auto It = ItBeg; It != ItEnd; ++It, ++i) {
    if (SpecialInsts.isUsedAfterFunction(i))
      UsedAfterFunction.push_back(&*It);
  }

  auto AllocaIt = Args.begin() + Input.size();
  for (auto It = Output.begin(), EIt = Output.end(); It != EIt;
       ++It, ++AllocaIt) {
    Instruction *CurrentInst = *It;
    if (!isInstUsedOutsideParent(CurrentInst) &&
        !isValUsedByInsts(CurrentInst, UsedAfterFunction))
      continue;

    auto BBLoadInst = Builder.CreateLoad(*AllocaIt);
    auto BitCasted = GetValueForArgs(BBLoadInst, CurrentInst->getType());
    Replaces.emplace_back(CurrentInst, BitCasted);
  }

  // 6) Store all Insts, used after function call
  for (auto I : UsedAfterFunction) {
    InsertInst(I);
  }

  // 7) Finish BB with TerminatorInst
  for (auto It = ItEnd, EIt = BB->end(); It != EIt; ++It) {
    InsertInst(&*It);
  }

  return NewBB;
}

/// Replaces \p Old basic block with \p New one, using
/// \p Replaces auxiliary information
void replaceBBs(BasicBlock *Old, BasicBlock *New,
                const SmallVectorImpl<std::pair<Value *, Value *>> &Replaces) {
  New->takeName(Old);
  New->insertInto(Old->getParent(), Old);
  Old->replaceAllUsesWith(New);
  for (auto Insts : Replaces) {
    Insts.first->takeName(Insts.second);
    Insts.first->replaceAllUsesWith(Insts.second);
  }

  Old->removeFromParent();

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

/// Checks if the BB can be really tail called to the factored out function
static bool beforeReturnBaseBlock(const BasicBlock *BB,
                                  const Value *OutputVal) {
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

/// Common steps of replacing equal basic blocks
/// 1) Get common basic block info(inputs, outputs, ...)
/// 2) Prepare procedure abstraction cost (PAC)
/// 3) Find suitable for merging function, or create if not found
/// 4) Replace Basic blocks with factored out function.
/// \param BBs array of equal basic blocks
/// \return true if any BB was changed
bool MergeBB::replace(const SmallVectorImpl<BasicBlock *> &BBs,
                      IProceduralAbstractionCost *PAC) {
  assert(BBs.size() >= 2 && "No sence in merging");
  assert(!skipFromMerging(BBs.front()) && "BB shouldn't be merged");

  auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(
      *BBs.front()->getParent());

  BBsCommonInfo CommonInfo(BBs, TTI);

  PAC->init(TTI, CommonInfo.getSpecialInsts(), getBeginIt(BBs.front()),
            getEndIt(BBs.front()));

  if (PAC->isTiny()) {
    debugPrint(BBs.front(), "Block family is not worth merging");
    return false;
  }

  SmallVector<BBInfo, 8> BBInfos;
  std::for_each(BBs.begin(), BBs.end(),
                [&CommonInfo, &BBInfos](BasicBlock *BB) {
                  BBInfos.emplace_back(BB, CommonInfo);
                });
  // check for tail call
  bool IsReallyTail =
      CommonInfo.getOutputIds().size() <= 1 &&
      all_of(BBInfos, [](const BBInfo &Info) {
        const Value *OutputValue =
            Info.getOutputs().size() == 1 ? Info.getOutputs().front() : nullptr;
        return beforeReturnBaseBlock(Info.getBB(), OutputValue);
      });
  PAC->setTail(IsReallyTail);

  if (!PAC->replaceWithCall(BBInfos.front().getInputs().size(),
                            CommonInfo.getOutputIds().size())) {
    debugPrint(BBs.front(), "BB factoring out won't decrease the code size");
    return false;
  }

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

  // if function was not found, create it
  if (F == nullptr) {
    if (!PAC->replaceWithCall(BBInfos.size(),
                              BBInfos.front().getInputs().size(),
                              CommonInfo.getOutputIds().size())) {
      debugPrint(BBs.front(),
                 "Unprofitable to factor out, creating a function");
      return false;
    }

    auto &Model = BBInfos.front();

    F = createFuncFromBB(Model);
    DEBUG(CreatedInfo = "created");
  }

  assert(F != nullptr && "Should not be reached");
  SmallVector<std::pair<Value *, Value *>, 128> Replaces;

  for (auto &Info : BBInfos) {
    BasicBlock *NewBB = createBBWithCall(Info, F, Replaces);
    replaceBBs(Info.getBB(), NewBB, Replaces);
    delete Info.getBB();
    Info.setBB(NewBB);
  }

  DEBUG(dbgs() << "Number of basic blocks, replaced with " << CreatedInfo
               << " function " << F->getName() << ": " << BBInfos.size()
               << "\n");
  debugPrint(BBInfos.front().getBB(), "", false);
  DEBUG(F->print(dbgs()));
  DEBUG(dbgs() << "\n");

  return true;
}
