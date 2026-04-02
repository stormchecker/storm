#include "storm/modelchecker/prctl/helper/SparseMdpEndComponentInformation.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/Scheduler.h"
#include "storm/utility/graph.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType>
SparseMdpEndComponentInformation<ValueType>::SparseMdpEndComponentInformation(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& endComponentDecomposition, storm::storage::BitVector const& maybeStates)
    : NOT_IN_EC(std::numeric_limits<uint64_t>::max()),
      eliminatedEndComponents(!endComponentDecomposition.empty()),
      numberOfMaybeStatesInEc(0),
      numberOfMaybeStatesNotInEc(0),
      numberOfEc(endComponentDecomposition.size()) {
    // (1) Compute how many maybe states there are before each other maybe state.
    maybeStatesBefore = maybeStates.getNumberOfSetBitsBeforeIndices();

    // (2) Create mapping from maybe states to their MEC. If they are not contained in an MEC, their value
    // is set to a special constant.
    maybeStateToEc.resize(maybeStates.getNumberOfSetBits(), NOT_IN_EC);
    uint64_t mecIndex = 0;
    for (auto const& mec : endComponentDecomposition) {
        for (auto const& stateActions : mec) {
            maybeStateToEc[maybeStatesBefore[stateActions.first]] = mecIndex;
            ++numberOfMaybeStatesInEc;
        }
        ++mecIndex;
    }

    // (3) Compute number of states not in MECs.
    numberOfMaybeStatesNotInEc = maybeStateToEc.size() - numberOfMaybeStatesInEc;

    // (4) Compute the number of maybe states that are not in ECs before every maybe state.
    maybeStatesNotInEcBefore = std::vector<uint64_t>(maybeStateToEc.size());

    uint64_t count = 0;
    auto resultIt = maybeStatesNotInEcBefore.begin();
    for (auto const& e : maybeStateToEc) {
        *resultIt = count;
        if (e == NOT_IN_EC) {
            ++count;
        }
        ++resultIt;
    }
}

template<typename ValueType>
bool SparseMdpEndComponentInformation<ValueType>::isMaybeStateInEc(uint64_t maybeState) const {
    return maybeStateToEc[maybeState] != NOT_IN_EC;
}

template<typename ValueType>
bool SparseMdpEndComponentInformation<ValueType>::isStateInEc(uint64_t state) const {
    return maybeStateToEc[maybeStatesBefore[state]] != NOT_IN_EC;
}

template<typename ValueType>
std::vector<uint64_t> const& SparseMdpEndComponentInformation<ValueType>::getNumberOfMaybeStatesNotInEcBeforeIndices() const {
    return maybeStatesNotInEcBefore;
}

template<typename ValueType>
uint64_t SparseMdpEndComponentInformation<ValueType>::getNumberOfMaybeStatesNotInEc() const {
    return numberOfMaybeStatesNotInEc;
}

template<typename ValueType>
uint64_t SparseMdpEndComponentInformation<ValueType>::getEc(uint64_t state) const {
    return maybeStateToEc[maybeStatesBefore[state]];
}

template<typename ValueType>
uint64_t SparseMdpEndComponentInformation<ValueType>::getRowGroupAfterElimination(uint64_t state) const {
    if (this->isStateInEc(state)) {
        return numberOfMaybeStatesNotInEc + getEc(state);
    } else {
        return maybeStatesNotInEcBefore[maybeStatesBefore[state]];
    }
}

template<typename ValueType>
bool SparseMdpEndComponentInformation<ValueType>::getEliminatedEndComponents() const {
    return eliminatedEndComponents;
}

template<typename ValueType>
uint64_t SparseMdpEndComponentInformation<ValueType>::getNotInEcMarker() const {
    return NOT_IN_EC;
}

