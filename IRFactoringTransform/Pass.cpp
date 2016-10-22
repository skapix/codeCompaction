#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include <forward_list>

#define DEBUG_TYPE "factor"

// TODO: tests tests tests
// TODO: llvm code style: http://llvm.org/docs/CodingStandards.html
// TODO: high-level optimize: use identicalBB as ref

// TODO: next step: compare equality of base blocks. Instruction args should be mapped.


using namespace llvm;

namespace {
  const unsigned g_svBB = 10;

  // TODO: set different upcodes for 15 most frequently used instructions and rest 1 for all the others.
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
    assert(!F.isDeclaration());
    for (auto it = F.begin(), endIt = std::prev(F.end()); it != endIt; ++it)
    {
      appendBlock(&*it);
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

  bool equivalentInstructions(const Instruction& i1, const Instruction& i2, DenseMap<Value*, Value*>& b2ValuesToB1)
  {
    if (i1.getOpcode() != i2.getOpcode() ||
        i1.getNumOperands() != i2.getNumOperands())
    {
      return false;
    }
    // compare operands
    return std::equal(i1.op_begin(), i1.op_end(), i2.op_begin(), [&b2ValuesToB1](const Use& u1, const Use& u2)
    {
      if  (u1.get()->getType() != u2.get()->getType())
      {
        return false;
      }
      if (auto C1 = dyn_cast<const Constant>(u1))
      {
        if (auto C2 = dyn_cast<const Constant>(u2))
        {
          return C1 == C2;
        }
        return false;
      }

      if (b2ValuesToB1.count(u1.get()) == 0)
      {
        b2ValuesToB1.insert(std::make_pair(u2.get(), u1.get()));
        return true;
      }
      return b2ValuesToB1[u2.get()] == u1.get();
    });

  }

  // block sizes should be equal
  bool areBlocksEqual(const BasicBlock& b1, const BasicBlock& b2)
  {
    DenseMap<Value*, Value*> b2ValuesTobB1;
    auto it1 = b1.begin(), it2 = b2.begin(), eit1 = std::prev(b1.end()), eit2 = std::prev(b2.end());

    for (; it1 != eit1 && it2 != eit2; ++it1, ++it2)
    {
      if (!equivalentInstructions(*it1, *it2, b2ValuesTobB1))
        return false;
    }

    return it1 == eit1 && it2 == eit2;
  }

  std::forward_list<SmallVector<const BasicBlock*, g_svBB>> getEqualVectorOfBlocks(const SmallVectorImpl<const BasicBlock*> &almostEqual)
  {
    std::forward_list<SmallVector<const BasicBlock*, g_svBB>> result;
    std::vector<bool> gotEqual(almostEqual.size(), false);
    for (int i = 0; i < almostEqual.size()-1; ++i)
    {
      if (gotEqual[i])
        continue;
      result.push_front(SmallVector<const BasicBlock*, g_svBB>(1, almostEqual[i]));
      for (int j = i+1; j < almostEqual.size(); ++j)
      {
        if (gotEqual[j])
          continue;
        if (areBlocksEqual(*almostEqual[i], *almostEqual[j]))
        {
          result.front().push_back(almostEqual[j]);
          gotEqual[j] = true;
        }
      }
    }
    return result;
  }


  struct Factoring : public ModulePass {
    static char ID;

    Factoring() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      DEBUG(errs() << "Module name: ");
      DEBUG(errs().write_escaped(M.getName()) << '\n');

      CommonPatterns identicalBBs;

      // calculate fingerprints for all basic blocks in every function
      for (auto &F : M.functions()) {
        if (F.isDeclaration())
          continue;
        identicalBBs.appendFunction(F);
      }

      auto commonBBs = identicalBBs.getIdenticalBB();
      for (auto it : commonBBs)
      {
        errs() << "Code: " << it.first << '\n';
        errs() << "Number of blocks: " << it.second.size() << '\n';

        if (it.second.size() == 1)
          continue;
        auto listOfEqualBlocks = getEqualVectorOfBlocks(it.second);
        int i = 0;
        for (auto itBB : listOfEqualBlocks)
        {
          errs() << '[' << i++ << "] Amount of equal with the same fingerpring = " << itBB.size() << '\n';
          errs() << "# of instructions = " << itBB.front()->size() << '\n';
        }


        //printBlocks(it.second);
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