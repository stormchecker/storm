#include "storm/modelchecker/multiobjective/constraintbased/SparseCbQuery.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectiveRewardAnalysis.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/transformer/GoalStateMerger.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<class SparseModelType>
SparseCbQuery<SparseModelType>::SparseCbQuery(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult)
    : originalModel(preprocessorResult.originalModel),
      originalFormula(preprocessorResult.originalFormula),
      objectives(std::move(preprocessorResult.objectives)) {
    auto rewardAnalysis = preprocessing::SparseMultiObjectiveRewardAnalysis<SparseModelType>::analyze(preprocessorResult);
    STORM_LOG_THROW(rewardAnalysis.rewardFinitenessType != preprocessing::RewardFinitenessType::Infinite, storm::exceptions::NotSupportedException,
                    "TThere is no Pareto optimal scheduler that yields finite reward for all objectives. This is not supported.");

    STORM_LOG_THROW(rewardAnalysis.totalRewardLessInfinityEStates, storm::exceptions::UnexpectedException,
                    "The set of states with reward < infinity for some scheduler has not been computed during preprocessing.");
    STORM_LOG_THROW(!preprocessorResult.containsRewardBoundedObjective(), storm::exceptions::NotSupportedException,
                    "At least one objective was not reduced to an expected (total or cumulative) reward objective during preprocessing. This is not supported "
                    "by the considered weight vector checker.");
    STORM_LOG_THROW(preprocessorResult.preprocessedModel->getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "The model has multiple initial states.");

    // Build a subsystem of the preprocessor result model that discards states that yield infinite reward for all schedulers.
    // We can also merge the states that will have reward zero anyway.
    storm::storage::BitVector maybeStates = rewardAnalysis.totalRewardLessInfinityEStates.get() & ~rewardAnalysis.reward0AStates;
    std::set<std::string> relevantRewardModels;
    for (auto const& obj : this->objectives) {
        obj.formula->gatherReferencedRewardModels(relevantRewardModels);
    }
    storm::transformer::GoalStateMerger<SparseModelType> merger(*preprocessorResult.preprocessedModel);
    auto mergerResult = merger.mergeTargetAndSinkStates(maybeStates, rewardAnalysis.reward0AStates, storm::storage::BitVector(maybeStates.size(), false),
                                                        std::vector<std::string>(relevantRewardModels.begin(), relevantRewardModels.end()));

    preprocessedModel = mergerResult.model;
    reward0EStates = rewardAnalysis.totalReward0EStates % maybeStates;
    if (mergerResult.targetState) {
        // There is an additional state in the result
        reward0EStates.resize(reward0EStates.size() + 1, true);
    }
    expressionManager = std::make_shared<storm::expressions::ExpressionManager>();
}

template class SparseCbQuery<storm::models::sparse::Mdp<double>>;
template class SparseCbQuery<storm::models::sparse::MarkovAutomaton<double>>;

template class SparseCbQuery<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class SparseCbQuery<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
