#include "storm/solver/SolveGoal.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"

namespace storm {
namespace storage {
template<typename ValueType>
class SparseMatrix;
}

namespace solver {

template<typename ValueType, typename SolutionType>
SolveGoal<ValueType, SolutionType>::SolveGoal() {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
SolveGoal<ValueType, SolutionType>::SolveGoal(bool minimize)
    : optimizationDirection(minimize ? OptimizationDirection::Minimize : OptimizationDirection::Maximize) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
SolveGoal<ValueType, SolutionType>::SolveGoal(OptimizationDirection optimizationDirection) : optimizationDirection(optimizationDirection) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
SolveGoal<ValueType, SolutionType>::SolveGoal(OptimizationDirection optimizationDirection, storm::logic::ComparisonType boundComparisonType,
                                              SolutionType const& boundThreshold, storm::storage::BitVector const& relevantValues)
    : optimizationDirection(optimizationDirection), comparisonType(boundComparisonType), threshold(boundThreshold), relevantValueVector(relevantValues) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
SolveGoal<ValueType, SolutionType>::SolveGoal(OptimizationDirection optimizationDirection, storm::storage::BitVector const& relevantValues)
    : optimizationDirection(optimizationDirection), relevantValueVector(relevantValues) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
bool SolveGoal<ValueType, SolutionType>::hasDirection() const {
    return static_cast<bool>(optimizationDirection);
}

template<typename ValueType, typename SolutionType>
void SolveGoal<ValueType, SolutionType>::oneMinus() {
    if (optimizationDirection) {
        if (optimizationDirection == storm::solver::OptimizationDirection::Minimize) {
            optimizationDirection = storm::solver::OptimizationDirection::Maximize;
        } else {
            optimizationDirection = storm::solver::OptimizationDirection::Minimize;
        }
    }
    if (threshold) {
        this->threshold = storm::utility::one<SolutionType>() - this->threshold.get();
    }
    if (comparisonType) {
        switch (comparisonType.get()) {
            case storm::logic::ComparisonType::Less:
                comparisonType = storm::logic::ComparisonType::GreaterEqual;
                break;
            case storm::logic::ComparisonType::LessEqual:
                comparisonType = storm::logic::ComparisonType::Greater;
                break;
            case storm::logic::ComparisonType::Greater:
                comparisonType = storm::logic::ComparisonType::LessEqual;
                break;
            case storm::logic::ComparisonType::GreaterEqual:
                comparisonType = storm::logic::ComparisonType::Less;
                break;
        }
    }
}

template<typename ValueType, typename SolutionType>
bool SolveGoal<ValueType, SolutionType>::minimize() const {
    return optimizationDirection == OptimizationDirection::Minimize;
}

template<typename ValueType, typename SolutionType>
OptimizationDirection SolveGoal<ValueType, SolutionType>::direction() const {
    STORM_LOG_THROW(optimizationDirection.has_value(), storm::exceptions::InvalidPropertyException, "Optimization direction not set.");
    return optimizationDirection.get();
}

template<typename ValueType, typename SolutionType>
bool SolveGoal<ValueType, SolutionType>::isBounded() const {
    return comparisonType && threshold && relevantValueVector;
}

template<typename ValueType, typename SolutionType>
bool SolveGoal<ValueType, SolutionType>::boundIsALowerBound() const {
    return (comparisonType.get() == storm::logic::ComparisonType::Greater || comparisonType.get() == storm::logic::ComparisonType::GreaterEqual);
}

template<typename ValueType, typename SolutionType>
bool SolveGoal<ValueType, SolutionType>::boundIsStrict() const {
    return (comparisonType.get() == storm::logic::ComparisonType::Greater || comparisonType.get() == storm::logic::ComparisonType::Less);
}

template<typename ValueType, typename SolutionType>
UncertaintyResolutionMode SolveGoal<ValueType, SolutionType>::getUncertaintyResolutionMode() const {
    return uncertaintyResolutionMode;
}

template<typename ValueType, typename SolutionType>
storm::logic::ComparisonType SolveGoal<ValueType, SolutionType>::boundComparisonType() const {
    return comparisonType.get();
}

template<typename ValueType, typename SolutionType>
SolutionType const& SolveGoal<ValueType, SolutionType>::thresholdValue() const {
    return threshold.get();
}

template<typename ValueType, typename SolutionType>
bool SolveGoal<ValueType, SolutionType>::hasRelevantValues() const {
    return static_cast<bool>(relevantValueVector);
}

template<typename ValueType, typename SolutionType>
storm::storage::BitVector const& SolveGoal<ValueType, SolutionType>::relevantValues() const {
    return relevantValueVector.get();
}

template<typename ValueType, typename SolutionType>
storm::storage::BitVector& SolveGoal<ValueType, SolutionType>::relevantValues() {
    return relevantValueVector.get();
}

template<typename ValueType, typename SolutionType>
void SolveGoal<ValueType, SolutionType>::restrictRelevantValues(storm::storage::BitVector const& filter) {
    if (relevantValueVector) {
        relevantValueVector = relevantValueVector.get() % filter;
    }
}

template<typename ValueType, typename SolutionType>
void SolveGoal<ValueType, SolutionType>::setRelevantValues(storm::storage::BitVector&& values) {
    relevantValueVector = std::move(values);
}

template class SolveGoal<double>;
template class SolveGoal<storm::RationalNumber>;
template class SolveGoal<storm::RationalFunction>;
template class SolveGoal<storm::Interval, double>;
template class SolveGoal<storm::RationalInterval, storm::RationalNumber>;

}  // namespace solver
}  // namespace storm
