#include "storm-pars/transformer/SparseParametricModelSimplifier.h"
#include <memory>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/solver/stateelimination/NondeterministicModelStateEliminator.h"
#include "storm/storage/FlexibleSparseMatrix.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/transformer/GoalStateMerger.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {

namespace {

/*!
 * Eliminates all states that satisfy
 * * there is only one enabled action (i.e., there is no nondeterministic choice at the state),
 * * all outgoing transitions are constant,
 * * there is no statelabel defined, and
 * * (if rewardModelName is given) the reward collected at the state is constant.
 *
 * The resulting model will only have the rewardModel with the provided name (or no reward model at all if no name was given).
 * Labelings of eliminated states will be lost
 */
template<typename SparseModelType>
std::shared_ptr<SparseModelType> eliminateConstantDeterministicStates(SparseModelType const& model, bool preserveParametricTransitions,
                                                                      storm::storage::BitVector const& consideredStates,
                                                                      std::optional<std::string> const& rewardModelName) {
    storm::storage::SparseMatrix<typename SparseModelType::ValueType> const& sparseMatrix = model.getTransitionMatrix();
    auto backwardsSparseMatrix = sparseMatrix.transpose();

    // get the action-based reward values
    std::vector<typename SparseModelType::ValueType> actionRewards;
    if (rewardModelName) {
        actionRewards = model.getRewardModel(*rewardModelName).getTotalRewardVector(sparseMatrix);
    } else {
        actionRewards = std::vector<typename SparseModelType::ValueType>(model.getTransitionMatrix().getRowCount(),
                                                                         storm::utility::zero<typename SparseModelType::ValueType>());
    }

    // Find the states that are to be eliminated
    storm::storage::BitVector selectedStates = consideredStates;
    for (auto state : consideredStates) {
        if (sparseMatrix.getRowGroupSize(state) == 1 &&
            (!rewardModelName.has_value() || storm::utility::isConstant(actionRewards[sparseMatrix.getRowGroupIndices()[state]]))) {
            for (auto const& entry : sparseMatrix.getRowGroup(state)) {
                if (!storm::utility::isConstant(entry.getValue())) {
                    selectedStates.set(state, false);
                    break;
                }
            }
            if (state && preserveParametricTransitions) {
                for (auto const& entry : backwardsSparseMatrix.getRowGroup(state)) {
                    if (!storm::utility::isConstant(entry.getValue())) {
                        selectedStates.set(state, false);
                        break;
                    }
                }
            }
        } else {
            selectedStates.set(state, false);
        }
    }

    // invoke elimination and obtain resulting transition matrix
    storm::storage::FlexibleSparseMatrix<typename SparseModelType::ValueType> flexibleMatrix(sparseMatrix);
    storm::storage::FlexibleSparseMatrix<typename SparseModelType::ValueType> flexibleBackwardTransitions(backwardsSparseMatrix, true);
    storm::solver::stateelimination::NondeterministicModelStateEliminator<typename SparseModelType::ValueType> stateEliminator(
        flexibleMatrix, flexibleBackwardTransitions, actionRewards);
    for (auto state : selectedStates) {
        stateEliminator.eliminateState(state, true);
    }
    selectedStates.complement();
    auto keptRows = sparseMatrix.getRowFilter(selectedStates);
    storm::storage::SparseMatrix<typename SparseModelType::ValueType> newTransitionMatrix = flexibleMatrix.createSparseMatrix(keptRows, selectedStates);

    // obtain the reward model for the resulting system
    std::unordered_map<std::string, typename SparseModelType::RewardModelType> rewardModels;
    if (rewardModelName) {
        storm::utility::vector::filterVectorInPlace(actionRewards, keptRows);
        rewardModels.insert(std::make_pair(*rewardModelName, typename SparseModelType::RewardModelType(std::nullopt, std::move(actionRewards))));
    }

    return std::make_shared<SparseModelType>(std::move(newTransitionMatrix), model.getStateLabeling().getSubLabeling(selectedStates), std::move(rewardModels));
}

/*!
 * Eliminates all end components of the model satisfying
 * * ignoredStates is false for all states of the EC
 * * (if rewardModelName is given) there is no reward collected while staying inside the EC.
 *
 * Eliminating an EC means that it is replaced by a single state whose incoming and outgoing tansitions correspond to the incoming and outgoing transitions
 * of the EC. Only applicable to nondeterministic models.
 *
 * The resulting model will only have the rewardModel with the provided name (or no reward model at all if no name was given)
 */
template<typename SparseModelType>
std::shared_ptr<SparseModelType> eliminateNeutralEndComponents(SparseModelType const& model, storm::storage::BitVector const& ignoredStates,
                                                               std::optional<std::string> const& rewardModelName) {
    // Get the actions that can be part of an EC
    storm::storage::BitVector possibleECActions(model.getNumberOfChoices(), true);
    for (auto state : ignoredStates) {
        for (uint_fast64_t actionIndex = model.getTransitionMatrix().getRowGroupIndices()[state];
             actionIndex < model.getTransitionMatrix().getRowGroupIndices()[state + 1]; ++actionIndex) {
            possibleECActions.set(actionIndex, false);
        }
    }

    // Get the action-based reward values and unselect non-zero reward actions
    std::vector<typename SparseModelType::ValueType> actionRewards;
    if (rewardModelName) {
        actionRewards = model.getRewardModel(*rewardModelName).getTotalRewardVector(model.getTransitionMatrix());
        uint_fast64_t actionIndex = 0;
        for (auto const& actionReward : actionRewards) {
            if (!storm::utility::isZero(actionReward)) {
                possibleECActions.set(actionIndex, false);
            }
            ++actionIndex;
        }
    }

    // Invoke EC Elimination
    auto ecEliminatorResult = storm::transformer::EndComponentEliminator<typename SparseModelType::ValueType>::transform(
        model.getTransitionMatrix(), storm::storage::BitVector(model.getNumberOfStates(), true), possibleECActions,
        storm::storage::BitVector(model.getNumberOfStates(), false));

    // obtain the reward model for the resulting system
    std::unordered_map<std::string, typename SparseModelType::RewardModelType> rewardModels;
    if (rewardModelName) {
        std::vector<typename SparseModelType::ValueType> newActionRewards(ecEliminatorResult.matrix.getRowCount());
        storm::utility::vector::selectVectorValues(newActionRewards, ecEliminatorResult.newToOldRowMapping, actionRewards);
        rewardModels.insert(std::make_pair(*rewardModelName, typename SparseModelType::RewardModelType(std::nullopt, std::move(actionRewards))));
    }

    // the new labeling
    storm::models::sparse::StateLabeling labeling(ecEliminatorResult.matrix.getRowGroupCount());
    for (auto const& label : model.getStateLabeling().getLabels()) {
        auto const& origStatesWithLabel = model.getStates(label);
        storm::storage::BitVector newStatesWithLabel(ecEliminatorResult.matrix.getRowGroupCount(), false);
        for (auto const& origState : origStatesWithLabel) {
            newStatesWithLabel.set(ecEliminatorResult.oldToNewStateMapping[origState], true);
        }
        labeling.addLabel(label, std::move(newStatesWithLabel));
    }

    return std::make_shared<SparseModelType>(std::move(ecEliminatorResult.matrix), std::move(labeling), std::move(rewardModels));
}

}  // namespace

