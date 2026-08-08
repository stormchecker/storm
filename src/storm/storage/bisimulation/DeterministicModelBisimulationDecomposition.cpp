#include "storm/storage/bisimulation/DeterministicModelBisimulationDecomposition.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"

namespace storm {
namespace storage {

using namespace bisimulation;

template<typename ModelType>
DeterministicModelBisimulationDecomposition<ModelType>::DeterministicModelBisimulationDecomposition(
    ModelType const& model, typename BisimulationDecomposition<ModelType, DeterministicModelBisimulationDecomposition::BlockDataType>::Options const& options)
    : BisimulationDecomposition<ModelType, DeterministicModelBisimulationDecomposition::BlockDataType>(model, options),
      probabilitiesToCurrentSplitter(model.getNumberOfStates(), storm::utility::zero<ValueType>()) {
    // Intentionally left empty.
}

template<typename ModelType>
std::pair<storm::storage::BitVector, storm::storage::BitVector> DeterministicModelBisimulationDecomposition<ModelType>::getStatesWithProbability01() {
    return storm::utility::graph::performProb01(this->backwardTransitions, this->options.phiStates.value(), this->options.psiStates.value());
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::splitOffDivergentStates() {
    std::vector<storm::storage::sparse::state_type> stateStack;
    stateStack.reserve(this->model.getNumberOfStates());
    storm::storage::BitVector nondivergentStates(this->model.getNumberOfStates());

    uint_fast64_t currentSize = this->partition.size();
    for (uint_fast64_t blockIndex = 0; blockIndex < currentSize; ++blockIndex) {
        auto& block = *this->partition.getBlocks()[blockIndex];
        nondivergentStates.clear();

        for (auto stateIt = this->partition.begin(block), stateIte = this->partition.end(block); stateIt != stateIte; ++stateIt) {
            if (nondivergentStates.get(*stateIt)) {
                continue;
            }

            // Now traverse the forward transitions of the current state and check whether there is a
            // transition to some other block.
            bool isDirectlyNonDivergent = false;
            for (auto const& successor : this->model.getTransitionMatrix().getRowGroup(*stateIt)) {
                // If there is such a transition, then we can mark all states in the current block that can
                // reach the state as non-divergent.
                if (this->partition.getBlock(successor.getColumn()) != block) {
                    isDirectlyNonDivergent = true;
                    break;
                }
            }

            if (isDirectlyNonDivergent) {
                stateStack.push_back(*stateIt);

                while (!stateStack.empty()) {
                    storm::storage::sparse::state_type currentState = stateStack.back();
                    stateStack.pop_back();
                    nondivergentStates.set(currentState);

                    for (auto const& predecessor : this->backwardTransitions.getRow(currentState)) {
                        if (this->partition.getBlock(predecessor.getColumn()) == block && !nondivergentStates.get(predecessor.getColumn())) {
                            stateStack.push_back(predecessor.getColumn());
                        }
                    }
                }
            }
        }

        if (!nondivergentStates.empty() && nondivergentStates.getNumberOfSetBits() != block.getNumberOfStates()) {
            // After performing the split, the current block will contain the divergent states only.
            this->partition.splitStates(block, nondivergentStates);

            // Since the remaining states in the block are divergent, we can mark the block as absorbing.
            // This also guarantees that the self-loop will be added to the state of the quotient
            // representing this block of states.
            block.data().setAbsorbing(true);
        } else if (nondivergentStates.empty()) {
            // If there are only diverging states in the block, we need to make it absorbing.
            block.data().setAbsorbing(true);
        }
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::initializeSilentProbabilities() {
    silentProbabilities.resize(this->model.getNumberOfStates(), storm::utility::zero<ValueType>());
    for (storm::storage::sparse::state_type state = 0; state < this->model.getNumberOfStates(); ++state) {
        Block<BlockDataType> const* currentBlockPtr = &this->partition.getBlock(state);
        for (auto const& successorEntry : this->model.getTransitionMatrix().getRowGroup(state)) {
            if (&this->partition.getBlock(successorEntry.getColumn()) == currentBlockPtr) {
                silentProbabilities[state] += getTransitionValue(successorEntry, state);
            }
        }
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::initializeWeakDtmcBisimulation() {
    // If we are creating the initial partition for weak bisimulation on DTMCs, we need to (a) split off all
    // divergent states of each initial block and (b) initialize the vector of silent probabilities.
    this->splitOffDivergentStates();
    this->initializeSilentProbabilities();
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::postProcessInitialPartition() {
    if (this->options.getType() == BisimulationType::Weak && this->model.getType() == storm::models::ModelType::Dtmc) {
        this->initializeWeakDtmcBisimulation();
    }

    if (this->options.getKeepRewards() && this->model.hasRewardModel() && this->options.getType() == BisimulationType::Weak) {
        // For a weak bisimulation that is to preserve reward properties, we have to flag all blocks of states
        // with non-zero reward as reward blocks so they can be refined wrt. strong bisimulation.

        // Here, we assume that the initial partition already respects state (and action) rewards. Therefore, it suffices to
        // check the first state of each block for a non-zero reward.
        std::optional<std::vector<ValueType>> const& optionalStateRewardVector = this->model.getUniqueRewardModel().getOptionalStateRewardVector();
        std::optional<std::vector<ValueType>> const& optionalStateActionRewardVector = this->model.getUniqueRewardModel().getOptionalStateActionRewardVector();
        for (auto& block : this->partition.getBlocks()) {
            auto state = *this->partition.begin(*block);
            block->data().setHasRewards((optionalStateRewardVector && !storm::utility::isZero(optionalStateRewardVector.value()[state])) ||
                                        (optionalStateActionRewardVector && !storm::utility::isZero(optionalStateActionRewardVector.value()[state])));
        }
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::initializeMeasureDrivenPartition() {
    BisimulationDecomposition<ModelType, BlockDataType>::initializeMeasureDrivenPartition();
    postProcessInitialPartition();
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::initializeLabelBasedPartition() {
    BisimulationDecomposition<ModelType, BlockDataType>::initializeLabelBasedPartition();
    postProcessInitialPartition();
}

template<typename ModelType>
typename DeterministicModelBisimulationDecomposition<ModelType>::ValueType const&
DeterministicModelBisimulationDecomposition<ModelType>::getProbabilityToSplitter(storm::storage::sparse::state_type const& state) const {
    return probabilitiesToCurrentSplitter[state];
}

template<typename ModelType>
bool DeterministicModelBisimulationDecomposition<ModelType>::isSilent(storm::storage::sparse::state_type const& state) const {
    return this->comparator.isOne(silentProbabilities[state]);
}

template<typename ModelType>
bool DeterministicModelBisimulationDecomposition<ModelType>::hasNonZeroSilentProbability(storm::storage::sparse::state_type const& state) const {
    return !this->comparator.isZero(silentProbabilities[state]);
}

template<typename ModelType>
typename DeterministicModelBisimulationDecomposition<ModelType>::ValueType DeterministicModelBisimulationDecomposition<ModelType>::getSilentProbability(
    storm::storage::sparse::state_type const& state) const {
    return silentProbabilities[state];
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::refinePredecessorBlockOfSplitterStrong(
    bisimulation::Block<BlockDataType>& block, std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue) {
    STORM_LOG_TRACE("Refining predecessor " << block.getId() << " of splitter");

    // Depending on the actions we need to take, the block to refine changes, so we need to keep track of it.
    Block<BlockDataType>* blockToRefineProbabilistically = &block;

    bool split = false;
    // If the new begin index has shifted to a non-trivial position, we need to split the block.
    if (block.getBeginIndex() != block.data().marker1() && block.getEndIndex() != block.data().marker1()) {
        split = true;
        this->partition.splitBlock(block, block.data().marker1());
        blockToRefineProbabilistically = block.getPreviousBlockPointer();

        // Keep track of whether this is a block with reward states.
        blockToRefineProbabilistically->data().setHasRewards(block.data().hasRewards());
    }

    split |= this->partition.splitBlock(
        *blockToRefineProbabilistically,
        [this](storm::storage::sparse::state_type state1, storm::storage::sparse::state_type state2) {
            return this->comparator.isLess(getProbabilityToSplitter(state1), getProbabilityToSplitter(state2));
        },
        [&splitterQueue, &block](Block<BlockDataType>& newBlock) {
            splitterQueue.emplace_back(&newBlock);
            newBlock.data().setSplitter();

            // Keep track of whether this is a block with reward states.
            newBlock.data().setHasRewards(block.data().hasRewards());
        });

    // If the predecessor block was split, we need to insert it into the splitter vector if it is not already
    // marked as a splitter.
    if (split && !blockToRefineProbabilistically->data().splitter()) {
        splitterQueue.emplace_back(blockToRefineProbabilistically);
        blockToRefineProbabilistically->data().setSplitter();
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::refinePredecessorBlocksOfSplitterStrong(
    std::list<Block<BlockDataType>*> const& predecessorBlocks, std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue) {
    for (auto block : predecessorBlocks) {
        refinePredecessorBlockOfSplitterStrong(*block, splitterQueue);

        // If the block was *not* split, we need to reset the markers by notifying the data.
        block->resetMarkers();

        // Remember that we have refined the block.
        block->data().setNeedsRefinement(false);
    }
}

template<typename ModelType>
bool DeterministicModelBisimulationDecomposition<ModelType>::possiblyNeedsRefinement(bisimulation::Block<BlockDataType> const& predecessorBlock) const {
    return predecessorBlock.getNumberOfStates() > 1 && !predecessorBlock.data().absorbing();
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::increaseProbabilityToSplitter(storm::storage::sparse::state_type predecessor,
                                                                                           bisimulation::Block<BlockDataType> const& predecessorBlock,
                                                                                           ValueType const& value) {
    STORM_LOG_TRACE("Increasing probability of " << predecessor << " to splitter by " << value << ".");
    storm::storage::sparse::state_type predecessorPosition = this->partition.getPosition(predecessor);

    // If the position of the state is to the right of marker1, we have not seen it before.
    if (predecessorPosition >= predecessorBlock.data().marker1()) {
        // Then, we just set the value.
        probabilitiesToCurrentSplitter[predecessor] = value;
    } else {
        // If the state was seen as a predecessor before, we add the value to the existing value.
        probabilitiesToCurrentSplitter[predecessor] += value;
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::moveStateToMarker1(storm::storage::sparse::state_type predecessor,
                                                                                bisimulation::Block<BlockDataType>& predecessorBlock) {
    this->partition.swapStates(predecessor, this->partition.getState(predecessorBlock.data().marker1()));
    predecessorBlock.data().incrementMarker1();
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::moveStateToMarker2(storm::storage::sparse::state_type predecessor,
                                                                                bisimulation::Block<BlockDataType>& predecessorBlock) {
    this->partition.swapStates(predecessor, this->partition.getState(predecessorBlock.data().marker2()));
    predecessorBlock.data().incrementMarker2();
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::moveStateInSplitter(storm::storage::sparse::state_type predecessor,
                                                                                 bisimulation::Block<BlockDataType>& predecessorBlock,
                                                                                 storm::storage::sparse::state_type currentPositionInSplitter,
                                                                                 uint_fast64_t& elementsToSkip) {
    storm::storage::sparse::state_type predecessorPosition = this->partition.getPosition(predecessor);

    // If the predecessors of the given predecessor were already explored, we can move it easily.
    if (predecessorPosition <= currentPositionInSplitter + elementsToSkip) {
        this->partition.swapStates(predecessor, this->partition.getState(predecessorBlock.data().marker1()));
        predecessorBlock.data().incrementMarker1();
    } else {
        // Otherwise, we need to move the predecessor, but we need to make sure that we explore its
        // predecessors later. We do this by moving it to a range at the beginning of the block that will hold
        // all predecessors in the splitter whose predecessors have yet to be explored.
        if (predecessorBlock.data().marker2() == predecessorBlock.data().marker1()) {
            this->partition.swapStatesAtPositions(predecessorBlock.data().marker2(), predecessorPosition);
            this->partition.swapStatesAtPositions(predecessorPosition, currentPositionInSplitter + elementsToSkip + 1);
        } else {
            this->partition.swapStatesAtPositions(predecessorBlock.data().marker2(), predecessorPosition);
            this->partition.swapStatesAtPositions(predecessorPosition, predecessorBlock.data().marker1());
            this->partition.swapStatesAtPositions(predecessorPosition, currentPositionInSplitter + elementsToSkip + 1);
        }

        // Since we had to move an already explored state to the right of the current position,
        ++elementsToSkip;
        predecessorBlock.data().incrementMarker1();
        predecessorBlock.data().incrementMarker2();
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::exploreRemainingStatesOfSplitter(
    bisimulation::Block<BlockDataType>& splitter, std::list<bisimulation::Block<BlockDataType>*>& predecessorBlocks) {
    for (auto splitterIt = this->partition.begin(splitter),
              splitterIte = this->partition.begin(splitter) + (splitter.data().marker2() - splitter.getBeginIndex());
         splitterIt != splitterIte; ++splitterIt) {
        storm::storage::sparse::state_type currentState = *splitterIt;

        for (auto const& predecessorEntry : this->backwardTransitions.getRow(currentState)) {
            storm::storage::sparse::state_type predecessor = predecessorEntry.getColumn();
            Block<BlockDataType>& predecessorBlock = this->partition.getBlock(predecessor);

            // If the block does not need to be refined, we skip it.
            if (!possiblyNeedsRefinement(predecessorBlock)) {
                continue;
            }

            // If we are computing a weak bisimulation on CTMCs and the predecessor block is the splitter, we
            // need to ignore it and proceed to the next predecessor.
            if (this->options.getType() == BisimulationType::Weak && this->model.getType() == storm::models::ModelType::Ctmc && predecessorBlock == splitter) {
                continue;
            }

            // We keep track of the probability of the predecessor moving to the splitter.
            increaseProbabilityToSplitter(predecessor, predecessorBlock, getTransitionValue(predecessorEntry, predecessor));

            // Only move the state if it has not been seen as a predecessor before.
            storm::storage::sparse::state_type predecessorPosition = this->partition.getPosition(predecessor);
            if (predecessorPosition >= predecessorBlock.data().marker1()) {
                moveStateToMarker1(predecessor, predecessorBlock);
            }

            // We must not insert the splitter itself if we are not computing a weak bisimulation on CTMCs.
            if (this->options.getType() != BisimulationType::Weak || this->model.getType() != storm::models::ModelType::Ctmc || predecessorBlock != splitter) {
                insertIntoPredecessorList(predecessorBlock, predecessorBlocks);
            }
        }
    }

    // Finally, we can reset the second marker.
    splitter.data().setMarker2(splitter.getBeginIndex());
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::updateSilentProbabilitiesBasedOnProbabilitiesToSplitter(
    bisimulation::Block<BlockDataType>& block) {
    // For all predecessors, we can set the probability to the current probability of moving to the splitter.
    for (auto stateIt = this->partition.begin(block), stateIte = this->partition.begin() + block.data().marker1(); stateIt != stateIte; ++stateIt) {
        silentProbabilities[*stateIt] = probabilitiesToCurrentSplitter[*stateIt];
    }
    // All non-predecessors have a silent probability of zero.
    for (auto stateIt = this->partition.begin() + block.data().marker1(), stateIte = this->partition.end(block); stateIt != stateIte; ++stateIt) {
        silentProbabilities[*stateIt] = storm::utility::zero<ValueType>();
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::updateSilentProbabilitiesBasedOnTransitions(bisimulation::Block<BlockDataType>& block) {
    for (auto stateIt = this->partition.begin(block), stateIte = this->partition.end(block); stateIt != stateIte; ++stateIt) {
        if (hasNonZeroSilentProbability(*stateIt)) {
            ValueType newSilentProbability = storm::utility::zero<ValueType>();
            for (auto const& successorEntry : this->model.getTransitionMatrix().getRow(*stateIt)) {
                if (this->partition.getBlock(successorEntry.getColumn()) == block) {
                    newSilentProbability += getTransitionValue(successorEntry, *stateIt);
                }
            }
            silentProbabilities[*stateIt] = newSilentProbability;
        }
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::computeConditionalProbabilitiesForNonSilentStates(bisimulation::Block<BlockDataType>& block) {
    for (auto stateIt = this->partition.begin() + block.getBeginIndex(), stateIte = this->partition.begin() + block.data().marker1(); stateIt != stateIte;
         ++stateIt) {
        if (!this->comparator.isOne(getSilentProbability(*stateIt))) {
            probabilitiesToCurrentSplitter[*stateIt] /= storm::utility::one<ValueType>() - getSilentProbability(*stateIt);
        }
    }
}

template<typename ModelType>
std::vector<uint_fast64_t> DeterministicModelBisimulationDecomposition<ModelType>::computeNonSilentBlocks(bisimulation::Block<BlockDataType>& block) {
    auto less = [this](storm::storage::sparse::state_type state1, storm::storage::sparse::state_type state2) {
        return probabilitiesToCurrentSplitter[state1] < probabilitiesToCurrentSplitter[state2];
    };
    this->partition.sortRange(block.getBeginIndex(), block.data().marker1(), less);
    return this->partition.computeRangesOfEqualValue(block.getBeginIndex(), block.data().marker1(), less);
}

template<typename ModelType>
std::vector<storm::storage::BitVector> DeterministicModelBisimulationDecomposition<ModelType>::computeWeakStateLabelingBasedOnNonSilentBlocks(
    bisimulation::Block<BlockDataType> const& block, std::vector<uint_fast64_t> const& nonSilentBlockIndices) {
    // Now that we have the split points of the non-silent states, we perform a backward search from
    // each non-silent state and label the predecessors with the class of the non-silent state.
    std::vector<storm::storage::BitVector> stateLabels(block.getNumberOfStates(), storm::storage::BitVector(nonSilentBlockIndices.size() - 1));

    std::vector<storm::storage::sparse::state_type> stateStack;
    stateStack.reserve(block.getNumberOfStates());
    for (uint_fast64_t stateClassIndex = 0; stateClassIndex < nonSilentBlockIndices.size() - 1; ++stateClassIndex) {
        for (auto stateIt = this->partition.begin() + nonSilentBlockIndices[stateClassIndex],
                  stateIte = this->partition.begin() + nonSilentBlockIndices[stateClassIndex + 1];
             stateIt != stateIte; ++stateIt) {
            stateStack.push_back(*stateIt);
            stateLabels[this->partition.getPosition(*stateIt) - block.getBeginIndex()].set(stateClassIndex);
            while (!stateStack.empty()) {
                storm::storage::sparse::state_type currentState = stateStack.back();
                stateStack.pop_back();

                for (auto const& predecessorEntry : this->backwardTransitions.getRow(currentState)) {
                    storm::storage::sparse::state_type predecessor = predecessorEntry.getColumn();

                    if (storm::utility::isZero(predecessorEntry.getValue())) {
                        continue;
                    }

                    // Only if the state is in the same block, is a silent state and it has not yet been
                    // labeled with the current label.
                    if (this->partition.getBlock(predecessor) == block && isSilent(predecessor) &&
                        !stateLabels[this->partition.getPosition(predecessor) - block.getBeginIndex()].get(stateClassIndex)) {
                        stateStack.push_back(predecessor);
                        stateLabels[this->partition.getPosition(predecessor) - block.getBeginIndex()].set(stateClassIndex);
                    }
                }
            }
        }
    }

    return stateLabels;
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::refinePredecessorBlockOfSplitterWeak(
    bisimulation::Block<BlockDataType>& block, std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue) {
    // First, we need to turn the one-step probabilities to go to the splitter to the conditional probabilities
    // for all non-silent states.
    computeConditionalProbabilitiesForNonSilentStates(block);

    // Then, we need to compute a labeling of the states that expresses which of the non-silent blocks they can reach.
    std::vector<uint_fast64_t> nonSilentBlockIndices = computeNonSilentBlocks(block);
    std::vector<storm::storage::BitVector> weakStateLabels = computeWeakStateLabelingBasedOnNonSilentBlocks(block, nonSilentBlockIndices);

    // Then split the block according to this labeling.
    // CAUTION: that this assumes that the positions of the states in the partition are not update until after
    // the sorting is over. Otherwise, this interferes with the data used in the sorting process.
    storm::storage::sparse::state_type originalBlockIndex = block.getBeginIndex();
    auto split = this->partition.splitBlock(
        block,
        [&weakStateLabels, originalBlockIndex, this](storm::storage::sparse::state_type state1, storm::storage::sparse::state_type state2) {
            return weakStateLabels[this->partition.getPosition(state1) - originalBlockIndex] <
                   weakStateLabels[this->partition.getPosition(state2) - originalBlockIndex];
        },
        [this, &splitterQueue, &block](bisimulation::Block<BlockDataType>& newBlock) {
            updateSilentProbabilitiesBasedOnTransitions(newBlock);

            // Insert the new block as a splitter.
            newBlock.data().setSplitter();
            splitterQueue.emplace_back(&newBlock);

            // Keep track of whether this is a block with reward states.
            newBlock.data().setHasRewards(block.data().hasRewards());
        });

    // If the block was split, we also update the silent probabilities.
    if (split) {
        updateSilentProbabilitiesBasedOnTransitions(block);

        if (!block.data().splitter()) {
            // Insert the new block as a splitter.
            block.data().setSplitter();
            splitterQueue.emplace_back(&block);
        }
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::refinePredecessorBlocksOfSplitterWeak(
    bisimulation::Block<BlockDataType> const& splitter, std::list<bisimulation::Block<BlockDataType>*> const& predecessorBlocks,
    std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue) {
    for (auto block : predecessorBlocks) {
        if (block->data().hasRewards()) {
            refinePredecessorBlockOfSplitterStrong(*block, splitterQueue);
        } else {
            if (*block != splitter) {
                refinePredecessorBlockOfSplitterWeak(*block, splitterQueue);
            } else {
                // If the block to split is the splitter itself, we must not do any splitting here.
            }
        }

        block->resetMarkers();
        block->data().setNeedsRefinement(false);
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::insertIntoPredecessorList(bisimulation::Block<BlockDataType>& predecessorBlock,
                                                                                       std::list<bisimulation::Block<BlockDataType>*>& predecessorBlocks) {
    // Insert the block into the list of blocks to refine (if that has not already happened).
    if (!predecessorBlock.data().needsRefinement()) {
        predecessorBlocks.emplace_back(&predecessorBlock);
        predecessorBlock.data().setNeedsRefinement();
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::refinePartitionBasedOnSplitter(bisimulation::Block<BlockDataType>& splitter,
                                                                                            std::vector<bisimulation::Block<BlockDataType>*>& splitterQueue) {
    STORM_LOG_TRACE("Refining partition based on splitter " << splitter.getId());

    // The outline of the refinement is as follows.
    //
    // We iterate over all states of the splitter and determine for each predecessor the state the probability
    // entering the splitter. These probabilities are written to a member vector so that after the iteration
    // process we have the probabilities of all predecessors of the splitter of entering the splitter in one
    // step. To directly separate the states having a transition into the splitter from the ones who do not,
    // we move the states to certain locations. That is, on encountering a predecessor of the splitter, it is
    // moved to the beginning of its block. If the predecessor is in the splitter itself, we have to be a bit
    // careful about where to move states.
    //
    // After this iteration, there may be states of the splitter whose predecessors have not yet been explored,
    // so this needs to be done now.
    //
    // Finally, we use the information obtained in the first part for the actual splitting process in which all
    // predecessor blocks of the splitter are split based on the probabilities computed earlier.
    std::list<Block<BlockDataType>*> predecessorBlocks;
    storm::storage::sparse::state_type currentPosition = splitter.getBeginIndex();
    bool splitterIsPredecessorBlock = false;
    for (auto splitterIt = this->partition.begin(splitter), splitterIte = this->partition.end(splitter); splitterIt != splitterIte;
         ++splitterIt, ++currentPosition) {
        storm::storage::sparse::state_type currentState = *splitterIt;

        uint_fast64_t elementsToSkip = 0;
        for (auto const& predecessorEntry : this->backwardTransitions.getRow(currentState)) {
            storm::storage::sparse::state_type predecessor = predecessorEntry.getColumn();
            storm::storage::sparse::state_type predecessorPosition = this->partition.getPosition(predecessor);
            Block<BlockDataType>& predecessorBlock = this->partition.getBlock(predecessor);

            // If the block does not need to be refined, we skip it.
            if (!possiblyNeedsRefinement(predecessorBlock)) {
                continue;
            }

            // If we are computing a weak bisimulation on CTMCs and the predecessor block is the splitter, we
            // need to ignore it and proceed to the next predecessor.
            if (this->options.getType() == BisimulationType::Weak && this->model.getType() == storm::models::ModelType::Ctmc && predecessorBlock == splitter) {
                continue;
            }

            // We keep track of the probability of the predecessor moving to the splitter.
            increaseProbabilityToSplitter(predecessor, predecessorBlock, getTransitionValue(predecessorEntry, predecessor));

            // We only need to move the predecessor if it is not already known as a predecessor already.
            if (predecessorPosition >= predecessorBlock.data().marker1()) {
                // If the predecessor block is not the splitter, we can move the state easily.
                if (predecessorBlock != splitter) {
                    moveStateToMarker1(predecessor, predecessorBlock);
                } else {
                    // If the predecessor is in the splitter, we need to be a bit more careful.
                    splitterIsPredecessorBlock = true;
                    moveStateInSplitter(predecessor, predecessorBlock, currentPosition, elementsToSkip);
                }

                insertIntoPredecessorList(predecessorBlock, predecessorBlocks);
            }
        }

        // If, as a consequence of shifting states, we need to skip some elements, do so now.
        splitterIt += elementsToSkip;
        currentPosition += elementsToSkip;
    }

    // If the splitter was a predecessor block of itself, we potentially need to explore some states that have
    // not been explored yet.
    if (splitterIsPredecessorBlock) {
        exploreRemainingStatesOfSplitter(splitter, predecessorBlocks);
    }

    // Finally, we split the block based on the precomputed probabilities and the chosen bisimulation type.
    if (this->options.getType() == BisimulationType::Strong || this->model.getType() == storm::models::ModelType::Ctmc) {
        // In the case of CTMCs and weak bisimulation, we still call the "splitStrong" method, but we already have
        // taken care of not adding the splitter to the predecessor blocks, so this is safe.
        refinePredecessorBlocksOfSplitterStrong(predecessorBlocks, splitterQueue);
    } else {
        // If the splitter is a predecessor of we can use the computed probabilities to update the silent
        // probabilities.
        if (splitterIsPredecessorBlock) {
            updateSilentProbabilitiesBasedOnProbabilitiesToSplitter(splitter);
        }

        refinePredecessorBlocksOfSplitterWeak(splitter, predecessorBlocks, splitterQueue);
    }
}

template<typename ModelType>
void DeterministicModelBisimulationDecomposition<ModelType>::buildQuotient() {
    // In order to create the quotient model, we need to construct
    // (a) the new transition matrix,
    // (b) the new labeling,
    // (c) the new reward structures.

    // Prepare a matrix builder for (a).
    storm::storage::SparseMatrixBuilder<ValueType> builder(this->size(), this->size());

    // Prepare the new state labeling for (b).
    storm::models::sparse::StateLabeling newLabeling(this->size());
    std::set<std::string> atomicPropositionsSet = this->options.respectedAtomicPropositions.value();
    atomicPropositionsSet.insert("init");
    std::vector<std::string> atomicPropositions = std::vector<std::string>(atomicPropositionsSet.begin(), atomicPropositionsSet.end());
    for (auto const& ap : atomicPropositions) {
        newLabeling.addLabel(ap);
    }

    // If the model had state rewards, we need to build the state rewards for the quotient as well.
    std::optional<std::vector<ValueType>> stateRewards;
    if (this->options.getKeepRewards() && this->model.hasRewardModel()) {
        stateRewards = std::vector<ValueType>(this->blocks.size());
    }

    // Now build (a) and (b) by traversing all blocks.
    for (uint_fast64_t blockIndex = 0; blockIndex < this->blocks.size(); ++blockIndex) {
        auto const& block = this->blocks[blockIndex];

        // Pick one representative state. For strong bisimulation it doesn't matter which state it is, because
        // they all behave equally.
        storm::storage::sparse::state_type representativeState = *block.begin();

        // However, for weak bisimulation, we need to make sure the representative state is a non-silent one (if
        // there is any such state).
        if (this->options.getType() == BisimulationType::Weak && this->model.getType() == storm::models::ModelType::Dtmc) {
            for (auto const& state : block) {
                if (!isSilent(state)) {
                    representativeState = state;
                    break;
                }
            }
        }

        Block<BlockDataType> const& oldBlock = this->partition.getBlock(representativeState);

        // If the block is absorbing, we simply add a self-loop.
        if (oldBlock.data().absorbing()) {
            builder.addNextValue(blockIndex, blockIndex, storm::utility::one<ValueType>());

            // If the block has a special representative state, we retrieve it now.
            if (oldBlock.data().hasRepresentativeState()) {
                representativeState = oldBlock.data().representativeState();
            }

            // Add all of the selected atomic propositions that hold in the representative state to the state
            // representing the block.
            for (auto const& ap : atomicPropositions) {
                if (this->model.getStateLabeling().getStateHasLabel(ap, representativeState)) {
                    newLabeling.addLabelToState(ap, blockIndex);
                }
            }
        } else {
            // Compute the outgoing transitions of the block.
            std::map<storm::storage::sparse::state_type, ValueType> blockProbability;
            for (auto const& entry : this->model.getTransitionMatrix().getRow(representativeState)) {
                storm::storage::sparse::state_type targetBlock = this->partition.getBlock(entry.getColumn()).getId();

                // If we are computing a weak bisimulation quotient, there is no need to add self-loops.
                if ((this->options.getType() == BisimulationType::Weak) && targetBlock == blockIndex && !oldBlock.data().hasRewards()) {
                    continue;
                }

                auto probIterator = blockProbability.find(targetBlock);
                if (probIterator != blockProbability.end()) {
                    probIterator->second += getTransitionValue(entry, representativeState);
                } else {
                    blockProbability[targetBlock] = getTransitionValue(entry, representativeState);
                }
            }

            // Now add them to the actual matrix.
            for (auto const& probabilityEntry : blockProbability) {
                if (this->options.getType() == BisimulationType::Weak && this->model.getType() == storm::models::ModelType::Dtmc &&
                    !oldBlock.data().hasRewards()) {
                    builder.addNextValue(blockIndex, probabilityEntry.first,
                                         probabilityEntry.second / (storm::utility::one<ValueType>() - getSilentProbability(representativeState)));
                } else {
                    builder.addNextValue(blockIndex, probabilityEntry.first, probabilityEntry.second);
                }
            }

            // Otherwise add all atomic propositions to the equivalence class that the representative state
            // satisfies.
            for (auto const& ap : atomicPropositions) {
                if (this->model.getStateLabeling().getStateHasLabel(ap, representativeState)) {
                    newLabeling.addLabelToState(ap, blockIndex);
                }
            }
        }

        // If the model has state rewards, we simply copy the state reward of the representative state, because
        // all states in a block are guaranteed to have the same state reward.
        if (this->options.getKeepRewards() && this->model.hasRewardModel()) {
            auto const& rewardModel = this->model.getUniqueRewardModel();
            if (rewardModel.hasStateRewards()) {
                stateRewards.value()[blockIndex] = rewardModel.getStateRewardVector()[representativeState];
            }
            if (rewardModel.hasStateActionRewards()) {
                stateRewards.value()[blockIndex] += rewardModel.getStateActionRewardVector()[representativeState];
            }
        }
    }

    // Now check which of the blocks of the partition contain at least one initial state.
    for (auto initialState : this->model.getInitialStates()) {
        Block<BlockDataType> const& initialBlock = this->partition.getBlock(initialState);
        newLabeling.addLabelToState("init", initialBlock.getId());
    }

    // Construct the reward model mapping.
    std::unordered_map<std::string, typename ModelType::RewardModelType> rewardModels;
    if (this->options.getKeepRewards() && this->model.hasRewardModel()) {
        STORM_LOG_THROW(this->model.hasUniqueRewardModel(), storm::exceptions::IllegalFunctionCallException, "Cannot preserve more than one reward model.");
        typename std::unordered_map<std::string, typename ModelType::RewardModelType>::const_iterator nameRewardModelPair =
            this->model.getRewardModels().begin();
        rewardModels.insert(std::make_pair(nameRewardModelPair->first, typename ModelType::RewardModelType(stateRewards)));
    }

    // Finally construct the quotient model.
    this->quotient = std::make_shared<ModelType>(builder.build(), std::move(newLabeling), std::move(rewardModels));
}

template<typename ModelType>
DeterministicModelBisimulationDecomposition<ModelType>::ValueType DeterministicModelBisimulationDecomposition<ModelType>::getTransitionValue(
    storm::storage::MatrixEntry<storm::storage::sparse::state_type, ValueType> const& matrixEntry, storm::storage::sparse::state_type state) const {
    if constexpr (std::is_same_v<ModelType, storm::models::sparse::Ctmc<typename ModelType::ValueType>>) {
        auto transitionValue = matrixEntry.getValue();
        // TODO: enable when removing CTMC rate matrix
        // transitionValue *= this->model.getExitRateVector().at(state);
        return transitionValue;
    } else {
        STORM_LOG_ASSERT(this->model.isDiscreteTimeModel(), "Unhandled model type");
        return matrixEntry.getValue();
    }
}

template class DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>>;
template class DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<double>>;

template class DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<storm::RationalNumber>>;
template class DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<storm::RationalNumber>>;

template class DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<storm::RationalFunction>>;
template class DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<storm::RationalFunction>>;
}  // namespace storage
}  // namespace storm
