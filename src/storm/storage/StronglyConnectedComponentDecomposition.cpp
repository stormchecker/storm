#include "storm/storage/StronglyConnectedComponentDecomposition.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::storage {

StronglyConnectedComponentDecompositionOptions& StronglyConnectedComponentDecompositionOptions::subsystem(storm::storage::BitVector const& subsystem) {
    optSubsystem.reset(subsystem);
    return *this;
}

StronglyConnectedComponentDecompositionOptions& StronglyConnectedComponentDecompositionOptions::choices(storm::storage::BitVector const& choices) {
    optChoices.reset(choices);
    return *this;
}

StronglyConnectedComponentDecompositionOptions& StronglyConnectedComponentDecompositionOptions::dropNaiveSccs(bool value) {
    areNaiveSccsDropped = value;
    return *this;
}

StronglyConnectedComponentDecompositionOptions& StronglyConnectedComponentDecompositionOptions::onlyBottomSccs(bool value) {
    areOnlyBottomSccsConsidered = value;
    return *this;
}

StronglyConnectedComponentDecompositionOptions& StronglyConnectedComponentDecompositionOptions::forceTopologicalSort(bool value) {
    isTopologicalSortForced = value;
    return *this;
}

StronglyConnectedComponentDecompositionOptions& StronglyConnectedComponentDecompositionOptions::computeSccDepths(bool value) {
    isComputeSccDepthsSet = value;
    return *this;
}

void SccDecompositionMemoryCache::initialize(uint64_t numStates) {
    preorderNumbers.assign(numStates, std::numeric_limits<uint64_t>::max());
    recursionStateStack.clear();
    s.clear();
    p.clear();
}

bool SccDecompositionMemoryCache::hasPreorderNumber(uint64_t stateIndex) const {
    return preorderNumbers[stateIndex] != std::numeric_limits<uint64_t>::max();
}

void SccDecompositionResult::initialize(uint64_t numStates, bool computeSccDepths) {
    sccCount = 0;
    stateToSccMapping.assign(numStates, std::numeric_limits<uint64_t>::max());
    nonTrivialStates.clear();
    nonTrivialStates.resize(numStates, false);
    if (computeSccDepths) {
        if (sccDepths) {
            sccDepths->clear();
        } else {
            sccDepths.emplace();
        }
    } else {
        sccDepths = std::nullopt;
    }
}

bool SccDecompositionResult::stateHasScc(uint64_t stateIndex) const {
    return stateToSccMapping[stateIndex] != std::numeric_limits<uint64_t>::max();
}

template<typename ValueType>
StronglyConnectedComponentDecomposition<ValueType>::StronglyConnectedComponentDecomposition() : Decomposition() {
    // Intentionally left empty.
}

template<typename ValueType>
StronglyConnectedComponentDecomposition<ValueType>::StronglyConnectedComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                            StronglyConnectedComponentDecompositionOptions const& options) {
    performSccDecomposition(transitionMatrix, options);
}

template<typename ValueType>
StronglyConnectedComponentDecomposition<ValueType>::StronglyConnectedComponentDecomposition(StronglyConnectedComponentDecomposition const& other)
    : Decomposition(other) {
    // Intentionally left empty.
}

template<typename ValueType>
StronglyConnectedComponentDecomposition<ValueType>& StronglyConnectedComponentDecomposition<ValueType>::operator=(
    StronglyConnectedComponentDecomposition const& other) {
    this->blocks = other.blocks;
    return *this;
}

template<typename ValueType>
StronglyConnectedComponentDecomposition<ValueType>::StronglyConnectedComponentDecomposition(StronglyConnectedComponentDecomposition&& other)
    : Decomposition(std::move(other)) {
    // Intentionally left empty.
}

template<typename ValueType>
StronglyConnectedComponentDecomposition<ValueType>& StronglyConnectedComponentDecomposition<ValueType>::operator=(
    StronglyConnectedComponentDecomposition&& other) {
    this->blocks = std::move(other.blocks);
    return *this;
}

/*!
 * Uses the algorithm by Gabow/Cheriyan/Mehlhorn ("Path-based strongly connected component algorithm") to
 * compute a mapping of states to their SCCs. All arguments given by (non-const) reference are modified by
 * the function as a side-effect.
 *
 * @param transitionMatrix The transition matrix of the system to decompose.
 * @param startState The starting state for the search of Tarjan's algorithm.
 * @param nonTrivialStates A bit vector where entries for non-trivial states (states that either have a selfloop or whose SCC is not a singleton) will be set to
 * true
 * @param subsystem An optional bit vector indicating which subsystem to consider.
 * @param choices An optional bit vector indicating which choices belong to the subsystem.
 * @param currentIndex The next free index that can be assigned to states.
 * @param hasPreorderNumber A bit that is used to keep track of the states that already have a preorder number.
 * @param preorderNumbers A vector storing the preorder number for each state.
 * @param recursionStateStack memory used for the recursion stack.
 * @param s The stack S used by the algorithm.
 * @param p The stack S used by the algorithm.
 * @param stateHasScc A bit vector containing all states that have already been assigned to an SCC.
 * @param stateToSccMapping A mapping from states to the SCC indices they belong to. As a side effect of this
 * function this mapping is filled (for all states reachable from the starting state).
 * @param sccCount The number of SCCs that have been computed. As a side effect of this function, this count
 * is increased.
 */
