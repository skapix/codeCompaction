#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
	struct Factoring : public ModulePass {
		static char ID;
		Factoring() : ModulePass(ID) {}

		bool runOnModule(Module &M) override {
			errs() << "Hello, factor: ";
			errs().write_escaped(M.getName()) << '\n';
			return false;
		}
	};
}

char Factoring::ID = 0;
static RegisterPass<Factoring> X("factor", "Factoring Pass", false, false);