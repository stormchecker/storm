#include "storm/storage/SchedulerChoice.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

template<typename ValueType>
SchedulerChoice<ValueType>::SchedulerChoice() {
    // Intentionally left empty
}

template<typename ValueType>
SchedulerChoice<ValueType>::SchedulerChoice(uint_fast64_t deterministicChoice) {
    distribution.addProbability(deterministicChoice, storm::utility::one<ValueType>());
}

template<typename ValueType>
SchedulerChoice<ValueType>::SchedulerChoice(storm::storage::Distribution<ValueType, uint_fast64_t> const& randomizedChoice) : distribution(randomizedChoice) {
    // Intentionally left empty
}

template<typename ValueType>
SchedulerChoice<ValueType>::SchedulerChoice(storm::storage::Distribution<ValueType, uint_fast64_t>&& randomizedChoice)
    : distribution(std::move(randomizedChoice)) {
    // Intentionally left empty
}

template<typename ValueType>
bool SchedulerChoice<ValueType>::isDefined() const {
    return distribution.size() != 0;
}

template<typename ValueType>
bool SchedulerChoice<ValueType>::isDeterministic() const {
    return distribution.size() == 1;
}

template<typename ValueType>
uint_fast64_t SchedulerChoice<ValueType>::getDeterministicChoice() const {
    STORM_LOG_THROW(isDeterministic(), storm::exceptions::InvalidOperationException,
                    "Tried to obtain the deterministic choice of a scheduler, but the choice is not deterministic");
    return distribution.begin()->first;
}

template<typename ValueType>
storm::storage::Distribution<ValueType, uint_fast64_t> const& SchedulerChoice<ValueType>::getChoiceAsDistribution() const {
    return distribution;
}

template<typename ValueType>
std::ostream& operator<<(std::ostream& out, SchedulerChoice<ValueType> const& schedulerChoice) {
    if (schedulerChoice.isDefined()) {
        if (schedulerChoice.isDeterministic()) {
            out << schedulerChoice.getDeterministicChoice();
        } else {
            out << schedulerChoice.getChoiceAsDistribution();
        }
    } else {
        out << "undefined";
    }
    return out;
}

template class SchedulerChoice<double>;
template std::ostream& operator<<(std::ostream& out, SchedulerChoice<double> const& schedulerChoice);
template class SchedulerChoice<storm::RationalNumber>;
template std::ostream& operator<<(std::ostream& out, SchedulerChoice<storm::RationalNumber> const& schedulerChoice);
template class SchedulerChoice<storm::RationalFunction>;
template std::ostream& operator<<(std::ostream& out, SchedulerChoice<storm::RationalFunction> const& schedulerChoice);
template class SchedulerChoice<storm::Interval>;
template std::ostream& operator<<(std::ostream& out, SchedulerChoice<storm::Interval> const& schedulerChoice);
template class SchedulerChoice<storm::RationalInterval>;
template std::ostream& operator<<(std::ostream& out, SchedulerChoice<storm::RationalInterval> const& schedulerChoice);

}  // namespace storage
}  // namespace storm
