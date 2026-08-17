#include "storm/solver/AbstractEquationSolver.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<typename ValueType>
AbstractEquationSolver<ValueType>::AbstractEquationSolver() {
    if (storm::settings::getModule<storm::settings::modules::GeneralSettings>().isVerboseSet()) {
        this->progressMeasurement = storm::utility::ProgressMeasurement("iterations");
    }
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setTerminationCondition(std::unique_ptr<TerminationCondition<ValueType>> terminationCondition) {
    this->terminationCondition = std::move(terminationCondition);
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::resetTerminationCondition() {
    this->terminationCondition = nullptr;
}

template<typename ValueType>
bool AbstractEquationSolver<ValueType>::hasCustomTerminationCondition() const {
    return static_cast<bool>(this->terminationCondition);
}

template<typename ValueType>
TerminationCondition<ValueType> const& AbstractEquationSolver<ValueType>::getTerminationCondition() const {
    return *terminationCondition;
}

template<typename ValueType>
std::unique_ptr<TerminationCondition<ValueType>> const& AbstractEquationSolver<ValueType>::getTerminationConditionPointer() const {
    return terminationCondition;
}

template<typename ValueType>
bool AbstractEquationSolver<ValueType>::terminateNow(std::vector<ValueType> const& values, SolverGuarantee const& guarantee) const {
    if (!this->hasCustomTerminationCondition()) {
        return false;
    }

    return this->getTerminationCondition().terminateNow(values, guarantee);
}

template<typename ValueType>
bool AbstractEquationSolver<ValueType>::hasRelevantValues() const {
    return static_cast<bool>(relevantValues);
}

template<typename ValueType>
storm::storage::BitVector const& AbstractEquationSolver<ValueType>::getRelevantValues() const {
    return relevantValues.get();
}

template<typename ValueType>
boost::optional<storm::storage::BitVector> const& AbstractEquationSolver<ValueType>::getOptionalRelevantValues() const {
    return relevantValues;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setRelevantValues(storm::storage::BitVector&& relevantValues) {
    this->relevantValues = std::move(relevantValues);
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setRelevantValues(storm::storage::BitVector const& relevantValues) {
    this->relevantValues = relevantValues;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::clearRelevantValues() {
    relevantValues = boost::none;
}

template<typename ValueType>
bool AbstractEquationSolver<ValueType>::hasLowerBound(BoundType const& type) const {
    if (type == BoundType::Any) {
        return static_cast<bool>(lowerBound) || static_cast<bool>(lowerBounds);
    } else if (type == BoundType::Global) {
        return static_cast<bool>(lowerBound);
    } else if (type == BoundType::Local) {
        return static_cast<bool>(lowerBounds);
    }
    return false;
}

template<typename ValueType>
bool AbstractEquationSolver<ValueType>::hasUpperBound(BoundType const& type) const {
    if (type == BoundType::Any) {
        return static_cast<bool>(upperBound) || static_cast<bool>(upperBounds);
    } else if (type == BoundType::Global) {
        return static_cast<bool>(upperBound);
    } else if (type == BoundType::Local) {
        return static_cast<bool>(upperBounds);
    }
    return false;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setLowerBound(ValueType const& value) {
    lowerBound = value;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setUpperBound(ValueType const& value) {
    upperBound = value;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setBounds(ValueType const& lower, ValueType const& upper) {
    setLowerBound(lower);
    setUpperBound(upper);
}

template<typename ValueType>
ValueType const& AbstractEquationSolver<ValueType>::getLowerBound() const {
    return lowerBound.get();
}

template<typename ValueType>
ValueType const& AbstractEquationSolver<ValueType>::getLowerBound(uint64_t const& index) const {
    if (lowerBounds) {
        STORM_LOG_ASSERT(index < lowerBounds->size(), "Invalid row index " << index << " for vector of size " << lowerBounds->size());
        if (lowerBound) {
            return std::max(lowerBound.get(), lowerBounds.get()[index]);
        } else {
            return lowerBounds.get()[index];
        }
    } else {
        STORM_LOG_ASSERT(lowerBound, "Lower bound requested but was not specified before.");
        return lowerBound.get();
    }
}

template<typename ValueType>
ValueType AbstractEquationSolver<ValueType>::getLowerBound(bool convertLocalBounds) const {
    if (lowerBound) {
        return lowerBound.get();
    } else if (convertLocalBounds) {
        return *std::min_element(lowerBounds->begin(), lowerBounds->end());
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "No lower bound available but some was requested.");
    return ValueType();
}

template<typename ValueType>
ValueType const& AbstractEquationSolver<ValueType>::getUpperBound() const {
    return upperBound.get();
}

template<typename ValueType>
ValueType const& AbstractEquationSolver<ValueType>::getUpperBound(uint64_t const& index) const {
    if (upperBounds) {
        STORM_LOG_ASSERT(index < upperBounds->size(), "Invalid row index " << index << " for vector of size " << upperBounds->size());
        if (upperBound) {
            return std::min(upperBound.get(), upperBounds.get()[index]);
        } else {
            return upperBounds.get()[index];
        }
    } else {
        STORM_LOG_ASSERT(upperBound, "Upper bound requested but was not specified before.");
        return upperBound.get();
    }
}

template<typename ValueType>
ValueType AbstractEquationSolver<ValueType>::getUpperBound(bool convertLocalBounds) const {
    if (upperBound) {
        return upperBound.get();
    } else if (convertLocalBounds) {
        return *std::max_element(upperBounds->begin(), upperBounds->end());
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "No upper bound available but some was requested.");
    return ValueType();
}

template<typename ValueType>
std::vector<ValueType> const& AbstractEquationSolver<ValueType>::getLowerBounds() const {
    return lowerBounds.get();
}

template<typename ValueType>
std::vector<ValueType> const& AbstractEquationSolver<ValueType>::getUpperBounds() const {
    return upperBounds.get();
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setLowerBounds(std::vector<ValueType> const& values) {
    lowerBounds = values;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setLowerBounds(std::vector<ValueType>&& values) {
    lowerBounds = std::move(values);
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setUpperBounds(std::vector<ValueType> const& values) {
    upperBounds = values;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setUpperBounds(std::vector<ValueType>&& values) {
    upperBounds = std::move(values);
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setBounds(std::vector<ValueType> const& lower, std::vector<ValueType> const& upper) {
    setLowerBounds(lower);
    setUpperBounds(upper);
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::setBoundsFromOtherSolver(AbstractEquationSolver<ValueType> const& other) {
    if (other.hasLowerBound(BoundType::Global)) {
        this->setLowerBound(other.getLowerBound());
    }
    if (other.hasLowerBound(BoundType::Local)) {
        this->setLowerBounds(other.getLowerBounds());
    }
    if (other.hasUpperBound(BoundType::Global)) {
        this->setUpperBound(other.getUpperBound());
    }
    if (other.hasUpperBound(BoundType::Local)) {
        this->setUpperBounds(other.getUpperBounds());
    }
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::clearBounds() {
    lowerBound = boost::none;
    upperBound = boost::none;
    lowerBounds = boost::none;
    upperBounds = boost::none;
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::createLowerBoundsVector(std::vector<ValueType>& lowerBoundsVector) const {
    if (this->hasLowerBound(BoundType::Local)) {
        lowerBoundsVector = this->getLowerBounds();
    } else {
        ValueType lowerBound = this->hasLowerBound(BoundType::Global) ? this->getLowerBound() : storm::utility::zero<ValueType>();
        for (auto& e : lowerBoundsVector) {
            e = lowerBound;
        }
    }
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::createUpperBoundsVector(std::vector<ValueType>& upperBoundsVector) const {
    STORM_LOG_ASSERT(this->hasUpperBound(), "Expecting upper bound(s).");
    if (this->hasUpperBound(BoundType::Global)) {
        upperBoundsVector.assign(upperBoundsVector.size(), this->getUpperBound());
    } else {
        upperBoundsVector.assign(this->getUpperBounds().begin(), this->getUpperBounds().end());
    }
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::createUpperBoundsVector(std::unique_ptr<std::vector<ValueType>>& upperBoundsVector, uint64_t length) const {
    STORM_LOG_ASSERT(this->hasUpperBound(), "Expecting upper bound(s).");
    if (!upperBoundsVector) {
        if (this->hasUpperBound(BoundType::Local)) {
            STORM_LOG_ASSERT(length == this->getUpperBounds().size(), "Mismatching sizes.");
            upperBoundsVector = std::make_unique<std::vector<ValueType>>(this->getUpperBounds());
        } else {
            upperBoundsVector = std::make_unique<std::vector<ValueType>>(length, this->getUpperBound());
        }
    } else {
        createUpperBoundsVector(*upperBoundsVector);
    }
}

template<typename ValueType>
bool AbstractEquationSolver<ValueType>::isShowProgressSet() const {
    return this->progressMeasurement.is_initialized();
}

template<typename ValueType>
uint64_t AbstractEquationSolver<ValueType>::getShowProgressDelay() const {
    STORM_LOG_ASSERT(this->isShowProgressSet(), "Tried to get the progress message delay but progress is not shown.");
    return this->progressMeasurement->getShowProgressDelay();
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::startMeasureProgress(uint64_t startingIteration) const {
    if (this->isShowProgressSet()) {
        this->progressMeasurement->startNewMeasurement(startingIteration);
    }
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::showProgressIterative(uint64_t iteration, boost::optional<uint64_t> const& bound) const {
    if (this->isShowProgressSet()) {
        if (bound) {
            this->progressMeasurement->setMaxCount(bound.get());
        }
        this->progressMeasurement->updateProgress(iteration);
    }
}

template<typename ValueType>
void AbstractEquationSolver<ValueType>::reportStatus(SolverStatus status, boost::optional<uint64_t> const& iterations) const {
    if (iterations) {
        switch (status) {
            case SolverStatus::Converged:
                STORM_LOG_TRACE("Iterative solver converged after " << iterations.get() << " iterations.");
                break;
            case SolverStatus::TerminatedEarly:
                STORM_LOG_TRACE("Iterative solver terminated early after " << iterations.get() << " iterations.");
                break;
            case SolverStatus::MaximalIterationsExceeded:
                STORM_LOG_WARN("Iterative solver did not converge after " << iterations.get() << " iterations.");
                break;
            case SolverStatus::Aborted:
                STORM_LOG_WARN("Iterative solver was aborted after " << iterations.get() << " iterations.");
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidStateException, "Iterative solver terminated unexpectedly.");
        }
    } else {
        switch (status) {
            case SolverStatus::Converged:
                STORM_LOG_TRACE("Solver converged.");
                break;
            case SolverStatus::TerminatedEarly:
                STORM_LOG_TRACE("Solver terminated early.");
                break;
            case SolverStatus::MaximalIterationsExceeded:
                STORM_LOG_ASSERT(false, "Non-iterative solver should not exceed maximal number of iterations.");
                STORM_LOG_WARN("Solver did not converge.");
                break;
            case SolverStatus::Aborted:
                STORM_LOG_WARN("Solver was aborted.");
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidStateException, "Solver terminated unexpectedly.");
        }
    }
}

template<typename ValueType>
SolverStatus AbstractEquationSolver<ValueType>::updateStatus(SolverStatus status, bool earlyTermination, uint64_t iterations,
                                                             uint64_t maximalNumberOfIterations) const {
    if (status != SolverStatus::Converged) {
        if (earlyTermination) {
            status = SolverStatus::TerminatedEarly;
        } else if (iterations >= maximalNumberOfIterations) {
            status = SolverStatus::MaximalIterationsExceeded;
        } else if (storm::utility::resources::isTerminate()) {
            status = SolverStatus::Aborted;
        }
    }
    return status;
}

template<typename ValueType>
SolverStatus AbstractEquationSolver<ValueType>::updateStatus(SolverStatus status, std::vector<ValueType> const& x, SolverGuarantee const& guarantee,
                                                             uint64_t iterations, uint64_t maximalNumberOfIterations) const {
    return this->updateStatus(status, this->hasCustomTerminationCondition() && this->getTerminationCondition().terminateNow(x, guarantee), iterations,
                              maximalNumberOfIterations);
}

template class AbstractEquationSolver<double>;

template class AbstractEquationSolver<storm::RationalNumber>;
template class AbstractEquationSolver<storm::RationalFunction>;

}  // namespace solver
}  // namespace storm
