#include "storm/storage/dd/cudd/InternalCuddBdd.h"

#include <boost/functional/hash.hpp>

#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/PairHash.h"
#include "storm/storage/dd/Odd.h"
#include "storm/storage/dd/cudd/InternalCuddDdManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_CUDD
InternalBdd<DdType::CUDD>::InternalBdd(InternalDdManager<DdType::CUDD> const* ddManager, cudd::BDD cuddBdd) : ddManager(ddManager), cuddBdd(cuddBdd) {
    // Intentionally left empty.
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::fromVector(InternalDdManager<DdType::CUDD> const* ddManager, Odd const& odd,
                                                                std::vector<uint_fast64_t> const& sortedDdVariableIndices,
                                                                std::function<bool(uint64_t)> const& filter) {
    uint_fast64_t offset = 0;
    return InternalBdd<DdType::CUDD>(
        ddManager, cudd::BDD(ddManager->getCuddManager(), fromVectorRec(ddManager->getCuddManager().getManager(), offset, 0, sortedDdVariableIndices.size(),
                                                                        odd, sortedDdVariableIndices, filter)));
}

bool InternalBdd<DdType::CUDD>::operator==(InternalBdd<DdType::CUDD> const& other) const {
    return this->getCuddBdd() == other.getCuddBdd();
}

bool InternalBdd<DdType::CUDD>::operator!=(InternalBdd<DdType::CUDD> const& other) const {
    return !(*this == other);
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::relationalProduct(InternalBdd<DdType::CUDD> const& relation,
                                                                       std::vector<InternalBdd<DdType::CUDD>> const& rowVariables,
                                                                       std::vector<InternalBdd<DdType::CUDD>> const& columnVariables) const {
    InternalBdd<DdType::CUDD> cube = ddManager->getBddOne();
    for (auto const& variable : rowVariables) {
        cube &= variable;
    }

    InternalBdd<DdType::CUDD> result = this->andExists(relation, cube);
    result = result.swapVariables(rowVariables, columnVariables);
    return result;
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::inverseRelationalProduct(InternalBdd<DdType::CUDD> const& relation,
                                                                              std::vector<InternalBdd<DdType::CUDD>> const& rowVariables,
                                                                              std::vector<InternalBdd<DdType::CUDD>> const& columnVariables) const {
    InternalBdd<DdType::CUDD> cube = ddManager->getBddOne();
    for (auto const& variable : columnVariables) {
        cube &= variable;
    }

    InternalBdd<DdType::CUDD> result = this->swapVariables(rowVariables, columnVariables).andExists(relation, cube);
    return result;
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::inverseRelationalProductWithExtendedRelation(
    InternalBdd<DdType::CUDD> const& relation, std::vector<InternalBdd<DdType::CUDD>> const& rowVariables,
    std::vector<InternalBdd<DdType::CUDD>> const& columnVariables) const {
    return this->inverseRelationalProduct(relation, rowVariables, columnVariables);
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::ite(InternalBdd<DdType::CUDD> const& thenDd, InternalBdd<DdType::CUDD> const& elseDd) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().Ite(thenDd.getCuddBdd(), elseDd.getCuddBdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalBdd<DdType::CUDD>::ite(InternalAdd<DdType::CUDD, ValueType> const& thenAdd,
                                                                    InternalAdd<DdType::CUDD, ValueType> const& elseAdd) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddBdd().Add().Ite(thenAdd.getCuddAdd(), elseAdd.getCuddAdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::operator||(InternalBdd<DdType::CUDD> const& other) const {
    InternalBdd<DdType::CUDD> result(*this);
    result |= other;
    return result;
}

InternalBdd<DdType::CUDD>& InternalBdd<DdType::CUDD>::operator|=(InternalBdd<DdType::CUDD> const& other) {
    this->cuddBdd = this->getCuddBdd() | other.getCuddBdd();
    return *this;
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::operator&&(InternalBdd<DdType::CUDD> const& other) const {
    InternalBdd<DdType::CUDD> result(*this);
    result &= other;
    return result;
}

InternalBdd<DdType::CUDD>& InternalBdd<DdType::CUDD>::operator&=(InternalBdd<DdType::CUDD> const& other) {
    this->cuddBdd = this->getCuddBdd() & other.getCuddBdd();
    return *this;
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::iff(InternalBdd<DdType::CUDD> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().Xnor(other.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::exclusiveOr(InternalBdd<DdType::CUDD> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().Xor(other.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::implies(InternalBdd<DdType::CUDD> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().Ite(other.getCuddBdd(), ddManager->getBddOne().getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::operator!() const {
    InternalBdd<DdType::CUDD> result(*this);
    result.complement();
    return result;
}

InternalBdd<DdType::CUDD>& InternalBdd<DdType::CUDD>::complement() {
    this->cuddBdd = ~this->getCuddBdd();
    return *this;
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::existsAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().ExistAbstract(cube.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::existsAbstractRepresentative(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().ExistAbstractRepresentative(cube.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::universalAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().UnivAbstract(cube.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::andExists(InternalBdd<DdType::CUDD> const& other, InternalBdd<DdType::CUDD> const& cube) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().AndAbstract(other.getCuddBdd(), cube.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::constrain(InternalBdd<DdType::CUDD> const& constraint) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().Constrain(constraint.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::restrict(InternalBdd<DdType::CUDD> const& constraint) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().Restrict(constraint.getCuddBdd()));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::swapVariables(std::vector<InternalBdd<DdType::CUDD>> const& from,
                                                                   std::vector<InternalBdd<DdType::CUDD>> const& to) const {
    std::vector<cudd::BDD> fromBdd;
    std::vector<cudd::BDD> toBdd;
    for (auto it1 = from.begin(), ite1 = from.end(), it2 = to.begin(); it1 != ite1; ++it1, ++it2) {
        fromBdd.push_back(it1->getCuddBdd());
        toBdd.push_back(it2->getCuddBdd());
    }
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().SwapVariables(fromBdd, toBdd));
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::getSupport() const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddBdd().Support());
}

uint_fast64_t InternalBdd<DdType::CUDD>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    // If the number of DD variables is zero, CUDD returns a number greater 0 for constant nodes different from
    // zero, which is not the behaviour we expect.
    if (numberOfDdVariables == 0) {
        return 0;
    }
    return static_cast<uint_fast64_t>(this->getCuddBdd().CountMinterm(static_cast<int>(numberOfDdVariables)));
}

uint_fast64_t InternalBdd<DdType::CUDD>::getLeafCount() const {
    return static_cast<uint_fast64_t>(this->getCuddBdd().CountLeaves());
}

uint_fast64_t InternalBdd<DdType::CUDD>::getNodeCount() const {
    return static_cast<uint_fast64_t>(this->getCuddBdd().nodeCount());
}

bool InternalBdd<DdType::CUDD>::isOne() const {
    return this->getCuddBdd().IsOne();
}

bool InternalBdd<DdType::CUDD>::isZero() const {
    return this->getCuddBdd().IsZero();
}

uint_fast64_t InternalBdd<DdType::CUDD>::getIndex() const {
    return static_cast<uint_fast64_t>(this->getCuddBdd().NodeReadIndex());
}

uint_fast64_t InternalBdd<DdType::CUDD>::getLevel() const {
    return static_cast<uint_fast64_t>(ddManager->getCuddManager().ReadPerm(this->getIndex()));
}

void InternalBdd<DdType::CUDD>::exportToDot(std::string const& filename, std::vector<std::string> const& ddVariableNamesAsStrings,
                                            bool showVariablesIfPossible) const {
    // Build the name input of the DD.
    std::vector<char*> ddNames;
    std::string ddName("f");
    ddNames.push_back(new char[ddName.size() + 1]);
    std::copy(ddName.c_str(), ddName.c_str() + 2, ddNames.back());

    // Now build the variables names.
    std::vector<char*> ddVariableNames;
    for (auto const& element : ddVariableNamesAsStrings) {
        ddVariableNames.push_back(new char[element.size() + 1]);
        std::copy(element.c_str(), element.c_str() + element.size() + 1, ddVariableNames.back());
    }

    // Open the file, dump the DD and close it again.
    std::vector<cudd::BDD> cuddBddVector = {this->getCuddBdd()};
    FILE* filePointer = fopen(filename.c_str(), "a+");
    if (showVariablesIfPossible) {
        ddManager->getCuddManager().DumpDot(cuddBddVector, ddVariableNames.data(), &ddNames[0], filePointer);
    } else {
        ddManager->getCuddManager().DumpDot(cuddBddVector, nullptr, &ddNames[0], filePointer);
    }
    fclose(filePointer);

    // Finally, delete the names.
    for (char* element : ddNames) {
        delete element;
    }
    for (char* element : ddVariableNames) {
        delete element;
    }
}

void InternalBdd<DdType::CUDD>::exportToText(std::string const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

cudd::BDD InternalBdd<DdType::CUDD>::getCuddBdd() const {
    return this->cuddBdd;
}

DdNode* InternalBdd<DdType::CUDD>::getCuddDdNode() const {
    return this->getCuddBdd().getNode();
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalBdd<DdType::CUDD>::toAdd() const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddBdd().Add());
}

DdNode* InternalBdd<DdType::CUDD>::fromVectorRec(::DdManager* manager, uint_fast64_t& currentOffset, uint_fast64_t currentLevel, uint_fast64_t maxLevel,
                                                 Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                 std::function<bool(uint64_t)> const& filter) {
    if (currentLevel == maxLevel) {
        // If we are in a terminal node of the ODD, we need to check whether the then-offset of the ODD is one
        // (meaning the encoding is a valid one) or zero (meaning the encoding is not valid). Consequently, we
        // need to copy the next value of the vector iff the then-offset is greater than zero.
        if (odd.getThenOffset() > 0) {
            if (filter(currentOffset++)) {
                return Cudd_ReadOne(manager);
            } else {
                return Cudd_ReadLogicZero(manager);
            }
        } else {
            return Cudd_ReadZero(manager);
        }
    } else {
        // If the total offset is zero, we can just return the constant zero DD.
        if (odd.getThenOffset() + odd.getElseOffset() == 0) {
            return Cudd_ReadZero(manager);
        }

        // Determine the new else-successor.
        DdNode* elseSuccessor = nullptr;
        if (odd.getElseOffset() > 0) {
            elseSuccessor = fromVectorRec(manager, currentOffset, currentLevel + 1, maxLevel, odd.getElseSuccessor(), ddVariableIndices, filter);
        } else {
            elseSuccessor = Cudd_ReadLogicZero(manager);
        }
        Cudd_Ref(elseSuccessor);

        // Determine the new then-successor.
        DdNode* thenSuccessor = nullptr;
        if (odd.getThenOffset() > 0) {
            thenSuccessor = fromVectorRec(manager, currentOffset, currentLevel + 1, maxLevel, odd.getThenSuccessor(), ddVariableIndices, filter);
        } else {
            thenSuccessor = Cudd_ReadLogicZero(manager);
        }
        Cudd_Ref(thenSuccessor);

        // Create a node representing ITE(currentVar, thenSuccessor, elseSuccessor);
        DdNode* currentVar = Cudd_bddIthVar(manager, static_cast<int>(ddVariableIndices[currentLevel]));
        Cudd_Ref(currentVar);
        DdNode* result = Cudd_bddIte(manager, currentVar, thenSuccessor, elseSuccessor);
        Cudd_Ref(result);

        // Dispose of the intermediate results
        Cudd_RecursiveDeref(manager, currentVar);
        Cudd_RecursiveDeref(manager, thenSuccessor);
        Cudd_RecursiveDeref(manager, elseSuccessor);

        // Before returning, we remove the protection imposed by the previous call to Cudd_Ref.
        Cudd_Deref(result);

        return result;
    }
}

storm::storage::BitVector InternalBdd<DdType::CUDD>::toVector(storm::dd::Odd const& rowOdd, std::vector<uint_fast64_t> const& ddVariableIndices) const {
    storm::storage::BitVector result(rowOdd.getTotalOffset());
    this->toVectorRec(Cudd_Regular(this->getCuddDdNode()), ddManager->getCuddManager(), result, rowOdd, Cudd_IsComplement(this->getCuddDdNode()), 0,
                      ddVariableIndices.size(), 0, ddVariableIndices);
    return result;
}

void InternalBdd<DdType::CUDD>::toVectorRec(DdNode const* dd, cudd::Cudd const& manager, storm::storage::BitVector& result, Odd const& rowOdd, bool complement,
                                            uint_fast64_t currentRowLevel, uint_fast64_t maxLevel, uint_fast64_t currentRowOffset,
                                            std::vector<uint_fast64_t> const& ddRowVariableIndices) const {
    // If there are no more values to select, we can directly return.
    if (dd == Cudd_ReadLogicZero(manager.getManager()) && !complement) {
        return;
    } else if (dd == Cudd_ReadOne(manager.getManager()) && complement) {
        return;
    }

    // If we are at the maximal level, the value to be set is stored as a constant in the DD.
    if (currentRowLevel == maxLevel) {
        result.set(currentRowOffset, true);
    } else if (ddRowVariableIndices[currentRowLevel] < Cudd_NodeReadIndex(dd)) {
        toVectorRec(dd, manager, result, rowOdd.getElseSuccessor(), complement, currentRowLevel + 1, maxLevel, currentRowOffset, ddRowVariableIndices);
        toVectorRec(dd, manager, result, rowOdd.getThenSuccessor(), complement, currentRowLevel + 1, maxLevel, currentRowOffset + rowOdd.getElseOffset(),
                    ddRowVariableIndices);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        DdNode const* elseDdNode = Cudd_E_const(dd);
        DdNode const* thenDdNode = Cudd_T_const(dd);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = Cudd_IsComplement(elseDdNode) ^ complement;
        bool thenComplemented = Cudd_IsComplement(thenDdNode) ^ complement;

        toVectorRec(Cudd_Regular(elseDdNode), manager, result, rowOdd.getElseSuccessor(), elseComplemented, currentRowLevel + 1, maxLevel, currentRowOffset,
                    ddRowVariableIndices);
        toVectorRec(Cudd_Regular(thenDdNode), manager, result, rowOdd.getThenSuccessor(), thenComplemented, currentRowLevel + 1, maxLevel,
                    currentRowOffset + rowOdd.getElseOffset(), ddRowVariableIndices);
    }
}

Odd InternalBdd<DdType::CUDD>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    // Prepare a unique table for each level that keeps the constructed ODD nodes unique.
    std::vector<std::unordered_map<DdNode const*, std::shared_ptr<Odd>>> uniqueTableForLevels(ddVariableIndices.size() + 1);

    // Now construct the ODD structure from the BDD.
    std::shared_ptr<Odd> rootOdd =
        createOddRec(this->getCuddDdNode(), ddManager->getCuddManager(), 0, ddVariableIndices.size(), ddVariableIndices, uniqueTableForLevels);

    // Return a copy of the root node to remove the shared_ptr encapsulation.
    return Odd(*rootOdd);
}

std::size_t InternalBdd<DdType::CUDD>::HashFunctor::operator()(std::pair<DdNode const*, bool> const& key) const {
    std::size_t result = 0;
    boost::hash_combine(result, key.first);
    boost::hash_combine(result, key.second);
    return result;
}

std::shared_ptr<Odd> InternalBdd<DdType::CUDD>::createOddRec(DdNode const* dd, cudd::Cudd const& manager, uint_fast64_t currentLevel, uint_fast64_t maxLevel,
                                                             std::vector<uint_fast64_t> const& ddVariableIndices,
                                                             std::vector<std::unordered_map<DdNode const*, std::shared_ptr<Odd>>>& uniqueTableForLevels) {
    // Check whether the ODD for this node has already been computed (for this level) and if so, return this instead.
    auto it = uniqueTableForLevels[currentLevel].find(dd);
    if (it != uniqueTableForLevels[currentLevel].end()) {
        return it->second;
    } else {
        // Otherwise, we need to recursively compute the ODD.

        // If we are already at the maximal level that is to be considered, we can simply create an Odd without
        // successors
        if (currentLevel == maxLevel) {
            auto oddNode = std::make_shared<Odd>(nullptr, 0, nullptr, dd != Cudd_ReadLogicZero(manager.getManager()) ? 1 : 0);
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        } else if (ddVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
            // If we skipped the level in the DD, we compute the ODD just for the else-successor and use the same
            // node for the then-successor as well.
            std::shared_ptr<Odd> elseNode = createOddRec(dd, manager, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode = elseNode;

            auto oddNode = std::make_shared<Odd>(elseNode, elseNode->getTotalOffset(), thenNode, elseNode->getTotalOffset());
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        } else {
            // Otherwise, we compute the ODDs for both the then- and else successors.
            DdNode const* thenDdNode = Cudd_T_const(dd);
            DdNode const* elseDdNode = Cudd_E_const(dd);

            if (Cudd_IsComplement(dd)) {
                thenDdNode = Cudd_Not(thenDdNode);
                elseDdNode = Cudd_Not(elseDdNode);
            }

            std::shared_ptr<Odd> elseNode = createOddRec(elseDdNode, manager, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode = createOddRec(thenDdNode, manager, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);

            auto oddNode = std::make_shared<Odd>(elseNode, elseNode->getTotalOffset(), thenNode, thenNode->getTotalOffset());
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        }
    }
}

template<typename ValueType>
void InternalBdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                     std::vector<ValueType> const& sourceValues, std::vector<ValueType>& targetValues) const {
    uint_fast64_t currentIndex = 0;
    filterExplicitVectorRec(Cudd_Regular(this->getCuddDdNode()), ddManager->getCuddManager(), 0, Cudd_IsComplement(this->getCuddDdNode()),
                            ddVariableIndices.size(), ddVariableIndices, 0, odd, targetValues, currentIndex, sourceValues);
}

template<typename ValueType>
void InternalBdd<DdType::CUDD>::filterExplicitVectorRec(DdNode const* dd, cudd::Cudd const& manager, uint_fast64_t currentLevel, bool complement,
                                                        uint_fast64_t maxLevel, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                        uint_fast64_t currentOffset, storm::dd::Odd const& odd, std::vector<ValueType>& result,
                                                        uint_fast64_t& currentIndex, std::vector<ValueType> const& values) {
    // If there are no more values to select, we can directly return.
    if (dd == Cudd_ReadLogicZero(manager.getManager()) && !complement) {
        return;
    } else if (dd == Cudd_ReadOne(manager.getManager()) && complement) {
        return;
    }

    if (currentLevel == maxLevel) {
        result[currentIndex++] = values[currentOffset];
    } else if (ddVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
        // If we skipped a level, we need to enumerate the explicit entries for the case in which the bit is set
        // and for the one in which it is not set.
        filterExplicitVectorRec(dd, manager, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset, odd.getElseSuccessor(), result,
                                currentIndex, values);
        filterExplicitVectorRec(dd, manager, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset + odd.getElseOffset(),
                                odd.getThenSuccessor(), result, currentIndex, values);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        DdNode const* thenDdNode = Cudd_T_const(dd);
        DdNode const* elseDdNode = Cudd_E_const(dd);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = Cudd_IsComplement(elseDdNode) ^ complement;
        bool thenComplemented = Cudd_IsComplement(thenDdNode) ^ complement;

        filterExplicitVectorRec(Cudd_Regular(elseDdNode), manager, currentLevel + 1, elseComplemented, maxLevel, ddVariableIndices, currentOffset,
                                odd.getElseSuccessor(), result, currentIndex, values);
        filterExplicitVectorRec(Cudd_Regular(thenDdNode), manager, currentLevel + 1, thenComplemented, maxLevel, ddVariableIndices,
                                currentOffset + odd.getElseOffset(), odd.getThenSuccessor(), result, currentIndex, values);
    }
}

std::vector<InternalBdd<DdType::CUDD>> InternalBdd<DdType::CUDD>::splitIntoGroups(std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<InternalBdd<DdType::CUDD>> result;
    splitIntoGroupsRec(Cudd_Regular(this->getCuddDdNode()), Cudd_IsComplement(this->getCuddDdNode()), result, ddGroupVariableIndices, 0,
                       ddGroupVariableIndices.size());
    return result;
}

void InternalBdd<DdType::CUDD>::splitIntoGroupsRec(DdNode* dd, bool negated, std::vector<InternalBdd<DdType::CUDD>>& groups,
                                                   std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel,
                                                   uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    if (negated && dd == Cudd_ReadOne(ddManager->getCuddManager().getManager())) {
        return;
    }

    if (currentLevel == maxLevel) {
        groups.push_back(InternalBdd<DdType::CUDD>(ddManager, cudd::BDD(ddManager->getCuddManager(), negated ? Cudd_Complement(dd) : dd)));
    } else if (ddGroupVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
        splitIntoGroupsRec(dd, negated, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(dd, negated, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    } else {
        DdNode* elseNode = Cudd_E(dd);
        DdNode* thenNode = Cudd_T(dd);

        splitIntoGroupsRec(elseNode, negated ^ Cudd_IsComplement(elseNode), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(thenNode, negated ^ Cudd_IsComplement(thenNode), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    }
}

void InternalBdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                     storm::storage::BitVector const& sourceValues, storm::storage::BitVector& targetValues) const {
    uint_fast64_t currentIndex = 0;
    filterExplicitVectorRec(Cudd_Regular(this->getCuddDdNode()), ddManager->getCuddManager(), 0, Cudd_IsComplement(this->getCuddDdNode()),
                            ddVariableIndices.size(), ddVariableIndices, 0, odd, targetValues, currentIndex, sourceValues);
}

void InternalBdd<DdType::CUDD>::filterExplicitVectorRec(DdNode const* dd, cudd::Cudd const& manager, uint_fast64_t currentLevel, bool complement,
                                                        uint_fast64_t maxLevel, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                        uint_fast64_t currentOffset, storm::dd::Odd const& odd, storm::storage::BitVector& result,
                                                        uint_fast64_t& currentIndex, storm::storage::BitVector const& values) {
    // If there are no more values to select, we can directly return.
    if (dd == Cudd_ReadLogicZero(manager.getManager()) && !complement) {
        return;
    } else if (dd == Cudd_ReadOne(manager.getManager()) && complement) {
        return;
    }

    if (currentLevel == maxLevel) {
        result.set(currentIndex++, values.get(currentOffset));
    } else if (ddVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
        // If we skipped a level, we need to enumerate the explicit entries for the case in which the bit is set
        // and for the one in which it is not set.
        filterExplicitVectorRec(dd, manager, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset, odd.getElseSuccessor(), result,
                                currentIndex, values);
        filterExplicitVectorRec(dd, manager, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset + odd.getElseOffset(),
                                odd.getThenSuccessor(), result, currentIndex, values);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        DdNode const* thenDdNode = Cudd_T_const(dd);
        DdNode const* elseDdNode = Cudd_E_const(dd);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = Cudd_IsComplement(elseDdNode) ^ complement;
        bool thenComplemented = Cudd_IsComplement(thenDdNode) ^ complement;

        filterExplicitVectorRec(Cudd_Regular(elseDdNode), manager, currentLevel + 1, elseComplemented, maxLevel, ddVariableIndices, currentOffset,
                                odd.getElseSuccessor(), result, currentIndex, values);
        filterExplicitVectorRec(Cudd_Regular(thenDdNode), manager, currentLevel + 1, thenComplemented, maxLevel, ddVariableIndices,
                                currentOffset + odd.getElseOffset(), odd.getThenSuccessor(), result, currentIndex, values);
    }
}

std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> InternalBdd<DdType::CUDD>::toExpression(
    storm::expressions::ExpressionManager& manager) const {
    std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> result;

    // Create (and maintain) a mapping from the DD nodes to a counter that says the how-many-th node (within the
    // nodes of equal index) the node was.
    std::unordered_map<DdNode const*, uint_fast64_t> nodeToCounterMap;
    std::vector<uint_fast64_t> nextCounterForIndex(ddManager->getNumberOfDdVariables(), 0);
    std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable> countIndexToVariablePair;

    bool negated = Cudd_Regular(this->getCuddDdNode()) != this->getCuddDdNode();

    // Translate from the top node downwards.
    storm::expressions::Variable topVariable = storm::dd::InternalBdd<storm::dd::DdType::CUDD>::toExpressionRec(
        Cudd_Regular(this->getCuddDdNode()), ddManager->getCuddManager(), manager, result.first, result.second, countIndexToVariablePair, nodeToCounterMap,
        nextCounterForIndex);

    // Create the final expression.
    if (negated) {
        result.first.push_back(!topVariable);
    } else {
        result.first.push_back(topVariable);
    }

    return result;
}

storm::expressions::Variable InternalBdd<DdType::CUDD>::toExpressionRec(
    DdNode const* dd, cudd::Cudd const& ddManager, storm::expressions::ExpressionManager& manager, std::vector<storm::expressions::Expression>& expressions,
    std::unordered_map<uint_fast64_t, storm::expressions::Variable>& indexToVariableMap,
    std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable>& countIndexToVariablePair,
    std::unordered_map<DdNode const*, uint_fast64_t>& nodeToCounterMap, std::vector<uint_fast64_t>& nextCounterForIndex) {
    STORM_LOG_ASSERT(dd == Cudd_Regular(dd), "Expected non-negated BDD node.");

    // First, try to look up the current node if it's not a terminal node.
    auto nodeCounterIt = nodeToCounterMap.find(dd);
    if (nodeCounterIt != nodeToCounterMap.end()) {
        // If we have found the node, this means we can look up the counter-index pair and get the corresponding variable.
        auto variableIt = countIndexToVariablePair.find(std::make_pair(nodeCounterIt->second, Cudd_NodeReadIndex(dd)));
        STORM_LOG_ASSERT(variableIt != countIndexToVariablePair.end(), "Unable to find node.");
        return variableIt->second;
    }

    // If the node was not yet encountered, we create a variable and associate it with the appropriate expression.
    storm::expressions::Variable newNodeVariable = manager.declareFreshBooleanVariable();

    // Since we want to reuse the variable whenever possible, we insert the appropriate entries in the hash table.
    if (!Cudd_IsConstant_const(dd)) {
        // If we are dealing with a non-terminal node, we count it as a new node with this index.
        nodeToCounterMap[dd] = nextCounterForIndex[Cudd_NodeReadIndex(dd)];
        countIndexToVariablePair[std::make_pair(nextCounterForIndex[Cudd_NodeReadIndex(dd)], Cudd_NodeReadIndex(dd))] = newNodeVariable;
        ++nextCounterForIndex[Cudd_NodeReadIndex(dd)];
    } else {
        // If it's a terminal node, it is the one leaf and there's no need to keep track of a counter for this level.
        nodeToCounterMap[dd] = 0;
        countIndexToVariablePair[std::make_pair(0, Cudd_NodeReadIndex(dd))] = newNodeVariable;
    }

    // In the terminal case, we can only have a one since we are considering non-negated nodes only.
    if (dd == Cudd_ReadOne(ddManager.getManager())) {
        // Push the expression that enforces that the new variable is true.
        expressions.push_back(storm::expressions::iff(manager.boolean(true), newNodeVariable));
    } else {
        // In the non-terminal case, we recursively translate the children nodes and then construct and appropriate ite-expression.
        DdNode const* t = Cudd_T_const(dd);
        DdNode const* e = Cudd_E_const(dd);
        DdNode const* T = Cudd_Regular(t);
        DdNode const* E = Cudd_Regular(e);
        storm::expressions::Variable thenVariable =
            toExpressionRec(T, ddManager, manager, expressions, indexToVariableMap, countIndexToVariablePair, nodeToCounterMap, nextCounterForIndex);
        storm::expressions::Variable elseVariable =
            toExpressionRec(E, ddManager, manager, expressions, indexToVariableMap, countIndexToVariablePair, nodeToCounterMap, nextCounterForIndex);

        // Create the appropriate expression.
        auto indexVariable = indexToVariableMap.find(Cudd_NodeReadIndex(dd));
        storm::expressions::Variable levelVariable;
        if (indexVariable == indexToVariableMap.end()) {
            levelVariable = manager.declareFreshBooleanVariable();
            indexToVariableMap[Cudd_NodeReadIndex(dd)] = levelVariable;
        } else {
            levelVariable = indexVariable->second;
        }
        expressions.push_back(storm::expressions::iff(
            newNodeVariable, storm::expressions::ite(levelVariable, t == T ? thenVariable : !thenVariable, e == E ? elseVariable : !elseVariable)));
    }

    // Return the variable for this node.
    return newNodeVariable;
}
#else
InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::fromVector(InternalDdManager<DdType::CUDD> const* ddManager, Odd const& odd,
                                                                std::vector<uint_fast64_t> const& sortedDdVariableIndices,
                                                                std::function<bool(uint64_t)> const& filter) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

bool InternalBdd<DdType::CUDD>::operator==(InternalBdd<DdType::CUDD> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

bool InternalBdd<DdType::CUDD>::operator!=(InternalBdd<DdType::CUDD> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::relationalProduct(InternalBdd<DdType::CUDD> const& relation,
                                                                       std::vector<InternalBdd<DdType::CUDD>> const& rowVariables,
                                                                       std::vector<InternalBdd<DdType::CUDD>> const& columnVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::inverseRelationalProduct(InternalBdd<DdType::CUDD> const& relation,
                                                                              std::vector<InternalBdd<DdType::CUDD>> const& rowVariables,
                                                                              std::vector<InternalBdd<DdType::CUDD>> const& columnVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::inverseRelationalProductWithExtendedRelation(
    InternalBdd<DdType::CUDD> const& relation, std::vector<InternalBdd<DdType::CUDD>> const& rowVariables,
    std::vector<InternalBdd<DdType::CUDD>> const& columnVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::ite(InternalBdd<DdType::CUDD> const& thenDd, InternalBdd<DdType::CUDD> const& elseDd) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalBdd<DdType::CUDD>::ite(InternalAdd<DdType::CUDD, ValueType> const& thenAdd,
                                                                    InternalAdd<DdType::CUDD, ValueType> const& elseAdd) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::operator||(InternalBdd<DdType::CUDD> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD>& InternalBdd<DdType::CUDD>::operator|=(InternalBdd<DdType::CUDD> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::operator&&(InternalBdd<DdType::CUDD> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD>& InternalBdd<DdType::CUDD>::operator&=(InternalBdd<DdType::CUDD> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::iff(InternalBdd<DdType::CUDD> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::exclusiveOr(InternalBdd<DdType::CUDD> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::implies(InternalBdd<DdType::CUDD> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::operator!() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD>& InternalBdd<DdType::CUDD>::complement() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::existsAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::existsAbstractRepresentative(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::universalAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::andExists(InternalBdd<DdType::CUDD> const& other, InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::constrain(InternalBdd<DdType::CUDD> const& constraint) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::restrict(InternalBdd<DdType::CUDD> const& constraint) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::swapVariables(std::vector<InternalBdd<DdType::CUDD>> const& from,
                                                                   std::vector<InternalBdd<DdType::CUDD>> const& to) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalBdd<DdType::CUDD>::getSupport() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

uint_fast64_t InternalBdd<DdType::CUDD>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

uint_fast64_t InternalBdd<DdType::CUDD>::getLeafCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

uint_fast64_t InternalBdd<DdType::CUDD>::getNodeCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

bool InternalBdd<DdType::CUDD>::isOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

bool InternalBdd<DdType::CUDD>::isZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

uint_fast64_t InternalBdd<DdType::CUDD>::getIndex() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

uint_fast64_t InternalBdd<DdType::CUDD>::getLevel() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

void InternalBdd<DdType::CUDD>::exportToDot(std::string const& filename, std::vector<std::string> const& ddVariableNamesAsStrings,
                                            bool showVariablesIfPossible) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

void InternalBdd<DdType::CUDD>::exportToText(std::string const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalBdd<DdType::CUDD>::toAdd() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

storm::storage::BitVector InternalBdd<DdType::CUDD>::toVector(storm::dd::Odd const& rowOdd, std::vector<uint_fast64_t> const& ddVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

Odd InternalBdd<DdType::CUDD>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
void InternalBdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                     std::vector<ValueType> const& sourceValues, std::vector<ValueType>& targetValues) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

std::vector<InternalBdd<DdType::CUDD>> InternalBdd<DdType::CUDD>::splitIntoGroups(std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

void InternalBdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                     storm::storage::BitVector const& sourceValues, storm::storage::BitVector& targetValues) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> InternalBdd<DdType::CUDD>::toExpression(
    storm::expressions::ExpressionManager& manager) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

#endif

template InternalAdd<DdType::CUDD, double> InternalBdd<DdType::CUDD>::toAdd() const;
template InternalAdd<DdType::CUDD, uint_fast64_t> InternalBdd<DdType::CUDD>::toAdd() const;
template InternalAdd<DdType::CUDD, storm::RationalNumber> InternalBdd<DdType::CUDD>::toAdd() const;

#ifdef STORM_HAVE_CUDD
template void InternalBdd<DdType::CUDD>::filterExplicitVectorRec<double>(DdNode const* dd, cudd::Cudd const& manager, uint_fast64_t currentLevel,
                                                                         bool complement, uint_fast64_t maxLevel,
                                                                         std::vector<uint_fast64_t> const& ddVariableIndices, uint_fast64_t currentOffset,
                                                                         storm::dd::Odd const& odd, std::vector<double>& result, uint_fast64_t& currentIndex,
                                                                         std::vector<double> const& values);
template void InternalBdd<DdType::CUDD>::filterExplicitVectorRec<uint_fast64_t>(DdNode const* dd, cudd::Cudd const& manager, uint_fast64_t currentLevel,
                                                                                bool complement, uint_fast64_t maxLevel,
                                                                                std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                                uint_fast64_t currentOffset, storm::dd::Odd const& odd,
                                                                                std::vector<uint_fast64_t>& result, uint_fast64_t& currentIndex,
                                                                                std::vector<uint_fast64_t> const& values);
#endif

template void InternalBdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                              std::vector<double> const& sourceValues, std::vector<double>& targetValues) const;
template void InternalBdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                              std::vector<uint_fast64_t> const& sourceValues, std::vector<uint_fast64_t>& targetValues) const;

template InternalAdd<DdType::CUDD, double> InternalBdd<DdType::CUDD>::ite(InternalAdd<DdType::CUDD, double> const& thenAdd,
                                                                          InternalAdd<DdType::CUDD, double> const& elseAdd) const;
template InternalAdd<DdType::CUDD, uint_fast64_t> InternalBdd<DdType::CUDD>::ite(InternalAdd<DdType::CUDD, uint_fast64_t> const& thenAdd,
                                                                                 InternalAdd<DdType::CUDD, uint_fast64_t> const& elseAdd) const;
template InternalAdd<DdType::CUDD, storm::RationalNumber> InternalBdd<DdType::CUDD>::ite(InternalAdd<DdType::CUDD, storm::RationalNumber> const& thenAdd,
                                                                                         InternalAdd<DdType::CUDD, storm::RationalNumber> const& elseAdd) const;
}  // namespace dd
}  // namespace storm