template<typename SparseModelType>
SparseParametricModelSimplifier<SparseModelType>::SparseParametricModelSimplifier(SparseModelType const& model) : originalModel(model) {
    // intentionally left empty
}

template<typename SparseModelType>
bool SparseParametricModelSimplifier<SparseModelType>::simplify(storm::logic::Formula const& formula) {
    // Make sure that there is no old result from a previous call
    simplifiedModel = nullptr;
    simplifiedFormula = nullptr;

    // Go through various cases for the formula, simplify if possible, and decide if state elimination and end component elimination preserves the formula.
    auto const& operatorFormula = formula.asOperatorFormula();
    STORM_LOG_THROW(!originalModel.isNondeterministicModel() || operatorFormula.hasOptimalityType() || operatorFormula.hasBound(),
                    storm::exceptions::InvalidPropertyException,
                    "The formula " << formula << " is not supported for nondeterministic models. It must either have an optimality type or a bound.");
    bool const minimizing = originalModel.isNondeterministicModel() &&
                            (operatorFormula.hasOptimalityType() ? storm::solver::minimize(operatorFormula.getOptimalityType())
                                                                 : storm::logic::isLowerBound(operatorFormula.getBound().comparisonType));
    bool enableStateElimination = false;
    bool enableEndComponentElimination = false;
    std::optional<std::string> rewardModelName;
    if (operatorFormula.isProbabilityOperatorFormula()) {
        if (operatorFormula.getSubformula().isUntilFormula()) {
            // GoalStateMerger merges all states violating the left subformula (if any) into the sink state, so it can safely be dropped here. Note that the
            // subformula may either be an until formula (phi U psi) or a plain reachability-probability formula (F psi, i.e. implicitly true U psi).
            auto eventuallyFormula = std::make_shared<storm::logic::EventuallyFormula const>(
                operatorFormula.getSubformula().asUntilFormula().getRightSubformula().asSharedPointer(), storm::logic::FormulaContext::Probability);
            simplifiedFormula = std::make_shared<storm::logic::ProbabilityOperatorFormula const>(eventuallyFormula, operatorFormula.getOperatorInformation());
            enableStateElimination = true;
            enableEndComponentElimination = originalModel.isNondeterministicModel() && !minimizing;  // end components can only exist for Pmax queries.
        } else if (operatorFormula.getSubformula().isReachabilityProbabilityFormula()) {
            enableStateElimination = true;
            enableEndComponentElimination = originalModel.isNondeterministicModel() && !minimizing;  // end components can only exist for Pmax queries.
        } else if (operatorFormula.getSubformula().isBoundedUntilFormula()) {
            // multidimensional or reward bounded until formulas are not considered in this simplifier.
            if (auto const& boundedUntilFormula = operatorFormula.getSubformula().asBoundedUntilFormula();
                !boundedUntilFormula.isMultiDimensional() && !boundedUntilFormula.getTimeBoundReference().isRewardBound()) {
                // Since we have a discrete-time model, we may assume a step bound.
                // Similar to (unbounded) until above, we can also drop the left subformula.
                boost::optional<storm::logic::TimeBound> lowerBound, upperBound;
                if (boundedUntilFormula.hasLowerBound()) {
                    lowerBound.emplace(boundedUntilFormula.isLowerBoundStrict(), boundedUntilFormula.getLowerBound());
                }
                if (boundedUntilFormula.hasUpperBound()) {
                    upperBound.emplace(boundedUntilFormula.isUpperBoundStrict(), boundedUntilFormula.getUpperBound());
                }
                auto newBoundedUntilFormula = std::make_shared<storm::logic::BoundedUntilFormula const>(
                    storm::logic::Formula::getTrueFormula(), boundedUntilFormula.getRightSubformula().asSharedPointer(), lowerBound, upperBound,
                    storm::logic::TimeBoundReference(storm::logic::TimeBoundType::Steps));
                simplifiedFormula =
                    std::make_shared<storm::logic::ProbabilityOperatorFormula const>(newBoundedUntilFormula, operatorFormula.getOperatorInformation());
                // neither state elimination nor end component elimination preserve step-bounded formulas.
            }
        }
    } else if (operatorFormula.isRewardOperatorFormula()) {
        if (operatorFormula.asRewardOperatorFormula().hasRewardModelName()) {
            rewardModelName = operatorFormula.asRewardOperatorFormula().getRewardModelName();
            STORM_LOG_THROW(originalModel.hasRewardModel(*rewardModelName), storm::exceptions::InvalidPropertyException,
                            "The reward model specified by formula " << formula << " is not available in the given model.");
        } else {
            STORM_LOG_THROW(originalModel.hasUniqueRewardModel(), storm::exceptions::InvalidPropertyException,
                            "The formula " << formula << " does not specify a reward model and the reward model is not unique.");
            rewardModelName = originalModel.getUniqueRewardModelName();
        }
        if (operatorFormula.getSubformula().isReachabilityRewardFormula()) {
            enableStateElimination = true;
            enableEndComponentElimination = originalModel.isNondeterministicModel() && minimizing;  // end components can only exist for Rmin queries.
        } else if (operatorFormula.getSubformula().isCumulativeRewardFormula()) {
            // multidimensional or reward bounded cumulative reward formulas are not considered in this simplifier
            if (auto const& cumulativeRewardFormula = operatorFormula.getSubformula().asCumulativeRewardFormula();
                !cumulativeRewardFormula.isMultiDimensional() && !cumulativeRewardFormula.getTimeBoundReference().isRewardBound()) {
                // Since we have a discrete-time model, we may assume a step bound.
                boost::optional<storm::logic::RewardAccumulation> rewAcc;
                if (cumulativeRewardFormula.hasRewardAccumulation()) {
                    rewAcc = cumulativeRewardFormula.getRewardAccumulation();
                }
                storm::logic::TimeBound timeBound(cumulativeRewardFormula.isBoundStrict(), cumulativeRewardFormula.getBound());
                auto newCumulativeRewardFormula =
                    std::make_shared<storm::logic::CumulativeRewardFormula const>(timeBound, cumulativeRewardFormula.getTimeBoundReference(), rewAcc);
                simplifiedFormula = std::make_shared<storm::logic::RewardOperatorFormula const>(newCumulativeRewardFormula, rewardModelName.value(),
                                                                                                operatorFormula.getOperatorInformation());
                // neither state elimination nor end component elimination preserve step-bounded formulas.
            }
        }
    }

    // If we have not set a simplified formula yet, we do so now by taking the original formula.
    if (!simplifiedFormula) {
        simplifiedFormula = formula.clone();
    }

    // Perform the actual simplifications.
    // First merge equivalent states based on the formula (e.g. prob0/1 states for until probabilities)
    storm::transformer::GoalStateMerger<typename SparseModelType::ValueType> goalStateMerger(originalModel);
    auto mergerResult = goalStateMerger.mergeForFormula(formula, true);
    if (!mergerResult) {
        STORM_LOG_DEBUG("Simplification not possible because the goal state merger does not support it.");
        return false;
    }
    simplifiedModel = mergerResult->model->template as<SparseModelType>();

    // Next do state elimination and end component elimination.
    std::string bottomLabel = "";
    if (enableEndComponentElimination) {
        // Insert a bottom label *before* doing state elimination. This is so that we still know what the sink/target states are after elimination.
        storm::storage::BitVector bottomStates(simplifiedModel->getNumberOfStates(), false);
        if (mergerResult->targetState) {
            bottomStates.set(*mergerResult->targetState, true);
        }
        if (mergerResult->sinkState) {
            bottomStates.set(*mergerResult->sinkState, true);
        }
        bottomLabel = simplifiedModel->getStateLabeling().addUniqueLabel("bottom", std::move(bottomStates));
    }
    if (enableStateElimination) {
        // Eliminate all states for which all outgoing transitions are constant
        storm::storage::BitVector considerForElimination = ~simplifiedModel->getInitialStates();
        if (mergerResult->targetState) {
            considerForElimination.set(*mergerResult->targetState, false);
        }
        if (mergerResult->sinkState) {
            considerForElimination.set(*mergerResult->sinkState, false);
        }
        simplifiedModel = eliminateConstantDeterministicStates(*simplifiedModel, preserveParametricTransitions, considerForElimination, rewardModelName);
    }
    if (enableEndComponentElimination) {
        // Eliminate the end components that do not contain a target or a sink state
        simplifiedModel = eliminateNeutralEndComponents(*simplifiedModel, simplifiedModel->getStates(bottomLabel), rewardModelName);
    }

    return true;  // simplification successful
}

