#include "DiscountingHelper.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/solver/helper/DiscountedValueIterationHelper.h"
#include "storm/solver/helper/SchedulerTrackingHelper.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, bool TrivialRowGrouping>
DiscountingHelper<ValueType, TrivialRowGrouping>::DiscountingHelper(storm::storage::SparseMatrix<ValueType> const& A, ValueType discountFactor)
    : localA(nullptr), A(&A), discountFactor(discountFactor) {
    progressMeasurement = storm::utility::ProgressMeasurement("iterations");
    storm::storage::SparseMatrix<ValueType> discountedMatrix(A);
    discountedMatrix.scaleRowsInPlace(std::vector<ValueType>(discountedMatrix.getRowCount(), discountFactor));
    discountedA = discountedMatrix;
}

template<typename ValueType, bool TrivialRowGrouping>
DiscountingHelper<ValueType, TrivialRowGrouping>::DiscountingHelper(storm::storage::SparseMatrix<ValueType> const& A, ValueType discountFactor,
                                                                    bool trackScheduler)
    : localA(nullptr), A(&A), discountFactor(discountFactor), trackScheduler(trackScheduler) {
    progressMeasurement = storm::utility::ProgressMeasurement("iterations");
    storm::storage::SparseMatrix<ValueType> discountedMatrix(A);
    discountedMatrix.scaleRowsInPlace(std::vector<ValueType>(discountedMatrix.getRowCount(), discountFactor));
    discountedA = discountedMatrix;
}

template<typename ValueType, bool TrivialRowGrouping>
void DiscountingHelper<ValueType, TrivialRowGrouping>::setUpViOperator() const {
    if (!viOperator) {
        viOperator = std::make_shared<solver::helper::ValueIterationOperator<ValueType, TrivialRowGrouping>>();
        viOperator->setMatrixBackwards(this->discountedA);
    }
}

template<typename ValueType, bool TrivialRowGrouping>
void DiscountingHelper<ValueType, TrivialRowGrouping>::showProgressIterative(uint64_t iteration) const {
    progressMeasurement->updateProgress(iteration);
}

template<typename ValueType, bool TrivialRowGrouping>
bool DiscountingHelper<ValueType, TrivialRowGrouping>::solveWithDiscountedValueIteration(storm::Environment const& env,
                                                                                         std::optional<OptimizationDirection> dir, std::vector<ValueType>& x,
                                                                                         std::vector<ValueType> const& b) const {
    setUpViOperator();
    // This is currently missing progress indications, we can add them later
    storm::solver::helper::DiscountedValueIterationHelper<ValueType, TrivialRowGrouping> viHelper(viOperator);
    uint64_t numIterations{0};
    auto viCallback = [&](solver::SolverStatus const& current) { return current; };
    auto maximalAbsoluteReward = storm::utility::zero<ValueType>();
    for (auto const& entry : b) {
        if (storm::utility::abs(entry) > maximalAbsoluteReward) {
            maximalAbsoluteReward = storm::utility::abs(entry);
        }
    }
    progressMeasurement->startNewMeasurement(0);
    auto status = viHelper.DiscountedVI(x, b, numIterations, env.solver().minMax().getRelativeTerminationCriterion(),
                                        storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()), discountFactor, maximalAbsoluteReward,
                                        dir, viCallback, env.solver().minMax().getMultiplicationStyle());

    // If requested, we store the scheduler for retrieval.
    if (this->isTrackSchedulerSet()) {
        this->extractScheduler(x, b, dir.value(), UncertaintyResolutionMode::Robust);
    }
    return status == solver::SolverStatus::Converged || status == solver::SolverStatus::TerminatedEarly;
}

template<typename ValueType, bool TrivialRowGrouping>
storm::storage::Scheduler<ValueType> DiscountingHelper<ValueType, TrivialRowGrouping>::computeScheduler() const {
    STORM_LOG_THROW(hasScheduler(), storm::exceptions::IllegalFunctionCallException, "Cannot retrieve scheduler, because none was generated.");
    storm::storage::Scheduler<ValueType> result(schedulerChoices->size());
    uint_fast64_t state = 0;
    for (auto const& schedulerChoice : schedulerChoices.get()) {
        result.setChoice(schedulerChoice, state);
        ++state;
    }
    return result;
}

template<typename ValueType, bool TrivialRowGrouping>
bool DiscountingHelper<ValueType, TrivialRowGrouping>::hasScheduler() const {
    return static_cast<bool>(schedulerChoices);
}

template<>
void DiscountingHelper<double, true>::extractScheduler(std::vector<double>& x, std::vector<double> const& b, OptimizationDirection const& dir,
                                                       UncertaintyResolutionMode uncertaintyResolutionMode) const {}

template<>
void DiscountingHelper<storm::RationalNumber, true>::extractScheduler(std::vector<storm::RationalNumber>& x, std::vector<storm::RationalNumber> const& b,
                                                                      OptimizationDirection const& dir,
                                                                      UncertaintyResolutionMode uncertaintyResolutionMode) const {}

template<typename ValueType, bool TrivialRowGrouping>
void DiscountingHelper<ValueType, TrivialRowGrouping>::extractScheduler(std::vector<ValueType>& x, std::vector<ValueType> const& b,
                                                                        OptimizationDirection const& dir,
                                                                        UncertaintyResolutionMode uncertaintyResolutionMode) const {
    // Make sure that storage for scheduler choices is available
    if (!this->schedulerChoices) {
        this->schedulerChoices = std::vector<uint64_t>(x.size(), 0);
    } else {
        this->schedulerChoices->resize(x.size(), 0);
    }
    // Set the correct choices.
    STORM_LOG_WARN_COND(viOperator, "Expected VI operator to be initialized for scheduler extraction. Initializing now, but this is inefficient.");
    if (!viOperator) {
        setUpViOperator();
    }
    storm::solver::helper::SchedulerTrackingHelper<ValueType> schedHelper(viOperator);
    schedHelper.computeScheduler(x, b, dir, *this->schedulerChoices, uncertaintyResolutionMode, nullptr);
}

template<typename ValueType, bool TrivialRowGrouping>
void DiscountingHelper<ValueType, TrivialRowGrouping>::setTrackScheduler(bool trackScheduler) {
    this->trackScheduler = trackScheduler;
    if (!this->trackScheduler) {
        schedulerChoices = boost::none;
    }
}

template<typename ValueType, bool TrivialRowGrouping>
bool DiscountingHelper<ValueType, TrivialRowGrouping>::isTrackSchedulerSet() const {
    return this->trackScheduler;
}

template class DiscountingHelper<double>;
template class DiscountingHelper<storm::RationalNumber>;

template class DiscountingHelper<double, true>;
template class DiscountingHelper<storm::RationalNumber, true>;
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
