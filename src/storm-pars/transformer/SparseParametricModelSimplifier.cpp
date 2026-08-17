#include "storm-pars/transformer/SparseParametricModelSimplifier.h"
#include <memory>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/solver/stateelimination/NondeterministicModelStateEliminator.h"
#include "storm/storage/FlexibleSparseMatrix.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {

template<typename SparseModelType>
SparseParametricModelSimplifier<SparseModelType>::SparseParametricModelSimplifier(SparseModelType const& model) : originalModel(model) {
    // intentionally left empty
}

template<typename SparseModelType>
bool SparseParametricModelSimplifier<SparseModelType>::simplify(storm::logic::Formula const& formula) {
    // Make sure that there is no old result from a previous call
    simplifiedModel = nullptr;
    simplifiedFormula = nullptr;
    if (formula.isProbabilityOperatorFormula()) {
        storm::logic::ProbabilityOperatorFormula const& probOpForm = formula.asProbabilityOperatorFormula();
        if (probOpForm.getSubformula().isUntilFormula()) {
            return simplifyForUntilProbabilities(probOpForm);
        } else if (probOpForm.getSubformula().isReachabilityProbabilityFormula()) {
            return simplifyForReachabilityProbabilities(probOpForm);
        } else if (probOpForm.getSubformula().isBoundedUntilFormula()) {
            return simplifyForBoundedUntilProbabilities(probOpForm);
        }
    } else if (formula.isRewardOperatorFormula()) {
        storm::logic::RewardOperatorFormula rewOpForm = formula.asRewardOperatorFormula();
        STORM_LOG_THROW((rewOpForm.hasRewardModelName() && originalModel.hasRewardModel(rewOpForm.getRewardModelName())) ||
                            (!rewOpForm.hasRewardModelName() && originalModel.hasUniqueRewardModel()),
                        storm::exceptions::InvalidPropertyException,
                        "The reward model specified by formula " << formula << " is not available in the given model.");
        if (rewOpForm.getSubformula().isReachabilityRewardFormula()) {
            return simplifyForReachabilityRewards(rewOpForm);
        } else if (rewOpForm.getSubformula().isCumulativeRewardFormula()) {
            return simplifyForCumulativeRewards(rewOpForm);
        }
    }
    // reaching this point means that the provided formula is not supported. Thus, no simplification is possible.
    STORM_LOG_DEBUG("Simplification not possible because the formula is not supported. Formula: " << formula);
    return false;
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
bool SparseParametricModelSimplifier<SparseModelType>::simplifyForUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula) {
    // If this method was not overridden by any subclass, simplification is not possible
    STORM_LOG_DEBUG("Simplification not possible because the formula is not supported. Formula: " << formula);
    return false;
}

template<typename SparseModelType>
bool SparseParametricModelSimplifier<SparseModelType>::simplifyForReachabilityProbabilities(storm::logic::ProbabilityOperatorFormula const& formula) {
    // transform to until formula
    auto untilFormula = std::make_shared<storm::logic::UntilFormula const>(storm::logic::Formula::getTrueFormula(),
                                                                           formula.getSubformula().asEventuallyFormula().getSubformula().asSharedPointer());
    return simplifyForUntilProbabilities(storm::logic::ProbabilityOperatorFormula(untilFormula, formula.getOperatorInformation()));
}

template<typename SparseModelType>
bool SparseParametricModelSimplifier<SparseModelType>::simplifyForBoundedUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula) {
    // If this method was not overridden by any subclass, simplification is not possible
    STORM_LOG_DEBUG("Simplification not possible because the formula is not supported. Formula: " << formula);
    return false;
}

template<typename SparseModelType>
bool SparseParametricModelSimplifier<SparseModelType>::simplifyForReachabilityRewards(storm::logic::RewardOperatorFormula const& formula) {
    // If this method was not overridden by any subclass, simplification is not possible
    STORM_LOG_DEBUG("Simplification not possible because the formula is not supported. Formula: " << formula);
    return false;
}

template<typename SparseModelType>
bool SparseParametricModelSimplifier<SparseModelType>::simplifyForCumulativeRewards(storm::logic::RewardOperatorFormula const& formula) {
    // If this method was not overridden by any subclass, simplification is not possible
    STORM_LOG_DEBUG("Simplification not possible because the formula is not supported. Formula: " << formula);
    return false;
}

template<typename SparseModelType>
std::shared_ptr<SparseModelType> SparseParametricModelSimplifier<SparseModelType>::eliminateConstantDeterministicStates(
    SparseModelType const& model, storm::storage::BitVector const& consideredStates, boost::optional<std::string> const& rewardModelName) {
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
            (!rewardModelName.is_initialized() || storm::utility::isConstant(actionRewards[sparseMatrix.getRowGroupIndices()[state]]))) {
            for (auto const& entry : sparseMatrix.getRowGroup(state)) {
                if (!storm::utility::isConstant(entry.getValue())) {
                    selectedStates.set(state, false);
                    break;
                }
            }
            if (state && this->preserveParametricTransitions) {
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
