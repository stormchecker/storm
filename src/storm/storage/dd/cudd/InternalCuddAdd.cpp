#include "storm/storage/dd/cudd/InternalCuddAdd.h"

#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/dd/Odd.h"
#include "storm/storage/dd/cudd/CuddAddIterator.h"
#include "storm/storage/dd/cudd/InternalCuddBdd.h"
#include "storm/storage/dd/cudd/InternalCuddDdManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_CUDD
template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>::InternalAdd(InternalDdManager<DdType::CUDD> const* ddManager, cudd::ADD cuddAdd)
    : ddManager(ddManager), cuddAdd(cuddAdd) {
    // Intentionally left empty.
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::operator==(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return this->getCuddAdd() == other.getCuddAdd();
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::operator!=(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return !(*this == other);
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator+(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd() + other.getCuddAdd());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator+=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    this->cuddAdd = this->getCuddAdd() + other.getCuddAdd();
    return *this;
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator*(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd() * other.getCuddAdd());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator*=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    this->cuddAdd = this->getCuddAdd() * other.getCuddAdd();
    return *this;
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator-(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd() - other.getCuddAdd());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator-=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    this->cuddAdd = this->getCuddAdd() - other.getCuddAdd();
    return *this;
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator/(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Divide(other.getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator/=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    this->cuddAdd = this->getCuddAdd().Divide(other.getCuddAdd());
    return *this;
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::equals(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().EqualsBdd(other.getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::notEquals(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().NotEqualsBdd(other.getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::less(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().LessThanBdd(other.getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::lessOrEqual(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().LessThanOrEqualBdd(other.getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greater(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().GreaterThanBdd(other.getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greaterOrEqual(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().GreaterThanOrEqualBdd(other.getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::pow(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Pow(other.getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::mod(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Mod(other.getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::logxy(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().LogXY(other.getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::floor() const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Floor());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::ceil() const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Ceil());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, storm::RationalNumber> InternalAdd<DdType::CUDD, ValueType>::sharpenKwekMehlhorn(size_t /*precision*/) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::minimum(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Minimum(other.getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::maximum(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Maximum(other.getCuddAdd()));
}

template<typename ValueType>
template<typename TargetValueType>
typename std::enable_if<std::is_same<ValueType, TargetValueType>::value, InternalAdd<DdType::CUDD, TargetValueType>>

    ::type InternalAdd<DdType::CUDD, ValueType>::toValueType() const {
    return *this;
}

template<typename ValueType>
template<typename TargetValueType>
typename std::enable_if<!std::is_same<ValueType, TargetValueType>::value, InternalAdd<DdType::CUDD, TargetValueType>>

    ::type InternalAdd<DdType::CUDD, ValueType>::toValueType() const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::sumAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().ExistAbstract(cube.toAdd<ValueType>().getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::minAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().MinAbstract(cube.toAdd<ValueType>().getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::minAbstractRepresentative(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().MinAbstractRepresentative(cube.toAdd<ValueType>().getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::maxAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().MaxAbstract(cube.toAdd<ValueType>().getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::maxAbstractRepresentative(InternalBdd<DdType::CUDD> const& cube) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().MaxAbstractRepresentative(cube.toAdd<ValueType>().getCuddAdd()));
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::equalModuloPrecision(InternalAdd<DdType::CUDD, ValueType> const& other, ValueType const& precision,
                                                                bool relative) const {
    if (relative) {
        return this->getCuddAdd().EqualSupNormRel(other.getCuddAdd(), precision);
    } else {
        return this->getCuddAdd().EqualSupNorm(other.getCuddAdd(), precision);
    }
}

template<>
bool InternalAdd<DdType::CUDD, storm::RationalNumber>::equalModuloPrecision(InternalAdd<DdType::CUDD, storm::RationalNumber> const& /*other*/,
                                                                            storm::RationalNumber const& /*precision*/, bool /*relative*/) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::swapVariables(std::vector<InternalBdd<DdType::CUDD>> const& from,
                                                                                         std::vector<InternalBdd<DdType::CUDD>> const& to) const {
    std::vector<cudd::ADD> fromAdd;
    std::vector<cudd::ADD> toAdd;
    STORM_LOG_ASSERT(fromAdd.size() == toAdd.size(), "Sizes of vectors do not match.");
    for (auto it1 = from.begin(), ite1 = from.end(), it2 = to.begin(); it1 != ite1; ++it1, ++it2) {
        fromAdd.push_back(it1->getCuddBdd().Add());
        toAdd.push_back(it2->getCuddBdd().Add());
    }
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().SwapVariables(fromAdd, toAdd));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::permuteVariables(std::vector<InternalBdd<DdType::CUDD>> const& from,
                                                                                            std::vector<InternalBdd<DdType::CUDD>> const& to) const {
    // Build the full permutation.
    uint64_t numberOfVariables = ddManager->getCuddManager().ReadSize();
    int* permutation = new int[numberOfVariables];
    for (uint64_t variable = 0; variable < numberOfVariables; ++variable) {
        permutation[variable] = variable;
    }

    for (auto it1 = from.begin(), ite1 = from.end(), it2 = to.begin(); it1 != ite1; ++it1, ++it2) {
        permutation[it1->getIndex()] = it2->getIndex();
    }
    InternalAdd<DdType::CUDD, ValueType> result(ddManager, this->getCuddAdd().Permute(permutation));

    delete[] permutation;
    return result;
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::multiplyMatrix(
    InternalAdd<DdType::CUDD, ValueType> const& otherMatrix, std::vector<InternalBdd<DdType::CUDD>> const& summationDdVariables) const {
    // Create the CUDD summation variables.
    std::vector<cudd::ADD> summationAdds;
    for (auto const& ddVariable : summationDdVariables) {
        summationAdds.push_back(ddVariable.toAdd<ValueType>().getCuddAdd());
    }

    //            return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().TimesPlus(otherMatrix.getCuddAdd(), summationAdds));
    //            return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Triangle(otherMatrix.getCuddAdd(), summationAdds));
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().MatrixMultiply(otherMatrix.getCuddAdd(), summationAdds));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::multiplyMatrix(
    InternalBdd<DdType::CUDD> const& otherMatrix, std::vector<InternalBdd<DdType::CUDD>> const& summationDdVariables) const {
    return this->multiplyMatrix(otherMatrix.template toAdd<ValueType>(), summationDdVariables);
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greater(ValueType const& value) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().BddStrictThreshold(value));
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::greater(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greaterOrEqual(ValueType const& value) const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().BddThreshold(value));
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::greaterOrEqual(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::less(ValueType const& value) const {
    return InternalBdd<DdType::CUDD>(ddManager, ~this->getCuddAdd().BddThreshold(value));
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::less(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::lessOrEqual(ValueType const& value) const {
    return InternalBdd<DdType::CUDD>(ddManager, ~this->getCuddAdd().BddStrictThreshold(value));
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::lessOrEqual(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::notZero() const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().BddPattern());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::constrain(InternalAdd<DdType::CUDD, ValueType> const& constraint) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Constrain(constraint.getCuddAdd()));
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::restrict(InternalAdd<DdType::CUDD, ValueType> const& constraint) const {
    return InternalAdd<DdType::CUDD, ValueType>(ddManager, this->getCuddAdd().Restrict(constraint.getCuddAdd()));
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::getSupport() const {
    return InternalBdd<DdType::CUDD>(ddManager, this->getCuddAdd().Support());
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    // If the number of DD variables is zero, CUDD returns a number greater 0 for constant nodes different from
    // zero, which is not the behaviour we expect.
    if (numberOfDdVariables == 0) {
        return 0;
    }
    return static_cast<uint_fast64_t>(this->getCuddAdd().CountMinterm(static_cast<int>(numberOfDdVariables)));
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getLeafCount() const {
    return static_cast<uint_fast64_t>(this->getCuddAdd().CountLeaves());
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getNodeCount() const {
    return static_cast<uint_fast64_t>(this->getCuddAdd().nodeCount());
}

template<typename ValueType>
ValueType InternalAdd<DdType::CUDD, ValueType>::getMin() const {
    cudd::ADD constantMinAdd = this->getCuddAdd().FindMin();
    return storm::utility::convertNumber<ValueType>(Cudd_V(constantMinAdd.getNode()));
}

template<typename ValueType>
ValueType InternalAdd<DdType::CUDD, ValueType>::getMax() const {
    cudd::ADD constantMaxAdd = this->getCuddAdd().FindMax();
    return storm::utility::convertNumber<ValueType>(Cudd_V(constantMaxAdd.getNode()));
}

template<typename ValueType>
ValueType InternalAdd<DdType::CUDD, ValueType>::getValue() const {
    return storm::utility::convertNumber<ValueType>(Cudd_V(this->getCuddAdd().getNode()));
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::isOne() const {
    return this->getCuddAdd().IsOne();
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::isZero() const {
    return this->getCuddAdd().IsZero();
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::isConstant() const {
    return Cudd_IsConstant(this->getCuddAdd().getNode());
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getIndex() const {
    return static_cast<uint_fast64_t>(this->getCuddAdd().NodeReadIndex());
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getLevel() const {
    return static_cast<uint_fast64_t>(ddManager->getCuddManager().ReadPerm(this->getIndex()));
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::exportToDot(std::string const& filename, std::vector<std::string> const& ddVariableNamesAsStrings,
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
    FILE* filePointer = fopen(filename.c_str(), "w");
    std::vector<cudd::ADD> cuddAddVector = {this->getCuddAdd()};
    if (showVariablesIfPossible) {
        ddManager->getCuddManager().DumpDot(cuddAddVector, ddVariableNames.data(), &ddNames[0], filePointer);
    } else {
        ddManager->getCuddManager().DumpDot(cuddAddVector, nullptr, &ddNames[0], filePointer);
    }
    fclose(filePointer);

    // Finally, delete the names.
    for (char* element : ddNames) {
        delete[] element;
    }
    for (char* element : ddVariableNames) {
        delete[] element;
    }
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::exportToText(std::string const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::begin(DdManager<DdType::CUDD> const& fullDdManager, InternalBdd<DdType::CUDD> const&,
                                                                                 uint_fast64_t, std::set<storm::expressions::Variable> const& metaVariables,
                                                                                 bool enumerateDontCareMetaVariables) const {
    int* cube;
    double value;
    DdGen* generator = this->getCuddAdd().FirstCube(&cube, &value);
    return AddIterator<DdType::CUDD, ValueType>(fullDdManager, generator, cube, storm::utility::convertNumber<ValueType>(value),
                                                (Cudd_IsGenEmpty(generator) != 0), &metaVariables, enumerateDontCareMetaVariables);
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::end(DdManager<DdType::CUDD> const& fullDdManager) const {
    return AddIterator<DdType::CUDD, ValueType>(fullDdManager, nullptr, nullptr, 0, true, nullptr, false);
}

template<typename ValueType>
cudd::ADD InternalAdd<DdType::CUDD, ValueType>::getCuddAdd() const {
    return this->cuddAdd;
}

template<typename ValueType>
DdNode* InternalAdd<DdType::CUDD, ValueType>::getCuddDdNode() const {
    return this->getCuddAdd().getNode();
}

template<typename ValueType>
std::string InternalAdd<DdType::CUDD, ValueType>::getStringId() const {
    std::stringstream ss;
    ss << this->getCuddDdNode();
    return ss.str();
}

template<typename ValueType>
Odd InternalAdd<DdType::CUDD, ValueType>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    // Prepare a unique table for each level that keeps the constructed ODD nodes unique.
    std::vector<std::unordered_map<DdNode*, std::shared_ptr<Odd>>> uniqueTableForLevels(ddVariableIndices.size() + 1);

    // Now construct the ODD structure from the ADD.
    std::shared_ptr<Odd> rootOdd =
        createOddRec(this->getCuddDdNode(), ddManager->getCuddManager(), 0, ddVariableIndices.size(), ddVariableIndices, uniqueTableForLevels);

    // Return a copy of the root node to remove the shared_ptr encapsulation.
    return Odd(*rootOdd);
}

template<typename ValueType>
std::shared_ptr<Odd> InternalAdd<DdType::CUDD, ValueType>::createOddRec(DdNode* dd, cudd::Cudd const& manager, uint_fast64_t currentLevel,
                                                                        uint_fast64_t maxLevel, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                        std::vector<std::unordered_map<DdNode*, std::shared_ptr<Odd>>>& uniqueTableForLevels) {
    // Check whether the ODD for this node has already been computed (for this level) and if so, return this instead.
    auto const& iterator = uniqueTableForLevels[currentLevel].find(dd);
    if (iterator != uniqueTableForLevels[currentLevel].end()) {
        return iterator->second;
    } else {
        // Otherwise, we need to recursively compute the ODD.

        // If we are already past the maximal level that is to be considered, we can simply create an Odd without
        // successors
        if (currentLevel == maxLevel) {
            uint_fast64_t elseOffset = 0;
            uint_fast64_t thenOffset = 0;

            // If the DD is not the zero leaf, then the then-offset is 1.
            if (dd != Cudd_ReadZero(manager.getManager())) {
                thenOffset = 1;
            }

            auto oddNode = std::make_shared<Odd>(nullptr, elseOffset, nullptr, thenOffset);
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        } else if (ddVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
            // If we skipped the level in the DD, we compute the ODD just for the else-successor and use the same
            // node for the then-successor as well.
            std::shared_ptr<Odd> elseNode = createOddRec(dd, manager, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode = elseNode;
            auto oddNode = std::make_shared<Odd>(elseNode, elseNode->getElseOffset() + elseNode->getThenOffset(), thenNode,
                                                 thenNode->getElseOffset() + thenNode->getThenOffset());
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        } else {
            // Otherwise, we compute the ODDs for both the then- and else successors.
            std::shared_ptr<Odd> elseNode = createOddRec(Cudd_E(dd), manager, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);
            std::shared_ptr<Odd> thenNode = createOddRec(Cudd_T(dd), manager, currentLevel + 1, maxLevel, ddVariableIndices, uniqueTableForLevels);

            uint_fast64_t totalElseOffset = elseNode->getElseOffset() + elseNode->getThenOffset();
            uint_fast64_t totalThenOffset = thenNode->getElseOffset() + thenNode->getThenOffset();

            auto oddNode = std::make_shared<Odd>(elseNode, totalElseOffset, thenNode, totalThenOffset);
            uniqueTableForLevels[currentLevel].emplace(dd, oddNode);
            return oddNode;
        }
    }
}

template<typename ValueType>
InternalDdManager<DdType::CUDD> const& InternalAdd<DdType::CUDD, ValueType>::getInternalDdManager() const {
    return *ddManager;
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                     std::vector<ValueType>& targetVector,
                                                                     std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    forEachRec(this->getCuddDdNode(), 0, ddVariableIndices.size(), 0, odd, ddVariableIndices,
               [&function, &targetVector](uint64_t const& offset, ValueType const& value) { targetVector[offset] = function(targetVector[offset], value); });
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::forEach(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                   std::function<void(uint64_t const&, ValueType const&)> const& function) const {
    forEachRec(this->getCuddDdNode(), 0, ddVariableIndices.size(), 0, odd, ddVariableIndices, function);
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                     std::vector<uint_fast64_t> const& offsets, std::vector<ValueType>& targetVector,
                                                                     std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    forEachRec(this->getCuddDdNode(), 0, ddVariableIndices.size(), 0, odd, ddVariableIndices,
               [&function, &targetVector, &offsets](uint64_t const& offset, ValueType const& value) {
                   ValueType& targetValue = targetVector[offsets[offset]];
                   targetValue = function(targetValue, value);
               });
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::forEachRec(DdNode const* dd, uint_fast64_t currentLevel, uint_fast64_t maxLevel, uint_fast64_t currentOffset,
                                                      Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                      std::function<void(uint64_t const&, ValueType const&)> const& function) const {
    // For the empty DD, we do not need to add any entries.
    if (dd == Cudd_ReadZero(ddManager->getCuddManager().getManager())) {
        return;
    }

    // If we are at the maximal level, the value to be set is stored as a constant in the DD.
    if (currentLevel == maxLevel) {
        function(currentOffset, storm::utility::convertNumber<ValueType>(Cudd_V(dd)));
    } else if (ddVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
        // If we skipped a level, we need to enumerate the explicit entries for the case in which the bit is set
        // and for the one in which it is not set.
        forEachRec(dd, currentLevel + 1, maxLevel, currentOffset, odd.getElseSuccessor(), ddVariableIndices, function);
        forEachRec(dd, currentLevel + 1, maxLevel, currentOffset + odd.getElseOffset(), odd.getThenSuccessor(), ddVariableIndices, function);
    } else {
        // Otherwise, we simply recursively call the function for both (different) cases.
        forEachRec(Cudd_E_const(dd), currentLevel + 1, maxLevel, currentOffset, odd.getElseSuccessor(), ddVariableIndices, function);
        forEachRec(Cudd_T_const(dd), currentLevel + 1, maxLevel, currentOffset + odd.getElseOffset(), odd.getThenSuccessor(), ddVariableIndices, function);
    }
}

template<typename ValueType>
std::vector<uint64_t> InternalAdd<DdType::CUDD, ValueType>::decodeGroupLabels(std::vector<uint_fast64_t> const& ddGroupVariableIndices,
                                                                              storm::storage::BitVector const& ddLabelVariableIndices) const {
    std::vector<uint64_t> result;
    decodeGroupLabelsRec(this->getCuddDdNode(), result, ddGroupVariableIndices, ddLabelVariableIndices, 0, ddGroupVariableIndices.size(), 0);
    return result;
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::decodeGroupLabelsRec(DdNode* dd, std::vector<uint64_t>& labels,
                                                                std::vector<uint_fast64_t> const& ddGroupVariableIndices,
                                                                storm::storage::BitVector const& ddLabelVariableIndices, uint_fast64_t currentLevel,
                                                                uint_fast64_t maxLevel, uint64_t label) const {
    // For the empty DD, we do not need to create a group.
    if (dd == Cudd_ReadZero(ddManager->getCuddManager().getManager())) {
        return;
    }

    if (currentLevel == maxLevel) {
        labels.push_back(label);
    } else {
        uint64_t elseLabel = label;
        uint64_t thenLabel = label;

        if (ddLabelVariableIndices.get(currentLevel)) {
            elseLabel <<= 1;
            thenLabel = (thenLabel << 1) | 1;
        }

        if (ddGroupVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
            decodeGroupLabelsRec(dd, labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, elseLabel);
            decodeGroupLabelsRec(dd, labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, thenLabel);
        } else {
            decodeGroupLabelsRec(Cudd_E(dd), labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, elseLabel);
            decodeGroupLabelsRec(Cudd_T(dd), labels, ddGroupVariableIndices, ddLabelVariableIndices, currentLevel + 1, maxLevel, thenLabel);
        }
    }
}

template<typename ValueType>
std::vector<InternalAdd<DdType::CUDD, ValueType>> InternalAdd<DdType::CUDD, ValueType>::splitIntoGroups(
    std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<InternalAdd<DdType::CUDD, ValueType>> result;
    splitIntoGroupsRec(this->getCuddDdNode(), result, ddGroupVariableIndices, 0, ddGroupVariableIndices.size());
    return result;
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::splitIntoGroupsRec(DdNode* dd, std::vector<InternalAdd<DdType::CUDD, ValueType>>& groups,
                                                              std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel,
                                                              uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    if (dd == Cudd_ReadZero(ddManager->getCuddManager().getManager())) {
        return;
    }

    if (currentLevel == maxLevel) {
        groups.push_back(InternalAdd<DdType::CUDD, ValueType>(ddManager, cudd::ADD(ddManager->getCuddManager(), dd)));
    } else if (ddGroupVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd)) {
        splitIntoGroupsRec(dd, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(dd, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    } else {
        // FIXME: We first traverse the else successor (unlike other variants of this method).
        // Otherwise, the GameBasedMdpModelCheckerTest would not terminate. See github issue #64
        splitIntoGroupsRec(Cudd_E(dd), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(Cudd_T(dd), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    }
}

template<typename ValueType>
std::vector<std::pair<InternalAdd<DdType::CUDD, ValueType>, InternalAdd<DdType::CUDD, ValueType>>> InternalAdd<DdType::CUDD, ValueType>::splitIntoGroups(
    InternalAdd<DdType::CUDD, ValueType> vector, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<std::pair<InternalAdd<DdType::CUDD, ValueType>, InternalAdd<DdType::CUDD, ValueType>>> result;
    splitIntoGroupsRec(this->getCuddDdNode(), vector.getCuddDdNode(), result, ddGroupVariableIndices, 0, ddGroupVariableIndices.size());
    return result;
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::splitIntoGroupsRec(
    DdNode* dd1, DdNode* dd2, std::vector<std::pair<InternalAdd<DdType::CUDD, ValueType>, InternalAdd<DdType::CUDD, ValueType>>>& groups,
    std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel, uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    if (dd1 == Cudd_ReadZero(ddManager->getCuddManager().getManager()) && dd2 == Cudd_ReadZero(ddManager->getCuddManager().getManager())) {
        return;
    }

    if (currentLevel == maxLevel) {
        groups.push_back(std::make_pair(InternalAdd<DdType::CUDD, ValueType>(ddManager, cudd::ADD(ddManager->getCuddManager(), dd1)),
                                        InternalAdd<DdType::CUDD, ValueType>(ddManager, cudd::ADD(ddManager->getCuddManager(), dd2))));
    } else if (ddGroupVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd1)) {
        if (ddGroupVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd2)) {
            splitIntoGroupsRec(dd1, dd2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
            splitIntoGroupsRec(dd1, dd2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        } else {
            splitIntoGroupsRec(dd1, Cudd_T(dd2), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
            splitIntoGroupsRec(dd1, Cudd_E(dd2), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        }
    } else if (ddGroupVariableIndices[currentLevel] < Cudd_NodeReadIndex(dd2)) {
        splitIntoGroupsRec(Cudd_T(dd1), dd2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(Cudd_E(dd1), dd2, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    } else {
        splitIntoGroupsRec(Cudd_T(dd1), Cudd_T(dd2), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(Cudd_E(dd1), Cudd_E(dd2), groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    }
}

template<typename ValueType>
std::vector<std::vector<InternalAdd<DdType::CUDD, ValueType>>> InternalAdd<DdType::CUDD, ValueType>::splitIntoGroups(
    std::vector<InternalAdd<DdType::CUDD, ValueType>> const& vectors, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    std::vector<std::vector<InternalAdd<DdType::CUDD, ValueType>>> result;
    std::vector<DdNode*> dds;
    for (auto const& vector : vectors) {
        dds.push_back(vector.getCuddDdNode());
    }
    dds.push_back(this->getCuddDdNode());

    splitIntoGroupsRec(dds, result, ddGroupVariableIndices, 0, ddGroupVariableIndices.size());
    return result;
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::splitIntoGroupsRec(std::vector<DdNode*> const& dds,
                                                              std::vector<std::vector<InternalAdd<DdType::CUDD, ValueType>>>& groups,
                                                              std::vector<uint_fast64_t> const& ddGroupVariableIndices, uint_fast64_t currentLevel,
                                                              uint_fast64_t maxLevel) const {
    // For the empty DD, we do not need to create a group.
    {
        bool emptyDd = true;
        for (auto const& dd : dds) {
            if (dd != Cudd_ReadZero(ddManager->getCuddManager().getManager())) {
                emptyDd = false;
                break;
            }
        }
        if (emptyDd) {
            return;
        }
    }

    if (currentLevel == maxLevel) {
        std::vector<InternalAdd<DdType::CUDD, ValueType>> newGroup;
        for (auto dd : dds) {
            newGroup.emplace_back(ddManager, cudd::ADD(ddManager->getCuddManager(), dd));
        }
        groups.push_back(std::move(newGroup));
    } else {
        std::vector<DdNode*> thenSubDds(dds), elseSubDds(dds);
        for (uint64_t ddIndex = 0; ddIndex < dds.size(); ++ddIndex) {
            auto const& dd = dds[ddIndex];
            if (ddGroupVariableIndices[currentLevel] == Cudd_NodeReadIndex(dd)) {
                thenSubDds[ddIndex] = Cudd_T(dd);
                elseSubDds[ddIndex] = Cudd_E(dd);
            }
        }
        splitIntoGroupsRec(thenSubDds, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
        splitIntoGroupsRec(elseSubDds, groups, ddGroupVariableIndices, currentLevel + 1, maxLevel);
    }
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::toMatrixComponents(std::vector<uint_fast64_t> const& rowGroupIndices, std::vector<uint_fast64_t>& rowIndications,
                                                              std::vector<storm::storage::MatrixEntry<uint_fast64_t, ValueType>>& columnsAndValues,
                                                              Odd const& rowOdd, Odd const& columnOdd, std::vector<uint_fast64_t> const& ddRowVariableIndices,
                                                              std::vector<uint_fast64_t> const& ddColumnVariableIndices, bool writeValues) const {
    return toMatrixComponentsRec(this->getCuddDdNode(), rowGroupIndices, rowIndications, columnsAndValues, rowOdd, columnOdd, 0, 0,
                                 ddRowVariableIndices.size() + ddColumnVariableIndices.size(), 0, 0, ddRowVariableIndices, ddColumnVariableIndices,
                                 writeValues);
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::toMatrixComponentsRec(DdNode const* dd, std::vector<uint_fast64_t> const& rowGroupOffsets,
                                                                 std::vector<uint_fast64_t>& rowIndications,
                                                                 std::vector<storm::storage::MatrixEntry<uint_fast64_t, ValueType>>& columnsAndValues,
                                                                 Odd const& rowOdd, Odd const& columnOdd, uint_fast64_t currentRowLevel,
                                                                 uint_fast64_t currentColumnLevel, uint_fast64_t maxLevel, uint_fast64_t currentRowOffset,
                                                                 uint_fast64_t currentColumnOffset, std::vector<uint_fast64_t> const& ddRowVariableIndices,
                                                                 std::vector<uint_fast64_t> const& ddColumnVariableIndices, bool generateValues) const {
    // For the empty DD, we do not need to add any entries.
    if (dd == Cudd_ReadZero(ddManager->getCuddManager().getManager())) {
        return;
    }

    // If we are at the maximal level, the value to be set is stored as a constant in the DD.
    if (currentRowLevel + currentColumnLevel == maxLevel) {
        if (generateValues) {
            columnsAndValues[rowIndications[rowGroupOffsets[currentRowOffset]]] =
                storm::storage::MatrixEntry<uint_fast64_t, ValueType>(currentColumnOffset, storm::utility::convertNumber<ValueType>(Cudd_V(dd)));
        }
        ++rowIndications[rowGroupOffsets[currentRowOffset]];
    } else {
        DdNode const* elseElse;
        DdNode const* elseThen;
        DdNode const* thenElse;
        DdNode const* thenThen;

        if (ddColumnVariableIndices[currentColumnLevel] < Cudd_NodeReadIndex(dd)) {
            elseElse = elseThen = thenElse = thenThen = dd;
        } else if (ddRowVariableIndices[currentColumnLevel] < Cudd_NodeReadIndex(dd)) {
            elseElse = thenElse = Cudd_E_const(dd);
            elseThen = thenThen = Cudd_T_const(dd);
        } else {
            DdNode const* elseNode = Cudd_E_const(dd);
            if (ddColumnVariableIndices[currentColumnLevel] < Cudd_NodeReadIndex(elseNode)) {
                elseElse = elseThen = elseNode;
            } else {
                elseElse = Cudd_E_const(elseNode);
                elseThen = Cudd_T_const(elseNode);
            }

            DdNode const* thenNode = Cudd_T_const(dd);
            if (ddColumnVariableIndices[currentColumnLevel] < Cudd_NodeReadIndex(thenNode)) {
                thenElse = thenThen = thenNode;
            } else {
                thenElse = Cudd_E_const(thenNode);
                thenThen = Cudd_T_const(thenNode);
            }
        }

        // Visit else-else.
        toMatrixComponentsRec(elseElse, rowGroupOffsets, rowIndications, columnsAndValues, rowOdd.getElseSuccessor(), columnOdd.getElseSuccessor(),
                              currentRowLevel + 1, currentColumnLevel + 1, maxLevel, currentRowOffset, currentColumnOffset, ddRowVariableIndices,
                              ddColumnVariableIndices, generateValues);
        // Visit else-then.
        toMatrixComponentsRec(elseThen, rowGroupOffsets, rowIndications, columnsAndValues, rowOdd.getElseSuccessor(), columnOdd.getThenSuccessor(),
                              currentRowLevel + 1, currentColumnLevel + 1, maxLevel, currentRowOffset, currentColumnOffset + columnOdd.getElseOffset(),
                              ddRowVariableIndices, ddColumnVariableIndices, generateValues);
        // Visit then-else.
        toMatrixComponentsRec(thenElse, rowGroupOffsets, rowIndications, columnsAndValues, rowOdd.getThenSuccessor(), columnOdd.getElseSuccessor(),
                              currentRowLevel + 1, currentColumnLevel + 1, maxLevel, currentRowOffset + rowOdd.getElseOffset(), currentColumnOffset,
                              ddRowVariableIndices, ddColumnVariableIndices, generateValues);
        // Visit then-then.
        toMatrixComponentsRec(thenThen, rowGroupOffsets, rowIndications, columnsAndValues, rowOdd.getThenSuccessor(), columnOdd.getThenSuccessor(),
                              currentRowLevel + 1, currentColumnLevel + 1, maxLevel, currentRowOffset + rowOdd.getElseOffset(),
                              currentColumnOffset + columnOdd.getElseOffset(), ddRowVariableIndices, ddColumnVariableIndices, generateValues);
    }
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::fromVector(InternalDdManager<DdType::CUDD> const* ddManager,
                                                                                      std::vector<ValueType> const& values, storm::dd::Odd const& odd,
                                                                                      std::vector<uint_fast64_t> const& ddVariableIndices) {
    uint_fast64_t offset = 0;
    return InternalAdd<DdType::CUDD, ValueType>(
        ddManager, cudd::ADD(ddManager->getCuddManager(),
                             fromVectorRec(ddManager->getCuddManager().getManager(), offset, 0, ddVariableIndices.size(), values, odd, ddVariableIndices)));
}

template<typename ValueType>
DdNode* InternalAdd<DdType::CUDD, ValueType>::fromVectorRec(::DdManager* manager, uint_fast64_t& currentOffset, uint_fast64_t currentLevel,
                                                            uint_fast64_t maxLevel, std::vector<ValueType> const& values, Odd const& odd,
                                                            std::vector<uint_fast64_t> const& ddVariableIndices) {
    if (currentLevel == maxLevel) {
        // If we are in a terminal node of the ODD, we need to check whether the then-offset of the ODD is one
        // (meaning the encoding is a valid one) or zero (meaning the encoding is not valid). Consequently, we
        // need to copy the next value of the vector iff the then-offset is greater than zero.
        if (odd.getThenOffset() > 0) {
            return Cudd_addConst(manager, values[currentOffset++]);
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
            elseSuccessor = fromVectorRec(manager, currentOffset, currentLevel + 1, maxLevel, values, odd.getElseSuccessor(), ddVariableIndices);
        } else {
            elseSuccessor = Cudd_ReadZero(manager);
        }
        Cudd_Ref(elseSuccessor);

        // Determine the new then-successor.
        DdNode* thenSuccessor = nullptr;
        if (odd.getThenOffset() > 0) {
            thenSuccessor = fromVectorRec(manager, currentOffset, currentLevel + 1, maxLevel, values, odd.getThenSuccessor(), ddVariableIndices);
        } else {
            thenSuccessor = Cudd_ReadZero(manager);
        }
        Cudd_Ref(thenSuccessor);

        // Create a node representing ITE(currentVar, thenSuccessor, elseSuccessor);
        DdNode* result = Cudd_addIthVar(manager, static_cast<int>(ddVariableIndices[currentLevel]));
        Cudd_Ref(result);
        DdNode* newResult = Cudd_addIte(manager, result, thenSuccessor, elseSuccessor);
        Cudd_Ref(newResult);

        // Dispose of the intermediate results
        Cudd_RecursiveDeref(manager, result);
        Cudd_RecursiveDeref(manager, thenSuccessor);
        Cudd_RecursiveDeref(manager, elseSuccessor);

        // Before returning, we remove the protection imposed by the previous call to Cudd_Ref.
        Cudd_Deref(newResult);

        return newResult;
    }
}

template<>
DdNode* InternalAdd<DdType::CUDD, storm::RationalNumber>::fromVectorRec(::DdManager* manager, uint_fast64_t& currentOffset, uint_fast64_t currentLevel,
                                                                        uint_fast64_t maxLevel, std::vector<storm::RationalNumber> const& values,
                                                                        Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

#else
template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::operator==(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::operator!=(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator+(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator+=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator*(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator*=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator-(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator-=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::operator/(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType>& InternalAdd<DdType::CUDD, ValueType>::operator/=(InternalAdd<DdType::CUDD, ValueType> const& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::equals(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::notEquals(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::less(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::lessOrEqual(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greater(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greaterOrEqual(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::pow(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::mod(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::logxy(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::floor() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::ceil() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, storm::RationalNumber> InternalAdd<DdType::CUDD, ValueType>::sharpenKwekMehlhorn(size_t /*precision*/) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::minimum(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::maximum(InternalAdd<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
template<typename TargetValueType>
typename std::enable_if<std::is_same<ValueType, TargetValueType>::value, InternalAdd<DdType::CUDD, TargetValueType>>::type
InternalAdd<DdType::CUDD, ValueType>::toValueType() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
template<typename TargetValueType>
typename std::enable_if<!std::is_same<ValueType, TargetValueType>::value, InternalAdd<DdType::CUDD, TargetValueType>>::type
InternalAdd<DdType::CUDD, ValueType>::toValueType() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::sumAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::minAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::minAbstractRepresentative(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::maxAbstract(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::maxAbstractRepresentative(InternalBdd<DdType::CUDD> const& cube) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::equalModuloPrecision(InternalAdd<DdType::CUDD, ValueType> const& other, ValueType const& precision,
                                                                bool relative) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<>
bool InternalAdd<DdType::CUDD, storm::RationalNumber>::equalModuloPrecision(InternalAdd<DdType::CUDD, storm::RationalNumber> const& /*other*/,
                                                                            storm::RationalNumber const& /*precision*/, bool /*relative*/) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::swapVariables(std::vector<InternalBdd<DdType::CUDD>> const& from,
                                                                                         std::vector<InternalBdd<DdType::CUDD>> const& to) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::permuteVariables(std::vector<InternalBdd<DdType::CUDD>> const& from,
                                                                                            std::vector<InternalBdd<DdType::CUDD>> const& to) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::multiplyMatrix(
    InternalAdd<DdType::CUDD, ValueType> const& otherMatrix, std::vector<InternalBdd<DdType::CUDD>> const& summationDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::multiplyMatrix(
    InternalBdd<DdType::CUDD> const& otherMatrix, std::vector<InternalBdd<DdType::CUDD>> const& summationDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greater(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::greater(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::greaterOrEqual(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::greaterOrEqual(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::less(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::less(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::lessOrEqual(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, storm::RationalNumber>::lessOrEqual(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::notZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::constrain(InternalAdd<DdType::CUDD, ValueType> const& constraint) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::restrict(InternalAdd<DdType::CUDD, ValueType> const& constraint) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalBdd<DdType::CUDD> InternalAdd<DdType::CUDD, ValueType>::getSupport() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getNonZeroCount(uint_fast64_t numberOfDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getLeafCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getNodeCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
ValueType InternalAdd<DdType::CUDD, ValueType>::getMin() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
ValueType InternalAdd<DdType::CUDD, ValueType>::getMax() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
ValueType InternalAdd<DdType::CUDD, ValueType>::getValue() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::isOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::isZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
bool InternalAdd<DdType::CUDD, ValueType>::isConstant() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getIndex() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
uint_fast64_t InternalAdd<DdType::CUDD, ValueType>::getLevel() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::exportToDot(std::string const& filename, std::vector<std::string> const& ddVariableNamesAsStrings,
                                                       bool showVariablesIfPossible) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::exportToText(std::string const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::begin(DdManager<DdType::CUDD> const& fullDdManager, InternalBdd<DdType::CUDD> const&,
                                                                                 uint_fast64_t, std::set<storm::expressions::Variable> const& metaVariables,
                                                                                 bool enumerateDontCareMetaVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::end(DdManager<DdType::CUDD> const& fullDdManager) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
std::string InternalAdd<DdType::CUDD, ValueType>::getStringId() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
Odd InternalAdd<DdType::CUDD, ValueType>::createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalDdManager<DdType::CUDD> const& InternalAdd<DdType::CUDD, ValueType>::getInternalDdManager() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                     std::vector<ValueType>& targetVector,
                                                                     std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::forEach(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                   std::function<void(uint64_t const&, ValueType const&)> const& function) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::composeWithExplicitVector(storm::dd::Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices,
                                                                     std::vector<uint_fast64_t> const& offsets, std::vector<ValueType>& targetVector,
                                                                     std::function<ValueType(ValueType const&, ValueType const&)> const& function) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
std::vector<uint64_t> InternalAdd<DdType::CUDD, ValueType>::decodeGroupLabels(std::vector<uint_fast64_t> const& ddGroupVariableIndices,
                                                                              storm::storage::BitVector const& ddLabelVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
std::vector<InternalAdd<DdType::CUDD, ValueType>> InternalAdd<DdType::CUDD, ValueType>::splitIntoGroups(
    std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
std::vector<std::pair<InternalAdd<DdType::CUDD, ValueType>, InternalAdd<DdType::CUDD, ValueType>>> InternalAdd<DdType::CUDD, ValueType>::splitIntoGroups(
    InternalAdd<DdType::CUDD, ValueType> vector, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
std::vector<std::vector<InternalAdd<DdType::CUDD, ValueType>>> InternalAdd<DdType::CUDD, ValueType>::splitIntoGroups(
    std::vector<InternalAdd<DdType::CUDD, ValueType>> const& vectors, std::vector<uint_fast64_t> const& ddGroupVariableIndices) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
void InternalAdd<DdType::CUDD, ValueType>::toMatrixComponents(std::vector<uint_fast64_t> const& rowGroupIndices, std::vector<uint_fast64_t>& rowIndications,
                                                              std::vector<storm::storage::MatrixEntry<uint_fast64_t, ValueType>>& columnsAndValues,
                                                              Odd const& rowOdd, Odd const& columnOdd, std::vector<uint_fast64_t> const& ddRowVariableIndices,
                                                              std::vector<uint_fast64_t> const& ddColumnVariableIndices, bool writeValues) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalAdd<DdType::CUDD, ValueType>::fromVector(InternalDdManager<DdType::CUDD> const* ddManager,
                                                                                      std::vector<ValueType> const& values, storm::dd::Odd const& odd,
                                                                                      std::vector<uint_fast64_t> const& ddVariableIndices) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

#endif

template class InternalAdd<DdType::CUDD, double>;
template InternalAdd<DdType::CUDD, double> InternalAdd<DdType::CUDD, double>::toValueType<double>() const;
template class InternalAdd<DdType::CUDD, uint_fast64_t>;
template InternalAdd<DdType::CUDD, uint_fast64_t> InternalAdd<DdType::CUDD, uint_fast64_t>::toValueType<uint_fast64_t>() const;

template class InternalAdd<DdType::CUDD, storm::RationalNumber>;
template InternalAdd<DdType::CUDD, storm::RationalNumber> InternalAdd<DdType::CUDD, storm::RationalNumber>::toValueType<storm::RationalNumber>() const;
template InternalAdd<DdType::CUDD, storm::RationalNumber> InternalAdd<DdType::CUDD, double>::toValueType<storm::RationalNumber>() const;
template InternalAdd<DdType::CUDD, double> InternalAdd<DdType::CUDD, storm::RationalNumber>::toValueType<double>() const;
}  // namespace dd
}  // namespace storm