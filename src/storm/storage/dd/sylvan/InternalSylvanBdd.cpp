#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

#include <boost/functional/hash.hpp>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/PairHash.h"
#include "storm/storage/dd/sylvan/InternalSylvanAdd.h"
#include "storm/storage/dd/sylvan/InternalSylvanDdManager.h"
#include "storm/storage/dd/sylvan/SylvanAddIterator.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_SYLVAN
InternalBdd<DdType::Sylvan>::InternalBdd() : ddManager(nullptr), sylvanBdd() {
    // Intentionally left empty.
}

InternalBdd<DdType::Sylvan>::InternalBdd(InternalDdManager<DdType::Sylvan> const* ddManager, sylvan::Bdd const& sylvanBdd)
    : ddManager(ddManager), sylvanBdd(sylvanBdd) {
    // Intentionally left empty.
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::fromVector(InternalDdManager<DdType::Sylvan> const* ddManager, Odd const& odd,
                                                                    std::vector<uint_fast64_t> const& sortedDdVariableIndices,
                                                                    std::function<bool(uint64_t)> const& filter) {
    uint_fast64_t offset = 0;
    return InternalBdd<DdType::Sylvan>(ddManager, sylvan::Bdd(fromVectorRec(offset, 0, sortedDdVariableIndices.size(), odd, sortedDdVariableIndices, filter)));
}

BDD InternalBdd<DdType::Sylvan>::fromVectorRec(uint_fast64_t& currentOffset, uint_fast64_t currentLevel, uint_fast64_t maxLevel, Odd const& odd,
                                               std::vector<uint_fast64_t> const& ddVariableIndices, std::function<bool(uint64_t)> const& filter) {
    if (currentLevel == maxLevel) {
        // If we are in a terminal node of the ODD, we need to check whether the then-offset of the ODD is one
        // (meaning the encoding is a valid one) or zero (meaning the encoding is not valid). Consequently, we
        // need to copy the next value of the vector iff the then-offset is greater than zero.
        if (odd.getThenOffset() > 0) {
            if (filter(currentOffset++)) {
                return sylvan_true;
            } else {
                return sylvan_false;
            }
        } else {
            return sylvan_false;
        }
    } else {
        // If the total offset is zero, we can just return the constant zero DD.
        if (odd.getThenOffset() + odd.getElseOffset() == 0) {
            return sylvan_false;
        }

        // Determine the new else-successor.
        BDD elseSuccessor;
        if (odd.getElseOffset() > 0) {
            elseSuccessor = fromVectorRec(currentOffset, currentLevel + 1, maxLevel, odd.getElseSuccessor(), ddVariableIndices, filter);
        } else {
            elseSuccessor = sylvan_false;
        }
        bdd_refs_push(elseSuccessor);

        // Determine the new then-successor.
        BDD thenSuccessor;
        if (odd.getThenOffset() > 0) {
            thenSuccessor = fromVectorRec(currentOffset, currentLevel + 1, maxLevel, odd.getThenSuccessor(), ddVariableIndices, filter);
        } else {
            thenSuccessor = sylvan_false;
        }
        bdd_refs_push(thenSuccessor);

        // Create a node representing ITE(currentVar, thenSuccessor, elseSuccessor);
        BDD currentVar = sylvan_ithvar(static_cast<BDDVAR>(ddVariableIndices[currentLevel]));
        bdd_refs_push(currentVar);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
        BDD result = sylvan_ite(currentVar, thenSuccessor, elseSuccessor);
#pragma clang diagnostic pop

        // Dispose of the intermediate results.
        bdd_refs_pop(3);

        return result;
    }
}

bool InternalBdd<DdType::Sylvan>::operator==(InternalBdd<DdType::Sylvan> const& other) const {
    return sylvanBdd == other.sylvanBdd;
}

bool InternalBdd<DdType::Sylvan>::operator!=(InternalBdd<DdType::Sylvan> const& other) const {
    return sylvanBdd != other.sylvanBdd;
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::relationalProduct(InternalBdd<DdType::Sylvan> const& relation,
                                                                           std::vector<InternalBdd<DdType::Sylvan>> const&,
                                                                           std::vector<InternalBdd<DdType::Sylvan>> const&) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.RelNext(relation.sylvanBdd, sylvan::Bdd(sylvan_false)));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::inverseRelationalProduct(InternalBdd<DdType::Sylvan> const& relation,
                                                                                  std::vector<InternalBdd<DdType::Sylvan>> const&,
                                                                                  std::vector<InternalBdd<DdType::Sylvan>> const&) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.RelPrev(relation.sylvanBdd, sylvan::Bdd(sylvan_false)));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::inverseRelationalProductWithExtendedRelation(
    InternalBdd<DdType::Sylvan> const& relation, std::vector<InternalBdd<DdType::Sylvan>> const& rowVariables,
    std::vector<InternalBdd<DdType::Sylvan>> const& columnVariables) const {
    // Currently, there is no specialized version to perform this operation, so we fall back to the regular operations.

    InternalBdd<DdType::Sylvan> columnCube = ddManager->getBddOne();
    for (auto const& variable : columnVariables) {
        columnCube &= variable;
    }

    return this->swapVariables(rowVariables, columnVariables).andExists(relation, columnCube);
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::ite(InternalBdd<DdType::Sylvan> const& thenDd, InternalBdd<DdType::Sylvan> const& elseDd) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.Ite(thenDd.sylvanBdd, elseDd.sylvanBdd));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalBdd<DdType::Sylvan>::ite(InternalAdd<DdType::Sylvan, ValueType> const& thenAdd,
                                                                        InternalAdd<DdType::Sylvan, ValueType> const& elseAdd) const {
    return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanBdd.Ite(thenAdd.getSylvanMtbdd(), elseAdd.getSylvanMtbdd()));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::operator||(InternalBdd<DdType::Sylvan> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd | other.sylvanBdd);
}

