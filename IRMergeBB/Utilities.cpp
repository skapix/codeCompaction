//
// Created by skap on 4/28/17.
//

#include "Utilities.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Module.h"

using namespace llvm;
using namespace utilities;
std::string FunctionNameCreator::getName() {
    static const std::string Prefix = "MergeBB_unnamed_";
    std::string SlotStr = Prefix + utostr(Slot);

    while (1) {
      if (M.getValueSymbolTable().lookup(SlotStr) == nullptr)
        return SlotStr;
      SlotStr = Prefix + utostr(++Slot);
    }
}

BasicBlock *llvm::utilities::getMappedBBofIdenticalFunctions(const BasicBlock *BBToMap, Function *F) {
  const Function *FOut = BBToMap->getParent();
  auto FInIt = F->begin();
  for (auto FIt = FOut->begin(), FEIt = FOut->end();
       FIt != FEIt; ++FIt, ++FInIt) {
    if (&*FIt == BBToMap)
      return &*FInIt;
  }
  llvm_unreachable("Can't find Basic block in it's own parent");
}