template<typename ValueType>
SparseMdpEndComponentInformation<ValueType> SparseMdpEndComponentInformation<ValueType>::eliminateEndComponents(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& endComponentDecomposition,
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& maybeStates, storm::storage::BitVector const* sumColumns,
    storm::storage::BitVector const* selectedChoices, std::vector<ValueType> const* summand, storm::storage::SparseMatrix<ValueType>& submatrix,
    std::vector<ValueType>* columnSumVector, std::vector<ValueType>* summandResultVector, bool gatherExitChoices) {
    SparseMdpEndComponentInformation<ValueType> result(endComponentDecomposition, maybeStates);
    // TODO: Just like SparseMdpPrctlHelper::computeFixedPointSystemUntilProbabilities, this method must be adapted for intervals.
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                        "We do not support the elimination of end components and the creation of an adequate equation system with interval models.");
    }
    // (1) Compute the number of maybe states not in ECs before any other maybe state.
    std::vector<uint64_t> const& maybeStatesNotInEcBefore = result.getNumberOfMaybeStatesNotInEcBeforeIndices();
    uint64_t numberOfStates = result.numberOfMaybeStatesNotInEc + result.numberOfEc;

    STORM_LOG_TRACE("Creating " << numberOfStates << " states from " << result.numberOfMaybeStatesNotInEc << " states not in ECs and "
                                << result.numberOfMaybeStatesInEc << " states in " << result.numberOfEc << " ECs.");

    // Prepare builder and vector storage.
    storm::storage::SparseMatrixBuilder<ValueType> builder(0, numberOfStates, 0, true, true, numberOfStates);
    STORM_LOG_ASSERT((sumColumns && columnSumVector) || (!sumColumns && !columnSumVector),
                     "Expecting a bit vector for which columns to sum iff there is a column sum result vector.");
    if (columnSumVector) {
        // Clearing column sum vector as we do not know the number of choices at this point.
        columnSumVector->resize(0);
    }
    STORM_LOG_ASSERT((summand && summandResultVector) || (!summand && !summandResultVector), "Expecting summand iff there is a summand result vector.");
    if (summandResultVector) {
        // Clearing summand result vector as we do not know the number of choices at this point.
        summandResultVector->resize(0);
    }
    std::vector<std::pair<uint64_t, ValueType>> ecValuePairs;

    // (2) Create the parts of the submatrix and vector b that belong to states not contained in ECs.
    uint64_t currentRow = 0;
    for (auto state : maybeStates) {
        if (!result.isStateInEc(state)) {
            builder.newRowGroup(currentRow);
            for (uint64_t row = transitionMatrix.getRowGroupIndices()[state], endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
                // If the choices is not in the selected ones, drop it.
                if (selectedChoices && !selectedChoices->get(row)) {
                    continue;
                }

                ecValuePairs.clear();

                if (summand) {
                    summandResultVector->emplace_back((*summand)[row]);
                }
                if (columnSumVector) {
                    columnSumVector->emplace_back(storm::utility::zero<ValueType>());
                }
                for (auto const& e : transitionMatrix.getRow(row)) {
                    if (sumColumns && sumColumns->get(e.getColumn())) {
                        columnSumVector->back() += e.getValue();
                    } else if (maybeStates.get(e.getColumn())) {
                        // If the target state of the transition is not contained in an EC, we can just add the entry.
                        if (!result.isStateInEc(e.getColumn())) {
                            builder.addNextValue(currentRow, maybeStatesNotInEcBefore[result.maybeStatesBefore[e.getColumn()]], e.getValue());
                        } else {
                            // Otherwise, we store the information that the state can go to a certain EC.
                            ecValuePairs.emplace_back(result.getEc(e.getColumn()), e.getValue());
                        }
                    }
                }

                if (!ecValuePairs.empty()) {
                    std::sort(ecValuePairs.begin(), ecValuePairs.end());

                    for (auto const& e : ecValuePairs) {
                        builder.addNextValue(currentRow, result.numberOfMaybeStatesNotInEc + e.first, e.second);
                    }
                }

                ++currentRow;
            }
        }
    }

    // (3) Create the parts of the submatrix and vector b that belong to states contained in ECs.
    for (auto const& mec : endComponentDecomposition) {
        builder.newRowGroup(currentRow);
        std::vector<uint64_t> exitChoices;
        for (auto const& stateActions : mec) {
            uint64_t const& state = stateActions.first;
            for (uint64_t row = transitionMatrix.getRowGroupIndices()[state], endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
                // If the choice is contained in the MEC, drop it.
                if (stateActions.second.find(row) != stateActions.second.end()) {
                    continue;
                }

                // If the choices is not in the selected ones, drop it.
                if (selectedChoices && !selectedChoices->get(row)) {
                    continue;
                }

                ecValuePairs.clear();

                if (summand) {
                    summandResultVector->emplace_back((*summand)[row]);
                }
                if (columnSumVector) {
                    columnSumVector->emplace_back(storm::utility::zero<ValueType>());
                }
                for (auto const& e : transitionMatrix.getRow(row)) {
                    if (sumColumns && sumColumns->get(e.getColumn())) {
                        columnSumVector->back() += e.getValue();
                    } else if (maybeStates.get(e.getColumn())) {
                        // If the target state of the transition is not contained in an EC, we can just add the entry.
                        if (!result.isStateInEc(e.getColumn())) {
                            builder.addNextValue(currentRow, maybeStatesNotInEcBefore[result.maybeStatesBefore[e.getColumn()]], e.getValue());
                        } else {
                            // Otherwise, we store the information that the state can go to a certain EC.
                            ecValuePairs.emplace_back(result.getEc(e.getColumn()), e.getValue());
                        }
                    }
                }

                if (!ecValuePairs.empty()) {
                    std::sort(ecValuePairs.begin(), ecValuePairs.end());

                    for (auto const& e : ecValuePairs) {
                        builder.addNextValue(currentRow, result.getNumberOfMaybeStatesNotInEc() + e.first, e.second);
                    }
                }

                if (gatherExitChoices) {
                    exitChoices.push_back(row);
                }

                ++currentRow;
            }
        }
        if (gatherExitChoices) {
            result.ecToExitChoicesBefore.push_back(std::move(exitChoices));
        }
    }

    submatrix = builder.build(currentRow);
    return result;
}

