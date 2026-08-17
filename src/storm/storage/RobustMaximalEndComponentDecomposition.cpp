#include "storm/storage/RobustMaximalEndComponentDecomposition.h"

#include <numeric>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/StronglyConnectedComponent.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"

namespace storm {
namespace storage {

template<typename ValueType>
RobustMaximalEndComponentDecomposition<ValueType>::RobustMaximalEndComponentDecomposition() : Decomposition() {
    // Intentionally empty.
}

template<typename ValueType>
template<typename RewardModelType>
RobustMaximalEndComponentDecomposition<ValueType>::RobustMaximalEndComponentDecomposition(
    storm::models::sparse::DeterministicModel<ValueType, RewardModelType> const& model) {
    performRobustMaximalEndComponentDecomposition(model.getTransitionMatrix(), model.getBackwardTransitions());
}

template<typename ValueType>
RobustMaximalEndComponentDecomposition<ValueType>::RobustMaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                          storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                          std::vector<ValueType> const& vector) {
    performRobustMaximalEndComponentDecomposition(transitionMatrix, backwardTransitions, vector);
}

template<typename ValueType>
RobustMaximalEndComponentDecomposition<ValueType>::RobustMaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                          storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                          std::vector<ValueType> const& vector,
                                                                                          storm::storage::BitVector const& states) {
    performRobustMaximalEndComponentDecomposition(transitionMatrix, backwardTransitions, vector, states);
}

template<typename ValueType>
RobustMaximalEndComponentDecomposition<ValueType>::RobustMaximalEndComponentDecomposition(RobustMaximalEndComponentDecomposition const& other)
    : Decomposition(other) {
    // Intentionally left empty.
}

template<typename ValueType>
RobustMaximalEndComponentDecomposition<ValueType>& RobustMaximalEndComponentDecomposition<ValueType>::operator=(
    RobustMaximalEndComponentDecomposition const& other) {
    Decomposition::operator=(other);
    return *this;
}

template<typename ValueType>
RobustMaximalEndComponentDecomposition<ValueType>::RobustMaximalEndComponentDecomposition(RobustMaximalEndComponentDecomposition&& other)
    : Decomposition(std::move(other)) {
    // Intentionally left empty.
}

template<typename ValueType>
RobustMaximalEndComponentDecomposition<ValueType>& RobustMaximalEndComponentDecomposition<ValueType>::operator=(
    RobustMaximalEndComponentDecomposition&& other) {
    Decomposition::operator=(std::move(other));
    return *this;
}

template<typename ValueType>
void RobustMaximalEndComponentDecomposition<ValueType>::performRobustMaximalEndComponentDecomposition(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    storm::OptionalRef<std::vector<ValueType> const> vector, storm::OptionalRef<storm::storage::BitVector const> states) {
    // Adapted from Haddad-Monmege Algorithm 3
    storm::storage::BitVector remainingEcCandidates;
    SccDecompositionResult sccDecRes;
    SccDecompositionMemoryCache sccDecCache;
    StronglyConnectedComponentDecompositionOptions sccDecOptions;
    storm::storage::SparseMatrix<ValueType> updatingMatrix(transitionMatrix);
    sccDecOptions.dropNaiveSccs();
    if (states) {
        sccDecOptions.subsystem(*states);
    }

    while (true) {
        performSccDecomposition(updatingMatrix, sccDecOptions, sccDecRes, sccDecCache);

        remainingEcCandidates = sccDecRes.nonTrivialStates;
        storm::storage::BitVector ecSccIndices(sccDecRes.sccCount, true);
        storm::storage::BitVector nonTrivSccIndices(sccDecRes.sccCount, false);

        // find the choices that do not stay in their SCC
        for (auto state : remainingEcCandidates) {
            auto const sccIndex = sccDecRes.stateToSccMapping[state];
            nonTrivSccIndices.set(sccIndex, true);

            // If this probability is >= 1, we are able to stay in the SCC
            // Otherwise not
            double probabilityToStayInScc = 0;
            // If the lower of the vector is > 0, the probability to stay in the SCC stays zero
            if (vector && vector->at(state).lower() == 0) {
                // Check if we can stay in the SCC
                for (auto const& entry : transitionMatrix.getRow(state)) {
                    const bool targetInSCC = sccIndex == sccDecRes.stateToSccMapping[entry.getColumn()];
                    auto const& interval = entry.getValue();
                    if (!utility::isZero(interval.lower()) && !targetInSCC) {
                        // You have to leave the SCC here
                        probabilityToStayInScc = 0;
                        break;
                    } else if (targetInSCC) {
                        probabilityToStayInScc += interval.upper();
                    }
                }
            }

            // if (probabilityToStayInScc < 1 && !utility::isAlmostOne(probabilityToStayInScc)) {
            if (probabilityToStayInScc < 1) {
                // This state is not in an EC
                remainingEcCandidates.set(state, false);
                // This SCC is not an EC
                ecSccIndices.set(sccIndex, false);
            }
        }

        // process the MECs that we've found, i.e. SCCs where every state can stay inside the SCC
        ecSccIndices &= nonTrivSccIndices;

        for (auto sccIndex : ecSccIndices) {
            StronglyConnectedComponent newMec;
            for (auto state : remainingEcCandidates) {
                // skip states from different SCCs
                if (sccDecRes.stateToSccMapping[state] != sccIndex) {
                    continue;
                }
                // This is no longer a candidate
                remainingEcCandidates.set(state, false);
                // but a state in the EC
                newMec.insert(state);
            }
            this->blocks.emplace_back(std::move(newMec));
        }

        // Populate the transitions that stay inside the EC (sort of Haddad-Monmege line 10-11)
        for (auto sccIndex : nonTrivSccIndices) {
            for (uint64_t state = 0; state < transitionMatrix.getRowCount(); state++) {
                // Populate new edges for search that only consider intervals within the EC
                // Tally up lower probability to stay inside of the EC. Once this is >= 1, our EC is done.
                double stayInsideECProb = 0;
                for (auto& entry : updatingMatrix.getRow(state)) {
                    const bool targetInEC = sccIndex == sccDecRes.stateToSccMapping[entry.getColumn()];
                    if (!targetInEC) {
                        entry.setValue(0);
                        continue;
                    }
                    auto const& interval = entry.getValue();

                    // Haddad-Monmege line 11
                    if (interval.upper() > 0 && stayInsideECProb < 1) {
                        stayInsideECProb += interval.upper();
                    } else {
                        entry.setValue(0);
                    }
                }
            }
        }

        if (nonTrivSccIndices == ecSccIndices) {
            // All non trivial SCCs are MECs, nothing left to do!
            break;
        }

        sccDecOptions.subsystem(remainingEcCandidates);
    }

    STORM_LOG_DEBUG("MEC decomposition found " << this->size() << " MEC(s).");
}

template<typename ValueType>
std::vector<uint64_t> RobustMaximalEndComponentDecomposition<ValueType>::computeStateToSccIndexMap(uint64_t numberOfStates) const {
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

// Explicitly instantiate the MEC decomposition.
template class RobustMaximalEndComponentDecomposition<Interval>;
template RobustMaximalEndComponentDecomposition<Interval>::RobustMaximalEndComponentDecomposition(
    storm::models::sparse::DeterministicModel<Interval> const& model);

}  // namespace storage
}  // namespace storm