InternalBdd<DdType::Sylvan>& InternalBdd<DdType::Sylvan>::operator|=(InternalBdd<DdType::Sylvan> const& other) {
    this->sylvanBdd |= other.sylvanBdd;
    return *this;
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::operator&&(InternalBdd<DdType::Sylvan> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd & other.sylvanBdd);
}

InternalBdd<DdType::Sylvan>& InternalBdd<DdType::Sylvan>::operator&=(InternalBdd<DdType::Sylvan> const& other) {
    this->sylvanBdd &= other.sylvanBdd;
    return *this;
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::iff(InternalBdd<DdType::Sylvan> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, !(this->sylvanBdd ^ other.sylvanBdd));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::exclusiveOr(InternalBdd<DdType::Sylvan> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd ^ other.sylvanBdd);
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::implies(InternalBdd<DdType::Sylvan> const& other) const {
    return InternalBdd<DdType::Sylvan>(ddManager, (!this->sylvanBdd) | other.sylvanBdd);
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::operator!() const {
    return InternalBdd<DdType::Sylvan>(ddManager, !this->sylvanBdd);
}

InternalBdd<DdType::Sylvan>& InternalBdd<DdType::Sylvan>::complement() {
    this->sylvanBdd = !this->sylvanBdd;
    return *this;
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::existsAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.ExistAbstract(cube.sylvanBdd));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::existsAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.ExistAbstractRepresentative(cube.sylvanBdd));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::universalAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.UnivAbstract(cube.sylvanBdd));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::andExists(InternalBdd<DdType::Sylvan> const& other, InternalBdd<DdType::Sylvan> const& cube) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.AndAbstract(other.sylvanBdd, cube.sylvanBdd));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::constrain(InternalBdd<DdType::Sylvan> const& constraint) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.Constrain(constraint.sylvanBdd));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::restrict(InternalBdd<DdType::Sylvan> const& constraint) const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.Restrict(constraint.sylvanBdd));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::swapVariables(std::vector<InternalBdd<DdType::Sylvan>> const& from,
                                                                       std::vector<InternalBdd<DdType::Sylvan>> const& to) const {
    std::vector<uint32_t> fromIndices;
    std::vector<uint32_t> toIndices;
    for (auto it1 = from.begin(), ite1 = from.end(), it2 = to.begin(); it1 != ite1; ++it1, ++it2) {
        fromIndices.push_back(it1->getIndex());
        fromIndices.push_back(it2->getIndex());
        toIndices.push_back(it2->getIndex());
        toIndices.push_back(it1->getIndex());
    }
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.Permute(fromIndices, toIndices));
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::getSupport() const {
    return InternalBdd<DdType::Sylvan>(ddManager, this->sylvanBdd.Support());
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    if (numberOfDdVariables == 0) {
        return 0;
    }
    return static_cast<uint_fast64_t>(this->sylvanBdd.SatCount(numberOfDdVariables));
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getLeafCount() const {
    // For BDDs, the leaf count is always one, because the only leaf is the false leaf (and true is represented
    // by a negation edge to false).
    return 1;
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getNodeCount() const {
    // We have to add one to also count the false-leaf, which is the only leaf appearing in BDDs.
    return static_cast<uint_fast64_t>(this->sylvanBdd.NodeCount());
}

bool InternalBdd<DdType::Sylvan>::isOne() const {
    return this->sylvanBdd.isOne();
}

bool InternalBdd<DdType::Sylvan>::isZero() const {
    return this->sylvanBdd.isZero();
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getIndex() const {
    return static_cast<uint_fast64_t>(this->sylvanBdd.TopVar());
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getLevel() const {
    return this->getIndex();
}

void InternalBdd<DdType::Sylvan>::exportToDot(std::string const& filename, std::vector<std::string> const&, bool) const {
    FILE* filePointer = fopen(filename.c_str(), "a+");
    // fopen returns a nullptr on failure
    if (filePointer == nullptr) {
        STORM_LOG_ERROR("Failure to open file: " << filename);
    } else {
        this->sylvanBdd.PrintDot(filePointer);
        fclose(filePointer);
    }
}

void InternalBdd<DdType::Sylvan>::exportToText(std::string const& filename) const {
    FILE* filePointer = fopen(filename.c_str(), "a+");
    // fopen returns a nullptr on failure
    if (filePointer == nullptr) {
        STORM_LOG_ERROR("Failure to open file: " << filename);
    } else {
        this->sylvanBdd.PrintText(filePointer);
        fclose(filePointer);
    }
}

sylvan::Bdd& InternalBdd<DdType::Sylvan>::getSylvanBdd() {
    return sylvanBdd;
}

sylvan::Bdd const& InternalBdd<DdType::Sylvan>::getSylvanBdd() const {
    return sylvanBdd;
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalBdd<DdType::Sylvan>::toAdd() const {
    if (std::is_same<ValueType, double>::value) {
        return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanBdd.toDoubleMtbdd());
    } else if (std::is_same<ValueType, uint_fast64_t>::value) {
        return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanBdd.toInt64Mtbdd());
    } else if (std::is_same<ValueType, storm::RationalNumber>::value) {
        return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanBdd.toStormRationalNumberMtbdd());
    } else if (std::is_same<ValueType, storm::RationalFunction>::value) {
        return InternalAdd<DdType::Sylvan, ValueType>(ddManager, this->sylvanBdd.toStormRationalFunctionMtbdd());
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Illegal ADD type.");
    }
}

storm::storage::BitVector InternalBdd<DdType::Sylvan>::toVector(storm::dd::Odd const& rowOdd, std::vector<uint_fast64_t> const& ddVariableIndices) const {
    storm::storage::BitVector result(rowOdd.getTotalOffset());
    this->toVectorRec(bdd_regular(this->getSylvanBdd().GetBDD()), result, rowOdd, bdd_isnegated(this->getSylvanBdd().GetBDD()), 0, ddVariableIndices.size(), 0,
                      ddVariableIndices);
    return result;
}

void InternalBdd<DdType::Sylvan>::toVectorRec(BDD dd, storm::storage::BitVector& result, Odd const& rowOdd, bool complement, uint_fast64_t currentRowLevel,
                                              uint_fast64_t maxLevel, uint_fast64_t currentRowOffset,
                                              std::vector<uint_fast64_t> const& ddRowVariableIndices) const {
    // If there are no more values to select, we can directly return.
    if (dd == sylvan_false && !complement) {
        return;
    } else if (dd == sylvan_true && complement) {
        return;
    }

    // If we are at the maximal level, the value to be set is stored as a constant in the DD.
    if (currentRowLevel == maxLevel) {
        result.set(currentRowOffset, true);
    } else if (bdd_isterminal(dd) || ddRowVariableIndices[currentRowLevel] < sylvan_var(dd)) {
        toVectorRec(dd, result, rowOdd.getElseSuccessor(), complement, currentRowLevel + 1, maxLevel, currentRowOffset, ddRowVariableIndices);
        toVectorRec(dd, result, rowOdd.getThenSuccessor(), complement, currentRowLevel + 1, maxLevel, currentRowOffset + rowOdd.getElseOffset(),
                    ddRowVariableIndices);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        BDD elseDdNode = sylvan_low(dd);
        BDD thenDdNode = sylvan_high(dd);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = bdd_isnegated(elseDdNode) ^ complement;
        bool thenComplemented = bdd_isnegated(thenDdNode) ^ complement;

        toVectorRec(bdd_regular(elseDdNode), result, rowOdd.getElseSuccessor(), elseComplemented, currentRowLevel + 1, maxLevel, currentRowOffset,
                    ddRowVariableIndices);
        toVectorRec(bdd_regular(thenDdNode), result, rowOdd.getThenSuccessor(), thenComplemented, currentRowLevel + 1, maxLevel,
                    currentRowOffset + rowOdd.getElseOffset(), ddRowVariableIndices);
    }
}

Odd InternalBdd<DdType::Sylvan>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    // Prepare a unique table for each level that keeps the constructed ODD nodes unique.
    std::vector<std::unordered_map<std::pair<BDD, bool>, std::shared_ptr<Odd>, HashFunctor>> uniqueTableForLevels(ddVariableIndices.size() + 1);

    // Now construct the ODD structure from the BDD.
    std::shared_ptr<Odd> rootOdd = createOddRec(bdd_regular(this->getSylvanBdd().GetBDD()), bdd_isnegated(this->getSylvanBdd().GetBDD()), 0,
                                                ddVariableIndices.size(), ddVariableIndices, uniqueTableForLevels);

    // Return a copy of the root node to remove the shared_ptr encapsulation.
    return Odd(*rootOdd);
}

std::size_t InternalBdd<DdType::Sylvan>::HashFunctor::operator()(std::pair<BDD, bool> const& key) const {
    std::size_t result = 0;
    boost::hash_combine(result, key.first);
    boost::hash_combine(result, key.second);
    return result;
}

std::shared_ptr<Odd> InternalBdd<DdType::Sylvan>::createOddRec(
    BDD dd, bool complement, uint_fast64_t currentLevel, uint_fast64_t maxLevel, std::vector<uint_fast64_t> const& ddVariableIndices,
    std::vector<std::unordered_map<std::pair<BDD, bool>, std::shared_ptr<Odd>, HashFunctor>>& uniqueTableForLevels) {
    // Check whether the ODD for this node has already been computed (for this level) and if so, return this instead.
    auto const& iterator = uniqueTableForLevels[currentLevel].find(std::make_pair(dd, complement));
    if (iterator != uniqueTableForLevels[currentLevel].end()) {
        return iterator->second;
    } else {
        // Otherwise, we need to recursively compute the ODD.

        // If we are already at the maximal level that is to be considered, we can simply create an Odd without
        // successors.
        if (currentLevel == maxLevel) {
            uint_fast64_t elseOffset = 0;
            uint_fast64_t thenOffset = 0;

            // If the DD is not the zero leaf, then the then-offset is 1.
            if (dd != mtbdd_false) {
                thenOffset = 1;
            }

            // If we need to complement the 'terminal' node, we need to negate its offset.
            if (complement) {
                thenOffset = 1 - thenOffset;
            }

            auto oddNode = std::make_shared<Odd>(nullptr, elseOffset, nullptr, thenOffset);
            uniqueTableForLevels[currentLevel].emplace(std::make_pair(dd, complement), oddNode);
            return oddNode;
        } else if (bdd_isterminal(dd) || ddVariableIndices[currentLevel] < sylvan_var(dd)) {
            // If we skipped the level in the DD, we compute the ODD just for the else-successor and use the same
            // node for the then-successor as well.
            std::shared_ptr<Odd> elseNode = createOddRec(dd, complement, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode = elseNode;
            uint_fast64_t totalOffset = elseNode->getElseOffset() + elseNode->getThenOffset();
            auto oddNode = std::make_shared<Odd>(elseNode, totalOffset, thenNode, totalOffset);
            uniqueTableForLevels[currentLevel].emplace(std::make_pair(dd, complement), oddNode);
            return oddNode;
        } else {
            // Otherwise, we compute the ODDs for both the then- and else successors.
            BDD thenDdNode = sylvan_high(dd);
            BDD elseDdNode = sylvan_low(dd);

            // Determine whether we have to evaluate the successors as if they were complemented.
            bool elseComplemented = bdd_isnegated(elseDdNode) ^ complement;
            bool thenComplemented = bdd_isnegated(thenDdNode) ^ complement;

            std::shared_ptr<Odd> elseNode =
                createOddRec(bdd_regular(elseDdNode), elseComplemented, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode =
                createOddRec(bdd_regular(thenDdNode), thenComplemented, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);

            auto oddNode = std::make_shared<Odd>(elseNode, elseNode->getElseOffset() + elseNode->getThenOffset(), thenNode,
                                                 thenNode->getElseOffset() + thenNode->getThenOffset());
            uniqueTableForLevels[currentLevel].emplace(std::make_pair(dd, complement), oddNode);
            return oddNode;
        }
    }
}

template<typename ValueType>
void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                       std::vector<ValueType> const& sourceValues, std::vector<ValueType>& targetValues) const {
    uint_fast64_t currentIndex = 0;
    filterExplicitVectorRec(bdd_regular(this->getSylvanBdd().GetBDD()), 0, bdd_isnegated(this->getSylvanBdd().GetBDD()), ddVariableIndices.size(),
                            ddVariableIndices, 0, odd, targetValues, currentIndex, sourceValues);
}

template<typename ValueType>
void InternalBdd<DdType::Sylvan>::filterExplicitVectorRec(BDD dd, uint_fast64_t currentLevel, bool complement, uint_fast64_t maxLevel,
                                                          std::vector<uint_fast64_t> const& ddVariableIndices, uint_fast64_t currentOffset,
                                                          storm::dd::Odd const& odd, std::vector<ValueType>& result, uint_fast64_t& currentIndex,
                                                          std::vector<ValueType> const& values) {
    // If there are no more values to select, we can directly return.
    if (dd == sylvan_false && !complement) {
        return;
    } else if (dd == sylvan_true && complement) {
        return;
    }

    if (currentLevel == maxLevel) {
        result[currentIndex++] = values[currentOffset];
    } else if (bdd_isterminal(dd) || ddVariableIndices[currentLevel] < sylvan_var(dd)) {
        // If we skipped a level, we need to enumerate the explicit entries for the case in which the bit is set
        // and for the one in which it is not set.
        filterExplicitVectorRec(dd, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset, odd.getElseSuccessor(), result, currentIndex,
                                values);
        filterExplicitVectorRec(dd, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset + odd.getElseOffset(), odd.getThenSuccessor(),
                                result, currentIndex, values);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        BDD thenDdNode = sylvan_high(dd);
        BDD elseDdNode = sylvan_low(dd);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = bdd_isnegated(elseDdNode) ^ complement;
        bool thenComplemented = bdd_isnegated(thenDdNode) ^ complement;

        filterExplicitVectorRec(bdd_regular(elseDdNode), currentLevel + 1, elseComplemented, maxLevel, ddVariableIndices, currentOffset, odd.getElseSuccessor(),
                                result, currentIndex, values);
        filterExplicitVectorRec(bdd_regular(thenDdNode), currentLevel + 1, thenComplemented, maxLevel, ddVariableIndices, currentOffset + odd.getElseOffset(),
                                odd.getThenSuccessor(), result, currentIndex, values);
    }
}

void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                       storm::storage::BitVector const& sourceValues, storm::storage::BitVector& targetValues) const {
    uint_fast64_t currentIndex = 0;
    filterExplicitVectorRec(bdd_regular(this->getSylvanBdd().GetBDD()), 0, bdd_isnegated(this->getSylvanBdd().GetBDD()), ddVariableIndices.size(),
                            ddVariableIndices, 0, odd, targetValues, currentIndex, sourceValues);
}

void InternalBdd<DdType::Sylvan>::filterExplicitVectorRec(BDD dd, uint_fast64_t currentLevel, bool complement, uint_fast64_t maxLevel,
                                                          std::vector<uint_fast64_t> const& ddVariableIndices, uint_fast64_t currentOffset,
                                                          storm::dd::Odd const& odd, storm::storage::BitVector& result, uint_fast64_t& currentIndex,
                                                          storm::storage::BitVector const& values) {
    // If there are no more values to select, we can directly return.
    if (dd == sylvan_false && !complement) {
        return;
    } else if (dd == sylvan_true && complement) {
        return;
    }

    if (currentLevel == maxLevel) {
        result.set(currentIndex++, values.get(currentOffset));
    } else if (bdd_isterminal(dd) || ddVariableIndices[currentLevel] < sylvan_var(dd)) {
        // If we skipped a level, we need to enumerate the explicit entries for the case in which the bit is set
        // and for the one in which it is not set.
        filterExplicitVectorRec(dd, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset, odd.getElseSuccessor(), result, currentIndex,
                                values);
        filterExplicitVectorRec(dd, currentLevel + 1, complement, maxLevel, ddVariableIndices, currentOffset + odd.getElseOffset(), odd.getThenSuccessor(),
                                result, currentIndex, values);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        BDD thenDdNode = sylvan_high(dd);
        BDD elseDdNode = sylvan_low(dd);

        // Determine whether we have to evaluate the successors as if they were complemented.
        bool elseComplemented = bdd_isnegated(elseDdNode) ^ complement;
        bool thenComplemented = bdd_isnegated(thenDdNode) ^ complement;

        filterExplicitVectorRec(bdd_regular(elseDdNode), currentLevel + 1, elseComplemented, maxLevel, ddVariableIndices, currentOffset, odd.getElseSuccessor(),
                                result, currentIndex, values);
        filterExplicitVectorRec(bdd_regular(thenDdNode), currentLevel + 1, thenComplemented, maxLevel, ddVariableIndices, currentOffset + odd.getElseOffset(),
                                odd.getThenSuccessor(), result, currentIndex, values);
    }
}

std::vector<InternalBdd<DdType::Sylvan>> InternalBdd<DdType::Sylvan>::splitIntoGroups(std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<InternalBdd<DdType::Sylvan>> result;
    splitIntoGroupsRec(this->getSylvanBdd().GetBDD(), result, ddGroupVariableIndices, 0, ddGroupVariableIndices.size());
    return result;
}

void InternalBdd<DdType::Sylvan>::splitIntoGroupsRec(BDD dd, std::vector<InternalBdd<DdType::Sylvan>>& groups,
                                                     std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel,
                                                     uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    if (dd == sylvan_false) {
        return;
    }

    if (currentLevel == maxLevel) {
        groups.push_back(InternalBdd<DdType::Sylvan>(ddManager, sylvan::Bdd(dd)));
    } else if (bdd_isterminal(dd) || ddGroupVariableIndices[currentLevel] < sylvan_var(dd)) {
        splitIntoGroupsRec(dd, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(dd, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    } else {
        // Otherwise, we compute the ODDs for both the then- and else successors.
        BDD thenDdNode = sylvan_high(dd);
        BDD elseDdNode = sylvan_low(dd);

        splitIntoGroupsRec(elseDdNode, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(thenDdNode, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    }
}

std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>>
InternalBdd<DdType::Sylvan>::toExpression(storm::expressions::ExpressionManager& manager) const {
    std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> result;

    // Create (and maintain) a mapping from the DD nodes to a counter that says the how-many-th node (within the
    // nodes of equal index) the node was.
    std::unordered_map<BDD, uint_fast64_t> nodeToCounterMap;
    std::vector<uint_fast64_t> nextCounterForIndex(ddManager->getNumberOfDdVariables(), 0);
    std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable> countIndexToVariablePair;

    bool negated = bdd_isnegated(this->getSylvanBdd().GetBDD());

    // Translate from the top node downwards.
    storm::expressions::Variable topVariable = this->toExpressionRec(bdd_regular(this->getSylvanBdd().GetBDD()), manager, result.first, result.second,
                                                                     countIndexToVariablePair, nodeToCounterMap, nextCounterForIndex);

    // Create the final expression.
    if (negated) {
        result.first.push_back(!topVariable);
    } else {
        result.first.push_back(topVariable);
    }

    return result;
}

storm::expressions::Variable InternalBdd<DdType::Sylvan>::toExpressionRec(
    BDD dd, storm::expressions::ExpressionManager& manager, std::vector<storm::expressions::Expression>& expressions,
    std::unordered_map<uint_fast64_t, storm::expressions::Variable>& indexToVariableMap,
    std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable>& countIndexToVariablePair,
    std::unordered_map<BDD, uint_fast64_t>& nodeToCounterMap, std::vector<uint_fast64_t>& nextCounterForIndex) {
    STORM_LOG_ASSERT(!bdd_isnegated(dd), "Expected non-negated BDD node.");

    // First, try to look up the current node if it's not a terminal node.
    auto nodeCounterIt = nodeToCounterMap.find(dd);
    if (nodeCounterIt != nodeToCounterMap.end()) {
        // If we have found the node, this means we can look up the counter-index pair and get the corresponding variable.
        auto variableIt = countIndexToVariablePair.find(std::make_pair(nodeCounterIt->second, sylvan_var(dd)));
        STORM_LOG_ASSERT(variableIt != countIndexToVariablePair.end(), "Unable to find node.");
        return variableIt->second;
    }

    // If the node was not yet encountered, we create a variable and associate it with the appropriate expression.
    storm::expressions::Variable newNodeVariable = manager.declareFreshBooleanVariable();

    // Since we want to reuse the variable whenever possible, we insert the appropriate entries in the hash table.
    if (!bdd_isterminal(dd)) {
        // If we are dealing with a non-terminal node, we count it as a new node with this index.
        nodeToCounterMap[dd] = nextCounterForIndex[sylvan_var(dd)];
        countIndexToVariablePair[std::make_pair(nextCounterForIndex[sylvan_var(dd)], sylvan_var(dd))] = newNodeVariable;
        ++nextCounterForIndex[sylvan_var(dd)];
    } else {
        // If it's a terminal node, it is the one leaf and there's no need to keep track of a counter for this level.
        nodeToCounterMap[dd] = 0;
        countIndexToVariablePair[std::make_pair(0, sylvan_var(dd))] = newNodeVariable;
    }

    // In the terminal case, we can only have a one since we are considering non-negated nodes only.
    if (bdd_isterminal(dd)) {
        if (dd == sylvan_true) {
            expressions.push_back(storm::expressions::iff(manager.boolean(true), newNodeVariable));
        } else {
            expressions.push_back(storm::expressions::iff(manager.boolean(false), newNodeVariable));
        }
    } else {
        // In the non-terminal case, we recursively translate the children nodes and then construct and appropriate ite-expression.
        BDD t = sylvan_high(dd);
        BDD e = sylvan_low(dd);
        BDD T = bdd_regular(t);
        BDD E = bdd_regular(e);
        storm::expressions::Variable thenVariable =
            toExpressionRec(T, manager, expressions, indexToVariableMap, countIndexToVariablePair, nodeToCounterMap, nextCounterForIndex);
        storm::expressions::Variable elseVariable =
            toExpressionRec(E, manager, expressions, indexToVariableMap, countIndexToVariablePair, nodeToCounterMap, nextCounterForIndex);

        // Create the appropriate expression.
        // Create the appropriate expression.
        auto indexVariable = indexToVariableMap.find(sylvan_var(dd));
        storm::expressions::Variable levelVariable;
        if (indexVariable == indexToVariableMap.end()) {
            levelVariable = manager.declareFreshBooleanVariable();
            indexToVariableMap[sylvan_var(dd)] = levelVariable;
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
InternalBdd<DdType::Sylvan>::InternalBdd() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::fromVector(InternalDdManager<DdType::Sylvan> const* ddManager, Odd const& odd,
                                                                    std::vector<uint_fast64_t> const& sortedDdVariableIndices,
                                                                    std::function<bool(uint64_t)> const& filter) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

bool InternalBdd<DdType::Sylvan>::operator==(InternalBdd<DdType::Sylvan> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

bool InternalBdd<DdType::Sylvan>::operator!=(InternalBdd<DdType::Sylvan> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::relationalProduct(InternalBdd<DdType::Sylvan> const& relation,
                                                                           std::vector<InternalBdd<DdType::Sylvan>> const&,
                                                                           std::vector<InternalBdd<DdType::Sylvan>> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::inverseRelationalProduct(InternalBdd<DdType::Sylvan> const& relation,
                                                                                  std::vector<InternalBdd<DdType::Sylvan>> const&,
                                                                                  std::vector<InternalBdd<DdType::Sylvan>> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::inverseRelationalProductWithExtendedRelation(
    InternalBdd<DdType::Sylvan> const& relation, std::vector<InternalBdd<DdType::Sylvan>> const& rowVariables,
    std::vector<InternalBdd<DdType::Sylvan>> const& columnVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::ite(InternalBdd<DdType::Sylvan> const& thenDd, InternalBdd<DdType::Sylvan> const& elseDd) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalBdd<DdType::Sylvan>::ite(InternalAdd<DdType::Sylvan, ValueType> const& thenAdd,
                                                                        InternalAdd<DdType::Sylvan, ValueType> const& elseAdd) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::operator||(InternalBdd<DdType::Sylvan> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan>& InternalBdd<DdType::Sylvan>::operator|=(InternalBdd<DdType::Sylvan> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::operator&&(InternalBdd<DdType::Sylvan> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan>& InternalBdd<DdType::Sylvan>::operator&=(InternalBdd<DdType::Sylvan> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::iff(InternalBdd<DdType::Sylvan> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::exclusiveOr(InternalBdd<DdType::Sylvan> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::implies(InternalBdd<DdType::Sylvan> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::operator!() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan>& InternalBdd<DdType::Sylvan>::complement() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::existsAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::existsAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::universalAbstract(InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::andExists(InternalBdd<DdType::Sylvan> const& other, InternalBdd<DdType::Sylvan> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::constrain(InternalBdd<DdType::Sylvan> const& constraint) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::restrict(InternalBdd<DdType::Sylvan> const& constraint) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::swapVariables(std::vector<InternalBdd<DdType::Sylvan>> const& from,
                                                                       std::vector<InternalBdd<DdType::Sylvan>> const& to) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalBdd<DdType::Sylvan>::getSupport() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getLeafCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getNodeCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

bool InternalBdd<DdType::Sylvan>::isOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

bool InternalBdd<DdType::Sylvan>::isZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getIndex() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

uint_fast64_t InternalBdd<DdType::Sylvan>::getLevel() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

void InternalBdd<DdType::Sylvan>::exportToDot(std::string const& filename, std::vector<std::string> const&, bool) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

void InternalBdd<DdType::Sylvan>::exportToText(std::string const& filename) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalBdd<DdType::Sylvan>::toAdd() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

storm::storage::BitVector InternalBdd<DdType::Sylvan>::toVector(storm::dd::Odd const& rowOdd, std::vector<uint_fast64_t> const& ddVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

Odd InternalBdd<DdType::Sylvan>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                       std::vector<ValueType> const& sourceValues, std::vector<ValueType>& targetValues) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                       storm::storage::BitVector const& sourceValues, storm::storage::BitVector& targetValues) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<InternalBdd<DdType::Sylvan>> InternalBdd<DdType::Sylvan>::splitIntoGroups(std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>>
InternalBdd<DdType::Sylvan>::toExpression(storm::expressions::ExpressionManager& manager) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}
#endif

template InternalAdd<DdType::Sylvan, double> InternalBdd<DdType::Sylvan>::toAdd() const;
template InternalAdd<DdType::Sylvan, uint_fast64_t> InternalBdd<DdType::Sylvan>::toAdd() const;
template InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalBdd<DdType::Sylvan>::toAdd() const;
template InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalBdd<DdType::Sylvan>::toAdd() const;

template void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                std::vector<double> const& sourceValues, std::vector<double>& targetValues) const;
template void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                std::vector<uint_fast64_t> const& sourceValues, std::vector<uint_fast64_t>& targetValues) const;
template void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                std::vector<storm::RationalNumber> const& sourceValues,
                                                                std::vector<storm::RationalNumber>& targetValues) const;
template void InternalBdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                std::vector<storm::RationalFunction> const& sourceValues,
                                                                std::vector<storm::RationalFunction>& targetValues) const;

template InternalAdd<DdType::Sylvan, double> InternalBdd<DdType::Sylvan>::ite(InternalAdd<DdType::Sylvan, double> const& thenAdd,
                                                                              InternalAdd<DdType::Sylvan, double> const& elseAdd) const;
template InternalAdd<DdType::Sylvan, uint_fast64_t> InternalBdd<DdType::Sylvan>::ite(InternalAdd<DdType::Sylvan, uint_fast64_t> const& thenAdd,
                                                                                     InternalAdd<DdType::Sylvan, uint_fast64_t> const& elseAdd) const;
template InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalBdd<DdType::Sylvan>::ite(
    InternalAdd<DdType::Sylvan, storm::RationalNumber> const& thenAdd, InternalAdd<DdType::Sylvan, storm::RationalNumber> const& elseAdd) const;
template InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalBdd<DdType::Sylvan>::ite(
    InternalAdd<DdType::Sylvan, storm::RationalFunction> const& thenAdd, InternalAdd<DdType::Sylvan, storm::RationalFunction> const& elseAdd) const;
}  // namespace dd
}  // namespace storm