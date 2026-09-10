#include "storm/transformer/bisimulation/Refinement.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <ranges>
#include <type_traits>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/umb/model/Type.h"
#include "storm/transformer/bisimulation/Partition.h"
#include "storm/transformer/bisimulation/Signatures.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

namespace detail {

/*!
 * Stores a mapping from states to ValueType
 * Adding values, reading values, and clearing can all be done in constant time.
 * However, the size of the map is linear in the total number of states
 */
template<typename ValueType>
class StateMapping {
   public:
    explicit StateMapping(uint64_t const numStates) : values(numStates, defaultValue()) {}

    /*!
     * @return Retrieves the currently stored values
     */
    std::vector<ValueType> const& getValues() const {
        return values;
    }

    /*!
     * @return the list of states currently holding a non-zero value
     */
    std::vector<uint64_t> const& getNonDefaultStates() const {
        return nonDefaultStates;
    }

    /*!
     * Adds value to the currently mapped value of the given state
     */
    template<typename T>
    void addValue(uint64_t const state, T value) {
        if constexpr (std::is_same_v<ValueType, std::set<uint64_t>>) {
            if (values[state].empty()) {
                nonDefaultStates.push_back(state);
            }
            values[state].insert(value);
        } else {
            STORM_LOG_ASSERT(!storm::utility::isZero(value), "Did not expect adding 0 probability");
            if (storm::utility::isZero(values[state])) {
                nonDefaultStates.push_back(state);
                values[state] = std::move(value);
            } else {
                values[state] += std::move(value);
            }
        }
    }

    /*!
     * Clears the set, i.e., writes the default value for all states.
     */
    void clear() {
        for (auto const& state : nonDefaultStates) {
            values[state] = defaultValue();
        }
        nonDefaultStates.clear();
    }

   private:
    static ValueType defaultValue() {
        if constexpr (std::is_same_v<ValueType, std::set<uint64_t>>) {
            return {};  // empty set
        } else {
            return storm::utility::zero<ValueType>();
        }
    }

    std::vector<ValueType> values;           // stores the value for each state
    std::vector<uint64_t> nonDefaultStates;  // stores those states with a non-default value
};

template<typename ValueType, SplitterRefinementMode Mode>
struct SplitterRefinementContext {
    static constexpr bool isWeak = Mode != SplitterRefinementMode::Strong;
    static constexpr bool isWeakDiscreteTime = Mode == SplitterRefinementMode::WeakDiscreteTime;

    SplitterRefinementContext(storm::models::sparse::Model<ValueType> const& model, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                              storm::bisimulation::Partition& partition, ValueType const tolerance, storm::OptionalRef<WeakBisimulationData> weakData)
        : model(model), backwardTransitions(backwardTransitions), partition(partition), tolerance(tolerance), weakData(weakData), cache(partition) {
        STORM_LOG_ASSERT(isWeak == weakData.has_value(), "Weak bisimulation data must be given for (and only for) weak bisimulation.");
    }

    /*!
     * @return true iff the number of steps the given state takes within its own block is observable, i.e., iff it has to be treated as in strong bisimulation.
     */
    bool isStepSensitive(uint64_t const state) const
        requires isWeak
    {
        if constexpr (isWeakDiscreteTime) {
            return weakData->stepSensitiveStates.get(state);
        } else {
            // We ruled out all CTMCs with step-sensitive behavior (e.g. action-based rewards) during initialization.
            return false;
        }
    }

    storm::models::sparse::Model<ValueType> const& model;
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions;
    storm::bisimulation::Partition& partition;
    ValueType const tolerance;
    storm::bisimulation::Partition::OrderedBlockSet queue;

    /// Only set in the weak modes; its silentStates are kept up to date here, cf. updateSilentStates.
    storm::OptionalRef<WeakBisimulationData> weakData;

    /// The scratch space every mode needs.
    struct DefaultCache {
        explicit DefaultCache(Partition const& partition) : predecessorToSplitterProbabilities(partition.getNumberOfElements()), predecessorBlocks(partition) {}
        StateMapping<ValueType> predecessorToSplitterProbabilities;
        Partition::NonSuperBlockSet predecessorBlocks;
    };

    /// The additional scratch space that refining a block with respect to weak bisimulation on a discrete-time model needs, cf. refineBlockWeak.
    struct WeakDiscreteTimeCache : public DefaultCache {
        explicit WeakDiscreteTimeCache(Partition const& partition)
            : DefaultCache(partition),
              conditionalValues(partition.getNumberOfElements(), storm::utility::zero<ValueType>()),
              temporaryStateClasses(partition.getNumberOfElements(), std::numeric_limits<uint64_t>::max()) {}

