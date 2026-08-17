#pragma once

#include "storm/storage/DistributionWithReward.h"
#include "storm/storage/bisimulation/BisimulationDecomposition.h"
#include "storm/storage/bisimulation/DeterministicBlockData.h"

namespace storm {
namespace storage {

/*!
 * This class represents the decomposition of a nondeterministic model into its bisimulation quotient.
 */
template<typename ModelType>
class NondeterministicModelBisimulationDecomposition : public BisimulationDecomposition<ModelType, bisimulation::DeterministicBlockData> {
   public:
    typedef bisimulation::DeterministicBlockData BlockDataType;
    typedef typename ModelType::ValueType ValueType;
    typedef typename ModelType::RewardModelType RewardModelType;

    /*!
     * Computes the bisimulation relation for the given model. Which kind of bisimulation is computed, is
     * customizable via the given options.
     *
     * @param model The model to decompose.
     * @param options The options that customize the computed bisimulation.
     */
    NondeterministicModelBisimulationDecomposition(ModelType const& model,
                                                   typename BisimulationDecomposition<ModelType, BlockDataType>::Options const& options);

   protected:
    virtual std::pair<storm::storage::BitVector, storm::storage::BitVector> getStatesWithProbability01() override;

    virtual void buildQuotient() override;

    virtual void refinePartitionBasedOnSplitter(bisimulation::Block<BlockDataType>& splitter,
                                                std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue) override;

    virtual void initialize() override;

   private:
    // Creates the mapping from the choice indices to the states.
    void createChoiceToStateMapping();

    // Initializes the quotient distributions wrt. to the current partition.
    void initializeQuotientDistributions();

    // Retrieves whether the given block possibly needs refinement.
    bool possiblyNeedsRefinement(bisimulation::Block<BlockDataType> const& block) const;

    // Splits the given block according to the current quotient distributions.
    bool splitBlockAccordingToCurrentQuotientDistributions(bisimulation::Block<BlockDataType>& block,
                                                           std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue);

    // Retrieves whether the quotient distributions of state 1 are considered to be less than the ones of state 2.
    bool quotientDistributionsLess(storm::storage::sparse::state_type state1, storm::storage::sparse::state_type state2) const;

    // Updates the ordered list of quotient distribution for the given state.
    void updateOrderedQuotientDistributions(storm::storage::sparse::state_type state);

    // Updates the quotient distributions of the predecessors of the new block by taking the probability mass
    // away from the old block.
    void updateQuotientDistributionsOfPredecessors(bisimulation::Block<BlockDataType> const& newBlock, bisimulation::Block<BlockDataType> const& oldBlock,
                                                   std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue);

    bool checkQuotientDistributions() const;
    bool checkBlockStable(bisimulation::Block<BlockDataType> const& newBlock) const;
    bool printDistributions(storm::storage::sparse::state_type state) const;
    bool checkDistributionsDifferent(bisimulation::Block<BlockDataType> const& block, storm::storage::sparse::state_type end) const;

    // A mapping from choice indices to the state that has this choice.
    std::vector<storm::storage::sparse::state_type> choiceToStateMapping;

    // A vector that holds the quotient distributions for all nondeterministic choices of all states.
    std::vector<storm::storage::DistributionWithReward<ValueType>> quotientDistributions;

    // A vector that stores for each state the ordered list of quotient distributions.
    std::vector<storm::storage::DistributionWithReward<ValueType> const*> orderedQuotientDistributions;
};
}  // namespace storage
}  // namespace storm
