#include "storm/transformer/bisimulation/Refinement.h"

#include <algorithm>
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
    bool isStepSensitive(uint64_t const state) const requires isWeak {
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
        explicit DefaultCache(Partition const& partition)
            : predecessorToSplitterProbabilities(partition.getNumberOfElements()), predecessorBlocks(partition) {}
        StateMapping<ValueType> predecessorToSplitterProbabilities;
        Partition::NonSuperBlockSet predecessorBlocks;
    };

    /// The additional scratch space that refining a block with respect to weak bisimulation on a discrete-time model needs, cf. refineBlockWeak.
    struct WeakDiscreteTimeCache : public DefaultCache {
        explicit WeakDiscreteTimeCache(Partition const& partition)
            : DefaultCache(partition),
              weakLabels(partition.getNumberOfElements()),
              conditionalValues(partition.getNumberOfElements(), storm::utility::zero<ValueType>()) {}
        StateMapping<std::set<uint64_t>> weakLabels;  // for each state, the set of conditional-probability classes it can reach silently
        // for each state of the block being refined, its probability of moving to the splitter conditioned on leaving the own block. Only meaningful for the
        // non-silent states of that block.
        std::vector<ValueType> conditionalValues;
        std::vector<uint64_t> nonSilentStates;    // the non-silent states of the block currently being refined, sorted by their conditional probability
        std::vector<uint64_t> classStarts;        // indices into nonSilentStates at which a new conditional-probability class starts (plus a sentinel)
        std::vector<uint64_t> stateStack;  // work list of the backward search that computes weakLabels
    };

    /// Picks the scratch space that this mode actually uses, so that accessing the wrong one is a compile error.
    using Cache = std::conditional_t<isWeakDiscreteTime, WeakDiscreteTimeCache, DefaultCache>;
    Cache cache;
};

/*!
 * Updates the silent states after the given block has been split into sub-blocks.
 *
 * A state can only ever lose its silence, and it does so exactly if one of its successors ended up in a different sub-block than itself. Rather than
 * re-examining the transitions of every state of the block, we therefore only traverse the sub-blocks other than the largest one: a state of such a
 * sub-block is checked directly (forward), and a state of the largest sub-block is caught through the backward transitions of the states of the smaller
 * ones.
 */
template<typename ValueType, SplitterRefinementMode Mode>
void updateSilentStates(SplitterRefinementContext<ValueType, Mode>& context, storm::bisimulation::Partition::Block const& splitBlock) requires (Mode == SplitterRefinementMode::WeakDiscreteTime) {
    auto& silentStates = context.weakData->silentStates;

    Partition::Block largestSubBlock;  // default-constructed, i.e. empty, so that the first sub-block always wins
    context.partition.forEachSubBlock(splitBlock, [&largestSubBlock](auto const& subBlock) {
        if (subBlock.size() > largestSubBlock.size()) {
            largestSubBlock = subBlock;
        }
    });

    context.partition.forEachSubBlock(splitBlock, [&context, &silentStates, &largestSubBlock](auto const& subBlock) {
        if (context.partition.isEqualBlock(subBlock, largestSubBlock)) {
            return;
        }
        for (uint64_t const state : subBlock) {
            // Forward: a state of this sub-block that is still silent loses its silence if any of its successors now lies outside of the sub-block.
            if (silentStates.get(state)) {
                auto const row = context.model.getTransitionMatrix().getRow(state);
                silentStates.set(state, std::all_of(row.begin(), row.end(), [&context, &state](auto const& entry) {
                                     return storm::utility::isZero(entry.getValue()) || context.partition.isSameBlock(state, entry.getColumn());
                                 }));
            }
            // Backward: a still silent predecessor that ended up in a different sub-block has a successor outside of its own block and thus is not silent.
            for (auto const& predecessorEntry : context.backwardTransitions.getRow(state)) {
                uint64_t const predecessor = predecessorEntry.getColumn();
                if (silentStates.get(predecessor) && !context.partition.isSameBlock(state, predecessor)) {
                    silentStates.set(predecessor, false);
                }
            }
        }
    });
}

/*!
 * Recomputes the silent states of every block of the partition from scratch.
 */
