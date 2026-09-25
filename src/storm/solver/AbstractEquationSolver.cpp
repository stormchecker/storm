#include "storm/solver/AbstractEquationSolver.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<typename SolutionType>
AbstractEquationSolver<SolutionType>::AbstractEquationSolver() {
    // Intentionally left empty. Call setShowProgress() after construction to enable progress.
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setShowProgress(bool verbose, uint64_t delay) {
    if (verbose) {
        this->progressMeasurement = storm::utility::ProgressMeasurement("iterations", delay);
    }
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setTerminationCondition(std::unique_ptr<TerminationCondition<SolutionType>> terminationCondition) {
    this->terminationCondition = std::move(terminationCondition);
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::resetTerminationCondition() {
    this->terminationCondition = nullptr;
}

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::hasCustomTerminationCondition() const {
    return static_cast<bool>(this->terminationCondition);
}

template<typename SolutionType>
TerminationCondition<SolutionType> const& AbstractEquationSolver<SolutionType>::getTerminationCondition() const {
    return *terminationCondition;
}

template<typename SolutionType>
std::unique_ptr<TerminationCondition<SolutionType>> const& AbstractEquationSolver<SolutionType>::getTerminationConditionPointer() const {
    return terminationCondition;
}

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::terminateNow(std::vector<SolutionType> const& values, SolverGuarantee const& guarantee) const {
    if (!this->hasCustomTerminationCondition()) {
        return false;
    }

    return this->getTerminationCondition().terminateNow(values, guarantee);
}

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::hasRelevantValues() const {
    return static_cast<bool>(relevantValues);
}

template<typename SolutionType>
storm::storage::BitVector const& AbstractEquationSolver<SolutionType>::getRelevantValues() const {
    return relevantValues.get();
}

template<typename SolutionType>
boost::optional<storm::storage::BitVector> const& AbstractEquationSolver<SolutionType>::getOptionalRelevantValues() const {
    return relevantValues;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setRelevantValues(storm::storage::BitVector&& relevantValues) {
    this->relevantValues = std::move(relevantValues);
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setRelevantValues(storm::storage::BitVector const& relevantValues) {
    this->relevantValues = relevantValues;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::clearRelevantValues() {
    relevantValues = boost::none;
}

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::hasLowerBound(BoundType const& type) const {
    if (type == BoundType::Any) {
        return static_cast<bool>(lowerBound) || static_cast<bool>(lowerBounds);
    } else if (type == BoundType::Global) {
        return static_cast<bool>(lowerBound);
    } else if (type == BoundType::Local) {
        return static_cast<bool>(lowerBounds);
    }
    return false;
}

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::hasUpperBound(BoundType const& type) const {
    if (type == BoundType::Any) {
        return static_cast<bool>(upperBound) || static_cast<bool>(upperBounds);
    } else if (type == BoundType::Global) {
        return static_cast<bool>(upperBound);
    } else if (type == BoundType::Local) {
        return static_cast<bool>(upperBounds);
    }
    return false;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setLowerBound(SolutionType const& value) {
    lowerBound = value;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setUpperBound(SolutionType const& value) {
    upperBound = value;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setBounds(SolutionType const& lower, SolutionType const& upper) {
    setLowerBound(lower);
    setUpperBound(upper);
}

template<typename SolutionType>
SolutionType const& AbstractEquationSolver<SolutionType>::getLowerBound() const {
    return lowerBound.get();
}

template<typename SolutionType>
SolutionType const& AbstractEquationSolver<SolutionType>::getLowerBound(uint64_t const& index) const {
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

template<typename SolutionType>
SolutionType AbstractEquationSolver<SolutionType>::getLowerBound(bool convertLocalBounds) const {
    if (lowerBound) {
        return lowerBound.get();
    } else if (convertLocalBounds) {
        return *std::min_element(lowerBounds->begin(), lowerBounds->end());
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "No lower bound available but some was requested.");
    return SolutionType();
}

template<typename SolutionType>
SolutionType const& AbstractEquationSolver<SolutionType>::getUpperBound() const {
    return upperBound.get();
}

template<typename SolutionType>
SolutionType const& AbstractEquationSolver<SolutionType>::getUpperBound(uint64_t const& index) const {
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

template<typename SolutionType>
SolutionType AbstractEquationSolver<SolutionType>::getUpperBound(bool convertLocalBounds) const {
    if (upperBound) {
        return upperBound.get();
    } else if (convertLocalBounds) {
        return *std::max_element(upperBounds->begin(), upperBounds->end());
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "No upper bound available but some was requested.");
    return SolutionType();
}

template<typename SolutionType>
std::vector<SolutionType> const& AbstractEquationSolver<SolutionType>::getLowerBounds() const {
    return lowerBounds.get();
}

template<typename SolutionType>
std::vector<SolutionType> const& AbstractEquationSolver<SolutionType>::getUpperBounds() const {
    return upperBounds.get();
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setLowerBounds(std::vector<SolutionType> const& values) {
    lowerBounds = values;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setLowerBounds(std::vector<SolutionType>&& values) {
    lowerBounds = std::move(values);
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setUpperBounds(std::vector<SolutionType> const& values) {
    upperBounds = values;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setUpperBounds(std::vector<SolutionType>&& values) {
    upperBounds = std::move(values);
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setBounds(std::vector<SolutionType> const& lower, std::vector<SolutionType> const& upper) {
    setLowerBounds(lower);
    setUpperBounds(upper);
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setBoundsFromOtherSolver(AbstractEquationSolver<SolutionType> const& other) {
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

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::hasSolutionLowerBounds() const {
    return solutionBounds.hasLower();
}

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::hasSolutionUpperBounds() const {
    return solutionBounds.hasUpper();
}

template<typename SolutionType>
std::vector<SolutionType> const& AbstractEquationSolver<SolutionType>::getSolutionLowerBounds() const {
    STORM_LOG_ASSERT(this->hasSolutionLowerBounds(), "No lower bound on the solution was computed.");
    return *solutionBounds.lower;
}

template<typename SolutionType>
std::vector<SolutionType> const& AbstractEquationSolver<SolutionType>::getSolutionUpperBounds() const {
    STORM_LOG_ASSERT(this->hasSolutionUpperBounds(), "No upper bound on the solution was computed.");
    return *solutionBounds.upper;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setSolutionBounds(SolutionBounds<SolutionType> bounds) const {
    STORM_LOG_ASSERT(!bounds.hasLower() || !bounds.hasUpper() || bounds.lower->size() == bounds.upper->size(),
                     "Bounds on the solution must have the same size.");
    solutionBounds = std::move(bounds);
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::setSolutionBoundsExact(std::vector<SolutionType> const& x) const {
    SolutionBounds<SolutionType> bounds;
    bounds.setExact(x);
    this->setSolutionBounds(std::move(bounds));
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::clearSolutionBounds() const {
    solutionBounds.clear();
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::clearBounds() {
    lowerBound = boost::none;
    upperBound = boost::none;
    lowerBounds = boost::none;
    upperBounds = boost::none;
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::createLowerBoundsVector(std::vector<SolutionType>& lowerBoundsVector) const {
    if (this->hasLowerBound(BoundType::Local)) {
        lowerBoundsVector = this->getLowerBounds();
    } else {
        SolutionType lowerBound = this->hasLowerBound(BoundType::Global) ? this->getLowerBound() : storm::utility::zero<SolutionType>();
        for (auto& e : lowerBoundsVector) {
            e = lowerBound;
        }
    }
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::createUpperBoundsVector(std::vector<SolutionType>& upperBoundsVector) const {
    STORM_LOG_ASSERT(this->hasUpperBound(), "Expecting upper bound(s).");
    if (this->hasUpperBound(BoundType::Global)) {
        upperBoundsVector.assign(upperBoundsVector.size(), this->getUpperBound());
    } else {
        upperBoundsVector.assign(this->getUpperBounds().begin(), this->getUpperBounds().end());
    }
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::createUpperBoundsVector(std::unique_ptr<std::vector<SolutionType>>& upperBoundsVector, uint64_t length) const {
    STORM_LOG_ASSERT(this->hasUpperBound(), "Expecting upper bound(s).");
    if (!upperBoundsVector) {
        if (this->hasUpperBound(BoundType::Local)) {
            STORM_LOG_ASSERT(length == this->getUpperBounds().size(), "Mismatching sizes.");
            upperBoundsVector = std::make_unique<std::vector<SolutionType>>(this->getUpperBounds());
        } else {
            upperBoundsVector = std::make_unique<std::vector<SolutionType>>(length, this->getUpperBound());
        }
    } else {
        createUpperBoundsVector(*upperBoundsVector);
    }
}

template<typename SolutionType>
bool AbstractEquationSolver<SolutionType>::isShowProgressSet() const {
    return this->progressMeasurement.is_initialized();
}

template<typename SolutionType>
uint64_t AbstractEquationSolver<SolutionType>::getShowProgressDelay() const {
    STORM_LOG_ASSERT(this->isShowProgressSet(), "Tried to get the progress message delay but progress is not shown.");
    return this->progressMeasurement->getShowProgressDelay();
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::startMeasureProgress(uint64_t startingIteration) const {
    if (this->isShowProgressSet()) {
        this->progressMeasurement->startNewMeasurement(startingIteration);
    }
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::showProgressIterative(uint64_t iteration, boost::optional<uint64_t> const& bound) const {
    if (this->isShowProgressSet()) {
        if (bound) {
            this->progressMeasurement->setMaxCount(bound.get());
        }
        this->progressMeasurement->updateProgress(iteration);
    }
}

template<typename SolutionType>
void AbstractEquationSolver<SolutionType>::reportStatus(SolverStatus status, boost::optional<uint64_t> const& iterations) const {
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

template<typename SolutionType>
SolverStatus AbstractEquationSolver<SolutionType>::updateStatus(SolverStatus status, bool earlyTermination, uint64_t iterations,
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

template<typename SolutionType>
SolverStatus AbstractEquationSolver<SolutionType>::updateStatus(SolverStatus status, std::vector<SolutionType> const& x, SolverGuarantee const& guarantee,
                                                                uint64_t iterations, uint64_t maximalNumberOfIterations) const {
    return this->updateStatus(status, this->hasCustomTerminationCondition() && this->getTerminationCondition().terminateNow(x, guarantee), iterations,
                              maximalNumberOfIterations);
}

template class AbstractEquationSolver<double>;

template class AbstractEquationSolver<storm::RationalNumber>;
template class AbstractEquationSolver<storm::RationalFunction>;

}  // namespace solver
}  // namespace storm
