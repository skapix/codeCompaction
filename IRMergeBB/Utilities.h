#ifndef LLVMTRANSFORM_UTILITIES_H
#define LLVMTRANSFORM_UTILITIES_H
#include "CompareBB.h"

namespace llvm {
namespace utilities {
/// Auxiliary class, that holds basic block and it's hash.
/// Used BB for comparison
class BBNode {
  mutable llvm::BasicBlock *BB;
  BBComparator::BasicBlockHash Hash;

public:
  // Note the hash is recalculated potentially multiple times, but it is cheap.
  BBNode(BasicBlock *BB) : BB(BB), Hash(BBComparator::basicBlockHash(*BB)) {}

  BasicBlock *getBB() const { return BB; }

  BBComparator::BasicBlockHash getHash() const { return Hash; }
};


/// Comparator for BBNode
class BBNodeCmp {
  enum
  {
    ArraySize = 3
  };
  using BaseHashElem = std::tuple<uintptr_t, uintptr_t, int>;
  mutable struct SmallHashMap {
    BaseHashElem Elems[ArraySize];
    SmallHashMap() : Elems({std::make_tuple(0, 0, 0)}) {};
    size_t idx = 0;

    void push_back(uintptr_t V1, uintptr_t V2, int R) {
      if (V1 > V2)
        Elems[idx] = std::make_tuple(V2, V1, -1 * R);
      else
        Elems[idx] = std::make_tuple(V1, V2, R);
      idx = (idx + 1) % ArraySize;
    }

    Optional<int> getResult(uintptr_t V1, uintptr_t V2) const {
      int Mult = 1;
      if (V1 > V2) {
        std::swap(V1, V2);
        Mult = -1;
      }

      for (size_t i = 0; i < ArraySize; ++i) {
        if (std::get<0>(Elems[i]) == V1 && std::get<1>(Elems[i]) == V2)
          return std::get<2>(Elems[i]) * Mult;
      }
      return Optional<int>();
    }

  } LastHasher;

  BBComparator BBCmp;
public:
  BBNodeCmp(GlobalNumberState *GN, const DataLayout &DL) : BBCmp(GN, DL) {}

  bool operator()(const BBNode &LHS, const BBNode &RHS) const {
    // Order first by hashes, then full function comparison.

    if (LHS.getHash() != RHS.getHash())
      return LHS.getHash() < RHS.getHash();
    Optional<int> Hashed = LastHasher.getResult(reinterpret_cast<uintptr_t >(LHS.getBB()),
                                                reinterpret_cast<uintptr_t >(RHS.getBB()));

    if (Hashed)
      return *Hashed == -1;

    int Result = BBCmp.compare(LHS.getBB(), RHS.getBB());
    LastHasher.push_back(reinterpret_cast<uintptr_t >(LHS.getBB()),
                         reinterpret_cast<uintptr_t >(RHS.getBB()), Result);

    return Result == -1;
  }
};
/// Give unique name to function
class FunctionNameCreator {
public:
  FunctionNameCreator(const Module &M) : M(M), Slot(0)
  {}
  std::string getName();

private:
  const Module &M;
  uint64_t Slot;
};



////////// Location of merged instructions //////////

/// Class intends to handle special instruction indicies that are not going to
/// be factored out
class InstructionLocation {
public:
  enum class Type : char {
    Usual,
    CopyBefore,
    MoveBefore,
    CopyAfter,
    MoveAfter
  };

  void push_back(const Type T) { SpecialInsts.push_back(T); }

  bool isUsual(const size_t Id) const {
    return SpecialInsts[Id] == Type::Usual;
  }

  bool isUsedBeforeFunction(const size_t Id) const {
    return SpecialInsts[Id] == Type::CopyBefore ||
           SpecialInsts[Id] == Type::MoveBefore;
  }

  bool isUsedInsideFunction(const size_t Id) const {
    return SpecialInsts[Id] == Type::Usual ||
           SpecialInsts[Id] == Type::CopyBefore ||
           SpecialInsts[Id] == Type::CopyAfter;
  }

  bool isUsedAfterFunction(const size_t Id) const {
    return SpecialInsts[Id] == Type::CopyAfter ||
           SpecialInsts[Id] == Type::MoveAfter;
  }

  bool isUsedOutsideFunction(const size_t Id) const {
    return !isUsual(Id);
  }

  Type operator[](const size_t i) const { return SpecialInsts[i]; }

  Type &operator[](const size_t i) { return SpecialInsts[i]; }

  size_t amountInsts() const { return SpecialInsts.size(); }

private:
  SmallVector<Type, 64> SpecialInsts;
};


/// Class is used to find values in sorted array.
/// Values, that should be found must be ordered.
/// Class is useful when we need to traverse all
/// BB insts [getBeginIt, getEndIt) and find particular instructions
template <typename T> class SmartSortedSet {
public:
  SmartSortedSet() { resetIt(); }

  SmartSortedSet(const SmallVectorImpl<T> &Other) : Values(Other) { resetIt(); }
  SmartSortedSet(SmallVectorImpl<T> &&Other) : Values(std::move(Other)) {
    resetIt();
  }

  SmartSortedSet &operator=(const SmartSortedSet &Other) {
    Values = Other.Values;
    resetIt();
    return *this;
  }
  SmartSortedSet &operator=(SmartSortedSet &&Other) {
    Values = std::move(Other.Values);
    resetIt();
    return *this;
  }

  void checkBegin() const {
    assert(Cur == Values.begin() &&
           "Cur should point to the beginning of the array");
  }

  void resetIt() const { Cur = Values.begin(); }
  const SmallVectorImpl<T> &get() const { return Values; }

  /// \param InstId number of instruction
  /// \return true if \p InstId is number of instruction
  /// should be skipped from factoring out
  bool contains(T InstId) const;

private:
  /// Ascendingly sorted vector of instruction numbers
  SmallVector<T, 8> Values;
  mutable typename SmallVectorImpl<T>::const_iterator Cur;
};

template <typename T> bool SmartSortedSet<T>::contains(T InstId) const {
  if (Cur == Values.end()) // no skipped elements
    return false;
  if (*Cur != InstId)
    return false;
  Cur = (Cur == std::prev(Values.end())) ? Values.begin() : Cur + 1;
  return true;
}

BasicBlock *getMappedBBofIdenticalFunctions(const BasicBlock *BBToMap,
                                            Function *F);

} // namespace utilities
} // namespace llvm



#endif //LLVMTRANSFORM_UTILITIES_H