template<typename SparseModelType>
std::shared_ptr<SparseModelType> SparseParametricModelSimplifier<SparseModelType>::getSimplifiedModel() const {
    STORM_LOG_THROW(simplifiedModel, storm::exceptions::InvalidStateException, "Tried to get the simplified model but simplification was not invoked before.");
    return simplifiedModel;
}

template<typename SparseModelType>
std::shared_ptr<storm::logic::Formula const> SparseParametricModelSimplifier<SparseModelType>::getSimplifiedFormula() const {
    STORM_LOG_THROW(simplifiedFormula, storm::exceptions::InvalidStateException,
                    "Tried to get the simplified formula but simplification was not invoked before.");
    return simplifiedFormula;
}

template<typename SparseModelType>
void SparseParametricModelSimplifier<SparseModelType>::setPreserveParametricTransitions(bool preserveParametricTransitions) {
    this->preserveParametricTransitions = preserveParametricTransitions;
}

template<typename SparseModelType>
bool SparseParametricModelSimplifier<SparseModelType>::isPreserveParametricTransitionsSet() const {
    return this->preserveParametricTransitions;
}

template class SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>;
template class SparseParametricModelSimplifier<storm::models::sparse::Mdp<storm::RationalFunction>>;
}  // namespace transformer
}  // namespace storm
