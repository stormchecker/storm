#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
bool ExplicitModelCheckerHint<ValueType>::isEmpty() const {
    return !hasResultHint() && !hasSchedulerHint() && !hasMaybeStates();
}

template<typename ValueType>
bool ExplicitModelCheckerHint<ValueType>::isExplicitModelCheckerHint() const {
    return true;
}

template<typename ValueType>
bool ExplicitModelCheckerHint<ValueType>::hasResultHint() const {
    return resultHint.is_initialized();
}

template<typename ValueType>
std::vector<ValueType> const& ExplicitModelCheckerHint<ValueType>::getResultHint() const {
    return *resultHint;
}

template<typename ValueType>
std::vector<ValueType>& ExplicitModelCheckerHint<ValueType>::getResultHint() {
    return *resultHint;
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setResultHint(boost::optional<std::vector<ValueType>> const& resultHint) {
    this->resultHint = resultHint;
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setResultHint(boost::optional<std::vector<ValueType>>&& resultHint) {
    this->resultHint = resultHint;
}

template<typename ValueType>
bool ExplicitModelCheckerHint<ValueType>::getComputeOnlyMaybeStates() const {
    STORM_LOG_THROW(!computeOnlyMaybeStates || (hasMaybeStates() && hasResultHint()), storm::exceptions::InvalidOperationException,
                    "Computing only maybestates is activated but no maybestates or no result hint is specified.");
    return computeOnlyMaybeStates;
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setComputeOnlyMaybeStates(bool value) {
    STORM_LOG_THROW(!value || (hasMaybeStates() && hasResultHint()), storm::exceptions::InvalidOperationException,
                    "Tried to activate that only maybestates need to be computed, but no maybestates or no result hint was given before.");
    this->computeOnlyMaybeStates = value;
}

template<typename ValueType>
bool ExplicitModelCheckerHint<ValueType>::hasMaybeStates() const {
    return maybeStates.is_initialized();
}

template<typename ValueType>
storm::storage::BitVector const& ExplicitModelCheckerHint<ValueType>::getMaybeStates() const {
    return maybeStates.get();
}

template<typename ValueType>
storm::storage::BitVector& ExplicitModelCheckerHint<ValueType>::getMaybeStates() {
    return maybeStates.get();
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setMaybeStates(storm::storage::BitVector const& newMaybeStates) {
    this->maybeStates = newMaybeStates;
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setMaybeStates(storm::storage::BitVector&& newMaybeStates) {
    this->maybeStates = std::move(newMaybeStates);
}

template<typename ValueType>
bool ExplicitModelCheckerHint<ValueType>::hasSchedulerHint() const {
    return schedulerHint.is_initialized();
}

template<typename ValueType>
storm::storage::Scheduler<ValueType> const& ExplicitModelCheckerHint<ValueType>::getSchedulerHint() const {
    return *schedulerHint;
}

template<typename ValueType>
storm::storage::Scheduler<ValueType>& ExplicitModelCheckerHint<ValueType>::getSchedulerHint() {
    return *schedulerHint;
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setSchedulerHint(boost::optional<storm::storage::Scheduler<ValueType>> const& schedulerHint) {
    this->schedulerHint = schedulerHint;
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setSchedulerHint(boost::optional<storm::storage::Scheduler<ValueType>>&& schedulerHint) {
    this->schedulerHint = schedulerHint;
}

template<typename ValueType>
bool ExplicitModelCheckerHint<ValueType>::getNoEndComponentsInMaybeStates() const {
    return noEndComponentsInMaybeStates;
}

template<typename ValueType>
void ExplicitModelCheckerHint<ValueType>::setNoEndComponentsInMaybeStates(bool value) {
    noEndComponentsInMaybeStates = value;
}

template class ExplicitModelCheckerHint<double>;
template class ExplicitModelCheckerHint<storm::RationalNumber>;
template class ExplicitModelCheckerHint<storm::RationalFunction>;
template class ExplicitModelCheckerHint<storm::Interval>;
template class ExplicitModelCheckerHint<storm::RationalInterval>;

}  // namespace modelchecker
}  // namespace storm