        std::vector<ValueType> conditionalValues; // stores the 1-step probability of leaving the block for each state, conditioned on leaving the block at all
        std::vector<uint64_t> frontierStates;  // the non-silent states of the block currently being refined
        std::vector<uint64_t> temporaryStateClasses; // temporarily classifies the states of the block currently being refined
        std::deque<uint64_t> bfsQueue;       // work list of the backward search that computes temporaryStateClasses
        std::vector<uint64_t> nonSilentCandidates; // collects candidates of states that might become non-silent after refinement
    };

    /// Picks the scratch space that this mode actually uses, so that accessing the wrong one is a compile error.
    using Cache = std::conditional_t<isWeakDiscreteTime, WeakDiscreteTimeCache, DefaultCache>;
    Cache cache;
};

/*!
 * Recomputes the silent states of every block of the partition from scratch.
 */
template<typename ValueType, SplitterRefinementMode Mode>
void recomputeSilentStates(SplitterRefinementContext<ValueType, Mode>& context, auto const& candidates)
    requires(Mode != SplitterRefinementMode::Strong)
{
    auto& silentStates = context.weakData->silentStates;

    // A state can only ever go from silent to non-silent.
    for (uint64_t const state : candidates) {
        auto const row = context.model.getTransitionMatrix().getRow(state);
        silentStates.set(state, std::all_of(row.begin(), row.end(), [&context, &state](auto const& entry) {
                             return storm::utility::isZero(entry.getValue()) || context.partition.isSameBlock(state, entry.getColumn());
                         }));
    }
}

/*!
 * Checks that the recorded silent states match the current partition. Useful for sanity checks (e.g. via assertions). Does not assert itself.
 */
template<typename ValueType, SplitterRefinementMode Mode>
bool checkSilentStates(SplitterRefinementContext<ValueType, Mode> const& context)
    requires(Mode != SplitterRefinementMode::Strong)
{
    bool result = true;
    context.partition.forEachBlock([&context, &result](auto const& block) {
        for (uint64_t const state : block) {
            auto const row = context.model.getTransitionMatrix().getRow(state);
            bool const silent = std::all_of(row.begin(), row.end(), [&context, &state](auto const& entry) {
                return storm::utility::isZero(entry.getValue()) || context.partition.isSameBlock(state, entry.getColumn());
            });
            result = result && context.weakData->silentStates.get(state) == silent;
        }
    });
    return result;
}

/*!
 * Refines the given predecessor block of the current splitter with respect to the probability of moving to the splitter.
 * In WeakDiscreteTime mode this is applied to the blocks whose states carry a reward, for which the moves within the own block are observable.
 * Also applied in WeakContinuousTime mode, where the splitter probabilities are the transition rates and the splitter is not equal to predecessorBlockToSplit.
 */
template<typename ValueType, SplitterRefinementMode Mode>
void refineBlockStrong(SplitterRefinementContext<ValueType, Mode>& context, storm::bisimulation::Partition::Block const predecessorBlockToSplit) {
    auto& predecessorToSplitterProbabilities = context.cache.predecessorToSplitterProbabilities;

    // First split the block by whether a state is a predecessor of the splitter block or not
    // We do this by either iterating over the splitterPredecessors or the predecessorBlockToSplit, depending on what is shorter.
    auto [noPredecessors, predecessors] =
        predecessorToSplitterProbabilities.getNonDefaultStates().size() < predecessorBlockToSplit.size()
            ? context.partition.splitBlockByRange(predecessorBlockToSplit, predecessorToSplitterProbabilities.getNonDefaultStates())
            : context.partition.splitBlockByPredicate(
                  predecessorBlockToSplit, [&predecessorToSplitterProbabilities](
                                               auto const& state) { return !storm::utility::isZero(predecessorToSplitterProbabilities.getValues()[state]); });

    STORM_LOG_ASSERT(!predecessors.empty(), "The predecessor block should contain at least one predecessor state.");
    bool wasSplit = noPredecessors.size() > 0;

    if (wasSplit) {
        // add the block of states with no transition to the current splitter
        context.queue.insert(noPredecessors);
    }

    // Splitting with interval probabilities is not trivial: it is not clear whether the entire toSplitterProbs interval (which might be the sum of several
    // transitions) is feasible.
    static_assert(!storm::IsIntervalType<ValueType>, "Interval-valued splitter probabilities are not supported.");
    auto const& toSplitterProbs = predecessorToSplitterProbabilities.getValues();
    auto const less = [&toSplitterProbs](uint64_t const state1, uint64_t const state2) { return toSplitterProbs[state1] < toSplitterProbs[state2]; };
    if (storm::utility::isZero(context.tolerance)) {
        // Attention: Do not short circuit, i.e., wasSplit = wasSplit || foo() might not execute foo()
        wasSplit |= context.partition.splitBlockByOrder(predecessors, less);
    } else {
        auto const lessTolerance = [&toSplitterProbs, &context](uint64_t const state1, uint64_t const state2) {
            return toSplitterProbs[state1] + context.tolerance < toSplitterProbs[state2];
        };
        // Attention: Do not short circuit
        wasSplit |= context.partition.splitBlockByOrder(predecessors, less, lessTolerance);
    }

    if (wasSplit) {
        // Add all remaining blocks that were split to splitter queue.
        context.queue.erase(predecessorBlockToSplit);
        context.partition.forEachSubBlock(predecessors, [&context](auto const& block) { context.queue.insert(block); });
    }
}

/*!
 * Refines the given predecessor block of the current splitter with respect to the probability of moving to the splitter, conditioned on leaving the own block.
 * The non-silent states are classified by their conditional probabilities. The silent states are classified by the set of different non-silent states they
 * can reach. Specifically, the given block is split into
 * - several blocks of states that can only reach exactly one class of non-silent state (one such block per distinct conditional probability), and
 * - a block of states that can reach multiple different classes of non-silent states.
 */
template<typename ValueType>
void refineBlockWeak(SplitterRefinementContext<ValueType, SplitterRefinementMode::WeakDiscreteTime>& context,
                      storm::bisimulation::Partition::Block const block, storm::bisimulation::Partition::Block const splitterBlock) {
    STORM_LOG_ASSERT(!context.weakData->isDivergent(block), "Assumed a non-divergent block as a predecessor block of the splitter.");

    // Step 1: Gather the non-silent states of the given block. If the block is large, it is usually faster to iterate over the non-silent states
    auto& frontierStates = context.cache.frontierStates;
    auto const& silentStates = context.weakData->silentStates;
    frontierStates.clear();
    if (block.size() * 64 > silentStates.size()) {
        for (uint64_t nonSilentState = silentStates.getNextUnsetIndex(0); nonSilentState < silentStates.size();
             nonSilentState = silentStates.getNextUnsetIndex(nonSilentState + 1)) {
            if (context.partition.contains(block, nonSilentState)) {
                frontierStates.push_back(nonSilentState);
            }
        }
    } else {
        for (uint64_t const state : block) {
            if (!silentStates.get(state)) {
                frontierStates.push_back(state);
            }
        }
    }

    // Step 2: Compute conditional escape probability at frontier states
    auto& conditionalValues = context.cache.conditionalValues;
    auto const& toSplitterProbs = context.cache.predecessorToSplitterProbabilities.getValues();
    for (uint64_t const state : frontierStates) {
        if (storm::utility::isZero(toSplitterProbs[state])) {
            conditionalValues[state] = storm::utility::zero<ValueType>();  // the state has no transition into the splitter
            continue;
        }
        auto const row = context.model.getTransitionMatrix().getRow(state);
        STORM_LOG_ASSERT(std::any_of(row.begin(), row.end(),
                             [&context, &splitterBlock](auto const& entry) {
                                 return !storm::utility::isZero(entry.getValue()) && context.partition.contains(splitterBlock, entry.getColumn());
                             }), "Expected a transition into a splitter, but none was found.");
        ValueType escapeValue = storm::utility::zero<ValueType>();
        bool leavesOnlyToSplitter = true;
        for (auto const& entry : row) {
            if (storm::utility::isZero(entry.getValue()) || context.partition.isSameBlock(state, entry.getColumn())) {
                continue;  // moves within the own block are unobservable
            }
            escapeValue += entry.getValue();
            leavesOnlyToSplitter = leavesOnlyToSplitter && context.partition.contains(splitterBlock, entry.getColumn());
        }
        STORM_LOG_ASSERT(!storm::utility::isZero(escapeValue), "A non-silent state must be able to leave its block.");

        // If there are only transitions to the splitter, we set the conditional probability to 1 explicitly.
        // This avoids numerical issues where toSplitterProbs[state] and escapeValue do not match exactly.
        conditionalValues[state] = leavesOnlyToSplitter ? storm::utility::one<ValueType>() : toSplitterProbs[state] / escapeValue;
    }
    // Every state of a non-divergent block can leave the block; the first state that is non-silent on such a path witnesses this.
    STORM_LOG_ASSERT(!frontierStates.empty(), "A non-divergent block must contain a non-silent state.");

    // Step 3: Divide the frontier states into equivalence classes based on their conditional values
    auto& stateClasses = context.cache.temporaryStateClasses;
    uint64_t constexpr Unclassified = std::numeric_limits<uint64_t>::max(); // Indicates that no class has been assigned to a state
    uint64_t constexpr MultipleFrontiers =
        std::numeric_limits<uint64_t>::max() - 1;  // Indicates that multiple frontier states with different conditional values can be reached

    std::sort(frontierStates.begin(), frontierStates.end(),
              [&conditionalValues](uint64_t const state1, uint64_t const state2) { return conditionalValues[state1] < conditionalValues[state2]; });
    {
        // true if state1 and state2 should be in different classes. Assumes conditionalValues[state1] <= conditionalValues[state2].
        auto const haveDifferentClass = [&conditionalValues, &context](uint64_t const state1, uint64_t const state2) {
            if (storm::utility::isZero(context.tolerance)) {
                return conditionalValues[state1] != conditionalValues[state2];
            }
            return conditionalValues[state1] + context.tolerance < conditionalValues[state2];
        };

        uint64_t currClassRepresentative = frontierStates.front();
        stateClasses[currClassRepresentative] = currClassRepresentative;
        for (uint64_t const state : frontierStates | std::views::drop(1)) {
            if (haveDifferentClass(currClassRepresentative, state)) {
                currClassRepresentative = state;
            }
            stateClasses[state] = currClassRepresentative;
        }
    }

    // Step 4: Catch the trivial case where all states have the same class
    if (stateClasses[frontierStates.front()] == stateClasses[frontierStates.back()]) {
        // Just a single class, nothing to split.
        // Clean-up the touched classes before return.
        for (uint64_t const state : frontierStates) {
            stateClasses[state] = Unclassified;
        }
        return;
    }

    // Step 5: Classify the remaining states. Two states state1 and state2 are in the same class iff
    // - a) all frontier states reachable from state1 or from state2 are in the same class, or
    // - b) both, state1 and state2 each reach multiple frontier states with distinct classes.
    // We do a backwards search from the frontier states, propagating their class labels to their predecessors.
    // A BFS is used (rather than a DFS) to detect case b) more quickly.
    auto& bfsQueue = context.cache.bfsQueue;
    bfsQueue.clear();
    for (auto const state : frontierStates) {
        bfsQueue.push_back(state);
    }
    // We also collect candidates of states that might lose their silence after refinement: the states for which case b) applies that have a case a) successor.
    std::vector<uint64_t>& nonSilentCandidates = context.cache.nonSilentCandidates;
    nonSilentCandidates.clear();
    while (!bfsQueue.empty()) {
        uint64_t const currentState = bfsQueue.front();
        bfsQueue.pop_front();
        uint64_t const currentClass = stateClasses[currentState];
        STORM_LOG_ASSERT(currentClass != Unclassified, "The current state must have a class assigned.");
        for (auto const& predecessorEntry : context.backwardTransitions.getRow(currentState)) {
            uint64_t const predecessor = predecessorEntry.getColumn();
            uint64_t& predecessorClass = stateClasses[predecessor];
            if (predecessorClass == Unclassified) {
                // Ignore predecessors outside of the block.
                if (context.partition.isBlockOfElement(block, predecessor)) {
                    STORM_LOG_ASSERT(silentStates.get(predecessor), "An unclassified state must be silent.");
                    predecessorClass = currentClass; // propagate the current class
                    bfsQueue.push_back(predecessor);
                }
            } else if (predecessorClass == currentClass) {
                // The class of predecessor does not change. Nothing to do.
            } else if (predecessorClass == MultipleFrontiers) {
                // case b) applies for the predecessor, but case a) potentially applies for the current state.
                nonSilentCandidates.push_back(predecessor);
            } else if (silentStates.get(predecessor)) {
                // So far, we assumed that case a) applies for the predecessor, but we have now found a successor in a different class. Hence, Case b) applies.
                predecessorClass = MultipleFrontiers;
                bfsQueue.push_back(predecessor);
                nonSilentCandidates.push_back(predecessor);
            } else {
                // The last case only applies for a non-silent (i.e. frontier) predecessor. Hence, the class of the predecessor does not change. Nothing to do.
            }
        }
    }

    // Step 6: Split the block by the computed classes and enqueue the subblocks
    context.partition.splitBlockByOrder(
        block, [&stateClasses](uint64_t const state1, uint64_t const state2) { return stateClasses[state1] < stateClasses[state2]; });
    STORM_LOG_ASSERT(context.partition.isProperSuperBlock(block), "As there are multiple different frontier states, the block must be split.");
    context.queue.erase(block);
    context.partition.forEachSubBlock(block, [&context](auto const& subBlock) { context.queue.insert(subBlock); });

    // Step 7: update silent states and clear cached data.
    recomputeSilentStates(context, nonSilentCandidates);
    for (uint64_t const state : block) {
        stateClasses[state] = Unclassified;
    }
}

template<typename ValueType, SplitterRefinementMode Mode>
void refinePartitionBasedOnSplitter(SplitterRefinementContext<ValueType, Mode>& context, storm::bisimulation::Partition::Block const splitterBlock) {
    auto& predecessorToSplitterProbabilities = context.cache.predecessorToSplitterProbabilities;
    auto& predecessorBlocks = context.cache.predecessorBlocks;

    for (auto currentState : splitterBlock) {
        // Compute probability to enter splitter block for each predecessor
        for (const auto& predecessorEntry : context.backwardTransitions.getRow(currentState)) {
            auto predecessorState = predecessorEntry.getColumn();
            auto predecessorBlock = context.partition.getBlockOfElement(predecessorState);
            if (predecessorBlock.size() == 1) {
                continue;  // No need to try to split singleton blocks
            }
            if constexpr (SplitterRefinementContext<ValueType, Mode>::isWeak) {
                // The states of a divergent block can never leave it, so they all exhibit the same (unobservable) behavior and must not be split.
                if (context.weakData->divergentStates.get(predecessorState)) {
                    continue;
                }
                // For weak bisimulation, the moves of a non-step-sensitive state within its own block are unobservable.
                // Hence, the splitter must not split itself.
                if (context.partition.isEqualBlock(predecessorBlock, splitterBlock) && !context.isStepSensitive(predecessorState)) {
                    continue;
                }
            }
            predecessorToSplitterProbabilities.addValue(predecessorState, predecessorEntry.getValue());
            predecessorBlocks.insert(predecessorBlock);
        }
    }

    while (!predecessorBlocks.empty()) {
        auto const predecessorBlockToSplit = predecessorBlocks.pop();
        if constexpr (Mode == SplitterRefinementMode::WeakDiscreteTime) {
            if (!context.isStepSensitive(predecessorBlockToSplit.front())) {
                refineBlockWeak(context, predecessorBlockToSplit, splitterBlock);
                continue;
            }
        }
        refineBlockStrong(context, predecessorBlockToSplit);
    }

    // Reset the predecessorToSplitterProbabilities for the next iteration.
    predecessorToSplitterProbabilities.clear();
}

template<typename ValueType, SignatureMode SignatureMode>
struct SignatureRefinementContext {
    SignatureRefinementContext(storm::models::sparse::Model<ValueType> const& model, storm::bisimulation::Partition& partition,
                               Signatures<ValueType, SignatureMode>& signatures)
        : model(model), partition(partition), signatures(signatures), backwardTransitions(model.getBackwardTransitions()), cache(partition) {}