template<typename ValueType>
SparseMdpEndComponentInformation<ValueType> SparseMdpEndComponentInformation<ValueType>::eliminateEndComponents(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& endComponentDecomposition,
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType>& rhsVector, storm::storage::BitVector const& maybeStates,
    storm::storage::SparseMatrix<ValueType>& submatrix, std::vector<ValueType>& subvector, bool gatherExitChoices) {
    SparseMdpEndComponentInformation<ValueType> result(endComponentDecomposition, maybeStates);
    // TODO: Just like SparseMdpPrctlHelper::computeFixedPointSystemUntilProbabilities, this method must be adapted for intervals.
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                        "We do not support the elimination of end components and the creation of an adequate equation system with interval models.");
    }

    // (1) Compute the number of maybe states not in ECs before any other maybe state.
    std::vector<uint64_t> maybeStatesNotInEcBefore = result.getNumberOfMaybeStatesNotInEcBeforeIndices();
    uint64_t numberOfStates = result.numberOfMaybeStatesNotInEc + result.numberOfEc;

    STORM_LOG_TRACE("Found " << numberOfStates << " states, " << result.numberOfMaybeStatesNotInEc << " not in ECs, " << result.numberOfMaybeStatesInEc
                             << " in ECs and " << result.numberOfEc << " ECs.");

    // Prepare builder and vector storage.
    storm::storage::SparseMatrixBuilder<ValueType> builder(0, numberOfStates, 0, true, true, numberOfStates);
    subvector.resize(numberOfStates);

    std::vector<std::pair<uint64_t, ValueType>> ecValuePairs;

    // (2) Create the parts of the submatrix and vector b that belong to states not contained in ECs.
    uint64_t currentRow = 0;
    for (auto state : maybeStates) {
        if (!result.isStateInEc(state)) {
            builder.newRowGroup(currentRow);
            for (uint64_t row = transitionMatrix.getRowGroupIndices()[state], endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
                // Copy over the entry of the vector.
                subvector[currentRow] = rhsVector[row];

                ecValuePairs.clear();
                for (auto const& e : transitionMatrix.getRow(row)) {
                    if (maybeStates.get(e.getColumn())) {
                        // If the target state of the transition is not contained in an EC, we can just add the entry.
                        if (result.isStateInEc(e.getColumn())) {
                            builder.addNextValue(currentRow, maybeStatesNotInEcBefore[result.maybeStatesBefore[e.getColumn()]], e.getValue());
                        } else {
                            // Otherwise, we store the information that the state can go to a certain EC.
                            ecValuePairs.emplace_back(result.getEc(e.getColumn()), e.getValue());
                        }
                    }
                }

                if (!ecValuePairs.empty()) {
                    std::sort(ecValuePairs.begin(), ecValuePairs.end());

                    for (auto const& e : ecValuePairs) {
                        builder.addNextValue(currentRow, result.numberOfMaybeStatesNotInEc + e.first, e.second);
                    }
                }

                ++currentRow;
            }
        }
    }

    // (3) Create the parts of the submatrix and vector b that belong to states contained in ECs.
    for (auto const& mec : endComponentDecomposition) {
        builder.newRowGroup(currentRow);
        std::vector<uint64_t> exitChoices;
        for (auto const& stateActions : mec) {
            uint64_t const& state = stateActions.first;
            for (uint64_t row = transitionMatrix.getRowGroupIndices()[state], endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
                // If the choice is contained in the MEC, drop it.
                if (stateActions.second.find(row) != stateActions.second.end()) {
                    continue;
                }

                // Copy over the entry of the vector.
                subvector[currentRow] = rhsVector[row];

                ecValuePairs.clear();
                for (auto const& e : transitionMatrix.getRow(row)) {
                    if (maybeStates.get(e.getColumn())) {
                        // If the target state of the transition is not contained in an EC, we can just add the entry.
                        if (result.isStateInEc(e.getColumn())) {
                            builder.addNextValue(currentRow, maybeStatesNotInEcBefore[result.maybeStatesBefore[e.getColumn()]], e.getValue());
                        } else {
                            // Otherwise, we store the information that the state can go to a certain EC.
                            ecValuePairs.emplace_back(result.getEc(e.getColumn()), e.getValue());
                        }
                    }
                }

                if (!ecValuePairs.empty()) {
                    std::sort(ecValuePairs.begin(), ecValuePairs.end());

                    for (auto const& e : ecValuePairs) {
                        builder.addNextValue(currentRow, result.getNumberOfMaybeStatesNotInEc() + e.first, e.second);
                    }
                }

                if (gatherExitChoices) {
                    exitChoices.push_back(row);
                }

                ++currentRow;
            }
        }
        if (gatherExitChoices) {
            result.ecToExitChoicesBefore.push_back(std::move(exitChoices));
        }
    }

    submatrix = builder.build();
    return result;
}

