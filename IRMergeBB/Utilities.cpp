//
// Created by skap on 4/28/17.
//

#include "Utilities.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Object/ObjectFile.h"
#include <llvm/Object/SymbolSize.h>

namespace llvm {
namespace utilities {

std::string FunctionNameCreator::getName() {
  static const std::string Prefix = "MergeBB_unnamed_";
  std::string SlotStr = Prefix + utostr(Slot);

  while (1) {
    if (M.getValueSymbolTable().lookup(SlotStr) == nullptr)
      return SlotStr;
    SlotStr = Prefix + utostr(++Slot);
  }
}

BasicBlock *getMappedBBofIdenticalFunctions(const BasicBlock *BBToMap,
                                            Function *F) {
  const Function *FOut = BBToMap->getParent();
  auto FInIt = F->begin();
  for (auto FIt = FOut->begin(), FEIt = FOut->end(); FIt != FEIt;
       ++FIt, ++FInIt) {
    if (&*FIt == BBToMap)
      return &*FInIt;
  }
  llvm_unreachable("Can't find Basic block in it's own parent");
}

SmallVector<size_t, 8> getFunctionSizes(const object::ObjectFile &Obj,
                                        const SmallVectorImpl<StringRef> &Fs) {

  auto VecNameSize = object::computeSymbolSizes(Obj);
  llvm::SmallVector<size_t, 8> Result(Fs.size(), 0);

  for (auto &I : VecNameSize) {
    if (!I.second)
      continue;

    auto ExpType = I.first.getType();
    if (!ExpType) {
      consumeError(ExpType.takeError());
      continue;
    }

    if (*ExpType != object::SymbolRef::ST_Function)
      continue;

    auto ExpName = I.first.getName();
    if (!ExpName) {
      consumeError(ExpName.takeError());
      continue;
    }

    StringRef Name = *ExpName;
    size_t Id =
        find_if(Fs, [Name](StringRef FN) { return FN == Name; }) - Fs.begin();
    if (Id != Fs.size())
      Result[Id] = I.second;
  }

  assert(none_of(Result, [](size_t P) { return P == 0; }) &&
         "Some functions are not presented in module");
  return Result;
}

size_t getEHSize(const object::ObjectFile &F) {
  for (auto S : F.sections()) {
    StringRef SR;
    if (S.getName(SR))
      continue;
    if (SR == ".eh_frame")
      return S.getSize();
  }
  return 0;
}

} // namespace utilities
} // namespace llvm