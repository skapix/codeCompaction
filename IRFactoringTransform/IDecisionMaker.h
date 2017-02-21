//===-- IDecisionMaker.h - DM Interface------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains Decision maker (DM) interface. It is used for deciding,
/// whether we should factor out a BB, or replacement with a
/// call will lead us to code swelling.
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_IDECISIONMAKER_H
#define LLVMTRANSFORM_IDECISIONMAKER_H

#include <llvm/ADT/StringRef.h>
#include <memory>

namespace llvm {
class Instruction;
template <typename T> class SmallVectorImpl;
}

/// Interface's main aim is to aproximately calculate whether
/// we get any profit by factoring out the BB.
/// Interface is supposed to be target-dependent,
/// because it is rather subtle part of BB merging and final code size
/// majorly relies on this part of algorithm.
class IDecisionMaker {
public:
  /// Common method for creating instances for different architectures
  /// \param Arch final architecture of compiled file
  /// \return appropriate decision maker instance
  static std::unique_ptr<IDecisionMaker> Create(const llvm::StringRef Arch);

  /// Initialize decision maker with instructions \p Insts,
  /// that are going to be factored out
  virtual void
  init(const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) = 0;

  /// \return whether is function is too small and not worth
  /// considering to merge
  virtual bool isTiny() const = 0;

  /// Decides, whether it is profitable to factor out BBs, creating no function
  /// \param InputArgs amount of input arguments
  /// \param OutputArgs amount of output arguments
  /// \return true, if the code size will presumably be reduced,
  /// if suitable function already exists
  virtual bool replaceNoFunction(const size_t InputArgs,
                                 const size_t OutputArgs) const = 0;

  /// Decides, whether it is profitable to factor out BB, creating a function
  /// \param InputArgs amount of input arguments
  /// \param OutputArgs amount of output arguments
  /// \return true, if the code size will presumably be reduced,
  /// considering the size of auxiliary created function
  virtual bool replaceWithFunction(const size_t BBAmount,
                                   const size_t InputArgs,
                                   const size_t OutputArgs) const = 0;

  virtual ~IDecisionMaker() {}
};

#endif // LLVMTRANSFORM_IDECISIONMAKER_H