    storm::models::sparse::Model<ValueType> const& model;
    storm::bisimulation::Partition& partition;
    storm::bisimulation::Signatures<ValueType, SignatureMode>& signatures;
    storm::storage::SparseMatrix<ValueType> const backwardTransitions;
    storm::bisimulation::Partition::OrderedBlockMap<bool>
        queue;  // stores an extra flag for each element in the queue. The flag indicates whether we enforce exploring the predecessors of the block

    struct Cache {
        Cache(Partition const& partition) : predecessorToPivotBlocks(partition.getNumberOfElements()), predecessorBlocks(partition) {}
        StateMapping<std::set<uint64_t>> predecessorToPivotBlocks;
        Partition::NonSuperBlockSet predecessorBlocks;
    } cache;
};

template<typename ValueType, SignatureMode SignatureMode>
void refinePartitionBasedOnSignature(SignatureRefinementContext<ValueType, SignatureMode>& context, storm::bisimulation::Partition::Block const pivotBlock,
                                     bool const enforcePredecessorExploration) {
    // Split the pivot block B into B=B_1 cup B_2 cup ... cup B_n using signature refinement
    // First update the state signatures
    for (uint64_t const state : pivotBlock) {
        context.signatures.updateStateSignature(state);
    }
    // Then perform the signature-based split.
    bool pivotHasBeenSplit{false};
    if constexpr (SignatureMode == storm::bisimulation::SignatureMode::Exact) {
        pivotHasBeenSplit = context.partition.splitBlockByOrder(pivotBlock, context.signatures.getEquivalenceSplitOrder());
    } else {
        // In approximative mode, there provably is no transitive order on signatures that captures "approximately equal signatures".
        // We therefore split in two steps: first, we split by a coarse order based on structural properties of the state signature.
        // Then, we do a more expensive split based on clustering on each sub-block.
        pivotHasBeenSplit = context.partition.splitBlockByOrder(pivotBlock, context.signatures.getStructuralSplitOrder());
        auto const splitCondition = context.signatures.getApproximateSplitCondition();
        context.partition.forEachSubBlock(pivotBlock, [&context, &splitCondition, &pivotHasBeenSplit](auto const& subBlock) {
            pivotHasBeenSplit |= context.partition.splitBlockByClustering(subBlock, splitCondition);
        });
    }

    if (!pivotHasBeenSplit && !enforcePredecessorExploration) {
        // When the current pivot block is stable, there is no need to look into its predecessors. We can continue with the next pivot.
        return;
    }

    // When the pivot block is not stable, it means it has been split and the predecessor blocks have not been checked since that split.
    // Therefore, all the predecessor blocks need to checked again.
    // While we could just add all those predecessors to the queue, we instead try to split them first based on simple, graph-based criteria so that (expensive)
    // signature refinement is hopefully only applied to smaller blocks. Specifically, we split predecessor blocks based on which set of sub-blocks of the pivot
    // they can reach.

    // Gather predecessors of the pivot and their reachable sub-blocks
    auto& predecessorToPivotBlocks = context.cache.predecessorToPivotBlocks;
    auto& predecessorBlocks = context.cache.predecessorBlocks;
    uint64_t subBlockIndex = 0;
    context.partition.forEachSubBlock(pivotBlock, [&context, &predecessorToPivotBlocks, &predecessorBlocks, &subBlockIndex](auto const& subBlock) {
        for (uint64_t const state : subBlock) {
            for (auto const& predecessorEntry : context.backwardTransitions.getRow(state)) {
                auto const predecessorState = predecessorEntry.getColumn();
                auto const predecessorBlock = context.partition.getBlockOfElement(predecessorState);
                if (predecessorBlock.size() > 1) {
                    // No need to investigate singleton predecessor blocks as they cannot be split any further.
                    predecessorToPivotBlocks.addValue(predecessorState, subBlockIndex);
                    predecessorBlocks.insert(predecessorBlock);
                }
            }
        }
        ++subBlockIndex;
    });
    // Apply splitting of the pivot predecessors (similar to splitter-based refinement)
    while (!predecessorBlocks.empty()) {
        auto const predecessorBlock = predecessorBlocks.pop();
        // Split the predecessor block according to which sub-blocks of the pivot-block can be reached.
        auto const& toPivotBlocks = predecessorToPivotBlocks.getValues();

        // Split the block by whether a state is a predecessor of the pivot block or not
        // We do this by either iterating over the pivotPredecessorStates or the predecessorBlock, depending on what is shorter.
        auto [noPredecessors, predecessors] =
            predecessorToPivotBlocks.getNonDefaultStates().size() < predecessorBlock.size()
                ? context.partition.splitBlockByRange(predecessorBlock, predecessorToPivotBlocks.getNonDefaultStates())
                : context.partition.splitBlockByPredicate(predecessorBlock, [&toPivotBlocks](auto const& state) { return !toPivotBlocks[state].empty(); });

        // At least one state should be a predecessor of the pivot block (otherwise we wouldn't have found that block above)
        STORM_LOG_ASSERT(!predecessors.empty(), "The predecessor block should contain at least one predecessor state.");

        // Now apply the splitting based on which sub-blocks of the pivot block can be reached.
        // If we did not actually split the pivot block, this operation would have no effect.
        if (pivotHasBeenSplit) {
            context.partition.splitBlockByOrder(
                predecessors, [&toPivotBlocks](uint64_t const state1, uint64_t const state2) { return toPivotBlocks[state1] < toPivotBlocks[state2]; });
        } else {
            STORM_LOG_ASSERT(
                std::all_of(predecessors.begin(), predecessors.end(),
                            [&toPivotBlocks](uint64_t const& state) { return toPivotBlocks[state].size() == 1 && *toPivotBlocks[state].begin() == 0; }),
                "Expected all predecessor states to reach the pivot block.");
        }

        if (context.partition.isProperSuperBlock(predecessorBlock)) {
            // Erase the super block from the queue and the enforced unstable blocks (it might or might not be in there)
            context.queue.erase(predecessorBlock);
            // Add all sub-blocks to the queue. As we made a split, we must explore the predecessors of predecessorBlock.
            context.partition.forEachSubBlock(predecessorBlock, [&context](auto const& block) { context.queue[block] = true; });
        } else {
            // The simple, graph-based splitting was not effective. We must add the entire predecessorBlock to the queue. We do not have to
            // enforce that predecessors are explored.
            context.queue.try_emplace(predecessorBlock, false);
        }
    }
    // Reset the touched reachable-subblocks
    predecessorToPivotBlocks.clear();
}

}  // namespace detail

