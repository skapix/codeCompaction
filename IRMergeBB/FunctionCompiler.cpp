//===-- FunctionCompiler.cpp - Calculates function size--------------*- C++
//-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FunctionCompiler.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolSize.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Utils/Cloning.h"

// TODO: calculate unwind info, when computing function sizes
// TODO: it is also possible to map metadata. Will metadata mapping increase
// accuracy of exact size?
// TODO: now we are unable to include CommandFlags because of
// multiple declaration of the same variables
// But we need this include because of several inline functions
//#include "llvm/CodeGen/CommandFlags.h"

#define DEBUG_TYPE "functioncost"

using namespace llvm;

static Function *CreateFunction(const Function &F, Module *M,
                                const StringRef NewName) {
  assert(M->getFunction(NewName) == nullptr && "Function already exists");
  Function *NF =
      Function::Create(F.getFunctionType(), F.getLinkage(), NewName, M);
  NF->copyAttributesFrom(&F);
  if (NF->getLinkage() == Function::LinkageTypes::PrivateLinkage ||
      NF->getLinkage() == Function::LinkageTypes::InternalLinkage)
    NF->setLinkage(Function::ExternalLinkage);
  return NF;
}

static Function *CreateFunction(Function &Other, Module *M) {
  return CreateFunction(Other, M, Other.getName());
}

class ModuleMaterializer : public ValueMaterializer {
  virtual Value *materialize(Value *V);

public:
  ModuleMaterializer(Module &M) : M(&M) {}

private:
  Module *M;
};

Value *ModuleMaterializer::materialize(Value *V) {
  if (auto GV = dyn_cast<GlobalVariable>(V)) {
    assert(M->getGlobalVariable(V->getName()) == nullptr);
    GlobalVariable *NGV = new GlobalVariable(
        *M, GV->getValueType(), GV->isConstant(), GV->getLinkage(), nullptr,
        GV->getName(), nullptr, GV->getThreadLocalMode(),
        GV->getType()->getAddressSpace());
    NGV->copyAttributesFrom(&*GV);
    if (NGV->getLinkage() == GlobalVariable::LinkageTypes::PrivateLinkage ||
        NGV->getLinkage() == GlobalVariable::LinkageTypes::InternalLinkage)
      NGV->setLinkage(Function::ExternalLinkage);
    return NGV;
  } else if (auto F = dyn_cast<Function>(V)) {
    return CreateFunction(*F, M);
  }

  // TODO: ? GlobalAlias

  return nullptr;
}

// initializes all necessary info for calculating size
static void initializeAdditionInfo() {
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllTargetInfos();
  InitializeAllAsmPrinters();
}

static void copyModuleInfo(const Module &From, Module &To) {
  To.setTargetTriple(From.getTargetTriple());
  To.setDataLayout(From.getDataLayout());
  To.setPICLevel(From.getPICLevel());
  To.setPIELevel(From.getPIELevel());
}

FunctionCompiler::FunctionCompiler(const Module &OtherM)
    : M(make_unique<Module>("FunctionCost_auxiliary", OtherM.getContext())),
      Materializer(make_unique<ModuleMaterializer>(*M)), OS(OSBuf),
      IsInitialized(false) {

  std::string TripleName = OtherM.getTargetTriple().empty() ?
                           Triple::normalize(sys::getDefaultTargetTriple()) :
                           Triple::normalize(OtherM.getTargetTriple());

  copyModuleInfo(OtherM, *M);
  M->setTargetTriple(TripleName);
  // initialize copying info
  Mapper = make_unique<ValueMapper>(VtoV, RF_NullMapMissingGlobalValues,
                                    nullptr, Materializer.get());

  // initialize compiling info
  initializeAdditionInfo();

  std::string CPUStr; // = getCPUStr();
  std::string FeaturesStr; // = getFeaturesStr();


  std::string ErrorStr;

  const Target *TheTarget = TargetRegistry::lookupTarget(TripleName, ErrorStr);

  if (!TheTarget) {
    DEBUG(dbgs() << "Can't get target\n");
    DEBUG(dbgs() << ErrorStr);
    return;
  }

  // TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
  TargetOptions Options;

  TM.reset(TheTarget->createTargetMachine(TripleName, CPUStr, FeaturesStr,
                                          Options, Optional<Reloc::Model>()));
  M->setDataLayout(TM->createDataLayout());

  if (!TM.get()) {
    DEBUG(dbgs() << "Can't create TargetMachine\n");
    return;
  }

  TargetLibraryInfoImpl TLII = TargetLibraryInfoImpl(Triple(TripleName));

  PM.add(new TargetLibraryInfoWrapperPass(TLII));

  if (TM->addPassesToEmitFile(PM, OS, TargetMachine::CGFT_ObjectFile)) {
    DEBUG(dbgs() << "Can't initialize pass manager\n");
    return;
  }

  IsInitialized = true;
}

static void deinitializeAdditionInfo() {
  // TODO: ? we should memorize anyhow all already initialized targets/MCs
  // and deinitialize only targets, that were not initialized till this pass
}

