//===-- TTIPAC.h - TTI Procedural Abstraction Cost-------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains PAC implementation, that relies on TargetTransformInfo
/// interface.
///
//===----------------------------------------------------------------------===//

#ifndef LLVMTRANSFORM_TTIPAC_H
#define LLVMTRANSFORM_TTIPAC_H

#include "TargetDependent/CommonPAC.h"

class TTIPAC : public CommonPAC {
public:
  TTIPAC(const int Preponderance) : CommonPAC(Preponderance) {}
  virtual void init(const llvm::TargetTransformInfo &TTI,
                    const InstructionLocation &I,
                    const llvm::BasicBlock::const_iterator &Begin,
                    const llvm::BasicBlock::const_iterator &End) override;
  virtual void setTail(const bool) override {};
  virtual ~TTIPAC() {}
};

#endif //LLVMTRANSFORM_TTIPAC_H
