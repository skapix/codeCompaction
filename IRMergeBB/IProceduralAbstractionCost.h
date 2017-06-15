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
#include <llvm/IR/BasicBlock.h>
#include <memory>

namespace llvm {
class TargetTransformInfo;
class Instruction;
template <typename T> class SmallVectorImpl;
} // namespace llvm

class InstructionLocation {
public:
  virtual bool isUsedInsideFunction(const size_t i) const = 0;
  virtual bool isUsedOutsideFunction(const size_t i) const = 0;
  virtual size_t amountInsts() const = 0;
};

/// Interface's main aim is to aproximately calculate whether
/// we get any profit by factoring out the BB.
/// Interface is supposed to be target-dependent,
/// because it is rather subtle part of BB merging and final code size
/// majorly relies on this part of algorithm.
class IProceduralAbstractionCost {
public:
  /// Common method for creating instances for different architectures
  /// \param Arch final architecture of compiled file
  /// \return appropriate decision maker instance
  static std::unique_ptr<IProceduralAbstractionCost>
  Create(const llvm::StringRef Arch, int AddBlockWeight);

  /// Initialize decision maker with instructions \p Insts,
  /// that are going to be factored out
  virtual void init(const llvm::TargetTransformInfo &TTI,
                    const InstructionLocation &IL,
                    const llvm::BasicBlock::const_iterator &Begin,
                    const llvm::BasicBlock::const_iterator &End) = 0;

  virtual void setTail(const bool IsReallyTail);

  /// \return whether is function is too small and not worth
  /// considering to merge
  virtual bool isTiny() const = 0;

  /// Decides, whether it is profitable to factor out BBs, creating no function
  /// Function is used for cheap early-access check
  /// \param InputArgs amount of input arguments
  /// \param OutputArgs amount of output arguments
  /// \return true, if the code size will presumably be reduced,
  /// if suitable function already exists
  virtual bool replaceWithCall(const size_t InputArgs,
                               const size_t OutputArgs) const = 0;

  /// Decides, whether it is profitable to factor out BB, creating a function
  /// \param InputArgs amount of input arguments
  /// \param OutputArgs amount of output arguments
  /// \return true, if the code size will presumably be reduced,
  /// considering the size of auxiliary created function
  virtual bool replaceWithCall(const size_t BBAmount, const size_t InputArgs,
                               const size_t OutputArgs) const = 0;

  virtual ~IProceduralAbstractionCost() {}

protected:
  bool IsTail;
};

#endif // LLVMTRANSFORM_IDECISIONMAKER_H
