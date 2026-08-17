#include "SymbolicToSparseTransformer.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/AtomicExpressionFormula.h"
#include "storm/logic/AtomicLabelFormula.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/macros.h"

namespace storm {
namespace transformer {

struct LabelInformation {
    LabelInformation(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
        for (auto const& formula : formulas) {
            std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> atomicLabelFormulas = formula->getAtomicLabelFormulas();
            for (auto const& labelFormula : atomicLabelFormulas) {
                atomicLabels.insert(labelFormula->getLabel());
            }

            std::vector<std::shared_ptr<storm::logic::AtomicExpressionFormula const>> atomicExpressionFormulas = formula->getAtomicExpressionFormulas();
            for (auto const& expressionFormula : atomicExpressionFormulas) {
                std::stringstream ss;
                ss << expressionFormula->getExpression();
                expressionLabels[ss.str()] = expressionFormula->getExpression();
            }
        }
    }

    std::set<std::string> atomicLabels;
    std::map<std::string, storm::expressions::Expression> expressionLabels;
};

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> SymbolicDtmcToSparseDtmcTransformer<Type, ValueType>::translate(
    storm::models::symbolic::Dtmc<Type, ValueType> const& symbolicDtmc, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    this->odd = symbolicDtmc.getReachableStates().createOdd();
    storm::storage::SparseMatrix<ValueType> transitionMatrix = symbolicDtmc.getTransitionMatrix().toMatrix(this->odd, this->odd);
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> rewardModels;
    for (auto const& rewardModelNameAndModel : symbolicDtmc.getRewardModels()) {
        std::optional<std::vector<ValueType>> stateRewards;
        std::optional<std::vector<ValueType>> stateActionRewards;
        std::optional<storm::storage::SparseMatrix<ValueType>> transitionRewards;
        if (rewardModelNameAndModel.second.hasStateRewards()) {
            stateRewards = rewardModelNameAndModel.second.getStateRewardVector().toVector(this->odd);
        }
        if (rewardModelNameAndModel.second.hasStateActionRewards()) {
            stateActionRewards = rewardModelNameAndModel.second.getStateActionRewardVector().toVector(this->odd);
        }
        if (rewardModelNameAndModel.second.hasTransitionRewards()) {
            transitionRewards = rewardModelNameAndModel.second.getTransitionRewardMatrix().toMatrix(this->odd, this->odd);
        }
        rewardModels.emplace(rewardModelNameAndModel.first,
                             storm::models::sparse::StandardRewardModel<ValueType>(stateRewards, stateActionRewards, transitionRewards));
    }
    storm::models::sparse::StateLabeling labelling(transitionMatrix.getRowGroupCount());

    labelling.addLabel("init", symbolicDtmc.getInitialStates().toVector(this->odd));
    labelling.addLabel("deadlock", symbolicDtmc.getDeadlockStates().toVector(this->odd));
    if (formulas.empty()) {
        for (auto const& label : symbolicDtmc.getLabels()) {
            labelling.addLabel(label, symbolicDtmc.getStates(label).toVector(this->odd));
        }
    } else {
        LabelInformation labelInfo(formulas);
        for (auto const& label : labelInfo.atomicLabels) {
            labelling.addLabel(label, symbolicDtmc.getStates(label).toVector(this->odd));
        }
        for (auto const& expressionLabel : labelInfo.expressionLabels) {
            labelling.addLabel(expressionLabel.first, symbolicDtmc.getStates(expressionLabel.second).toVector(this->odd));
        }
    }
    return std::make_shared<storm::models::sparse::Dtmc<ValueType>>(transitionMatrix, labelling, rewardModels);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Odd const& SymbolicDtmcToSparseDtmcTransformer<Type, ValueType>::getOdd() const {
    return this->odd;
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::sparse::Mdp<ValueType>> SymbolicMdpToSparseMdpTransformer<Type, ValueType>::translate(
    storm::models::symbolic::Mdp<Type, ValueType> const& symbolicMdp, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    storm::dd::Odd odd = symbolicMdp.getReachableStates().createOdd();

    // Collect action reward vectors that need translation
    std::vector<storm::dd::Add<Type, ValueType>> symbolicActionRewardVectors;
    std::map<std::string, uint64_t> rewardNameToActionRewardIndexMap;
    for (auto const& rewardModelNameAndModel : symbolicMdp.getRewardModels()) {
        if (rewardModelNameAndModel.second.hasStateActionRewards()) {
            rewardNameToActionRewardIndexMap.emplace(rewardModelNameAndModel.first, symbolicActionRewardVectors.size());
            symbolicActionRewardVectors.push_back(rewardModelNameAndModel.second.getStateActionRewardVector());
        }
    }
    // Build transition matrix and (potentially) actionRewardVectors.
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
    std::vector<std::vector<ValueType>> actionRewardVectors;
    if (symbolicActionRewardVectors.empty()) {
        transitionMatrix = symbolicMdp.getTransitionMatrix().toMatrix(symbolicMdp.getNondeterminismVariables(), odd, odd);
    } else {
        auto matrRewards = symbolicMdp.getTransitionMatrix().toMatrixVectors(symbolicActionRewardVectors, symbolicMdp.getNondeterminismVariables(), odd, odd);
        transitionMatrix = std::move(matrRewards.first);
        actionRewardVectors = std::move(matrRewards.second);
    }

    // Translate reward models
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> rewardModels;
    for (auto const& rewardModelNameAndModel : symbolicMdp.getRewardModels()) {
        std::optional<std::vector<ValueType>> stateRewards;
        std::optional<std::vector<ValueType>> stateActionRewards;
        std::optional<storm::storage::SparseMatrix<ValueType>> transitionRewards;
        if (rewardModelNameAndModel.second.hasStateRewards()) {
            stateRewards = rewardModelNameAndModel.second.getStateRewardVector().toVector(odd);
        }
        auto actRewIndexIt = rewardNameToActionRewardIndexMap.find(rewardModelNameAndModel.first);
        if (actRewIndexIt != rewardNameToActionRewardIndexMap.end()) {
            stateActionRewards = std::move(actionRewardVectors[actRewIndexIt->second]);
        }
        STORM_LOG_THROW(!rewardModelNameAndModel.second.hasTransitionRewards(), storm::exceptions::NotImplementedException,
                        "Translation of symbolic to explicit transition rewards is not yet supported.");
        rewardModels.emplace(rewardModelNameAndModel.first,
                             storm::models::sparse::StandardRewardModel<ValueType>(stateRewards, stateActionRewards, transitionRewards));
    }

    storm::models::sparse::StateLabeling labelling(transitionMatrix.getRowGroupCount());

    labelling.addLabel("init", symbolicMdp.getInitialStates().toVector(odd));
    labelling.addLabel("deadlock", symbolicMdp.getDeadlockStates().toVector(odd));
    if (formulas.empty()) {
        for (auto const& label : symbolicMdp.getLabels()) {
            labelling.addLabel(label, symbolicMdp.getStates(label).toVector(odd));
        }
    } else {
        LabelInformation labelInfo(formulas);
        for (auto const& label : labelInfo.atomicLabels) {
            labelling.addLabel(label, symbolicMdp.getStates(label).toVector(odd));
        }
        for (auto const& expressionLabel : labelInfo.expressionLabels) {
            labelling.addLabel(expressionLabel.first, symbolicMdp.getStates(expressionLabel.second).toVector(odd));
        }
    }

    return std::make_shared<storm::models::sparse::Mdp<ValueType>>(transitionMatrix, labelling, rewardModels);
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> SymbolicCtmcToSparseCtmcTransformer<Type, ValueType>::translate(
    storm::models::symbolic::Ctmc<Type, ValueType> const& symbolicCtmc, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    storm::dd::Odd odd = symbolicCtmc.getReachableStates().createOdd();
    storm::storage::SparseMatrix<ValueType> transitionMatrix = symbolicCtmc.getTransitionMatrix().toMatrix(odd, odd);
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> rewardModels;
    for (auto const& rewardModelNameAndModel : symbolicCtmc.getRewardModels()) {
        std::optional<std::vector<ValueType>> stateRewards;
        std::optional<std::vector<ValueType>> stateActionRewards;
        std::optional<storm::storage::SparseMatrix<ValueType>> transitionRewards;
        if (rewardModelNameAndModel.second.hasStateRewards()) {
            stateRewards = rewardModelNameAndModel.second.getStateRewardVector().toVector(odd);
        }
        if (rewardModelNameAndModel.second.hasStateActionRewards()) {
            stateActionRewards = rewardModelNameAndModel.second.getStateActionRewardVector().toVector(odd);
        }
        if (rewardModelNameAndModel.second.hasTransitionRewards()) {
            transitionRewards = rewardModelNameAndModel.second.getTransitionRewardMatrix().toMatrix(odd, odd);
        }
        rewardModels.emplace(rewardModelNameAndModel.first,
                             storm::models::sparse::StandardRewardModel<ValueType>(stateRewards, stateActionRewards, transitionRewards));
    }
    storm::models::sparse::StateLabeling labelling(transitionMatrix.getRowGroupCount());

    labelling.addLabel("init", symbolicCtmc.getInitialStates().toVector(odd));
    labelling.addLabel("deadlock", symbolicCtmc.getDeadlockStates().toVector(odd));
    if (formulas.empty()) {
        for (auto const& label : symbolicCtmc.getLabels()) {
            labelling.addLabel(label, symbolicCtmc.getStates(label).toVector(odd));
        }
    } else {
        LabelInformation labelInfo(formulas);
        for (auto const& label : labelInfo.atomicLabels) {
            labelling.addLabel(label, symbolicCtmc.getStates(label).toVector(odd));
        }
        for (auto const& expressionLabel : labelInfo.expressionLabels) {
            labelling.addLabel(expressionLabel.first, symbolicCtmc.getStates(expressionLabel.second).toVector(odd));
        }
    }

    return std::make_shared<storm::models::sparse::Ctmc<ValueType>>(transitionMatrix, labelling, rewardModels);
}

template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> SymbolicMaToSparseMaTransformer<Type, ValueType>::translate(
    storm::models::symbolic::MarkovAutomaton<Type, ValueType> const& symbolicMa, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    storm::dd::Odd odd = symbolicMa.getReachableStates().createOdd();
    // Collect action reward vectors that need translation
    std::vector<storm::dd::Add<Type, ValueType>> symbolicActionRewardVectors;
    std::map<std::string, uint64_t> rewardNameToActionRewardIndexMap;
    for (auto const& rewardModelNameAndModel : symbolicMa.getRewardModels()) {
        if (rewardModelNameAndModel.second.hasStateActionRewards()) {
            rewardNameToActionRewardIndexMap.emplace(rewardModelNameAndModel.first, symbolicActionRewardVectors.size());
            symbolicActionRewardVectors.push_back(rewardModelNameAndModel.second.getStateActionRewardVector());
        }
    }
    // Build transition matrix and (potentially) actionRewardVectors.
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
    std::vector<std::vector<ValueType>> actionRewardVectors;
    if (symbolicActionRewardVectors.empty()) {
        transitionMatrix = symbolicMa.getTransitionMatrix().toMatrix(symbolicMa.getNondeterminismVariables(), odd, odd);
    } else {
        auto matrRewards = symbolicMa.getTransitionMatrix().toMatrixVectors(symbolicActionRewardVectors, symbolicMa.getNondeterminismVariables(), odd, odd);
        transitionMatrix = std::move(matrRewards.first);
        actionRewardVectors = std::move(matrRewards.second);
    }

    // Translate reward models
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> rewardModels;
    for (auto const& rewardModelNameAndModel : symbolicMa.getRewardModels()) {
        std::optional<std::vector<ValueType>> stateRewards;
        std::optional<std::vector<ValueType>> stateActionRewards;
        std::optional<storm::storage::SparseMatrix<ValueType>> transitionRewards;
        if (rewardModelNameAndModel.second.hasStateRewards()) {
            stateRewards = rewardModelNameAndModel.second.getStateRewardVector().toVector(odd);
        }
        auto actRewIndexIt = rewardNameToActionRewardIndexMap.find(rewardModelNameAndModel.first);
        if (actRewIndexIt != rewardNameToActionRewardIndexMap.end()) {
            stateActionRewards = std::move(actionRewardVectors[actRewIndexIt->second]);
        }
        STORM_LOG_THROW(!rewardModelNameAndModel.second.hasTransitionRewards(), storm::exceptions::NotImplementedException,
                        "Translation of symbolic to explicit transition rewards is not yet supported.");
        rewardModels.emplace(rewardModelNameAndModel.first,
                             storm::models::sparse::StandardRewardModel<ValueType>(stateRewards, stateActionRewards, transitionRewards));
    }

    storm::models::sparse::StateLabeling labelling(transitionMatrix.getRowGroupCount());

    labelling.addLabel("init", symbolicMa.getInitialStates().toVector(odd));
    labelling.addLabel("deadlock", symbolicMa.getDeadlockStates().toVector(odd));
    if (formulas.empty()) {
        for (auto const& label : symbolicMa.getLabels()) {
            labelling.addLabel(label, symbolicMa.getStates(label).toVector(odd));
        }
    } else {
        LabelInformation labelInfo(formulas);
        for (auto const& label : labelInfo.atomicLabels) {
            labelling.addLabel(label, symbolicMa.getStates(label).toVector(odd));
        }
        for (auto const& expressionLabel : labelInfo.expressionLabels) {
            labelling.addLabel(expressionLabel.first, symbolicMa.getStates(expressionLabel.second).toVector(odd));
        }
    }
    storm::storage::BitVector markovianStates = symbolicMa.getMarkovianStates().toVector(odd);
    storm::storage::sparse::ModelComponents<ValueType> components(std::move(transitionMatrix), std::move(labelling), std::move(rewardModels), false,
                                                                  std::move(markovianStates));
    components.exitRates = symbolicMa.getExitRateVector().toVector(odd);

    return std::make_shared<storm::models::sparse::MarkovAutomaton<ValueType>>(std::move(components));
}

template class SymbolicDtmcToSparseDtmcTransformer<storm::dd::DdType::CUDD, double>;
template class SymbolicDtmcToSparseDtmcTransformer<storm::dd::DdType::Sylvan, double>;
template class SymbolicDtmcToSparseDtmcTransformer<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SymbolicDtmcToSparseDtmcTransformer<storm::dd::DdType::Sylvan, storm::RationalFunction>;

template class SymbolicMdpToSparseMdpTransformer<storm::dd::DdType::CUDD, double>;
template class SymbolicMdpToSparseMdpTransformer<storm::dd::DdType::Sylvan, double>;
template class SymbolicMdpToSparseMdpTransformer<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SymbolicMdpToSparseMdpTransformer<storm::dd::DdType::Sylvan, storm::RationalFunction>;

template class SymbolicCtmcToSparseCtmcTransformer<storm::dd::DdType::CUDD, double>;
template class SymbolicCtmcToSparseCtmcTransformer<storm::dd::DdType::Sylvan, double>;
template class SymbolicCtmcToSparseCtmcTransformer<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SymbolicCtmcToSparseCtmcTransformer<storm::dd::DdType::Sylvan, storm::RationalFunction>;

template class SymbolicMaToSparseMaTransformer<storm::dd::DdType::CUDD, double>;
template class SymbolicMaToSparseMaTransformer<storm::dd::DdType::Sylvan, double>;
template class SymbolicMaToSparseMaTransformer<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SymbolicMaToSparseMaTransformer<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace transformer
}  // namespace storm
