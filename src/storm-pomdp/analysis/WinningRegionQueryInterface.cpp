#include "storm-pomdp/analysis/WinningRegionQueryInterface.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace pomdp {
template<typename ValueType>
WinningRegionQueryInterface<ValueType>::WinningRegionQueryInterface(storm::models::sparse::Pomdp<ValueType> const& pomdp, WinningRegion const& winningRegion)
    : pomdp(pomdp), winningRegion(winningRegion) {
    uint64_t nrObservations = pomdp.getNrObservations();
    for (uint64_t observation = 0; observation < nrObservations; ++observation) {
        statesPerObservation.push_back(std::vector<uint64_t>());
    }
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        statesPerObservation[pomdp.getObservation(state)].push_back(state);
    }
}

template<typename ValueType>
bool WinningRegionQueryInterface<ValueType>::isInWinningRegion(storm::storage::BitVector const& beliefSupport) const {
    STORM_LOG_ASSERT(beliefSupport.getNumberOfSetBits() > 0, "One cannot think one is literally nowhere");
    uint64_t observation = pomdp.getObservation(beliefSupport.getNextSetIndex(0));
    // TODO consider optimizations after testing.
    storm::storage::BitVector queryVector(statesPerObservation[observation].size());
    auto stateWithObsIt = statesPerObservation[observation].begin();
    uint64_t offset = 0;
    for (uint64_t possibleState : beliefSupport) {
        STORM_LOG_ASSERT(pomdp.getObservation(possibleState) == observation, "Support must be observation-consistent");
        while (possibleState > *stateWithObsIt) {
            stateWithObsIt++;
            offset++;
        }
        if (possibleState == *stateWithObsIt) {
            queryVector.set(offset);
        }
    }
    return winningRegion.query(observation, queryVector);
}

template<typename ValueType>
bool WinningRegionQueryInterface<ValueType>::staysInWinningRegion(storm::storage::BitVector const& currentBeliefSupport, uint64_t actionIndex) const {
    STORM_LOG_ASSERT(currentBeliefSupport.getNumberOfSetBits() > 0, "One cannot think one is literally nowhere");
    std::map<uint32_t, storm::storage::BitVector> successors;
    STORM_LOG_DEBUG("Stays in winning region? (" << currentBeliefSupport << ", " << actionIndex << ")");
    for (uint64_t oldState : currentBeliefSupport) {
        uint64_t row = pomdp.getTransitionMatrix().getRowGroupIndices()[oldState] + actionIndex;
        for (auto const& successor : pomdp.getTransitionMatrix().getRow(row)) {
            assert(!storm::utility::isZero(successor.getValue()));
            uint32_t obs = pomdp.getObservation(successor.getColumn());
            if (successors.count(obs) == 0) {
                successors[obs] = storm::storage::BitVector(pomdp.getNumberOfStates());
            }
            successors[obs].set(successor.getColumn(), true);
        }
    }

    for (auto const& entry : successors) {
        if (!isInWinningRegion(entry.second)) {
            STORM_LOG_DEBUG("Belief support " << entry.second << " (obs " << entry.first << ") is not winning");
            return false;
        } else {
            STORM_LOG_DEBUG("Belief support " << entry.second << " (obs " << entry.first << ") is winning");
        }
    }
    return true;
}

template<typename ValueType>
void WinningRegionQueryInterface<ValueType>::validate() const {
    for (uint64_t obs = 0; obs < pomdp.getNrObservations(); ++obs) {
        for (auto const& winningBelief : winningRegion.getWinningSetsPerObservation(obs)) {
            storm::storage::BitVector states(pomdp.getNumberOfStates());
            for (uint64_t offset : winningBelief) {
                states.set(statesPerObservation[obs][offset]);
            }
            bool safeActionExists = false;
            for (uint64_t actionIndex = 0; actionIndex < pomdp.getTransitionMatrix().getRowGroupSize(statesPerObservation[obs][0]); ++actionIndex) {
                if (staysInWinningRegion(states, actionIndex)) {
                    safeActionExists = true;
                    break;
                }
            }
            STORM_LOG_THROW(safeActionExists, storm::exceptions::UnexpectedException, "Observation " << obs << " , support " << states << ".");
        }
    }
}

template<typename ValueType>
void WinningRegionQueryInterface<ValueType>::validateIsMaximal(storm::storage::BitVector const& badStates) const {
    for (uint64_t obs = 0; obs < pomdp.getNrObservations(); ++obs) {
        STORM_LOG_DEBUG("Check listed belief supports for observation " << obs << " are maximal");
        for (auto const& winningBelief : winningRegion.getWinningSetsPerObservation(obs)) {
            storm::storage::BitVector remainders = ~winningBelief;
            for (auto const additional : remainders) {
                uint64_t addState = statesPerObservation[obs][additional];
                if (badStates.get(addState)) {
                    continue;
                }

                storm::storage::BitVector states(pomdp.getNumberOfStates());
                for (uint64_t offset : winningBelief) {
                    states.set(statesPerObservation[obs][offset]);
                }
                states.set(statesPerObservation[obs][additional]);
                assert(states.getNumberOfSetBits() == winningBelief.getNumberOfSetBits() + 1);

                bool safeActionExists = false;
                for (uint64_t actionIndex = 0; actionIndex < pomdp.getTransitionMatrix().getRowGroupSize(statesPerObservation[obs][0]); ++actionIndex) {
                    if (staysInWinningRegion(states, actionIndex)) {
                        STORM_LOG_DEBUG("Action " << actionIndex << " from " << states << " is safe. ");
                        safeActionExists = true;
                        break;
                    }
                }

                STORM_LOG_THROW(!safeActionExists, storm::exceptions::UnexpectedException, "Observation " << obs << ", support " << states << ".");
            }
        }
        STORM_LOG_DEBUG("All listed belief supports for observation " << obs << " are maximal. Continue with single states.");

        for (uint64_t offset = 0; offset < statesPerObservation[obs].size(); ++offset) {
            if (winningRegion.isWinning(obs, offset)) {
                continue;
            }
            uint64_t addState = statesPerObservation[obs][offset];
            if (badStates.get(addState)) {
                continue;
            }
            storm::storage::BitVector states(pomdp.getNumberOfStates());
            states.set(addState);
            bool safeActionExists = false;
            for (uint64_t actionIndex = 0; actionIndex < pomdp.getTransitionMatrix().getRowGroupSize(statesPerObservation[obs][0]); ++actionIndex) {
                if (staysInWinningRegion(states, actionIndex)) {
                    safeActionExists = true;
                    break;
                }
            }

            STORM_LOG_THROW(!safeActionExists, storm::exceptions::UnexpectedException, "Observation " << obs << "  , support " << states << ".");
        }
    }
}

template class WinningRegionQueryInterface<double>;
template class WinningRegionQueryInterface<storm::RationalNumber>;
}  // namespace pomdp

}  // namespace storm