static void getFunctionReplaces(Function &F, Function &NewF,
                                ValueToValueMapTy &Result) {
  Result.insert({&F, &NewF});
  for (auto AF = F.arg_begin(), NAF = NewF.arg_begin(), EAF = F.arg_end();
       AF != EAF; ++AF, ++NAF) {
    Result.insert({&*AF, &*NAF});
  }
}

static void resetFunctionReplaces(Function &F, ValueToValueMapTy &Result) {
  // don't erase mapping to Function itself
  for (auto AF = F.arg_begin(), EAF = F.arg_end(); AF != EAF; ++AF) {
    bool Erased = Result.erase(AF);
    (void)Erased;
    assert(Erased && "Inconsistency");
  }
}

Function *
FunctionCompiler::cloneFunctionToInnerModule(Function &F,
                                             BasicBlock **BBInterest) {
  assert(F.getParent() != M.get() && "Other method should be used");
  Function *NewFunction = M->getFunction(F.getName());
  assert((NewFunction == nullptr || NewFunction->isDeclaration()) &&
         "Function already exists");
  if (!NewFunction) {
    NewFunction = CreateFunction(F, M.get());
    VtoV[&F] = NewFunction;
  }

  getFunctionReplaces(F, *NewFunction, VtoV);
  // filling VtoV
  // 1) fill missed functions, searching CallInst and InvokeInst's
  // 2) search in arguments global values and insert them
  for (auto &BB : F) {
    for (auto &I : BB) {
      // no need to handle functions in a special way because
      // function itself is one of the arguments of instructions
      CallSite CS(&I);
      if (CS) {
        Function *Called = CS.getCalledFunction();
        // virtual functions can't be determined in link time
        if (Called)
          Mapper->mapConstant(*Called);
      }

      for (auto &Op : I.operands()) {
        if (auto C = dyn_cast<Constant>(Op.get())) {
          auto NewC = Mapper->mapConstant(*C);
          // add initializer if it exists
          if (auto GV = dyn_cast<GlobalVariable>(C)) {
            if (GV->hasInitializer()) {
              auto NewGV = cast<GlobalVariable>(NewC);
              NewGV->setInitializer(Mapper->mapConstant(*GV->getInitializer()));
            }
          }
        }

        // InsertConstantOperands(VtoV, C, M.get());
      } // for (auto &Op : I.operands()) {
    }   // for (auto &I : BB)
  }     // for (auto &BB : F)

  SmallVector<ReturnInst *, 8> Returns;
  CloneFunctionInto(NewFunction, &F, VtoV, true, Returns);

  if (BBInterest)
    *BBInterest = cast<BasicBlock>(VtoV[*BBInterest]);

  resetFunctionReplaces(F, VtoV);
  return NewFunction;
}

llvm::Function *FunctionCompiler::cloneInnerFunction(llvm::Function &F,
                                                     BasicBlock *&BB,
                                                     const StringRef NewName) {
  assert(F.getParent() == M.get() &&
         "Function from other module shouldn't be used");
  ValueToValueMapTy LocalVtoV;
  Function *NewFunction = CloneFunction(&F, LocalVtoV);
  NewFunction->setName(NewName);
  BB = cast<BasicBlock>(LocalVtoV[BB]);
  return NewFunction;
}

FunctionCompiler::~FunctionCompiler() { deinitializeAdditionInfo(); }


// \p M is for debug and catching errors
static void eraseSurroundings(Value &V, Module *M = nullptr) {

  for (auto U : V.users()) {
    if (auto I = dyn_cast<Instruction>(U)) {
      (void)I;
      assert(I->getModule() == M &&
             "We should not touch our primary module");
    }

    U->dropAllReferences();
  }

  assert(V.use_empty() && "V still contains users");
}

void FunctionCompiler::clearModule() {
  VtoV.clear();
  // TODO: ? don't erase declarations

  auto It = M->begin();
  while (It != M->end()) {
    if (It->hasNUsesOrMore(1)) {
      eraseSurroundings(*It, It->getParent());
    }
    It->eraseFromParent();
    It = M->begin();
  }
  auto GVIt = M->global_begin();
  while (GVIt != M->global_end()) {
    if (GVIt->hasNUsesOrMore(1)) {
      eraseSurroundings(*GVIt, GVIt->getParent());
    }
    GVIt->eraseFromParent();
    GVIt = M->global_begin();
  }
  auto AIt = M->alias_begin();
  while (AIt != M->alias_end()) {
    if (AIt->hasNUsesOrMore(1)) {
      eraseSurroundings(*AIt, AIt->getParent());
    }
    AIt->eraseFromParent();
    AIt = M->alias_begin();
  }

}

bool FunctionCompiler::compile() {
  PM.run(*M);
  auto Buf = MemoryBufferRef(StringRef(OSBuf.data(), OSBuf.size()), "");
  auto ExpectedObject = object::ObjectFile::createObjectFile(Buf);
  if (!ExpectedObject) {
    DEBUG(dbgs() << "Error: could not create an object file\n");
    return false;
  }
  Obj = std::move(*ExpectedObject);

  return true;
}

llvm::Value *FunctionCompiler::getInnerModuleValue(llvm::Value &V) {
  return Mapper->mapValue(V);
}