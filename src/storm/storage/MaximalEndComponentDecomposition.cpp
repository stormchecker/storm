#include "storm/storage/MaximalEndComponentDecomposition.h"

#include <algorithm>
#include <span>
#include <sstream>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/graph.h"

namespace storm {
namespace storage {

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition() : Decomposition() {
    // Intentionally left empty.
}

template<typename ValueType>
template<typename RewardModelType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition(
    storm::models::sparse::NondeterministicModel<ValueType, RewardModelType> const& model) {
    performMaximalEndComponentDecomposition(model.getTransitionMatrix(), model.getBackwardTransitions());
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                              storm::storage::SparseMatrix<ValueType> const& backwardTransitions) {
    performMaximalEndComponentDecomposition(transitionMatrix, backwardTransitions);
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                              storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                              storm::storage::BitVector const& states) {
    performMaximalEndComponentDecomposition(transitionMatrix, backwardTransitions, states);
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                              storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                              storm::storage::BitVector const& states,
                                                                              storm::storage::BitVector const& choices) {
    performMaximalEndComponentDecomposition(transitionMatrix, backwardTransitions, states, choices);
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition(storm::models::sparse::NondeterministicModel<ValueType> const& model,
                                                                              storm::storage::BitVector const& states) {
    performMaximalEndComponentDecomposition(model.getTransitionMatrix(), model.getBackwardTransitions(), states);
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition(MaximalEndComponentDecomposition const& other) : Decomposition(other) {
    // Intentionally left empty.
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>& MaximalEndComponentDecomposition<ValueType>::operator=(MaximalEndComponentDecomposition const& other) {
    Decomposition::operator=(other);
    return *this;
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>::MaximalEndComponentDecomposition(MaximalEndComponentDecomposition&& other) : Decomposition(std::move(other)) {
    // Intentionally left empty.
}

template<typename ValueType>
MaximalEndComponentDecomposition<ValueType>& MaximalEndComponentDecomposition<ValueType>::operator=(MaximalEndComponentDecomposition&& other) {
    Decomposition::operator=(std::move(other));
    return *this;
}

template<typename ValueType>
std::string MaximalEndComponentDecomposition<ValueType>::statistics(uint64_t totalNumberOfStates) const {
    if (this->empty()) {
        return "Empty MEC decomposition.";
    }
    uint64_t statesInMec = 0;
    uint64_t trivialMecs = 0;
    uint64_t smallestSize = std::numeric_limits<uint64_t>::max();
    uint64_t largestSize = 0;
    for (auto const& mec : this->blocks) {
        statesInMec += mec.size();
        if (mec.size() == 1u) {
            ++trivialMecs;
        } else {
            smallestSize = std::min<uint64_t>(smallestSize, mec.size());
            largestSize = std::max<uint64_t>(largestSize, mec.size());
        }
    }
    uint64_t const statesInNonTrivialMec = statesInMec - trivialMecs;
    auto getPercentage = [&totalNumberOfStates](uint64_t states) -> double {
        return (totalNumberOfStates == 0) ? 0.0 : (100.0 * states / totalNumberOfStates);
    };
    std::stringstream ss;
    ss << "MEC decomposition statistics: ";
    ss << "There are " << this->size() << " MECs out of which " << trivialMecs << " are trivial, i.e., consist of a single state.";
    ss << " " << statesInMec << " out of " << totalNumberOfStates << " states (" << getPercentage(statesInMec) << "%) are on some MEC. "
       << statesInNonTrivialMec << " states (" << getPercentage(statesInNonTrivialMec) << "%) are on a non-trivial mec. ";
    if (largestSize > 0) {
        ss << "The smallest non-trivial MEC has " << smallestSize << " states and the largest non-trivial MEC has " << largestSize << " states.";
    }
    return ss.str();
}

/*!
 * Compute a mapping from SCC index to the set of states in that SCC.
 * @param sccDecRes The result of the SCC decomposition.
 * @param sccStates flattened vector that contains all states of the different SCCs
 * @param sccIndications vector that contains the index of the first state of each SCC in the sccStates vector plus one last entry pointing to the end of the
 * sccStates vector That means that SCC i has its states in the range [sccIndications[i], sccIndications[i+1])
 */
void getFlatSccDecomposition(SccDecompositionResult const& sccDecRes, std::vector<uint64_t>& sccStates, std::vector<uint64_t>& sccIndications) {
    // initialize result vectors with correct size
    sccIndications.assign(sccDecRes.sccCount + 1, 0ull);
    sccStates.resize(sccDecRes.nonTrivialStates.getNumberOfSetBits());

    // count the number of states in each SCC. For efficiency, we re-use storage from sccIndications but make sure that sccIndications[0]==0 remains true
    std::span<uint64_t> sccCounts(sccIndications.begin() + 1, sccIndications.end());
    for (auto state : sccDecRes.nonTrivialStates) {
        ++sccCounts[sccDecRes.stateToSccMapping[state]];
    }

    // Now establish that sccCounts[i] points to the first entry in sccStates for SCC i
    // This means that sccCounts[i] is the sum of all scc sizes for all SCCs j < i
    uint64_t sum = 0;
    for (auto& count : sccCounts) {
        auto const oldSum = sum;
        sum += count;
        count = oldSum;
    }

    // Now fill the sccStates vector
    for (auto state : sccDecRes.nonTrivialStates) {
        auto const sccIndex = sccDecRes.stateToSccMapping[state];
        sccStates[sccCounts[sccIndex]] = state;
        ++sccCounts[sccIndex];
    }

    // At this point, sccCounts[i] points to the first entry in sccStates for SCC i+1
    // Since sccCounts[i] = sccIndications[i+1], we are done already
}

template<typename ValueType>
void MaximalEndComponentDecomposition<ValueType>::performMaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                          storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                          storm::OptionalRef<storm::storage::BitVector const> states,
                                                                                          storm::OptionalRef<storm::storage::BitVector const> choices) {
    STORM_LOG_ASSERT(!states.has_value() || transitionMatrix.getRowGroupCount() == states->size(), "Unexpected size of states bitvector.");
    STORM_LOG_ASSERT(!choices.has_value() || transitionMatrix.getRowCount() == choices->size(), "Unexpected size of choices bitvector.");
    // Get some data for convenient access.
    auto const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();

    storm::storage::BitVector remainingEcCandidates, ecChoices;
    SccDecompositionResult sccDecRes;
    SccDecompositionMemoryCache sccDecCache;
    StronglyConnectedComponentDecompositionOptions sccDecOptions;
    sccDecOptions.dropNaiveSccs();
    if (states) {
        sccDecOptions.subsystem(*states);
    }
    if (choices) {
        ecChoices = *choices;
        sccDecOptions.choices(ecChoices);
    } else {
        ecChoices.resize(transitionMatrix.getRowCount(), true);
    }

    // Reserve storage for the mapping of SCC indices to
    std::vector<uint64_t> ecSccStates, ecSccIndications;
    auto getSccStates = [&ecSccStates, &ecSccIndications](uint64_t const sccIndex) {
        return std::span(ecSccStates).subspan(ecSccIndications[sccIndex], ecSccIndications[sccIndex + 1] - ecSccIndications[sccIndex]);
    };
    while (true) {
        performSccDecomposition(transitionMatrix, sccDecOptions, sccDecRes, sccDecCache);
        getFlatSccDecomposition(sccDecRes, ecSccStates, ecSccIndications);

        remainingEcCandidates = sccDecRes.nonTrivialStates;
        storm::storage::BitVector ecSccIndices(sccDecRes.sccCount, true);
        storm::storage::BitVector nonTrivSccIndices(sccDecRes.sccCount, false);
        // find the choices that do not stay in their SCC
        for (auto state : remainingEcCandidates) {
            auto const sccIndex = sccDecRes.stateToSccMapping[state];
            nonTrivSccIndices.set(sccIndex, true);
            bool stateCanStayInScc = false;
            for (auto const choice : transitionMatrix.getRowGroupIndices(state)) {
                if (!ecChoices.get(choice)) {
                    continue;
                }
                auto row = transitionMatrix.getRow(choice);
                if (std::any_of(row.begin(), row.end(), [&sccIndex, &sccDecRes](auto const& entry) {
                        return sccIndex != sccDecRes.stateToSccMapping[entry.getColumn()] && !storm::utility::isZero(entry.getValue());
                    })) {
                    ecChoices.set(choice, false);       // The choice leaves the SCC
                    ecSccIndices.set(sccIndex, false);  // This SCC is not 'stable' yet
                } else {
                    stateCanStayInScc = true;  // The choice stays in the SCC
                }
            }
            if (!stateCanStayInScc) {
                remainingEcCandidates.set(state, false);  // This state is not in an EC
            }
        }

        // process the MECs that we've found, i.e. SCCs where every state can stay inside the SCC
        ecSccIndices &= nonTrivSccIndices;
        for (auto sccIndex : ecSccIndices) {
            MaximalEndComponent newMec;
            for (auto const state : getSccStates(sccIndex)) {
                // This is no longer a candidate
                remainingEcCandidates.set(state, false);
                // Add choices to the MEC
                MaximalEndComponent::set_type containedChoices;
                for (auto ecChoiceIt = ecChoices.begin(nondeterministicChoiceIndices[state]); *ecChoiceIt < nondeterministicChoiceIndices[state + 1];
                     ++ecChoiceIt) {
                    containedChoices.insert(*ecChoiceIt);
                }
                STORM_LOG_ASSERT(!containedChoices.empty(), "The contained choices of any state in an MEC must be non-empty.");
                newMec.addState(state, std::move(containedChoices));
            }
            this->blocks.emplace_back(std::move(newMec));
        }

        if (nonTrivSccIndices == ecSccIndices) {
            // All non trivial SCCs are MECs, nothing left to do!
            break;
        }

        // prepare next iteration.
        // It suffices to keep the candidates that have the possibility to always stay in the candidate set
        remainingEcCandidates = storm::utility::graph::performProbGreater0A(transitionMatrix, nondeterministicChoiceIndices, backwardTransitions,
                                                                            remainingEcCandidates, ~remainingEcCandidates, false, 0, ecChoices);
        remainingEcCandidates.complement();
        sccDecOptions.subsystem(remainingEcCandidates);
        sccDecOptions.choices(ecChoices);
    }

    STORM_LOG_DEBUG("MEC decomposition found " << this->size() << " MEC(s).");
}

// Explicitly instantiate the MEC decomposition.
template class MaximalEndComponentDecomposition<double>;
template MaximalEndComponentDecomposition<double>::MaximalEndComponentDecomposition(storm::models::sparse::NondeterministicModel<double> const& model);

template class MaximalEndComponentDecomposition<storm::RationalNumber>;
template MaximalEndComponentDecomposition<storm::RationalNumber>::MaximalEndComponentDecomposition(
    storm::models::sparse::NondeterministicModel<storm::RationalNumber> const& model);

template class MaximalEndComponentDecomposition<storm::Interval>;
template MaximalEndComponentDecomposition<storm::Interval>::MaximalEndComponentDecomposition(
    storm::models::sparse::NondeterministicModel<storm::Interval> const& model);

template class MaximalEndComponentDecomposition<storm::RationalInterval>;
template MaximalEndComponentDecomposition<storm::RationalInterval>::MaximalEndComponentDecomposition(
    storm::models::sparse::NondeterministicModel<storm::RationalInterval> const& model);

template class MaximalEndComponentDecomposition<storm::RationalFunction>;
template MaximalEndComponentDecomposition<storm::RationalFunction>::MaximalEndComponentDecomposition(
    storm::models::sparse::NondeterministicModel<storm::RationalFunction> const& model);

}  // namespace storage
}  // namespace storm
