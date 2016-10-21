#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include "llvm/Support/raw_ostream.h"

// TODO: llvm code style: http://llvm.org/docs/CodingStandards.html
// TODO: high-level optimize: use identicalBB as ref


// TODO: don't take into account the last instruction, which is either br or ret
// TODO: next step: compare equality of base blocks. Instruction args should be mapped.


using namespace llvm;

namespace {
  const unsigned g_svBB = 10;

  inline uint64_t instructionTo4BitCode(const Instruction& instruction)
  {
    return instruction.getOpcode() % 0xF;
  }

  uint64_t calculateFingerprint(const BasicBlock& BB)
  {
    uint64_t result = 0;
    int i = 0;
    for (auto& Instr: BB.getInstList())
    {
      if (i == 64)
        break;
      uint64_t upcode = instructionTo4BitCode(Instr);
      result |= (upcode << i);
      i += 4;
    }
    return result;
  }

  class CommonPatterns
  {
  public:
    CommonPatterns() = default;

    void appendFunction(const Function& F);
    void appendBlock(const BasicBlock* BB);

    DenseMap<uint64_t,  SmallVector<const BasicBlock*, g_svBB>> getIdenticalBB() const {return identicalBB; }

  private:
    // fingerprint to BasicBlock
    DenseMap<uint64_t,  SmallVector<const BasicBlock*, g_svBB>> identicalBB;
  };

  void CommonPatterns::appendBlock(const BasicBlock *BB) {
    identicalBB[calculateFingerprint(*BB)].push_back(BB);
  }

  void CommonPatterns::appendFunction(const Function &F) {
    for (auto& BB : F.getBasicBlockList())
    {
      appendBlock(&BB);
    }
  }

  void printUses(const BasicBlock& block)
  {
    if (!block.hasNUsesOrMore(1))
    {
      errs() << "No Uses\n";
      return;
    }
    errs() << "Uses:\n";
    int i = 0;
    for (auto& U : block.uses())
    {
      errs() << "Use " << i++ << "\n";
      U.getUser()->print(errs());
      errs() << '\n';
    }

  }

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
      printUses(BB);
      errs() << "Block " << i++ << "\n";
      BB.print(errs());
      errs() << '\n';
    }
  }

  void printBlocks(const SmallVector<const BasicBlock*, g_svBB>& BBVector)
  {
    int i = 0;
    for (auto& BB: BBVector)
    {
      errs() << "Block " << i++ << "\n";
      BB->print(errs());
      errs() << '\n';
    }
  }

  struct Factoring : public ModulePass {
    static char ID;

    Factoring() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      errs() << "Module name: ";
      errs().write_escaped(M.getName()) << '\n';

      CommonPatterns identicalBBs;

      // calculate fingerprints for all basic blocks in every function
      for (auto &F : M.functions()) {
        if (F.isDeclaration())
          continue;
        identicalBBs.appendFunction(F);

        //printFunctionBlocks(F);
      }

      auto commonBBs = identicalBBs.getIdenticalBB();
      errs() << commonBBs.size() << '\n';
      for (auto it : commonBBs)
      {
        errs() << "Code: " << it.first << '\n';
        errs() << "Number of blocks: " << it.second.size() << '\n';

        if (it.second.size() == 1)
          continue;
        printBlocks(it.second);
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


//bool Instruction::isUsedOutsideOfBlock