#pragma once

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "storm-pomdp/beliefs/utility/types.h"

namespace storm::pomdp::beliefs {

/** A successor entry recorded while exploring one action of a belief. */
template<typename ValueType, typename...>
struct BeliefExplorationTransition {
    ValueType probability;
    storm::pomdp::beliefs::BeliefId targetBelief;
};

template<typename ValueType, typename ExtraTransitionData>
struct BeliefExplorationTransition<ValueType, ExtraTransitionData> {
    ValueType probability;
    storm::pomdp::beliefs::BeliefId targetBelief;
    ExtraTransitionData data;
};

template<typename ValueType, typename FirstExtraTransitionData, typename... OtherExtraTransitionData>
struct BeliefExplorationTransition<ValueType, FirstExtraTransitionData, OtherExtraTransitionData...> {
    ValueType probability;
    storm::pomdp::beliefs::BeliefId targetBelief;
    std::tuple<FirstExtraTransitionData, OtherExtraTransitionData...> data;
};

/**
 * Sparse transition data for the explored part of a belief MDP.
 *
 * Rows correspond to actions and row groups to explored beliefs. Extra transition data is stored alongside each
 * probability, for example reward-bound updates or clipping corrections.
 */
template<typename ValueType, typename... ExtraTransitionData>
class BeliefExplorationMatrix {
   public:
    /*!
     * Initializes a new (empty) belief exploration matrix.
     */
    BeliefExplorationMatrix();

    /*!
     * While building the matrix, ends the current row in the matrix.
     */
    void endCurrentRow();

    /*!
     * While building the matrix, ends the current row group in the matrix.
     * @note This function should be called after endCurrentRow() has been called.
     */
    void endCurrentRowGroup();

    /*!
     * @return the current number of rows in the matrix
     */
    std::size_t rows() const;

    /*!
     * @return the current number of row groups in the matrix
     */
    std::size_t groups() const;

    /** @return whether labels have been recorded for the matrix choices. */
    bool hasChoiceLabels() const;

    /** Successor entries, grouped by rowIndications. */
    std::vector<BeliefExplorationTransition<ValueType, ExtraTransitionData...>> transitions;
    /** Start indices of action rows in transitions. */
    std::vector<uint64_t> rowIndications;
    /** Start indices of belief row groups in the action rows. */
    std::vector<uint64_t> rowGroupIndices;
    /** Optional labels for the action rows. */
    std::vector<std::set<std::string>> choiceLabels;
};

}  // namespace storm::pomdp::beliefs
