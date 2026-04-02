#include "storm/generator/Choice.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/BoostTypes.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace generator {

template<typename ValueType, typename StateType>
Choice<ValueType, StateType>::Choice(uint_fast64_t actionIndex, bool markovian)
    : markovian(markovian), actionIndex(actionIndex), distribution(), totalMass(storm::utility::zero<ValueType>()), rewards(), labels() {
    // Intentionally left empty.
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::add(Choice const& other) {
    STORM_LOG_THROW(this->markovian == other.markovian, storm::exceptions::InvalidOperationException, "Type of choices do not match.");
    STORM_LOG_THROW(this->actionIndex == other.actionIndex, storm::exceptions::InvalidOperationException, "Action index of choices do not match.");
    STORM_LOG_THROW(this->rewards.size() == other.rewards.size(), storm::exceptions::InvalidOperationException, "Reward value sizes of choices do not match.");

    // Add the elements to the distribution.
    this->distribution.add(other.distribution);

    // Update the total mass of the choice.
    this->totalMass += other.totalMass;

    // Add all reward values.
    auto otherRewIt = other.rewards.begin();
    for (auto& rewardValue : this->rewards) {
        rewardValue += *otherRewIt;
    }

    // Join label sets and origin data if given.
    if (other.labels) {
        this->addLabels(other.labels.get());
    }
    if (other.originData) {
        this->addOriginData(other.originData.get());
    }
}

template<typename ValueType, typename StateType>
StateType Choice<ValueType, StateType>::sampleFromDistribution(ValueType const& quantile) const {
    return distribution.sampleFromDistribution(quantile);
}

template<typename ValueType, typename StateType>
typename storm::storage::Distribution<ValueType, StateType>::iterator Choice<ValueType, StateType>::begin() {
    return distribution.begin();
}

template<typename ValueType, typename StateType>
typename storm::storage::Distribution<ValueType, StateType>::const_iterator Choice<ValueType, StateType>::begin() const {
    return distribution.cbegin();
}

template<typename ValueType, typename StateType>
typename storm::storage::Distribution<ValueType, StateType>::iterator Choice<ValueType, StateType>::end() {
    return distribution.end();
}

template<typename ValueType, typename StateType>
typename storm::storage::Distribution<ValueType, StateType>::const_iterator Choice<ValueType, StateType>::end() const {
    return distribution.cend();
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::addLabel(std::string const& newLabel) {
    if (!labels) {
        labels = std::set<std::string>();
    }
    labels->insert(newLabel);
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::addLabels(std::set<std::string> const& newLabels) {
    if (labels) {
        labels->insert(newLabels.begin(), newLabels.end());
    } else {
        labels = newLabels;
    }
}

template<typename ValueType, typename StateType>
bool Choice<ValueType, StateType>::hasLabels() const {
    return labels.is_initialized();
}

template<typename ValueType, typename StateType>
std::set<std::string> const& Choice<ValueType, StateType>::getLabels() const {
    return labels.get();
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::setPlayerIndex(storm::storage::PlayerIndex const& playerIndex) {
    this->playerIndex = playerIndex;
}

template<typename ValueType, typename StateType>
bool Choice<ValueType, StateType>::hasPlayerIndex() const {
    return playerIndex.is_initialized();
}

template<typename ValueType, typename StateType>
storm::storage::PlayerIndex const& Choice<ValueType, StateType>::getPlayerIndex() const {
    return playerIndex.get();
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::addOriginData(boost::any const& data) {
    if (!this->originData || this->originData->empty()) {
        this->originData = data;
    } else {
        if (!data.empty()) {
            // Reaching this point means that the both the existing and the given data are non-empty

            auto existingDataAsIndexSet = boost::any_cast<storm::storage::FlatSet<uint_fast64_t>>(&this->originData.get());
            if (existingDataAsIndexSet != nullptr) {
                auto givenDataAsIndexSet = boost::any_cast<storm::storage::FlatSet<uint_fast64_t>>(&data);
                STORM_LOG_THROW(givenDataAsIndexSet != nullptr, storm::exceptions::InvalidOperationException,
                                "Types of existing and given choice origin data do not match.");
                existingDataAsIndexSet->insert(givenDataAsIndexSet->begin(), givenDataAsIndexSet->end());
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                                "Type of choice origin data (aka " << data.type().name() << ") is not implemented.");
            }
        }
    }
}

template<typename ValueType, typename StateType>
bool Choice<ValueType, StateType>::hasOriginData() const {
    return originData.is_initialized();
}

template<typename ValueType, typename StateType>
boost::any const& Choice<ValueType, StateType>::getOriginData() const {
    return originData.get();
}

template<typename ValueType, typename StateType>
uint_fast64_t Choice<ValueType, StateType>::getActionIndex() const {
    return actionIndex;
}

template<typename ValueType, typename StateType>
ValueType Choice<ValueType, StateType>::getTotalMass() const {
    return totalMass;
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::addProbability(StateType const& state, ValueType const& value) {
    totalMass += value;
    distribution.addProbability(state, value);
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::addReward(ValueType const& value) {
    rewards.push_back(value);
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::addRewards(std::vector<ValueType>&& values) {
    this->rewards = std::move(values);
}

template<typename ValueType, typename StateType>
std::vector<ValueType> const& Choice<ValueType, StateType>::getRewards() const {
    return rewards;
}

template<typename ValueType, typename StateType>
bool Choice<ValueType, StateType>::isMarkovian() const {
    return markovian;
}

template<typename ValueType, typename StateType>
std::size_t Choice<ValueType, StateType>::size() const {
    return distribution.size();
}

template<typename ValueType, typename StateType>
void Choice<ValueType, StateType>::reserve(std::size_t const& size) {
    distribution.reserve(size);
}

template<typename ValueType, typename StateType>
std::ostream& operator<<(std::ostream& out, Choice<ValueType, StateType> const& choice) {
    out << "<";
    for (auto const& stateProbabilityPair : choice) {
        out << stateProbabilityPair.first << " : " << stateProbabilityPair.second << ", ";
    }
    out << ">";
    return out;
}

template struct Choice<double>;

template struct Choice<storm::RationalNumber>;
template struct Choice<storm::RationalFunction>;
template struct Choice<storm::Interval>;
template struct Choice<storm::RationalInterval>;
}  // namespace generator
}  // namespace storm