template<typename ValueType, SplitterRefinementMode Mode>
void performSplitterBasedRefinement(storm::models::sparse::Model<ValueType> const& model, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                    storm::bisimulation::Partition& partition, ValueType const tolerance, storm::OptionalRef<WeakBisimulationData> weakData) {
    static_assert(!storm::IsIntervalType<ValueType>, "Interval types are not supported for splitter-based refinement.");
    // Refinement for interval models requires limiting signatures to feasible intervals. This is rather difficult in a splitter-based setting.
    STORM_LOG_THROW(!model.isNondeterministicModel(), storm::exceptions::InvalidArgumentException,
                    "Splitter-based refinement is only supported for deterministic models.");
    STORM_LOG_THROW((storm::utility::isZero(tolerance) || !std::is_same_v<ValueType, storm::RationalFunction>), storm::exceptions::InvalidArgumentException,
                    "Splitter-based refinement with non-zero tolerance does not apply to parametric models.");
    detail::SplitterRefinementContext<ValueType, Mode> context(model, backwardTransitions, partition, tolerance, weakData);
    if constexpr (Mode != SplitterRefinementMode::Strong) {
        STORM_LOG_ASSERT(weakData->checkBlockHomogeneity(partition), "Partition is not homogeneous with respect to the weak bisimulation data.");
        STORM_LOG_ASSERT(detail::checkSilentStates(context), "The given silent states do not match the given partition.");
    }
    // Initially, add all current blocks to the queue.
    partition.forEachBlock([&context](auto const& block) { context.queue.insert(block); });

    // Perform the splitting until there are no more splitters.
    while (!context.queue.empty()) {
        // take the smallest block from the queue
        auto const splitterBlock = *context.queue.begin();
        context.queue.erase(context.queue.begin());
        STORM_LOG_ASSERT(!partition.isProperSuperBlock(splitterBlock), "Broken invariant: the queue should not contain blocks that have been split.");
        // Split the predecessor blocks and add them to the queue
        detail::refinePartitionBasedOnSplitter(context, splitterBlock);
    }

    if constexpr (Mode != SplitterRefinementMode::Strong) {
        // The refinement itself only keeps the silent states of the blocks refined through refineBlockWeak up to date.
        if (Mode == SplitterRefinementMode::WeakDiscreteTime) {
            detail::recomputeSilentStates(context, context.weakData->stepSensitiveStates);
        } else {
            detail::recomputeSilentStates(context, context.weakData->silentStates);
        }
        STORM_LOG_ASSERT(detail::checkSilentStates(context), "The silent states do not match the final partition.");
    }
}

