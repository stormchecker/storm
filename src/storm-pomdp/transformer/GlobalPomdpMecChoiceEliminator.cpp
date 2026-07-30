#include "storm-pomdp/transformer/GlobalPomdpMecChoiceEliminator.h"

#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/transformer/ChoiceSelector.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {
namespace transformer {

template<typename ValueType>
GlobalPomdpMecChoiceEliminator<ValueType>::GlobalPomdpMecChoiceEliminator(storm::models::sparse::Pomdp<ValueType> const& pomdp) : pomdp(pomdp) {}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> GlobalPomdpMecChoiceEliminator<ValueType>::transform(storm::logic::Formula const& formula) const {
    // check whether the property is minimizing or maximizing
    STORM_LOG_THROW(formula.isOperatorFormula(), storm::exceptions::InvalidPropertyException, "Expected an operator formula");
    STORM_LOG_THROW(formula.asOperatorFormula().hasOptimalityType() || formula.asOperatorFormula().hasBound(), storm::exceptions::InvalidPropertyException,
                    "The formula " << formula << " does not specify whether to minimize or maximize.");
    bool minimizes = (formula.asOperatorFormula().hasOptimalityType() && storm::solver::minimize(formula.asOperatorFormula().getOptimalityType())) ||
                     (formula.asOperatorFormula().hasBound() && storm::logic::isLowerBound(formula.asOperatorFormula().getBound().comparisonType));

    std::shared_ptr<storm::logic::Formula const> subformula = formula.asOperatorFormula().getSubformula().asSharedPointer();
    // If necessary, convert the subformula to a more general case
    if (subformula->isEventuallyFormula() && formula.isProbabilityOperatorFormula()) {
        subformula = std::make_shared<storm::logic::UntilFormula>(storm::logic::Formula::getTrueFormula(),
                                                                  subformula->asEventuallyFormula().getSubformula().asSharedPointer());
    }

    if (formula.isProbabilityOperatorFormula() && subformula->isUntilFormula()) {
        if (!minimizes) {
            return transformMax(subformula->asUntilFormula());
        }
    } else if (formula.isRewardOperatorFormula() && subformula->isEventuallyFormula()) {
        if (minimizes) {
            return transformMinReward(subformula->asEventuallyFormula());
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "Mec elimination is not supported for the property " << formula);
    return nullptr;
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> GlobalPomdpMecChoiceEliminator<ValueType>::transformMinReward(
    storm::logic::EventuallyFormula const& formula) const {
    assert(formula.isRewardPathFormula());
    auto backwardTransitions = pomdp.getBackwardTransitions();
    storm::storage::BitVector allStates(pomdp.getNumberOfStates(), true);
    auto prob1EStates = storm::utility::graph::performProb1E(pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(), backwardTransitions,
                                                             allStates, checkPropositionalFormula(formula.getSubformula()));
    STORM_LOG_THROW(prob1EStates.full(), storm::exceptions::InvalidPropertyException,
                    "There are states from which the set of target states is not reachable. This is not supported.");
    auto prob1AStates = storm::utility::graph::performProb1A(pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(), backwardTransitions,
                                                             allStates, checkPropositionalFormula(formula.getSubformula()));

    auto mecs = decomposeEndComponents(~prob1AStates, ~allStates);

    // Get the 'out' state for every MEC with just a single out state
    storm::storage::BitVector uniqueOutStates = getEndComponentsWithSingleOutStates(mecs);

    // For each observation of some 'out' state get the intersection of the choices that lead to the corresponding MEC
    std::vector<storm::storage::BitVector> mecChoicesPerObservation = getEndComponentChoicesPerObservation(mecs, uniqueOutStates);

    // Filter the observations that have a state that is not an out state
    storm::storage::BitVector stateFilter = ~uniqueOutStates;
    for (auto const state : stateFilter) {
        mecChoicesPerObservation[pomdp.getObservation(state)].clear();
    }

    // It should not be possible to clear all choices for an observation since we only consider states that lead outside of its MEC.
    for ([[maybe_unused]] auto& clearedChoices : mecChoicesPerObservation) {
        STORM_LOG_ASSERT(clearedChoices.size() == 0 || !clearedChoices.full(), "Tried to clear all choices for an observation.");
    }

    // transform the set of selected choices to global choice indices
    storm::storage::BitVector choiceFilter(pomdp.getNumberOfChoices(), true);
    stateFilter.complement();
    for (auto const state : stateFilter) {
        uint64_t offset = pomdp.getTransitionMatrix().getRowGroupIndices()[state];
        for (auto const& choice : mecChoicesPerObservation[pomdp.getObservation(state)]) {
            choiceFilter.set(offset + choice, false);
        }
    }

    ChoiceSelector<ValueType> cs(pomdp);
    auto res = cs.transform(choiceFilter)->template as<storm::models::sparse::Pomdp<ValueType>>();
    res->setIsCanonic();
    return res;
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> GlobalPomdpMecChoiceEliminator<ValueType>::transformMax(
    storm::logic::UntilFormula const& formula) const {
    auto backwardTransitions = pomdp.getBackwardTransitions();
    auto prob01States = storm::utility::graph::performProb01Max(pomdp.getTransitionMatrix(), pomdp.getTransitionMatrix().getRowGroupIndices(),
                                                                backwardTransitions, checkPropositionalFormula(formula.getLeftSubformula()),
                                                                checkPropositionalFormula(formula.getRightSubformula()));
    auto mecs = decomposeEndComponents(~(prob01States.first | prob01States.second), prob01States.first);

    // Get the 'out' state for every MEC with just a single out state
    storm::storage::BitVector uniqueOutStates = getEndComponentsWithSingleOutStates(mecs);

    // For each observation of some 'out' state get the intersection of the choices that lead to the corresponding MEC
    std::vector<storm::storage::BitVector> mecChoicesPerObservation = getEndComponentChoicesPerObservation(mecs, uniqueOutStates);

    // Filter the observations that have a state that is neither an out state, nor a prob0A state
    storm::storage::BitVector stateFilter = ~(uniqueOutStates | prob01States.first);
    for (auto const state : stateFilter) {
        mecChoicesPerObservation[pomdp.getObservation(state)].clear();
    }

    // It should not be possible to clear all choices for an observation since we only consider states that lead outside of its MEC.
    for ([[maybe_unused]] auto& clearedChoices : mecChoicesPerObservation) {
        STORM_LOG_ASSERT(clearedChoices.size() == 0 || !clearedChoices.full(), "Tried to clear all choices for an observation.");
    }

    // transform the set of selected choices to global choice indices
    storm::storage::BitVector choiceFilter(pomdp.getNumberOfChoices(), true);
    stateFilter.complement();
    for (auto const state : stateFilter) {
        uint64_t offset = pomdp.getTransitionMatrix().getRowGroupIndices()[state];
        for (auto const& choice : mecChoicesPerObservation[pomdp.getObservation(state)]) {
            choiceFilter.set(offset + choice, false);
        }
    }

    ChoiceSelector<ValueType> cs(pomdp);
    auto res = cs.transform(choiceFilter)->template as<storm::models::sparse::Pomdp<ValueType>>();
    res->setIsCanonic();
    return res;
}

template<typename ValueType>
storm::storage::BitVector GlobalPomdpMecChoiceEliminator<ValueType>::getEndComponentsWithSingleOutStates(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs) const {
    storm::storage::BitVector result(pomdp.getNumberOfStates(), false);
    for (auto const& mec : mecs) {
        std::optional<uint64_t> uniqueOutState = std::nullopt;
        for (auto const& stateActionsPair : mec) {
            // Check whether this is an 'out' state, i.e., not all actions stay inside the MEC
            if (stateActionsPair.second.size() != pomdp.getNumberOfChoices(stateActionsPair.first)) {
                if (uniqueOutState) {
                    // we already found one out state, so this mec is invalid
                    uniqueOutState = std::nullopt;
                    break;
                } else {
                    uniqueOutState = stateActionsPair.first;
                }
            }
        }
        if (uniqueOutState) {
            result.set(uniqueOutState.value(), true);
        }
    }
    return result;
}

template<typename ValueType>
std::vector<storm::storage::BitVector> GlobalPomdpMecChoiceEliminator<ValueType>::getEndComponentChoicesPerObservation(
    storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs, storm::storage::BitVector const& consideredStates) const {
    std::vector<storm::storage::BitVector> result(pomdp.getNrObservations());
    for (auto const& mec : mecs) {
        for (auto const& stateActions : mec) {
            if (consideredStates.get(stateActions.first)) {
                storm::storage::BitVector localChoiceIndices(pomdp.getNumberOfChoices(stateActions.first), false);
                uint64_t offset = pomdp.getTransitionMatrix().getRowGroupIndices()[stateActions.first];
                for (auto const& choice : stateActions.second) {
                    assert(choice >= offset);
                    localChoiceIndices.set(choice - offset, true);
                }

                auto& mecChoices = result[pomdp.getObservation(stateActions.first)];
                if (mecChoices.size() == 0) {
                    mecChoices = localChoiceIndices;
                } else {
                    STORM_LOG_ASSERT(mecChoices.size() == localChoiceIndices.size(),
                                     "Observation action count does not match for two states with the same observation");
                    mecChoices &= localChoiceIndices;
                }
            }
        }
    }
    return result;
}

template<typename ValueType>
storm::storage::MaximalEndComponentDecomposition<ValueType> GlobalPomdpMecChoiceEliminator<ValueType>::decomposeEndComponents(
    storm::storage::BitVector const& subsystem, storm::storage::BitVector const& redirectingStates) const {
    if (redirectingStates.empty()) {
        return storm::storage::MaximalEndComponentDecomposition<ValueType>(pomdp.getTransitionMatrix(), pomdp.getBackwardTransitions(), subsystem);
    } else {
        // Redirect all incoming transitions of a redirictingState back to the origin of the transition.
        storm::storage::SparseMatrixBuilder<ValueType> builder(pomdp.getTransitionMatrix().getRowCount(), pomdp.getTransitionMatrix().getColumnCount(), 0, true,
                                                               true, pomdp.getTransitionMatrix().getRowGroupCount());
        uint64_t row = 0;
        for (uint64_t rowGroup = 0; rowGroup < pomdp.getTransitionMatrix().getRowGroupCount(); ++rowGroup) {
            assert(row == pomdp.getTransitionMatrix().getRowGroupIndices()[rowGroup]);
            builder.newRowGroup(row);
            for (; row < pomdp.getTransitionMatrix().getRowGroupIndices()[rowGroup + 1]; ++row) {
                ValueType redirectedProbabilityMass = pomdp.getTransitionMatrix().getConstrainedRowSum(row, redirectingStates);
                bool insertSelfloop = !storm::utility::isZero(redirectedProbabilityMass);
                for (auto const& entry : pomdp.getTransitionMatrix().getRow(row)) {
                    if (!redirectingStates.get(entry.getColumn())) {
                        if (insertSelfloop && entry.getColumn() >= rowGroup) {
                            // insert selfloop now
                            insertSelfloop = false;
                            if (entry.getColumn() == rowGroup) {
                                builder.addNextValue(row, rowGroup, entry.getValue() + redirectedProbabilityMass);
                                continue;
                            } else {
                                builder.addNextValue(row, rowGroup, redirectedProbabilityMass);
                            }
                        }
                        builder.addNextValue(row, entry.getColumn(), entry.getValue());
                    }
                }
                if (insertSelfloop) {
                    builder.addNextValue(row, rowGroup, redirectedProbabilityMass);
                }
            }
        }
        storm::storage::SparseMatrix<ValueType> transitionMatrix = builder.build();
        return storm::storage::MaximalEndComponentDecomposition<ValueType>(transitionMatrix, transitionMatrix.transpose(true), subsystem);
    }
}

template<typename ValueType>
storm::storage::BitVector GlobalPomdpMecChoiceEliminator<ValueType>::checkPropositionalFormula(storm::logic::Formula const& propositionalFormula) const {
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Mdp<ValueType>> mc(pomdp);
    STORM_LOG_THROW(mc.canHandle(propositionalFormula), storm::exceptions::InvalidPropertyException,
                    "Propositional model checker can not handle formula " << propositionalFormula);
    return mc.check(propositionalFormula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
}

template class GlobalPomdpMecChoiceEliminator<double>;
template class GlobalPomdpMecChoiceEliminator<storm::RationalNumber>;
}  // namespace transformer
}  // namespace storm