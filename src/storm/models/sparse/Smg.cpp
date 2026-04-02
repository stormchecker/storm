#include "storm/models/sparse/Smg.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/vector.h"

namespace storm {
namespace models {
namespace sparse {

template<typename ValueType, typename RewardModelType>
Smg<ValueType, RewardModelType>::Smg(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components)
    : NondeterministicModel<ValueType, RewardModelType>(ModelType::Smg, components), statePlayerIndications(components.statePlayerIndications.get()) {
    if (components.playerNameToIndexMap) {
        playerNameToIndexMap = components.playerNameToIndexMap.get();
    }
    // Otherwise the map remains empty.
}

template<typename ValueType, typename RewardModelType>
Smg<ValueType, RewardModelType>::Smg(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components)
    : NondeterministicModel<ValueType, RewardModelType>(ModelType::Smg, std::move(components)),
      statePlayerIndications(std::move(components.statePlayerIndications.get())) {
    if (components.playerNameToIndexMap) {
        playerNameToIndexMap = std::move(components.playerNameToIndexMap.get());
    }
    // Otherwise the map remains empty.
}

template<typename ValueType, typename RewardModelType>
std::vector<storm::storage::PlayerIndex> const& Smg<ValueType, RewardModelType>::getStatePlayerIndications() const {
    return statePlayerIndications;
}

template<typename ValueType, typename RewardModelType>
storm::storage::PlayerIndex Smg<ValueType, RewardModelType>::getPlayerOfState(uint64_t stateIndex) const {
    STORM_LOG_ASSERT(stateIndex < this->getNumberOfStates(), "Invalid state index: " << stateIndex << ".");
    return statePlayerIndications[stateIndex];
}

template<typename ValueType, typename RewardModelType>
storm::storage::PlayerIndex Smg<ValueType, RewardModelType>::getPlayerIndex(std::string const& playerName) const {
    auto findIt = playerNameToIndexMap.find(playerName);
    STORM_LOG_THROW(findIt != playerNameToIndexMap.end(), storm::exceptions::InvalidArgumentException, "Unknown player name '" << playerName << "'.");
    return findIt->second;
}

template<typename ValueType, typename RewardModelType>
std::map<std::string, storm::storage::PlayerIndex> const& Smg<ValueType, RewardModelType>::getPlayerNamesToIndex() const {
    return playerNameToIndexMap;
}

template<typename ValueType, typename RewardModelType>
storm::storage::PlayerIndex Smg<ValueType, RewardModelType>::getNumberOfPlayers() const {
    return playerNameToIndexMap.size();
}

template<typename ValueType, typename RewardModelType>
storm::storage::BitVector Smg<ValueType, RewardModelType>::computeStatesOfCoalition(storm::logic::PlayerCoalition const& coalition) const {
    // Create a set and a bit vector encoding the coalition for faster access
    std::set<storm::storage::PlayerIndex> coalitionAsIndexSet;
    for (auto const& player : coalition.getPlayers()) {
        if (std::holds_alternative<std::string>(player)) {
            coalitionAsIndexSet.insert(getPlayerIndex(std::get<std::string>(player)));
        } else {
            STORM_LOG_ASSERT(std::holds_alternative<storm::storage::PlayerIndex>(player), "Player identifier has unexpected type.");
            coalitionAsIndexSet.insert(std::get<storm::storage::PlayerIndex>(player));
        }
    }
    storm::storage::BitVector coalitionAsBitVector(*coalitionAsIndexSet.rbegin() + 1, false);
    for (auto const& pi : coalitionAsIndexSet) {
        coalitionAsBitVector.set(pi);
    }

    // Now create the actual result
    storm::storage::BitVector result(this->getNumberOfStates(), false);
    for (uint64_t state = 0; state < this->getNumberOfStates(); ++state) {
        auto const& pi = statePlayerIndications[state];
        if (pi < coalitionAsBitVector.size() && coalitionAsBitVector.get(pi)) {
            result.set(state, true);
        }
    }

    return result;
}

template class Smg<double>;
template class Smg<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class Smg<storm::RationalNumber>;
template class Smg<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class Smg<storm::Interval>;
template class Smg<storm::RationalInterval>;
template class Smg<storm::RationalFunction>;

}  // namespace sparse
}  // namespace models
}  // namespace storm
