#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  void printFunctionBlocks(const Function& function)
  {
    if (function.isDeclaration())
      return;
    errs() << "Function: ";
    errs().write_escaped(function.getName());
    errs() << '\n';
    int i = 0;
    for (auto & BB: function.getBasicBlockList())
    {
      errs() << "Block " << i++ << "\n";
      BB.print(errs());
      errs() << '\n';
    }
  }


  struct Factoring : public ModulePass {
    static char ID;

    Factoring() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      errs() << "Module name: ";
      errs().write_escaped(M.getName()) << '\n';
      for (Function &F : M.functions()) {
        printFunctionBlocks(F);
      }
      return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      //AU.setPreservesCFG();
      //AU.addRequired<LoopInfoWrapperPass>();
      // preserve nothing
    }
  };
}

char Factoring::ID = 0;
static RegisterPass<Factoring> X("factor", "Factoring Pass", false, false);