template<typename ValueType, SignatureMode SignatureMode>
void performSignatureBasedRefinement(storm::models::sparse::Model<ValueType> const& model, storm::bisimulation::Partition& partition,
                                     Signatures<ValueType, SignatureMode>& signatures) {
    static_assert(!storm::IsIntervalType<ValueType>, "Interval types are not yet supported for signature-based refinement.");
    detail::SignatureRefinementContext<ValueType, SignatureMode> context(model, partition, signatures);
    // Initially, add all current blocks to the queue. No need to enforce exploring predecessors.
    partition.forEachBlock([&context](auto const& block) { context.queue.emplace(block, false); });

    while (!context.queue.empty()) {
        // take the smallest block from the queue
        auto const [pivotBlock, enforcePredecessorExploration] = *context.queue.begin();
        context.queue.erase(context.queue.begin());
        STORM_LOG_ASSERT(!partition.isProperSuperBlock(pivotBlock), "Broken invariant: the queue should not contain blocks that have been split.");
        // Split the pivotBlock based on its signature and split the predecessor blocks based on a simple, graph-based criterion
        detail::refinePartitionBasedOnSignature(context, pivotBlock, enforcePredecessorExploration);
    }

    // Singleton blocks are never re-examined once formed (because they cannot be split any further). Their cached signature can become stale if one of their
    // successor blocks is split afterwards. Refresh them here so that, as documented, all signatures are up to date once this function returns.
    partition.forEachBlock([&signatures](auto const& block) {
        if (block.size() == 1) {
            signatures.updateStateSignature(block.front());
        }
    });
}

