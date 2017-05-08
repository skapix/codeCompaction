//===-- FunctionCost.h - Calculates function size--------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains interface to calculate function size
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_IDECISIONMAKER_H
#define LLVMTRANSFORM_IDECISIONMAKER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Error.h"
#include "llvm/Transforms/Utils/ValueMapper.h" // typedef ValueToValueMapTy
#include <memory>

class ModuleMaterializer;

class FunctionCost {
public:
  FunctionCost(const llvm::Module &OtherM);

  // for creating functions directly in this module
  llvm::Module &getModule() {
    assert(isInitialized());
    return *M;
  }

  llvm::Expected<llvm::SmallVector<size_t, 8>>
  getFunctionSizes(const llvm::SmallVectorImpl<llvm::Function *> &Fs);

  ~FunctionCost();

  llvm::Function *cloneFunctionToInnerModule(llvm::Function &F,
                                             llvm::BasicBlock *&BB);

  llvm::Function *cloneInnerFunction(llvm::Function &F, llvm::BasicBlock *&BB,
                                     const llvm::StringRef NewName);

  bool isInitialized() { return IsInitialized; }

private:
  void clearFunctions();

private:
  std::unique_ptr<llvm::Module> M;
  // utilities for partial module cloning
  llvm::ValueToValueMapTy VtoV;
  std::unique_ptr<ModuleMaterializer> Materializer;
  std::unique_ptr<llvm::ValueMapper> Mapper;

  // utilities for compiling module
  llvm::legacy::PassManager PM;
  std::unique_ptr<llvm::TargetMachine> TM;
  llvm::raw_svector_ostream OS;
  llvm::SmallVector<char, 1024> OSBuf;

  bool IsInitialized;
};

#endif // LLVMTRANSFORM_IDECISIONMAKER_H
