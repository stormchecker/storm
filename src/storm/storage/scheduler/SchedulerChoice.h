#pragma once

#include "storm/storage/Distribution.h"
#include "storm/utility/constants.h"

namespace storm {
namespace storage {

template<typename ValueType>
class SchedulerChoice {
   public:
    /*!
     * Creates an undefined scheduler choice
     */
    SchedulerChoice();

    /*!
     * Creates a deterministic scheduler choice
     * @param deterministicChoice the (local) choice index
     */
    SchedulerChoice(uint_fast64_t deterministicChoice);

    /*!
     * Creates a scheduler choice that potentially considers randomization
     * @param randomizedChoice a distribution over the (local) choice indices
     */
    SchedulerChoice(storm::storage::Distribution<ValueType, uint_fast64_t> const& randomizedChoice);

    /*!
     * Creates a scheduler choice that potentially considers randomization
     * @param randomizedChoice a distribution over the (local) choice indices
     */
    SchedulerChoice(storm::storage::Distribution<ValueType, uint_fast64_t>&& randomizedChoice);

    /*!
     * Returns true iff this scheduler choice is defined
     */
    bool isDefined() const;

    /*!
     * Returns true iff this scheduler choice is deterministic (i.e., not randomized)
     */
    bool isDeterministic() const;

    /*!
     * If this choice is deterministic, this function returns the selected (local) choice index.
     * Otherwise, an exception is thrown.
     */
    uint_fast64_t getDeterministicChoice() const;

    /*!
     * Retrieves this choice in the form of a probability distribution.
     */
    storm::storage::Distribution<ValueType, uint_fast64_t> const& getChoiceAsDistribution() const;

    /*!
     * Changes the value type of this scheduler choice to the given one.
     */
    template<typename NewValueType>
    SchedulerChoice<NewValueType> toValueType() const {
        storm::storage::Distribution<NewValueType, uint_fast64_t> newDistribution;
        for (auto const& stateValuePair : distribution) {
            newDistribution.addProbability(stateValuePair.first, storm::utility::convertNumber<NewValueType>(stateValuePair.second));
        }
        return SchedulerChoice<NewValueType>(std::move(newDistribution));
    }

   private:
    storm::storage::Distribution<ValueType, uint_fast64_t> distribution;
};

template<typename ValueType>
std::ostream& operator<<(std::ostream& out, SchedulerChoice<ValueType> const& schedulerChoice);
}  // namespace storage
}  // namespace storm
