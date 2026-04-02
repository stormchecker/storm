#include "storm/models/sparse/StandardRewardModel.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/storage/SparseMatrixOperations.h"
#include "storm/utility/rationalfunction.h"
#include "storm/utility/vector.h"

namespace storm {
namespace models {
namespace sparse {
template<typename ValueType>
StandardRewardModel<ValueType>::StandardRewardModel(std::optional<std::vector<ValueType>> const& optionalStateRewardVector,
                                                    std::optional<std::vector<ValueType>> const& optionalStateActionRewardVector,
                                                    std::optional<storm::storage::SparseMatrix<ValueType>> const& optionalTransitionRewardMatrix)
    : optionalStateRewardVector(optionalStateRewardVector),
      optionalStateActionRewardVector(optionalStateActionRewardVector),
      optionalTransitionRewardMatrix(optionalTransitionRewardMatrix) {
    // Intentionally left empty.
}

template<typename ValueType>
StandardRewardModel<ValueType>::StandardRewardModel(std::optional<std::vector<ValueType>>&& optionalStateRewardVector,
                                                    std::optional<std::vector<ValueType>>&& optionalStateActionRewardVector,
                                                    std::optional<storm::storage::SparseMatrix<ValueType>>&& optionalTransitionRewardMatrix)
    : optionalStateRewardVector(std::move(optionalStateRewardVector)),
      optionalStateActionRewardVector(std::move(optionalStateActionRewardVector)),
      optionalTransitionRewardMatrix(std::move(optionalTransitionRewardMatrix)) {
    // Intentionally left empty.
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::hasStateRewards() const {
    return static_cast<bool>(this->optionalStateRewardVector);
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::hasOnlyStateRewards() const {
    return static_cast<bool>(this->optionalStateRewardVector) && !static_cast<bool>(this->optionalStateActionRewardVector) &&
           !static_cast<bool>(this->optionalTransitionRewardMatrix);
}

template<typename ValueType>
std::vector<ValueType> const& StandardRewardModel<ValueType>::getStateRewardVector() const {
    STORM_LOG_ASSERT(this->hasStateRewards(), "No state rewards available.");
    return this->optionalStateRewardVector.value();
}

template<typename ValueType>
std::vector<ValueType>& StandardRewardModel<ValueType>::getStateRewardVector() {
    STORM_LOG_ASSERT(this->hasStateRewards(), "No state rewards available.");
    return this->optionalStateRewardVector.value();
}

template<typename ValueType>
std::optional<std::vector<ValueType>> const& StandardRewardModel<ValueType>::getOptionalStateRewardVector() const {
    return this->optionalStateRewardVector;
}

template<typename ValueType>
ValueType const& StandardRewardModel<ValueType>::getStateReward(uint_fast64_t state) const {
    STORM_LOG_ASSERT(this->hasStateRewards(), "No state rewards available.");
    STORM_LOG_ASSERT(state < this->optionalStateRewardVector.value().size(), "Invalid state.");
    return this->optionalStateRewardVector.value()[state];
}

template<typename ValueType>
template<typename T>
void StandardRewardModel<ValueType>::setStateReward(uint_fast64_t state, T const& newReward) {
    STORM_LOG_ASSERT(this->hasStateRewards(), "No state rewards available.");
    STORM_LOG_ASSERT(state < this->optionalStateRewardVector.value().size(), "Invalid state.");
    this->optionalStateRewardVector.value()[state] = newReward;
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::hasStateActionRewards() const {
    return static_cast<bool>(this->optionalStateActionRewardVector);
}

template<typename ValueType>
std::vector<ValueType> const& StandardRewardModel<ValueType>::getStateActionRewardVector() const {
    STORM_LOG_ASSERT(this->hasStateActionRewards(), "No state action rewards available.");
    return this->optionalStateActionRewardVector.value();
}

template<typename ValueType>
std::vector<ValueType>& StandardRewardModel<ValueType>::getStateActionRewardVector() {
    STORM_LOG_ASSERT(this->hasStateActionRewards(), "No state action rewards available.");
    return this->optionalStateActionRewardVector.value();
}

template<typename ValueType>
ValueType const& StandardRewardModel<ValueType>::getStateActionReward(uint_fast64_t choiceIndex) const {
    STORM_LOG_ASSERT(this->hasStateActionRewards(), "No state action rewards available.");
    STORM_LOG_ASSERT(choiceIndex < this->optionalStateActionRewardVector.value().size(), "Invalid choiceIndex.");
    return this->optionalStateActionRewardVector.value()[choiceIndex];
}

template<typename ValueType>
template<typename T>
void StandardRewardModel<ValueType>::setStateActionReward(uint_fast64_t choiceIndex, T const& newValue) {
    STORM_LOG_ASSERT(this->hasStateActionRewards(), "No state action rewards available.");
    STORM_LOG_ASSERT(choiceIndex < this->optionalStateActionRewardVector.value().size(), "Invalid choiceIndex.");
    this->optionalStateActionRewardVector.value()[choiceIndex] = newValue;
}

template<typename ValueType>
std::optional<std::vector<ValueType>> const& StandardRewardModel<ValueType>::getOptionalStateActionRewardVector() const {
    return this->optionalStateActionRewardVector;
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::hasTransitionRewards() const {
    return static_cast<bool>(this->optionalTransitionRewardMatrix);
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> const& StandardRewardModel<ValueType>::getTransitionRewardMatrix() const {
    return this->optionalTransitionRewardMatrix.value();
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType>& StandardRewardModel<ValueType>::getTransitionRewardMatrix() {
    return this->optionalTransitionRewardMatrix.value();
}

template<typename ValueType>
std::optional<storm::storage::SparseMatrix<ValueType>> const& StandardRewardModel<ValueType>::getOptionalTransitionRewardMatrix() const {
    return this->optionalTransitionRewardMatrix;
}

template<typename ValueType>
StandardRewardModel<ValueType> StandardRewardModel<ValueType>::restrictActions(storm::storage::BitVector const& enabledActions) const {
    std::optional<std::vector<ValueType>> newStateRewardVector(this->getOptionalStateRewardVector());
    std::optional<std::vector<ValueType>> newStateActionRewardVector;
    if (this->hasStateActionRewards()) {
        newStateActionRewardVector = std::vector<ValueType>(enabledActions.getNumberOfSetBits());
        storm::utility::vector::selectVectorValues(newStateActionRewardVector.value(), enabledActions, this->getStateActionRewardVector());
    }
    std::optional<storm::storage::SparseMatrix<ValueType>> newTransitionRewardMatrix;
    if (this->hasTransitionRewards()) {
        newTransitionRewardMatrix = this->getTransitionRewardMatrix().restrictRows(enabledActions);
    }
    return StandardRewardModel(std::move(newStateRewardVector), std::move(newStateActionRewardVector), std::move(newTransitionRewardMatrix));
}

template<typename ValueType>
StandardRewardModel<ValueType> StandardRewardModel<ValueType>::permuteActions(std::vector<uint64_t> const& inversePermutation) const {
    std::optional<std::vector<ValueType>> newStateRewardVector(this->getOptionalStateRewardVector());
    std::optional<std::vector<ValueType>> newStateActionRewardVector;
    if (this->hasStateActionRewards()) {
        newStateActionRewardVector = storm::utility::vector::applyInversePermutation(inversePermutation, this->getStateActionRewardVector());
    }
    std::optional<storm::storage::SparseMatrix<ValueType>> newTransitionRewardMatrix;
    if (this->hasTransitionRewards()) {
        newTransitionRewardMatrix = this->getTransitionRewardMatrix().permuteRows(inversePermutation);
    }
    return StandardRewardModel(std::move(newStateRewardVector), std::move(newStateActionRewardVector), std::move(newTransitionRewardMatrix));
}

template<typename ValueType>
StandardRewardModel<ValueType> StandardRewardModel<ValueType>::permuteStates(std::vector<uint64_t> const& inversePermutation,
                                                                             storm::OptionalRef<std::vector<uint64_t> const> rowGroupIndices,
                                                                             storm::OptionalRef<std::vector<uint64_t> const> permutation) const {
    std::optional<std::vector<ValueType>> newStateRewardVector;
    if (hasStateRewards()) {
        newStateRewardVector = storm::utility::vector::applyInversePermutation(inversePermutation, getStateRewardVector());
    }
    std::optional<std::vector<ValueType>> newStateActionRewardVector;
    if (this->hasStateActionRewards()) {
        if (rowGroupIndices) {
            newStateActionRewardVector =
                storm::utility::vector::applyInversePermutationToGroupedVector(inversePermutation, this->getStateActionRewardVector(), rowGroupIndices.value());
        } else {
            STORM_LOG_ASSERT(inversePermutation.size() == this->getStateActionRewardVector().size(), "Invalid permutation size.");
            newStateActionRewardVector = storm::utility::vector::applyInversePermutation(inversePermutation, this->getStateActionRewardVector());
        }
    }
    std::optional<storm::storage::SparseMatrix<ValueType>> newTransitionRewardMatrix;
    if (this->hasTransitionRewards()) {
        STORM_LOG_ASSERT(permutation, "Permutation required for transition rewards.");
        STORM_LOG_ASSERT(rowGroupIndices.has_value() != getTransitionRewardMatrix().hasTrivialRowGrouping(),
                         "Row group indices required for transition rewards.");
        this->getTransitionRewardMatrix().permuteRowGroupsAndColumns(inversePermutation, permutation.value());
    }

    return StandardRewardModel(std::move(newStateRewardVector), std::move(newStateActionRewardVector), std::move(newTransitionRewardMatrix));
}

template<typename ValueType>
template<typename MatrixValueType>
ValueType StandardRewardModel<ValueType>::getStateActionAndTransitionReward(uint_fast64_t choiceIndex,
                                                                            storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const {
    ValueType result = this->hasStateActionRewards() ? this->getStateActionReward(choiceIndex) : storm::utility::zero<ValueType>();
    if (this->hasTransitionRewards()) {
        result += transitionMatrix.getPointwiseProductRowSum(getTransitionRewardMatrix(), choiceIndex);
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
ValueType StandardRewardModel<ValueType>::getTotalStateActionReward(uint_fast64_t stateIndex, uint_fast64_t choiceIndex,
                                                                    storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                                    MatrixValueType const& stateRewardWeight, MatrixValueType const& actionRewardWeight) const {
    ValueType result = actionRewardWeight * getStateActionAndTransitionReward(choiceIndex, transitionMatrix);
    if (this->hasStateRewards()) {
        result += stateRewardWeight * this->getStateReward(stateIndex);
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
void StandardRewardModel<ValueType>::reduceToStateBasedRewards(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix, bool reduceToStateRewards,
                                                               std::vector<MatrixValueType> const* weights) {
    if (this->hasTransitionRewards()) {
        if (this->hasStateActionRewards()) {
            storm::utility::vector::addVectors<ValueType>(this->getStateActionRewardVector(),
                                                          transitionMatrix.getPointwiseProductRowSumVector(this->getTransitionRewardMatrix()),
                                                          this->getStateActionRewardVector());
            this->optionalTransitionRewardMatrix = std::nullopt;
        } else {
            this->optionalStateActionRewardVector = transitionMatrix.getPointwiseProductRowSumVector(this->getTransitionRewardMatrix());
        }
    }

    if (reduceToStateRewards && this->hasStateActionRewards()) {
        STORM_LOG_THROW(transitionMatrix.getRowGroupCount() == this->getStateActionRewardVector().size(), storm::exceptions::InvalidOperationException,
                        "The reduction to state rewards is only possible if the size of the action reward vector equals the number of states.");
        if (weights) {
            if (this->hasStateRewards()) {
                storm::utility::vector::applyPointwiseTernary<ValueType, MatrixValueType, ValueType>(
                    this->getStateActionRewardVector(), *weights, this->getStateRewardVector(),
                    [](ValueType const& sar, MatrixValueType const& w, ValueType const& sr) -> ValueType { return sr + w * sar; });
            } else {
                this->optionalStateRewardVector = std::move(this->optionalStateActionRewardVector);
                storm::utility::vector::applyPointwise<ValueType, MatrixValueType, ValueType, std::multiplies<>>(
                    this->optionalStateRewardVector.value(), *weights, this->optionalStateRewardVector.value());
            }
        } else {
            if (this->hasStateRewards()) {
                storm::utility::vector::addVectors<ValueType>(this->getStateActionRewardVector(), this->getStateRewardVector(), this->getStateRewardVector());
            } else {
                this->optionalStateRewardVector = std::move(this->optionalStateActionRewardVector);
            }
        }
        this->optionalStateActionRewardVector = std::nullopt;
    }
}

template<typename ValueType>
template<typename MatrixValueType>
std::vector<ValueType> StandardRewardModel<ValueType>::getTotalRewardVector(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const {
    std::vector<ValueType> result = this->hasTransitionRewards() ? transitionMatrix.getPointwiseProductRowSumVector(this->getTransitionRewardMatrix())
                                                                 : (this->hasStateActionRewards() ? this->getStateActionRewardVector()
                                                                                                  : std::vector<ValueType>(transitionMatrix.getRowCount()));
    if (this->hasStateActionRewards() && this->hasTransitionRewards()) {
        storm::utility::vector::addVectors(result, this->getStateActionRewardVector(), result);
    }
    if (this->hasStateRewards()) {
        storm::utility::vector::addVectorToGroupedVector(result, this->getStateRewardVector(), transitionMatrix.getRowGroupIndices());
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
std::vector<ValueType> StandardRewardModel<ValueType>::getTotalRewardVector(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                                            std::vector<MatrixValueType> const& weights) const {
    std::vector<ValueType> result;
    if (this->hasTransitionRewards()) {
        result = transitionMatrix.getPointwiseProductRowSumVector(this->getTransitionRewardMatrix());
        storm::utility::vector::applyPointwiseTernary<MatrixValueType, ValueType, ValueType>(
            weights, this->getStateActionRewardVector(), result,
            [](MatrixValueType const& weight, ValueType const& rewardElement, ValueType const& resultElement) {
                return weight * (resultElement + rewardElement);
            });
    } else {
        result = std::vector<ValueType>(transitionMatrix.getRowCount());
        if (this->hasStateActionRewards()) {
            storm::utility::vector::applyPointwise<MatrixValueType, ValueType, ValueType>(
                weights, this->getStateActionRewardVector(), result,
                [](MatrixValueType const& weight, ValueType const& rewardElement) { return weight * rewardElement; });
        }
    }
    if (this->hasStateRewards()) {
        storm::utility::vector::addVectorToGroupedVector(result, this->getStateRewardVector(), transitionMatrix.getRowGroupIndices());
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
std::vector<ValueType> StandardRewardModel<ValueType>::getTotalRewardVector(uint_fast64_t numberOfRows,
                                                                            storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                                            storm::storage::BitVector const& filter) const {
    std::vector<ValueType> result(numberOfRows, storm::utility::zero<ValueType>());
    if (this->hasTransitionRewards()) {
        std::vector<ValueType> pointwiseProductRowSumVector = transitionMatrix.getPointwiseProductRowSumVector(this->getTransitionRewardMatrix());
        storm::utility::vector::selectVectorValues(result, filter, transitionMatrix.getRowGroupIndices(), pointwiseProductRowSumVector);
    }

    if (this->hasStateActionRewards()) {
        storm::utility::vector::addFilteredVectorGroupsToGroupedVector(result, this->getStateActionRewardVector(), filter,
                                                                       transitionMatrix.getRowGroupIndices());
    }
    if (this->hasStateRewards()) {
        storm::utility::vector::addFilteredVectorToGroupedVector(result, this->getStateRewardVector(), filter, transitionMatrix.getRowGroupIndices());
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
std::vector<ValueType> StandardRewardModel<ValueType>::getTotalActionRewardVector(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                                                  std::vector<MatrixValueType> const& stateRewardWeights) const {
    std::vector<ValueType> result;
    if (this->hasTransitionRewards()) {
        result = transitionMatrix.getPointwiseProductRowSumVector(this->getTransitionRewardMatrix());
    } else {
        result = std::vector<ValueType>(transitionMatrix.getRowCount());
    }
    if (this->hasStateActionRewards()) {
        storm::utility::vector::addVectors(result, this->getStateActionRewardVector(), result);
    }
    if (this->hasStateRewards()) {
        std::vector<ValueType> scaledStateRewardVector(transitionMatrix.getRowGroupCount());
        storm::utility::vector::multiplyVectorsPointwise(this->getStateRewardVector(), stateRewardWeights, scaledStateRewardVector);
        storm::utility::vector::addVectorToGroupedVector(result, scaledStateRewardVector, transitionMatrix.getRowGroupIndices());
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::BitVector StandardRewardModel<ValueType>::getStatesWithZeroReward(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const {
    return getStatesWithFilter(transitionMatrix, storm::utility::isZero<ValueType>);
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::BitVector StandardRewardModel<ValueType>::getStatesWithFilter(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                                              std::function<bool(ValueType const&)> const& filter) const {
    storm::storage::BitVector result = this->hasStateRewards() ? storm::utility::vector::filter(this->getStateRewardVector(), filter)
                                                               : storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true);
    if (this->hasStateActionRewards()) {
        for (uint_fast64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
            for (uint_fast64_t row = transitionMatrix.getRowGroupIndices()[state]; row < transitionMatrix.getRowGroupIndices()[state + 1]; ++row) {
                if (!filter(this->getStateActionRewardVector()[row])) {
                    result.set(state, false);
                    break;
                }
            }
        }
    }
    if (this->hasTransitionRewards()) {
        for (uint_fast64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
            for (auto const& rewardMatrixEntry : this->getTransitionRewardMatrix().getRowGroup(state)) {
                if (!filter(rewardMatrixEntry.getValue())) {
                    result.set(state, false);
                    break;
                }
            }
        }
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::BitVector StandardRewardModel<ValueType>::getChoicesWithZeroReward(
    storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const {
    return getChoicesWithFilter(transitionMatrix, storm::utility::isZero<ValueType>);
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::BitVector StandardRewardModel<ValueType>::getChoicesWithFilter(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                                               std::function<bool(ValueType const&)> const& filter) const {
    storm::storage::BitVector result;
    if (this->hasStateActionRewards()) {
        result = storm::utility::vector::filter(this->getStateActionRewardVector(), filter);
        if (this->hasStateRewards()) {
            result &= transitionMatrix.getRowFilter(storm::utility::vector::filter(this->getStateRewardVector(), filter));
        }
    } else {
        if (this->hasStateRewards()) {
            result = transitionMatrix.getRowFilter(storm::utility::vector::filter(this->getStateRewardVector(), filter));
        } else {
            result = storm::storage::BitVector(transitionMatrix.getRowCount(), true);
        }
    }
    if (this->hasTransitionRewards()) {
        for (uint_fast64_t row = 0; row < transitionMatrix.getRowCount(); ++row) {
            for (auto const& rewardMatrixEntry : this->getTransitionRewardMatrix().getRow(row)) {
                if (!filter(rewardMatrixEntry.getValue())) {
                    result.set(row, false);
                    break;
                }
            }
        }
    }
    return result;
}

template<typename ValueType>
template<typename MatrixValueType>
void StandardRewardModel<ValueType>::clearRewardAtState(uint_fast64_t state, storm::storage::SparseMatrix<MatrixValueType> const& transitions) {
    if (hasStateRewards()) {
        getStateRewardVector()[state] = storm::utility::zero<ValueType>();
    }
    if (hasStateActionRewards()) {
        for (uint_fast64_t choice = transitions.getRowGroupIndices()[state]; choice < transitions.getRowGroupIndices()[state + 1]; ++choice) {
            getStateActionRewardVector()[choice] = storm::utility::zero<ValueType>();
        }
    }
    if (hasTransitionRewards()) {
        for (auto& entry : getTransitionRewardMatrix().getRowGroup(state)) {
            entry.setValue(storm::utility::zero<ValueType>());
        }
    }
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::empty() const {
    return !(static_cast<bool>(this->optionalStateRewardVector) || static_cast<bool>(this->optionalStateActionRewardVector) ||
             static_cast<bool>(this->optionalTransitionRewardMatrix));
}

/*!
 * Auxiliary function to check if the given predicate is true for any of the reward values in the given reward model.
 */
template<typename ValueType>
bool anyOfRewardValues(StandardRewardModel<ValueType> const& rewardModel, auto const& predicate) {
    if (rewardModel.hasStateRewards() && std::any_of(rewardModel.getStateRewardVector().begin(), rewardModel.getStateRewardVector().end(), predicate)) {
        return true;
    }
    if (rewardModel.hasStateActionRewards() &&
        std::any_of(rewardModel.getStateActionRewardVector().begin(), rewardModel.getStateActionRewardVector().end(), predicate)) {
        return true;
    }
    if (rewardModel.hasTransitionRewards()) {
        for (auto const& entry : rewardModel.getTransitionRewardMatrix()) {
            if (predicate(entry.getValue())) {
                return true;
            }
        }
    }
    return false;
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::isAllZero() const {
    bool const hasNonZeroReward = anyOfRewardValues(*this, [](auto&& value) { return !storm::utility::isZero<ValueType>(value); });
    return !hasNonZeroReward;
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::hasNegativeRewards() const {
    if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
        // Rational functions can never be negative.
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Checking Rational functions for negativity is not possible.");
        return false;
    } else {
        return anyOfRewardValues(*this, [](auto&& value) { return value < storm::utility::zero<ValueType>(); });
    }
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::hasPositiveRewards() const {
    if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
        // Rational functions can never be negative.
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Checking Rational functions for negativity is not possible.");
        return false;
    } else {
        return anyOfRewardValues(*this, [](auto&& value) { return value > storm::utility::zero<ValueType>(); });
    }
}

template<typename ValueType>
bool StandardRewardModel<ValueType>::isCompatible(uint_fast64_t nrStates, uint_fast64_t nrChoices) const {
    if (hasStateRewards()) {
        if (optionalStateRewardVector.value().size() != nrStates)
            return false;
    }
    if (hasStateActionRewards()) {
        if (optionalStateActionRewardVector.value().size() != nrChoices)
            return false;
    }
    return true;
}

template<typename ValueType>
std::size_t StandardRewardModel<ValueType>::hash() const {
    size_t seed = 0;
    if (hasStateRewards()) {
        boost::hash_combine(seed, boost::hash_range(optionalStateRewardVector->begin(), optionalStateRewardVector->end()));
    }
    if (hasStateActionRewards()) {
        boost::hash_combine(seed, boost::hash_range(optionalStateActionRewardVector->begin(), optionalStateActionRewardVector->end()));
    }
    if (hasTransitionRewards()) {
        boost::hash_combine(seed, optionalTransitionRewardMatrix->hash());
    }
    return seed;
}

template<typename ValueType>
std::ostream& operator<<(std::ostream& out, StandardRewardModel<ValueType> const& rewardModel) {
    out << std::boolalpha << "reward model [state reward: " << rewardModel.hasStateRewards()
        << ", state-action rewards: " << rewardModel.hasStateActionRewards() << ", transition rewards: " << rewardModel.hasTransitionRewards() << "]"
        << std::noboolalpha;
    return out;
}

std::set<storm::RationalFunctionVariable> getRewardModelParameters(StandardRewardModel<storm::RationalFunction> const& rewModel) {
    std::set<storm::RationalFunctionVariable> vars;
    if (rewModel.hasTransitionRewards()) {
        vars = storm::storage::getVariables(rewModel.getTransitionRewardMatrix());
    }
    if (rewModel.hasStateActionRewards()) {
        std::set<storm::RationalFunctionVariable> tmp = storm::utility::vector::getVariables(rewModel.getStateActionRewardVector());
        vars.insert(tmp.begin(), tmp.end());
    }
    if (rewModel.hasStateRewards()) {
        std::set<storm::RationalFunctionVariable> tmp = storm::utility::vector::getVariables(rewModel.getStateRewardVector());
        vars.insert(tmp.begin(), tmp.end());
    }
    return vars;
}

// Explicitly instantiate the class.
template std::vector<double> StandardRewardModel<double>::getTotalRewardVector(storm::storage::SparseMatrix<double> const& transitionMatrix) const;
template std::vector<double> StandardRewardModel<double>::getTotalRewardVector(uint_fast64_t numberOfRows,
                                                                               storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                               storm::storage::BitVector const& filter) const;
template std::vector<double> StandardRewardModel<double>::getTotalRewardVector(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                               std::vector<double> const& weights) const;
template std::vector<double> StandardRewardModel<double>::getTotalActionRewardVector(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                     std::vector<double> const& stateRewardWeights) const;
template storm::storage::BitVector StandardRewardModel<double>::getStatesWithZeroReward(storm::storage::SparseMatrix<double> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<double>::getStatesWithFilter(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                    std::function<bool(double const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<double>::getChoicesWithZeroReward(storm::storage::SparseMatrix<double> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<double>::getChoicesWithFilter(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                     std::function<bool(double const&)> const& filter) const;
template double StandardRewardModel<double>::getStateActionAndTransitionReward(uint_fast64_t stateIndex,
                                                                               storm::storage::SparseMatrix<double> const& transitionMatrix) const;
template double StandardRewardModel<double>::getTotalStateActionReward(uint_fast64_t stateIndex, uint_fast64_t choiceIndex,
                                                                       storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                       double const& stateRewardWeight, double const& actionRewardWeight) const;
template void StandardRewardModel<double>::clearRewardAtState(uint_fast64_t state, storm::storage::SparseMatrix<double> const& transitionMatrix);
template void StandardRewardModel<double>::reduceToStateBasedRewards(storm::storage::SparseMatrix<double> const& transitionMatrix, bool reduceToStateRewards,
                                                                     std::vector<double> const* weights);
template void StandardRewardModel<double>::setStateActionReward(uint_fast64_t choiceIndex, double const& newValue);
template void StandardRewardModel<double>::setStateReward(uint_fast64_t state, double const& newValue);
template class StandardRewardModel<double>;
template std::ostream& operator<< <double>(std::ostream& out, StandardRewardModel<double> const& rewardModel);

template std::vector<storm::RationalNumber> StandardRewardModel<storm::RationalNumber>::getTotalRewardVector(
    uint_fast64_t numberOfRows, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, storm::storage::BitVector const& filter) const;
template std::vector<storm::RationalNumber> StandardRewardModel<storm::RationalNumber>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix) const;
template std::vector<storm::RationalNumber> StandardRewardModel<storm::RationalNumber>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::vector<storm::RationalNumber> const& weights) const;
template std::vector<storm::RationalNumber> StandardRewardModel<storm::RationalNumber>::getTotalActionRewardVector(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::vector<storm::RationalNumber> const& stateRewardWeights) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalNumber>::getStatesWithZeroReward(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalNumber>::getStatesWithFilter(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::function<bool(storm::RationalNumber const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalNumber>::getChoicesWithZeroReward(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalNumber>::getChoicesWithFilter(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::function<bool(storm::RationalNumber const&)> const& filter) const;
template storm::RationalNumber StandardRewardModel<storm::RationalNumber>::getStateActionAndTransitionReward(
    uint_fast64_t stateIndex, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix) const;
template storm::RationalNumber StandardRewardModel<storm::RationalNumber>::getTotalStateActionReward(
    uint_fast64_t stateIndex, uint_fast64_t choiceIndex, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
    storm::RationalNumber const& stateRewardWeight, storm::RationalNumber const& actionRewardWeight) const;
template void StandardRewardModel<storm::RationalNumber>::reduceToStateBasedRewards(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                                                    bool reduceToStateRewards,
                                                                                    std::vector<storm::RationalNumber> const* weights);
template void StandardRewardModel<storm::RationalNumber>::clearRewardAtState(uint_fast64_t state,
                                                                             storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix);
template void StandardRewardModel<storm::RationalNumber>::setStateActionReward(uint_fast64_t choiceIndex, storm::RationalNumber const& newValue);
template void StandardRewardModel<storm::RationalNumber>::setStateReward(uint_fast64_t state, storm::RationalNumber const& newValue);
template class StandardRewardModel<storm::RationalNumber>;
template std::ostream& operator<< <storm::RationalNumber>(std::ostream& out, StandardRewardModel<storm::RationalNumber> const& rewardModel);

template std::vector<storm::RationalFunction> StandardRewardModel<storm::RationalFunction>::getTotalRewardVector(
    uint_fast64_t numberOfRows, storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, storm::storage::BitVector const& filter) const;
template std::vector<storm::RationalFunction> StandardRewardModel<storm::RationalFunction>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix) const;
template std::vector<storm::RationalFunction> StandardRewardModel<storm::RationalFunction>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, std::vector<storm::RationalFunction> const& weights) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalFunction>::getStatesWithZeroReward(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalFunction>::getStatesWithFilter(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, std::function<bool(storm::RationalFunction const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalFunction>::getChoicesWithZeroReward(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalFunction>::getChoicesWithFilter(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, std::function<bool(storm::RationalFunction const&)> const& filter) const;
template void StandardRewardModel<storm::RationalFunction>::clearRewardAtState(uint_fast64_t state,
                                                                               storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix);
template std::vector<storm::RationalFunction> StandardRewardModel<storm::RationalFunction>::getTotalActionRewardVector(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, std::vector<storm::RationalFunction> const& stateRewardWeights) const;
template storm::RationalFunction StandardRewardModel<storm::RationalFunction>::getStateActionAndTransitionReward(
    uint_fast64_t stateIndex, storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix) const;
template storm::RationalFunction StandardRewardModel<storm::RationalFunction>::getTotalStateActionReward(
    uint_fast64_t stateIndex, uint_fast64_t choiceIndex, storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
    storm::RationalFunction const& stateRewardWeight, storm::RationalFunction const& actionRewardWeight) const;
template void StandardRewardModel<storm::RationalFunction>::reduceToStateBasedRewards(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, bool reduceToStateRewards,
    std::vector<storm::RationalFunction> const* weights);
template void StandardRewardModel<storm::RationalFunction>::setStateActionReward(uint_fast64_t choiceIndex, storm::RationalFunction const& newValue);
template void StandardRewardModel<storm::RationalFunction>::setStateReward(uint_fast64_t state, storm::RationalFunction const& newValue);
template class StandardRewardModel<storm::RationalFunction>;
template std::ostream& operator<< <storm::RationalFunction>(std::ostream& out, StandardRewardModel<storm::RationalFunction> const& rewardModel);

template std::vector<storm::Interval> StandardRewardModel<storm::Interval>::getTotalRewardVector(uint_fast64_t numberOfRows,
                                                                                                 storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                                 storm::storage::BitVector const& filter) const;
template std::vector<storm::Interval> StandardRewardModel<storm::Interval>::getTotalRewardVector(
    storm::storage::SparseMatrix<double> const& transitionMatrix) const;
template std::vector<storm::Interval> StandardRewardModel<storm::Interval>::getTotalRewardVector(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                                 std::vector<double> const& weights) const;
template std::vector<storm::Interval> StandardRewardModel<storm::Interval>::getTotalRewardVector(
    uint_fast64_t numberOfRows, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix, storm::storage::BitVector const& filter) const;
template std::vector<storm::Interval> StandardRewardModel<storm::Interval>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix) const;
template std::vector<storm::Interval> StandardRewardModel<storm::Interval>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix, std::vector<storm::Interval> const& weights) const;
template std::vector<storm::Interval> StandardRewardModel<storm::Interval>::getTotalActionRewardVector(
    storm::storage::SparseMatrix<double> const& transitionMatrix, std::vector<double> const& stateRewardWeights) const;
template storm::storage::BitVector StandardRewardModel<storm::Interval>::getStatesWithFilter(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                             std::function<bool(storm::Interval const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::Interval>::getStatesWithFilter(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix, std::function<bool(storm::Interval const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::Interval>::getChoicesWithFilter(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                              std::function<bool(storm::Interval const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::Interval>::getChoicesWithFilter(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix, std::function<bool(storm::Interval const&)> const& filter) const;
template void StandardRewardModel<storm::Interval>::setStateActionReward(uint_fast64_t choiceIndex, double const& newValue);
template void StandardRewardModel<storm::Interval>::setStateActionReward(uint_fast64_t choiceIndex, storm::Interval const& newValue);
template void StandardRewardModel<storm::Interval>::setStateReward(uint_fast64_t state, double const& newValue);
template void StandardRewardModel<storm::Interval>::setStateReward(uint_fast64_t state, storm::Interval const& newValue);
template void StandardRewardModel<storm::Interval>::clearRewardAtState(uint_fast64_t state, storm::storage::SparseMatrix<double> const& transitionMatrix);
template void StandardRewardModel<storm::Interval>::clearRewardAtState(uint_fast64_t state,
                                                                       storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix);
template void StandardRewardModel<storm::Interval>::reduceToStateBasedRewards(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                              bool reduceToStateRewards, std::vector<double> const* weights);
template void StandardRewardModel<storm::Interval>::reduceToStateBasedRewards(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                                              bool reduceToStateRewards, std::vector<storm::Interval> const* weights);
template storm::storage::BitVector StandardRewardModel<storm::Interval>::getStatesWithZeroReward(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<storm::Interval>::getChoicesWithZeroReward(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix) const;
template class StandardRewardModel<storm::Interval>;
template std::ostream& operator<< <storm::Interval>(std::ostream& out, StandardRewardModel<storm::Interval> const& rewardModel);

template std::vector<storm::RationalInterval> StandardRewardModel<storm::RationalInterval>::getTotalRewardVector(
    uint_fast64_t numberOfRows, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, storm::storage::BitVector const& filter) const;
template std::vector<storm::RationalInterval> StandardRewardModel<storm::RationalInterval>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix) const;
template std::vector<storm::RationalInterval> StandardRewardModel<storm::RationalInterval>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::vector<storm::RationalNumber> const& weights) const;
template std::vector<storm::RationalInterval> StandardRewardModel<storm::RationalInterval>::getTotalRewardVector(
    uint_fast64_t numberOfRows, storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix, storm::storage::BitVector const& filter) const;
template std::vector<storm::RationalInterval> StandardRewardModel<storm::RationalInterval>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix) const;
template std::vector<storm::RationalInterval> StandardRewardModel<storm::RationalInterval>::getTotalRewardVector(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix, std::vector<storm::RationalInterval> const& weights) const;
template std::vector<storm::RationalInterval> StandardRewardModel<storm::RationalInterval>::getTotalActionRewardVector(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::vector<storm::RationalNumber> const& stateRewardWeights) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalInterval>::getStatesWithFilter(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::function<bool(storm::RationalInterval const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalInterval>::getStatesWithFilter(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix, std::function<bool(storm::RationalInterval const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalInterval>::getChoicesWithFilter(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::function<bool(storm::RationalInterval const&)> const& filter) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalInterval>::getChoicesWithFilter(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix, std::function<bool(storm::RationalInterval const&)> const& filter) const;
template void StandardRewardModel<storm::RationalInterval>::setStateActionReward(uint_fast64_t choiceIndex, storm::RationalNumber const& newValue);
template void StandardRewardModel<storm::RationalInterval>::setStateActionReward(uint_fast64_t choiceIndex, storm::RationalInterval const& newValue);
template void StandardRewardModel<storm::RationalInterval>::setStateReward(uint_fast64_t state, storm::RationalNumber const& newValue);
template void StandardRewardModel<storm::RationalInterval>::setStateReward(uint_fast64_t state, storm::RationalInterval const& newValue);
template void StandardRewardModel<storm::RationalInterval>::clearRewardAtState(uint_fast64_t state,
                                                                               storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix);
template void StandardRewardModel<storm::RationalInterval>::clearRewardAtState(uint_fast64_t state,
                                                                               storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix);
template void StandardRewardModel<storm::RationalInterval>::reduceToStateBasedRewards(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, bool reduceToStateRewards, std::vector<storm::RationalNumber> const* weights);
template void StandardRewardModel<storm::RationalInterval>::reduceToStateBasedRewards(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix, bool reduceToStateRewards,
    std::vector<storm::RationalInterval> const* weights);
template storm::storage::BitVector StandardRewardModel<storm::RationalInterval>::getStatesWithZeroReward(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix) const;
template storm::storage::BitVector StandardRewardModel<storm::RationalInterval>::getChoicesWithZeroReward(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix) const;
template class StandardRewardModel<storm::RationalInterval>;
template std::ostream& operator<< <storm::RationalInterval>(std::ostream& out, StandardRewardModel<storm::RationalInterval> const& rewardModel);
}  // namespace sparse

}  // namespace models
}  // namespace storm