template<typename ValueType, SplitterRefinementMode Mode>
void recomputeSilentStates(SplitterRefinementContext<ValueType, Mode>& context) requires (Mode != SplitterRefinementMode::Strong) {
    auto& silentStates = context.weakData->silentStates;

    // A state can only ever go from silent to non-silent.
    for (uint64_t const state : silentStates) {
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
bool checkSilentStates(SplitterRefinementContext<ValueType, Mode> const& context) requires (Mode != SplitterRefinementMode::Strong) {
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
            : context.partition.splitBlockByPredicate(predecessorBlockToSplit, [&predecessorToSplitterProbabilities](auto const& state) {
                  return !storm::utility::isZero(predecessorToSplitterProbabilities.getValues()[state]);
              });

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
        if constexpr (Mode == SplitterRefinementMode::WeakDiscreteTime) {
            // In the weak mode, we also refresh the silent states.
            // Only the discrete-time mode reads the silent states during the refinement (cf. refineBlockWeak);
            // The continuous-time mode determines them once at the end.
            updateSilentStates(context, predecessorBlockToSplit);
        }
    }
}

/*!
 * Refines the given predecessor block of the current splitter with respect to weak bisimulation on a discrete-time model.
 *
 * The observable behavior of a non-silent state is its distribution over the *other* blocks, conditioned on leaving its own block. We therefore group the
 * non-silent states of the block by their conditional probability of moving to the splitter. A silent state, in turn, behaves like a convex combination of
 * the non-silent states it can reach without leaving the block, so we label every state with the set of groups it can reach that way and split by that
 * label. This is sound because every state that is reachable from a silent state without leaving the block is weakly bisimilar to it (a state that is
 * silent with respect to the block is in particular silent with respect to its - possibly much smaller - weak bisimulation class, so all its successors are
 * weakly bisimilar to it), and it is complete because, once no block can be split any further, the non-silent states of a block agree on their conditional
 * distributions, which is exactly the definition of weak bisimulation.
 */
template<typename ValueType>
void refineBlockWeak(SplitterRefinementContext<ValueType, SplitterRefinementMode::WeakDiscreteTime>& context, storm::bisimulation::Partition::Block const block,
                     storm::bisimulation::Partition::Block const splitterBlock) {
    auto const& silentStates = context.weakData->silentStates;
    auto const& toSplitterProbs = context.cache.predecessorToSplitterProbabilities.getValues();
    auto& conditionalValues = context.cache.conditionalValues;
    auto& nonSilentStates = context.cache.nonSilentStates;
    auto& classStarts = context.cache.classStarts;
    auto& weakLabels = context.cache.weakLabels;

    // Step 1: collect the non-silent states and compute their probability of moving to the splitter, conditioned on leaving the own block.
    //
    // The two extremal values are determined on the transition structure rather than on the computed value, because getting them exactly right is what decides
    // whether weakly bisimilar states stay together: the conditional probability is zero iff the state has no transition into the splitter at all, and it is
    // one iff every transition that leaves the block enters the splitter. A floating point division of two accumulated sums generally misses those exact
    // values by a few ulps (in particular when the two sums are accumulated in a different order), which would split states that are weakly bisimilar.
    nonSilentStates.clear();
    for (uint64_t const state : block) {
        if (silentStates.get(state)) {
            continue;  // A silent state cannot leave its block, so it has no conditional distribution of its own.
        }
        nonSilentStates.push_back(state);
        if (storm::utility::isZero(toSplitterProbs[state])) {
            conditionalValues[state] = storm::utility::zero<ValueType>();  // the state has no transition into the splitter
            continue;
        }
        ValueType escapeValue = storm::utility::zero<ValueType>();
        ValueType toSplitterValue = storm::utility::zero<ValueType>();
        bool leavesOnlyToSplitter = true;
        for (auto const& entry : context.model.getTransitionMatrix().getRow(state)) {
            if (storm::utility::isZero(entry.getValue())) {
                continue;
            }
            if (context.partition.isSameBlock(state, entry.getColumn())) {
                continue;  // moves within the own block are unobservable
            }
            escapeValue += entry.getValue();
            // Note that the splitter may itself have been split since this round started (which happens if it is step sensitive and its own predecessor), so
            // we ask whether the successor is among the states the splitter had back then rather than compare the blocks. Those are exactly the states
            // that the probabilities gathered by the predecessor scan refer to.
            if (context.partition.contains(entry.getColumn(), splitterBlock)) {
                toSplitterValue += entry.getValue();
            } else {
                leavesOnlyToSplitter = false;
            }
        }
        STORM_LOG_ASSERT(!storm::utility::isZero(escapeValue), "A non-silent state must be able to leave its block.");
        // We only get here for states that the predecessor scan found to have a transition into the splitter, so the scan above has to find one as well.
        // This would break if the membership test did not account for the splitter having been split in the meantime.
        STORM_LOG_ASSERT(!storm::utility::isZero(toSplitterValue), "The transitions into the splitter disagree with the predecessor scan.");
        conditionalValues[state] = leavesOnlyToSplitter ? storm::utility::one<ValueType>() : toSplitterValue / escapeValue;
    }
    // Every state of a non-divergent block can leave the block; the first state that is non-silent on such a path witnesses this.
    STORM_LOG_ASSERT(!nonSilentStates.empty(), "A non-divergent block must contain a non-silent state.");

    // Step 2: group the non-silent states into classes of (approximately) equal conditional probability. We do not split the partition here: the actual
    // split has to consider the silent states as well, and the new Partition does not allow undoing a split.
    std::sort(nonSilentStates.begin(), nonSilentStates.end(),
              [&conditionalValues](uint64_t const state1, uint64_t const state2) { return conditionalValues[state1] < conditionalValues[state2]; });
    auto const startsNewClass = [&conditionalValues, &context](uint64_t const classStart, uint64_t const state) {
        if (storm::utility::isZero(context.tolerance)) {
            return conditionalValues[classStart] < conditionalValues[state];
        }
        return conditionalValues[classStart] + context.tolerance < conditionalValues[state];
    };
    classStarts.assign(1, 0u);
    weakLabels.addValue(nonSilentStates.front(), 0u);
    for (uint64_t i = 1; i < nonSilentStates.size(); ++i) {
        if (startsNewClass(nonSilentStates[classStarts.back()], nonSilentStates[i])) {
            classStarts.push_back(i);
        }
        weakLabels.addValue(nonSilentStates[i], classStarts.size() - 1);
    }

    // If all non-silent states agree, the block is already stable with respect to this splitter: as argued above, every silent state reaches a non-silent
    // one, so it would end up with the very same (singleton) label.
    if (classStarts.size() == 1) {
        weakLabels.clear();
        return;
    }
    classStarts.push_back(nonSilentStates.size());  // sentinel

    // Step 3: label every silent state with the set of classes it can reach without leaving the block. We search backwards from the states of each class,
    // only traversing silent states: a non-silent state is already fully described by its own class.
    auto& stateStack = context.cache.stateStack;
    for (uint64_t classIndex = 0; classIndex + 1 < classStarts.size(); ++classIndex) {
        stateStack.assign(nonSilentStates.begin() + classStarts[classIndex], nonSilentStates.begin() + classStarts[classIndex + 1]);
        while (!stateStack.empty()) {
            uint64_t const currentState = stateStack.back();
            stateStack.pop_back();
            for (auto const& predecessorEntry : context.backwardTransitions.getRow(currentState)) {
                uint64_t const predecessor = predecessorEntry.getColumn();
                if (silentStates.get(predecessor) && context.partition.isBlockOfElement(block, predecessor) &&
                    !weakLabels.getValues()[predecessor].contains(classIndex)) {
                    weakLabels.addValue(predecessor, classIndex);
                    stateStack.push_back(predecessor);
                }
            }
        }
    }

    // Step 4: perform the actual split.
    auto const& labels = weakLabels.getValues();
    STORM_LOG_ASSERT(std::all_of(block.begin(), block.end(), [&labels](uint64_t const state) { return !labels[state].empty(); }),
                     "Every state of a non-divergent block must be able to reach a non-silent state without leaving the block.");
    bool const wasSplit = context.partition.splitBlockByOrder(
        block, [&labels](uint64_t const state1, uint64_t const state2) { return labels[state1] < labels[state2]; });
    weakLabels.clear();

    if (wasSplit) {
        context.queue.erase(block);
        // Note that we have to enqueue all sub-blocks (rather than all but the largest): the conditional probabilities of the states of a sub-block change
        // when their block shrinks, and it is precisely the stability with respect to the sibling blocks that re-establishes stability with respect to the
        // earlier splitters.
        context.partition.forEachSubBlock(block, [&context](auto const& subBlock) { context.queue.insert(subBlock); });
        updateSilentStates(context, block);
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
void performSplitterBasedRefinement(storm::models::sparse::Model<ValueType> const& model,
                                    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::bisimulation::Partition& partition,
                                    ValueType const tolerance, storm::OptionalRef<WeakBisimulationData> weakData) {
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

    if constexpr (Mode == SplitterRefinementMode::WeakContinuousTime) {
        detail::recomputeSilentStates(context);
    }
    if constexpr (Mode != SplitterRefinementMode::Strong) {
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