// double
template void performSplitterBasedRefinement<double, SplitterRefinementMode::Strong>(storm::models::sparse::Model<double> const& model,
                                                                                     storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                                     storm::bisimulation::Partition& partition, double const tolerance,
                                                                                     storm::OptionalRef<WeakBisimulationData> weakData);
template void performSplitterBasedRefinement<double, SplitterRefinementMode::WeakDiscreteTime>(storm::models::sparse::Model<double> const& model,
                                                                                               storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                                               storm::bisimulation::Partition& partition,
                                                                                               double const tolerance,
                                                                                               storm::OptionalRef<WeakBisimulationData> weakData);
template void performSplitterBasedRefinement<double, SplitterRefinementMode::WeakContinuousTime>(
    storm::models::sparse::Model<double> const& model, storm::storage::SparseMatrix<double> const& backwardTransitions,
    storm::bisimulation::Partition& partition, double const tolerance, storm::OptionalRef<WeakBisimulationData> weakData);
template void performSignatureBasedRefinement<double, SignatureMode::Exact>(storm::models::sparse::Model<double> const& model,
                                                                            storm::bisimulation::Partition& partition,
                                                                            Signatures<double, SignatureMode::Exact>& signatures);
template void performSignatureBasedRefinement<double, SignatureMode::Approximative>(storm::models::sparse::Model<double> const& model,
                                                                                    storm::bisimulation::Partition& partition,
                                                                                    Signatures<double, SignatureMode::Approximative>& signatures);

