#pragma once

#include <queue>
#include <utility>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/sparse/PrismChoiceOrigins.h"

namespace storm {
namespace counterexamples {

/*!
 * Computes a set of labels that is executed along all paths from any state to a target state.
 *
 * @param labelSet the considered label sets (a label set is assigned to each choice)
 *
 * @return The set of labels that is visited on all paths from any state to a target state.
 */
template<typename T>
std::vector<storm::storage::FlatSet<uint_fast64_t>> getGuaranteedLabelSets(storm::models::sparse::Model<T> const& model,
                                                                           std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                                                           storm::storage::BitVector const& psiStates,
                                                                           storm::storage::FlatSet<uint_fast64_t> const& relevantLabels) {
    STORM_LOG_THROW(model.getNumberOfChoices() == labelSets.size(), storm::exceptions::InvalidArgumentException,
                    "The given number of labels does not match the number of choices.");

    // Get some data from the model for convenient access.
    storm::storage::SparseMatrix<T> const& transitionMatrix = model.getTransitionMatrix();
    std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();
    storm::storage::SparseMatrix<T> backwardTransitions = model.getBackwardTransitions();

    // Now we compute the set of labels that is present on all paths from the initial to the target states.
    std::vector<storm::storage::FlatSet<uint_fast64_t>> analysisInformation(model.getNumberOfStates(), relevantLabels);

    std::queue<uint_fast64_t> worklist;
    storm::storage::BitVector statesInWorkList(model.getNumberOfStates());
    storm::storage::BitVector markedStates(model.getNumberOfStates());

    // Initially, put all predecessors of target states in the worklist and empty the analysis information them.
    for (uint64_t state : psiStates) {
        analysisInformation[state] = storm::storage::FlatSet<uint_fast64_t>();
        for (auto const& predecessorEntry : backwardTransitions.getRow(state)) {
            if (predecessorEntry.getColumn() != state && !statesInWorkList.get(predecessorEntry.getColumn()) && !psiStates.get(predecessorEntry.getColumn())) {
                worklist.push(predecessorEntry.getColumn());
                statesInWorkList.set(predecessorEntry.getColumn());
                markedStates.set(state);
            }
        }
    }

    while (!worklist.empty()) {
        uint_fast64_t const& currentState = worklist.front();

        size_t analysisInformationSizeBefore = analysisInformation[currentState].size();

        // Iterate over the successor states for all choices and compute new analysis information.
        for (uint_fast64_t currentChoice = nondeterministicChoiceIndices[currentState]; currentChoice < nondeterministicChoiceIndices[currentState + 1];
             ++currentChoice) {
            bool modifiedChoice = false;

            for (auto const& entry : transitionMatrix.getRow(currentChoice)) {
                if (markedStates.get(entry.getColumn())) {
                    modifiedChoice = true;
                    break;
                }
            }

            // If we can reach the target state with this choice, we need to intersect the current
            // analysis information with the union of the new analysis information of the target state
            // and the choice labels.
            if (modifiedChoice) {
                for (auto const& entry : transitionMatrix.getRow(currentChoice)) {
                    if (markedStates.get(entry.getColumn())) {
                        storm::storage::FlatSet<uint_fast64_t> tmpIntersection;
                        std::set_intersection(analysisInformation[currentState].begin(), analysisInformation[currentState].end(),
                                              analysisInformation[entry.getColumn()].begin(), analysisInformation[entry.getColumn()].end(),
                                              std::inserter(tmpIntersection, tmpIntersection.begin()));
                        std::set_intersection(analysisInformation[currentState].begin(), analysisInformation[currentState].end(),
                                              labelSets[currentChoice].begin(), labelSets[currentChoice].end(),
                                              std::inserter(tmpIntersection, tmpIntersection.begin()));
                        analysisInformation[currentState] = std::move(tmpIntersection);
                    }
                }
            }
        }

        // If the analysis information changed, we need to update it and put all the predecessors of this
        // state in the worklist.
        if (analysisInformation[currentState].size() != analysisInformationSizeBefore) {
            for (auto const& predecessorEntry : backwardTransitions.getRow(currentState)) {
                // Only put the predecessor in the worklist if it's not already a target state.
                if (!psiStates.get(predecessorEntry.getColumn()) && !statesInWorkList.get(predecessorEntry.getColumn())) {
                    worklist.push(predecessorEntry.getColumn());
                    statesInWorkList.set(predecessorEntry.getColumn());
                }
            }
            markedStates.set(currentState, true);
        } else {
            markedStates.set(currentState, false);
        }

        worklist.pop();
        statesInWorkList.set(currentState, false);
    }

    return analysisInformation;
}

/*!
 * Computes a set of labels that is executed along all paths from an initial state to a target state.
 *
 * @param labelSet the considered label sets (a label set is assigned to each choice)
 *
 * @return The set of labels that is executed on all paths from an initial state to a target state.
 */
template<typename T>
storm::storage::FlatSet<uint_fast64_t> getGuaranteedLabelSet(storm::models::sparse::Model<T> const& model,
                                                             std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                                             storm::storage::BitVector const& psiStates,
                                                             storm::storage::FlatSet<uint_fast64_t> const& relevantLabels) {
    std::vector<storm::storage::FlatSet<uint_fast64_t>> guaranteedLabels = getGuaranteedLabelSets(model, labelSets, psiStates, relevantLabels);

    storm::storage::FlatSet<uint_fast64_t> knownLabels(relevantLabels);
    storm::storage::FlatSet<uint_fast64_t> tempIntersection;
    for (auto initialState : model.getInitialStates()) {
        std::set_intersection(knownLabels.begin(), knownLabels.end(), guaranteedLabels[initialState].begin(), guaranteedLabels[initialState].end(),
                              std::inserter(tempIntersection, tempIntersection.end()));
        std::swap(knownLabels, tempIntersection);
    }

    return knownLabels;
}

}  // namespace counterexamples
}  // namespace storm