template<typename ValueType>
void performSccDecompositionGCM(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::OptionalRef<storm::storage::BitVector const> subsystem,
                                storm::OptionalRef<storm::storage::BitVector const> choices, bool /*forceTopologicalSort*/, uint64_t startState,
                                uint64_t& currentIndex, SccDecompositionResult& result, SccDecompositionMemoryCache& cache) {
    // The forceTopologicalSort flag can be ignored as this method always generates a topological sort.

    // Prepare the stack used for turning the recursive procedure into an iterative one.
    STORM_LOG_ASSERT(cache.recursionStateStack.empty(), "Expected an empty recursion stack.");
    cache.recursionStateStack.push_back(startState);

    while (!cache.recursionStateStack.empty()) {
        // Peek at the topmost state in the stack, but leave it on there for now.
        uint64_t currentState = cache.recursionStateStack.back();
        assert(!subsystem || subsystem->get(currentState));

        // If the state has not yet been seen, we need to assign it a preorder number and iterate over its successors.
        if (!cache.hasPreorderNumber(currentState)) {
            cache.preorderNumbers[currentState] = currentIndex++;

            cache.s.push_back(currentState);
            cache.p.push_back(currentState);

            for (uint64_t row = transitionMatrix.getRowGroupIndices()[currentState], rowEnd = transitionMatrix.getRowGroupIndices()[currentState + 1];
                 row != rowEnd; ++row) {
                if (choices && !choices->get(row)) {
                    continue;
                }

                for (auto const& successor : transitionMatrix.getRow(row)) {
                    if ((!subsystem || subsystem->get(successor.getColumn())) && successor.getValue() != storm::utility::zero<ValueType>()) {
                        if (currentState == successor.getColumn()) {
                            result.nonTrivialStates.set(currentState, true);
                        }

                        if (!cache.hasPreorderNumber(successor.getColumn())) {
                            // In this case, we must recursively visit the successor. We therefore push the state
                            // onto the recursion stack.
                            cache.recursionStateStack.push_back(successor.getColumn());
                        } else {
                            if (!result.stateHasScc(successor.getColumn())) {
                                while (cache.preorderNumbers[cache.p.back()] > cache.preorderNumbers[successor.getColumn()]) {
                                    cache.p.pop_back();
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // In this case, we have searched all successors of the current state and can exit the "recursion"
            // on the current state.
            if (currentState == cache.p.back()) {
                cache.p.pop_back();
                if (result.sccDepths) {
                    uint64_t sccDepth = 0;
                    // Find the largest depth over successor SCCs.
                    auto stateIt = cache.s.end();
                    do {
                        --stateIt;
                        for (uint64_t row = transitionMatrix.getRowGroupIndices()[*stateIt], rowEnd = transitionMatrix.getRowGroupIndices()[*stateIt + 1];
                             row != rowEnd; ++row) {
                            if (choices && !choices->get(row)) {
                                continue;
                            }
                            for (auto const& successor : transitionMatrix.getRow(row)) {
                                if ((!subsystem || subsystem->get(successor.getColumn())) && successor.getValue() != storm::utility::zero<ValueType>() &&
                                    result.stateHasScc(successor.getColumn())) {
                                    sccDepth = std::max(sccDepth, (*result.sccDepths)[result.stateToSccMapping[successor.getColumn()]] + 1);
                                }
                            }
                        }
                    } while (*stateIt != currentState);
                    result.sccDepths->push_back(sccDepth);
                }
                bool nonSingletonScc = cache.s.back() != currentState;
                uint64_t poppedState = 0;
                do {
                    poppedState = cache.s.back();
                    cache.s.pop_back();
                    result.stateToSccMapping[poppedState] = result.sccCount;
                    if (nonSingletonScc) {
                        result.nonTrivialStates.set(poppedState, true);
                    }
                } while (poppedState != currentState);
                ++result.sccCount;
            }

            cache.recursionStateStack.pop_back();
        }
    }
}

template<typename ValueType>
void StronglyConnectedComponentDecomposition<ValueType>::performSccDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                 StronglyConnectedComponentDecompositionOptions const& options) {
    SccDecompositionResult result;
    storm::storage::performSccDecomposition(transitionMatrix, options, result);

    STORM_LOG_ASSERT(!options.areOnlyBottomSccsConsidered || result.sccDepths.has_value(), "Scc depths not computed but needed.");
    if (result.sccDepths) {
        this->sccDepths = std::move(*result.sccDepths);
    }

    // After we obtained the state-to-SCC mapping, we build the actual blocks.
    this->blocks.resize(result.sccCount);
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        // Check if this state (and is SCC) should be considered in this decomposition.
        if ((!options.optSubsystem || options.optSubsystem->get(state))) {
            if (!options.areNaiveSccsDropped || result.nonTrivialStates.get(state)) {
                uint64_t sccIndex = result.stateToSccMapping[state];
                if (!options.areOnlyBottomSccsConsidered || sccDepths.value()[sccIndex] == 0) {
                    this->blocks[sccIndex].insert(state);
                    if (!result.nonTrivialStates.get(state)) {
                        this->blocks[sccIndex].setIsTrivial(true);
                        STORM_LOG_ASSERT(this->blocks[sccIndex].size() == 1, "Unexpected number of states in a trivial SCC.");
                    }
                }
            }
        }
    }

    // If requested, we need to delete all naive SCCs.
    if (options.areOnlyBottomSccsConsidered || options.areNaiveSccsDropped) {
        storm::storage::BitVector blocksToDrop(result.sccCount);
        for (uint64_t sccIndex = 0; sccIndex < result.sccCount; ++sccIndex) {
            if (this->blocks[sccIndex].empty()) {
                blocksToDrop.set(sccIndex, true);
            }
        }
        // Create the new set of blocks by moving all the blocks we need to keep into it.
        storm::utility::vector::filterVectorInPlace(this->blocks, ~blocksToDrop);
        if (this->sccDepths) {
            storm::utility::vector::filterVectorInPlace(this->sccDepths.value(), ~blocksToDrop);
        }
    }
}

template<typename ValueType>
void performSccDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, StronglyConnectedComponentDecompositionOptions const& options,
                             SccDecompositionResult& result) {
    SccDecompositionMemoryCache cache;
    performSccDecomposition(transitionMatrix, options, result, cache);
}

template<typename ValueType>
void performSccDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, StronglyConnectedComponentDecompositionOptions const& options,
                             SccDecompositionResult& result, SccDecompositionMemoryCache& cache) {
    STORM_LOG_ASSERT(!options.optChoices || options.optSubsystem, "Expecting subsystem if choices are given.");

    uint64_t numberOfStates = transitionMatrix.getRowGroupCount();
    result.initialize(numberOfStates, options.isComputeSccDepthsSet || options.areOnlyBottomSccsConsidered);
    cache.initialize(numberOfStates);

    // Start the search for SCCs from every state in the block.
    uint64_t currentIndex = 0;
    auto performSccDecompFromState = [&](uint64_t startState) {
        if (!cache.hasPreorderNumber(startState)) {
            performSccDecompositionGCM(transitionMatrix, options.optSubsystem, options.optChoices, options.isTopologicalSortForced, startState, currentIndex,
                                       result, cache);
        }
    };

    if (options.optSubsystem) {
        for (auto state : *options.optSubsystem) {
            performSccDecompFromState(state);
        }
    } else {
        for (uint64_t state = 0; state < numberOfStates; ++state) {
            performSccDecompFromState(state);
        }
    }
}

template<typename ValueType>
bool StronglyConnectedComponentDecomposition<ValueType>::hasSccDepth() const {
    return sccDepths.has_value();
}

template<typename ValueType>
uint64_t StronglyConnectedComponentDecomposition<ValueType>::getSccDepth(uint64_t const& sccIndex) const {
    STORM_LOG_THROW(sccDepths.has_value(), storm::exceptions::InvalidOperationException,
                    "Tried to get the SCC depth but SCC depths were not computed upon construction.");
    STORM_LOG_ASSERT(sccIndex < sccDepths->size(), "SCC index " << sccIndex << " is out of range (" << sccDepths->size() << ")");
    return sccDepths.value()[sccIndex];
}

template<typename ValueType>
uint64_t StronglyConnectedComponentDecomposition<ValueType>::getMaxSccDepth() const {
    STORM_LOG_THROW(sccDepths.has_value(), storm::exceptions::InvalidOperationException,
                    "Tried to get the maximum SCC depth but SCC depths were not computed upon construction.");
    STORM_LOG_THROW(!sccDepths->empty(), storm::exceptions::InvalidOperationException,
                    "Tried to get the maximum SCC depth but this SCC decomposition seems to be empty.");
    return *std::max_element(sccDepths->begin(), sccDepths->end());
}

template<typename ValueType>
std::vector<uint64_t> StronglyConnectedComponentDecomposition<ValueType>::computeStateToSccIndexMap(uint64_t numberOfStates) const {
    std::vector<uint64_t> result(numberOfStates, std::numeric_limits<uint64_t>::max());
    uint64_t sccIndex = 0;
    for (auto const& scc : *this) {
        for (auto const& state : scc) {
            result[state] = sccIndex;
        }
        ++sccIndex;
    }
    return result;
}

template class StronglyConnectedComponentDecomposition<double>;
template class StronglyConnectedComponentDecomposition<storm::RationalNumber>;
template class StronglyConnectedComponentDecomposition<storm::RationalFunction>;
template class StronglyConnectedComponentDecomposition<storm::Interval>;
template class StronglyConnectedComponentDecomposition<storm::RationalInterval>;

}  // namespace storm::storage
