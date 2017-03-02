//===-- TargetDependent/CommonPAC.h - Common BBFactor size utils-----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_WEIGHT_H
#define LLVMTRANSFORM_WEIGHT_H

#include "../IProceduralAbstractionCost.h"
#include <llvm/IR/Instructions.h>

/// The key idea of this class is to separate algorithms, and raw
/// target-dependent data.
/// If user wants to write an additional decision maker, he should implement
/// only init, and optionally: getNewBlockCost(size_t, size_t),
/// getOriginalBlockCost(), getFunctionCreationCost(size_t, size_t).
/// isTiny() const override is not finaled
/// because weight can be not equal to amount of instruction
class CommonPAC : public IProceduralAbstractionCost {
public:
  /// BlockWeight should be set in init, otherwise all methods should
  /// be overwritten
  virtual void
  init(const llvm::TargetTransformInfo &TTI,
       const llvm::SmallVectorImpl<llvm::Instruction *> &Insts) override;
  virtual bool isTiny() const override;
  virtual bool replaceWithCall(const size_t InputArgs,
                               const size_t OutputArgs) const override final;
  virtual bool replaceWithCall(const size_t BBAmount, const size_t InputArgs,
                               const size_t OutputArgs) const override final;
  virtual ~CommonPAC();

protected:
  /// Provides us with information, when Instruction does not
  /// produce machine code in architecture (e.g BitCastInst)
  /// \param I instruction
  /// \return true, if instruction \p I has no mapping to machine code
  static bool isSkippedInstruction(const llvm::TargetTransformInfo &TTI,
                                   const llvm::Instruction *I);

  size_t getFunctionCallWeight(const llvm::CallInst &Inst);

  virtual size_t getOriginalBlockWeight() const;

  /// \param InputArgs amount of input arguments
  /// \param OutputArgs amount of output armugents
  /// \return approximate amount of instructions for calling the function.
  /// Function also consider storing and retrieving function arguments
  virtual size_t getNewBlockWeight(const size_t InputArgs,
                                   const size_t OutputArgs) const;
  virtual size_t getFunctionCreationWeight(const size_t InputArgs,
                                           const size_t OutputArgs) const;

protected:
  const llvm::TargetTransformInfo *TTI;
  size_t BlockWeight;
};

#endif // LLVMTRANSFORM_WEIGHT_H
