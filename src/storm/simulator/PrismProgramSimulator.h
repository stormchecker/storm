#pragma once

#include <span>

#include "storm/generator/PrismNextStateGenerator.h"
#include "storm/storage/expressions/SimpleValuation.h"
#include "storm/storage/prism/Program.h"
#include "storm/utility/random.h"

namespace storm {
namespace simulator {

/**
 * This class provides a simulator interface on the prism program,
 * and uses the next state generator. While the next state generator has been tuned,
 * it is not targeted for simulation purposes. In particular, we (as of now)
 * always extend all actions as soon as we arrive in a state.
 * This may cause significant overhead, especially with a larger branching factor.
 *
 * On the other hand, this simulator is convenient for stepping through the model
 * as it potentially allows considering the next states.
 * Thus, while a performant alternative would be great, this simulator has its own merits.
 *
 * @tparam ValueType
 */
template<typename ValueType>
class DiscreteTimePrismProgramSimulator {
   public:
    /**
     * Initialize the simulator for a given prism program.
     *
     * @param program The prism program. Should have a unique initial state.
     * @param options The generator options that are used to generate successor states.
     */
    DiscreteTimePrismProgramSimulator(storm::prism::Program const& program, storm::generator::NextStateGeneratorOptions const& options);
    /**
     * Set the simulation seed.
     */
    void setSeed(uint64_t);
    /**
     *
     * @return A list of choices that encode the possibilities in the current state.
     * @note successor states are encoded using state indices that will potentially be invalidated as soon as the internal state of the simulator changes
     */
    std::span<generator::Choice<ValueType, uint32_t> const> getChoices() const;

    bool isSinkState() const;

    /**
     * Make a step and randomly select the successor. The action is given as an argument, the index reflects the index of the getChoices vector that can be
     * accessed.
     *
     * @param actionNumber The action to select.
     * @return true, if this action can be taken.
     */
    bool step(uint64_t actionNumber);
    /**
     * Accessor for the last state action reward and the current state reward, added together.
     * @return A vector with te number of rewards.
     */
    std::vector<ValueType> const& getLastRewards() const;
    generator::CompressedState const& getCurrentState() const;
    expressions::SimpleValuation getCurrentStateAsValuation() const;
    std::vector<std::string> getCurrentStateLabelling() const;

    storm::json<ValueType> getStateAsJson() const;

    storm::json<ValueType> getObservationAsJson() const;

    std::string getCurrentStateString() const;
    /**
     * Reset to the (unique) initial state.
     *
     * @return
     */
    bool resetToInitial();

    bool resetToState(generator::CompressedState const& compressedState);

    bool resetToState(expressions::SimpleValuation const& valuationState);

    /**
     * The names of the rewards that are returned.
     */
    std::vector<std::string> getRewardNames() const;

   protected:
    bool explore();
    void clearStateCaches();
    /**
     * Helper function for (temp) storing states.
     */
    uint32_t getOrAddStateIndex(generator::CompressedState const&);

    /// The program that we are simulating.
    storm::prism::Program const& program;
    /// The current state in the program, in its compressed form.
    generator::CompressedState currentState;
    /// Generator for the next states
    std::shared_ptr<storm::generator::PrismNextStateGenerator<ValueType, uint32_t>> stateGenerator;
    /// Obtained behavior of a state
    generator::StateBehavior<ValueType> behavior;
    /// Helper for last action reward construction
    std::vector<ValueType> zeroRewards;
    /// Stores the action rewards from the last action.
    std::vector<ValueType> lastActionRewards;
    /// Random number generator
    storm::utility::RandomProbabilityGenerator<ValueType> generator;
    /// Data structure to temp store states.
    storm::storage::BitVectorHashMap<uint32_t> stateToId;

    std::unordered_map<uint32_t, generator::CompressedState> idToState;

   private:
    // Create a callback for the next-state generator to enable it to request the index of states.
    std::function<uint32_t(generator::CompressedState const&)> stateToIdCallback =
        std::bind(&DiscreteTimePrismProgramSimulator<ValueType>::getOrAddStateIndex, this, std::placeholders::_1);
};
}  // namespace simulator
}  // namespace storm