// storm::RationalNumber
template void performSplitterBasedRefinement<storm::RationalNumber, SplitterRefinementMode::Strong>(
    storm::models::sparse::Model<storm::RationalNumber> const& model, storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
    storm::bisimulation::Partition& partition, storm::RationalNumber const tolerance, storm::OptionalRef<WeakBisimulationData> weakData);
template void performSplitterBasedRefinement<storm::RationalNumber, SplitterRefinementMode::WeakDiscreteTime>(
    storm::models::sparse::Model<storm::RationalNumber> const& model, storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
    storm::bisimulation::Partition& partition, storm::RationalNumber const tolerance, storm::OptionalRef<WeakBisimulationData> weakData);
template void performSplitterBasedRefinement<storm::RationalNumber, SplitterRefinementMode::WeakContinuousTime>(
    storm::models::sparse::Model<storm::RationalNumber> const& model, storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
    storm::bisimulation::Partition& partition, storm::RationalNumber const tolerance, storm::OptionalRef<WeakBisimulationData> weakData);
template void performSignatureBasedRefinement<storm::RationalNumber, SignatureMode::Exact>(storm::models::sparse::Model<storm::RationalNumber> const& model,
                                                                                           storm::bisimulation::Partition& partition,
                                                                                           Signatures<storm::RationalNumber, SignatureMode::Exact>& signatures);