template<typename ValueType>
void SparseMdpEndComponentInformation<ValueType>::setValues(std::vector<ValueType>& result, storm::storage::BitVector const& maybeStates,
                                                            std::vector<ValueType> const& fromResult) {
    // The following assumes that row groups associated to EC states are at the very end.
    auto notInEcResultIt = fromResult.begin();
    for (auto state : maybeStates) {
        if (this->isStateInEc(state)) {
            STORM_LOG_ASSERT(this->getRowGroupAfterElimination(state) >= this->getNumberOfMaybeStatesNotInEc(),
                             "Expected introduced EC states to be located at the end of the matrix.");
            result[state] = fromResult[this->getRowGroupAfterElimination(state)];
        } else {
            result[state] = *notInEcResultIt;
            ++notInEcResultIt;
        }
    }
    STORM_LOG_ASSERT(notInEcResultIt == fromResult.begin() + this->getNumberOfMaybeStatesNotInEc(), "Mismatching iterators.");
}

template<typename ValueType>
template<typename SolutionType>
void SparseMdpEndComponentInformation<ValueType>::setScheduler(storm::storage::Scheduler<SolutionType>& scheduler, storm::storage::BitVector const& maybeStates,
                                                               storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                               storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                               std::vector<uint64_t> const& fromResult) {
    // The following assumes that row groups associated to EC states are at the very end.
    storm::storage::BitVector maybeStatesWithoutChoice(maybeStates.size(), false);
    storm::storage::BitVector ecStayChoices(transitionMatrix.getRowCount(), false);
    auto notInEcResultIt = fromResult.begin();
    for (auto state : maybeStates) {
        if (this->isStateInEc(state)) {
            STORM_LOG_ASSERT(this->getRowGroupAfterElimination(state) >= this->getNumberOfMaybeStatesNotInEc(),
                             "Expected introduced EC states to be located at the end of the matrix.");
            STORM_LOG_ASSERT(!ecToExitChoicesBefore.empty(), "No EC exit choices available. End Components have probably been build without.");
            uint64_t ec = getEc(state);
            auto const& exitChoices = ecToExitChoicesBefore[ec];
            uint64_t afterEliminationChoice = fromResult[this->getRowGroupAfterElimination(state)];
            uint64_t beforeEliminationGlobalChoiceIndex = exitChoices[afterEliminationChoice];
            bool noChoice = true;
            for (uint64_t globalChoice = transitionMatrix.getRowGroupIndices()[state]; globalChoice < transitionMatrix.getRowGroupIndices()[state + 1];
                 ++globalChoice) {
                // Is this the selected exit choice?
                if (globalChoice == beforeEliminationGlobalChoiceIndex) {
                    scheduler.setChoice(beforeEliminationGlobalChoiceIndex - transitionMatrix.getRowGroupIndices()[state], state);
                    noChoice = false;
                } else {
                    // Check if this is an exit choice
                    if (std::find(exitChoices.begin(), exitChoices.end(), globalChoice) == exitChoices.end()) {
                        ecStayChoices.set(globalChoice, true);
                    }
                }
            }
            maybeStatesWithoutChoice.set(state, noChoice);
        } else {
            scheduler.setChoice(*notInEcResultIt, state);
            ++notInEcResultIt;
        }
    }

    STORM_LOG_ASSERT(notInEcResultIt == fromResult.begin() + this->getNumberOfMaybeStatesNotInEc(), "Mismatching iterators.");
    // The maybeStates without a choice (i.e. those within an end component for which we do not take an exiting choice) shall reach maybeStates with a choice
    // with probability 1 We have to make sure that choices for non-maybe states are not set
    auto maybeStatesWithChoice = maybeStates & ~maybeStatesWithoutChoice;
    storm::utility::graph::computeSchedulerProb1E(maybeStates, transitionMatrix, backwardTransitions, maybeStatesWithoutChoice, maybeStatesWithChoice,
                                                  scheduler, ecStayChoices);
}

