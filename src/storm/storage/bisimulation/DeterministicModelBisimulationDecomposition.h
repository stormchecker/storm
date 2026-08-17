#pragma once

#include "storm/storage/bisimulation/BisimulationDecomposition.h"
#include "storm/storage/bisimulation/DeterministicBlockData.h"

namespace storm {
namespace storage {

/*!
 * This class represents the decomposition of a deterministic model into its bisimulation quotient.
 */
template<typename ModelType>
class DeterministicModelBisimulationDecomposition : public BisimulationDecomposition<ModelType, bisimulation::DeterministicBlockData> {
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
    DeterministicModelBisimulationDecomposition(ModelType const& model, typename BisimulationDecomposition<ModelType, BlockDataType>::Options const& options);

   protected:
    virtual std::pair<storm::storage::BitVector, storm::storage::BitVector> getStatesWithProbability01() override;

    virtual void initializeMeasureDrivenPartition() override;

    virtual void initializeLabelBasedPartition() override;

    virtual void buildQuotient() override;

    virtual void refinePartitionBasedOnSplitter(bisimulation::Block<BlockDataType>& splitter,
                                                std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue) override;

   private:
    // Post-processes the initial partition to properly initialize it.
    void postProcessInitialPartition();

    // Refines the given block wrt to strong bisimulation.
    void refinePredecessorBlockOfSplitterStrong(bisimulation::Block<BlockDataType>& block, std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue);

    // Refines the predecessor blocks wrt. strong bisimulation.
    void refinePredecessorBlocksOfSplitterStrong(std::list<bisimulation::Block<BlockDataType>*> const& predecessorBlocks,
                                                 std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue);

    /*!
     * Performs the necessary steps to compute a weak bisimulation on a DTMC.
     */
    void initializeWeakDtmcBisimulation();

    /*!
     * Splits all blocks of the current partition into a block that contains all divergent states and another
     * block containing the non-divergent states.
     */
    void splitOffDivergentStates();

    /*!
     * Initializes the vector of silent probabilities.
     */
    void initializeSilentProbabilities();

    // Retrieves the probability of going into the splitter for the given state.
    ValueType const& getProbabilityToSplitter(storm::storage::sparse::state_type const& state) const;

    // Retrieves the silent probability for the given state.
    ValueType getSilentProbability(storm::storage::sparse::state_type const& state) const;

    // Retrieves whether the given state is silent.
    bool isSilent(storm::storage::sparse::state_type const& state) const;

    // Retrieves whether the given state has a non-zero silent probability.
    bool hasNonZeroSilentProbability(storm::storage::sparse::state_type const& state) const;

    // Retrieves whether the given predecessor of the splitters possibly needs refinement.
    bool possiblyNeedsRefinement(bisimulation::Block<BlockDataType> const& predecessorBlock) const;

    // Moves the given state to the position marked by marker1 moves the marker one step further.
    void moveStateToMarker1(storm::storage::sparse::state_type predecessor, bisimulation::Block<BlockDataType>& predecessorBlock);

    // Moves the given state to the position marked by marker2 the marker one step further.
    void moveStateToMarker2(storm::storage::sparse::state_type predecessor, bisimulation::Block<BlockDataType>& predecessorBlock);

    // Moves the given state to a proper place in the splitter, depending on where the predecessor is located.
    void moveStateInSplitter(storm::storage::sparse::state_type predecessor, bisimulation::Block<BlockDataType>& predecessorBlock,
                             storm::storage::sparse::state_type currentPositionInSplitter, uint_fast64_t& elementsToSkip);

    // Increases the probability of moving to the current splitter for the given state.
    void increaseProbabilityToSplitter(storm::storage::sparse::state_type predecessor, bisimulation::Block<BlockDataType> const& predecessorBlock,
                                       ValueType const& value);

    // Explores the remaining states of the splitter.
    void exploreRemainingStatesOfSplitter(bisimulation::Block<BlockDataType>& splitter, std::list<bisimulation::Block<BlockDataType>*>& predecessorBlocks);

    // Updates the silent probabilities of the states in the block based on the probabilities of going to the splitter.
    void updateSilentProbabilitiesBasedOnProbabilitiesToSplitter(bisimulation::Block<BlockDataType>& block);

    // Updates the silent probabilities of the states in the block based on a forward exploration of the transitions
    // of the states.
    void updateSilentProbabilitiesBasedOnTransitions(bisimulation::Block<BlockDataType>& block);

    // Refines the given block wrt to weak bisimulation in DTMCs.
    void refinePredecessorBlockOfSplitterWeak(bisimulation::Block<BlockDataType>& block, std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue);

    // Refines the predecessor blocks of the splitter wrt. weak bisimulation in DTMCs.
    void refinePredecessorBlocksOfSplitterWeak(bisimulation::Block<BlockDataType> const& splitter,
                                               std::list<bisimulation::Block<BlockDataType>*> const& predecessorBlocks,
                                               std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue);

    // Converts the one-step probabilities of going into the splitter into the conditional probabilities needed
    // for weak bisimulation (on DTMCs).
    void computeConditionalProbabilitiesForNonSilentStates(bisimulation::Block<BlockDataType>& block);

    // Computes the (indices of the) blocks of non-silent states within the block.
    std::vector<uint_fast64_t> computeNonSilentBlocks(bisimulation::Block<BlockDataType>& block);

    // Computes a labeling for all states of the block that identifies in which block they need to end up.
    std::vector<storm::storage::BitVector> computeWeakStateLabelingBasedOnNonSilentBlocks(bisimulation::Block<BlockDataType> const& block,
                                                                                          std::vector<uint_fast64_t> const& nonSilentBlockIndices);

    // Inserts the block into the list of predecessors if it is not already contained.
    void insertIntoPredecessorList(bisimulation::Block<BlockDataType>& predecessorBlock, std::list<bisimulation::Block<BlockDataType>*>& predecessorBlocks);

    ValueType getTransitionValue(storm::storage::MatrixEntry<storm::storage::sparse::state_type, ValueType> const& matrixEntry,
                                 [[maybe_unused]] storm::storage::sparse::state_type state) const;

    // A vector that holds the probabilities of states going into the splitter. This is used by the method that
    // refines a block based on probabilities.
    std::vector<ValueType> probabilitiesToCurrentSplitter;

    // A vector mapping each state to its silent probability.
    std::vector<ValueType> silentProbabilities;
};
}  // namespace storage
}  // namespace storm