template void performSignatureBasedRefinement<storm::RationalNumber, SignatureMode::Approximative>(
    storm::models::sparse::Model<storm::RationalNumber> const& model, storm::bisimulation::Partition& partition,
    Signatures<storm::RationalNumber, SignatureMode::Approximative>& signatures);

// storm::RationalFunction
template void performSplitterBasedRefinement<storm::RationalFunction, SplitterRefinementMode::Strong>(
    storm::models::sparse::Model<storm::RationalFunction> const& model, storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
    storm::bisimulation::Partition& partition, storm::RationalFunction const tolerance, storm::OptionalRef<WeakBisimulationData> weakData);
template void performSplitterBasedRefinement<storm::RationalFunction, SplitterRefinementMode::WeakDiscreteTime>(
    storm::models::sparse::Model<storm::RationalFunction> const& model, storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
    storm::bisimulation::Partition& partition, storm::RationalFunction const tolerance, storm::OptionalRef<WeakBisimulationData> weakData);
template void performSplitterBasedRefinement<storm::RationalFunction, SplitterRefinementMode::WeakContinuousTime>(
    storm::models::sparse::Model<storm::RationalFunction> const& model, storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
    storm::bisimulation::Partition& partition, storm::RationalFunction const tolerance, storm::OptionalRef<WeakBisimulationData> weakData);
template void performSignatureBasedRefinement<storm::RationalFunction, SignatureMode::Exact>(
    storm::models::sparse::Model<storm::RationalFunction> const& model, storm::bisimulation::Partition& partition,
    Signatures<storm::RationalFunction, SignatureMode::Exact>& signatures);

}  // namespace storm::bisimulation