template class SparseMdpEndComponentInformation<double>;
template class SparseMdpEndComponentInformation<storm::RationalNumber>;
template class SparseMdpEndComponentInformation<storm::Interval>;
template class SparseMdpEndComponentInformation<storm::RationalInterval>;

template void SparseMdpEndComponentInformation<double>::setScheduler(storm::storage::Scheduler<double>& scheduler, storm::storage::BitVector const& maybeStates,
                                                                     storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                     storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                     std::vector<uint64_t> const& fromResult);

template void SparseMdpEndComponentInformation<storm::RationalNumber>::setScheduler(
    storm::storage::Scheduler<storm::RationalNumber>& scheduler, storm::storage::BitVector const& maybeStates,
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
    std::vector<uint64_t> const& fromResult);

template void SparseMdpEndComponentInformation<storm::Interval>::setScheduler(storm::storage::Scheduler<double>& scheduler,
                                                                              storm::storage::BitVector const& maybeStates,
                                                                              storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                                              storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                                              std::vector<uint64_t> const& fromResult);

template void SparseMdpEndComponentInformation<storm::RationalInterval>::setScheduler(
    storm::storage::Scheduler<storm::RationalNumber>& scheduler, storm::storage::BitVector const& maybeStates,
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions, std::vector<uint64_t> const& fromResult);

// template class SparseMdpEndComponentInformation<storm::RationalFunction>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
