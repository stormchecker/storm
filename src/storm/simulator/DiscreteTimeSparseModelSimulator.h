#pragma once

#include "storm/models/sparse/Model.h"
#include "storm/utility/random.h"

namespace storm {
namespace simulator {

/**
 * This class is a low-level interface to quickly sample from Discrete-Time Models
 * stored explicitly as a SparseModel.
 * Additional information about state, actions, should be obtained via the model itself.
 *
 * TODO: It may be nice to write a CPP wrapper that does not require to actually obtain such informations yourself.
 * @tparam ModelType
 */
template<typename ValueType, typename RewardModelType = storm::models::sparse::StandardRewardModel<ValueType>>
class DiscreteTimeSparseModelSimulator {
   public:
    DiscreteTimeSparseModelSimulator(storm::models::sparse::Model<ValueType, RewardModelType> const& model);
    void setSeed(uint64_t);
    bool step(uint64_t action);
    bool randomStep();
    std::vector<ValueType> const& getLastRewards() const;
    uint64_t getCurrentState() const;
    bool resetToInitial();

   protected:
    storm::models::sparse::Model<ValueType, RewardModelType> const& model;
    uint64_t currentState;
    std::vector<ValueType> lastRewards;
    std::vector<ValueType> zeroRewards;
    storm::utility::RandomProbabilityGenerator<ValueType> generator;
};
}  // namespace simulator
}  // namespace storm