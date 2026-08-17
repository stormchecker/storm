#include "storm/solver/TerminationCondition.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

template<typename ValueType>
bool TerminationCondition<ValueType>::terminateNow(std::vector<ValueType> const& currentValues, SolverGuarantee const& guarantee) const {
    return terminateNow([&currentValues](uint64_t const& i) { return currentValues[i]; }, guarantee);
}

template<typename ValueType>
bool NoTerminationCondition<ValueType>::terminateNow(std::function<ValueType(uint64_t const&)> const& valueGetter, SolverGuarantee const& guarantee) const {
    return false;
}

template<typename ValueType>
bool NoTerminationCondition<ValueType>::requiresGuarantee(SolverGuarantee const&) const {
    return false;
}

template<typename ValueType>
TerminateIfFilteredSumExceedsThreshold<ValueType>::TerminateIfFilteredSumExceedsThreshold(storm::storage::BitVector const& filter, ValueType const& threshold,
                                                                                          bool strict)
    : threshold(threshold), filter(filter), strict(strict) {
    // Intentionally left empty.
}

template<typename ValueType>
bool TerminateIfFilteredSumExceedsThreshold<ValueType>::terminateNow(std::function<ValueType(uint64_t const&)> const& valueGetter,
                                                                     SolverGuarantee const& guarantee) const {
    if (guarantee != SolverGuarantee::LessOrEqual) {
        return false;
    }

    ValueType sum = storm::utility::zero<ValueType>();
    for (auto pos : filter) {
        sum += valueGetter(pos);
        // Exiting this loop early is not possible as values might be negative
    }
    return strict ? sum > this->threshold : sum >= this->threshold;
}

template<typename ValueType>
bool TerminateIfFilteredSumExceedsThreshold<ValueType>::requiresGuarantee(SolverGuarantee const& guarantee) const {
    return guarantee == SolverGuarantee::LessOrEqual;
}

template<typename ValueType>
TerminateIfFilteredExtremumExceedsThreshold<ValueType>::TerminateIfFilteredExtremumExceedsThreshold(storm::storage::BitVector const& filter, bool strict,
                                                                                                    ValueType const& threshold, bool useMinimum)
    : TerminateIfFilteredSumExceedsThreshold<ValueType>(filter, threshold, strict), useMinimum(useMinimum) {
    // Intentionally left empty.
    STORM_LOG_THROW(!this->filter.empty(), storm::exceptions::InvalidArgumentException, "Empty Filter; Can not take extremum over empty set.");
    cachedExtremumIndex = this->filter.getNextSetIndex(0);
}

template<typename ValueType>
bool TerminateIfFilteredExtremumExceedsThreshold<ValueType>::terminateNow(std::function<ValueType(uint64_t const&)> const& valueGetter,
                                                                          SolverGuarantee const& guarantee) const {
    if (guarantee != SolverGuarantee::LessOrEqual) {
        return false;
    }

    ValueType extremum = valueGetter(cachedExtremumIndex);
    if (useMinimum && (this->strict ? extremum <= this->threshold : extremum < this->threshold)) {
        // The extremum can only become smaller so we can return right now.
        return false;
    }

    if (useMinimum) {
        if (this->strict) {
            for (auto pos : this->filter) {
                extremum = std::min(valueGetter(pos), extremum);
                if (extremum <= this->threshold) {
                    cachedExtremumIndex = pos;
                    return false;
                }
            }
        } else {
            for (auto pos : this->filter) {
                extremum = std::min(valueGetter(pos), extremum);
                if (extremum < this->threshold) {
                    cachedExtremumIndex = pos;
                    return false;
                }
            }
        }
    } else {
        for (auto pos : this->filter) {
            extremum = std::max(valueGetter(pos), extremum);
        }
    }

    return this->strict ? extremum > this->threshold : extremum >= this->threshold;
}

template<typename ValueType>
bool TerminateIfFilteredExtremumExceedsThreshold<ValueType>::requiresGuarantee(SolverGuarantee const& guarantee) const {
    return guarantee == SolverGuarantee::LessOrEqual;
}

template<typename ValueType>
TerminateIfFilteredExtremumBelowThreshold<ValueType>::TerminateIfFilteredExtremumBelowThreshold(storm::storage::BitVector const& filter, bool strict,
                                                                                                ValueType const& threshold, bool useMinimum)
    : TerminateIfFilteredSumExceedsThreshold<ValueType>(filter, threshold, strict), useMinimum(useMinimum) {
    STORM_LOG_THROW(!this->filter.empty(), storm::exceptions::InvalidArgumentException, "Empty Filter; Can not take extremum over empty set.");
    cachedExtremumIndex = this->filter.getNextSetIndex(0);
}

template<typename ValueType>
bool TerminateIfFilteredExtremumBelowThreshold<ValueType>::terminateNow(std::function<ValueType(uint64_t const&)> const& valueGetter,
                                                                        SolverGuarantee const& guarantee) const {
    if (guarantee != SolverGuarantee::GreaterOrEqual) {
        return false;
    }

    ValueType extremum = valueGetter(cachedExtremumIndex);
    if (!useMinimum && (this->strict ? extremum >= this->threshold : extremum > this->threshold)) {
        // The extremum can only become larger so we can return right now.
        return false;
    }

    if (useMinimum) {
        for (auto pos : this->filter) {
            extremum = std::min(valueGetter(pos), extremum);
        }
    } else {
        if (this->strict) {
            for (auto pos : this->filter) {
                extremum = std::max(valueGetter(pos), extremum);
                if (extremum >= this->threshold) {
                    cachedExtremumIndex = pos;
                    return false;
                }
            }
        } else {
            for (auto pos : this->filter) {
                extremum = std::max(valueGetter(pos), extremum);
                if (extremum > this->threshold) {
                    cachedExtremumIndex = pos;
                    return false;
                }
            }
        }
    }

    return this->strict ? extremum < this->threshold : extremum <= this->threshold;
}

template<typename ValueType>
bool TerminateIfFilteredExtremumBelowThreshold<ValueType>::requiresGuarantee(SolverGuarantee const& guarantee) const {
    return guarantee == SolverGuarantee::GreaterOrEqual;
}

template class TerminationCondition<double>;
template class NoTerminationCondition<double>;
template class TerminateIfFilteredSumExceedsThreshold<double>;
template class TerminateIfFilteredExtremumExceedsThreshold<double>;
template class TerminateIfFilteredExtremumBelowThreshold<double>;

template class TerminationCondition<storm::RationalNumber>;
template class NoTerminationCondition<storm::RationalNumber>;
template class TerminateIfFilteredSumExceedsThreshold<storm::RationalNumber>;
template class TerminateIfFilteredExtremumExceedsThreshold<storm::RationalNumber>;
template class TerminateIfFilteredExtremumBelowThreshold<storm::RationalNumber>;

template class TerminationCondition<storm::Interval>;
template class NoTerminationCondition<storm::Interval>;
template class TerminateIfFilteredSumExceedsThreshold<storm::Interval>;
template class TerminateIfFilteredExtremumExceedsThreshold<storm::Interval>;
template class TerminateIfFilteredExtremumBelowThreshold<storm::Interval>;

}  // namespace solver
}  // namespace storm
