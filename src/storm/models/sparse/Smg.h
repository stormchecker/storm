#pragma once

#include "storm/logic/PlayerCoalition.h"
#include "storm/models/sparse/NondeterministicModel.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/PlayerIndex.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class represents a stochastic multiplayer game.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class Smg : public NondeterministicModel<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param components The components for this model.
     */
    Smg(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    Smg(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    Smg(Smg<ValueType, RewardModelType> const& other) = default;
    Smg& operator=(Smg<ValueType, RewardModelType> const& other) = default;

    Smg(Smg<ValueType, RewardModelType>&& other) = default;
    Smg& operator=(Smg<ValueType, RewardModelType>&& other) = default;

    std::vector<storm::storage::PlayerIndex> const& getStatePlayerIndications() const;
    storm::storage::PlayerIndex getPlayerOfState(uint64_t stateIndex) const;
    storm::storage::PlayerIndex getPlayerIndex(std::string const& playerName) const;
    std::map<std::string, storm::storage::PlayerIndex> const& getPlayerNamesToIndex() const;
    storm::storage::BitVector computeStatesOfCoalition(storm::logic::PlayerCoalition const& coalition) const;
    uint64_t getNumberOfPlayers() const;

   private:
    // Assigns the controlling player to each state.
    // If a state has storm::storage::INVALID_PLAYER_INDEX, it shall be the case that the choice at that state is unique
    std::vector<storm::storage::PlayerIndex> statePlayerIndications;
    // A mapping of player names to player indices.
    std::map<std::string, storm::storage::PlayerIndex> playerNameToIndexMap;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
