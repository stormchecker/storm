#include "storm/modelchecker/multiobjective/constraintbased/SparseCbAchievabilityQuery.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/constants.h"
#include "storm/utility/solver.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<class SparseModelType>
SparseCbAchievabilityQuery<SparseModelType>::SparseCbAchievabilityQuery(
    preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult)
    : SparseCbQuery<SparseModelType>(preprocessorResult) {
    STORM_LOG_ASSERT(preprocessorResult.queryType == preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>::QueryType::Achievability,
                     "Invalid query Type");
    solver = storm::utility::solver::SmtSolverFactory().create(*this->expressionManager);
}

template<class SparseModelType>
std::unique_ptr<CheckResult> SparseCbAchievabilityQuery<SparseModelType>::check(Environment const& env) {
    bool result = this->checkAchievability();

    return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<ValueType>(this->originalModel.getInitialStates().getNextSetIndex(0), result));
}

template<class SparseModelType>
bool SparseCbAchievabilityQuery<SparseModelType>::checkAchievability() {
    STORM_LOG_INFO("Building constraint system to check achievability.");
    // this->preprocessedModel->writeDotToStream(std::cout);
    storm::utility::Stopwatch swInitialization(true);
    initializeConstraintSystem();
    STORM_LOG_INFO("Constraint system consists of " << expectedChoiceVariables.size() << " + " << bottomStateVariables.size() << " variables");
    addObjectiveConstraints();
    swInitialization.stop();

    storm::utility::Stopwatch swCheck(true);
    STORM_LOG_INFO("Invoking SMT Solver.");
    storm::solver::SmtSolver::CheckResult result = solver->check();
    swCheck.stop();

    STORM_LOG_STATISTICS("Building the constraintsystem took " << swInitialization << " seconds and checking the SMT formula took " << swCheck
                                                               << " seconds.\n");

    switch (result) {
        case storm::solver::SmtSolver::CheckResult::Sat:
            // std::cout << "\nSatisfying assignment: \n" << solver->getModelAsValuation().toString(true) << '\n';
            return true;
        case storm::solver::SmtSolver::CheckResult::Unsat:
            // std::cout << "\nUnsatisfiability core: {\n";
            // for (auto const& expr : solver->getUnsatCore()) {
            //    std::cout << "\t " << expr << '\n';
            // }
            // std::cout << "}\n";
            return false;
        default:
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "SMT solver yielded an unexpected result.");
    }

    return false;
}

template<class SparseModelType>
void SparseCbAchievabilityQuery<SparseModelType>::initializeConstraintSystem() {
    uint_fast64_t numStates = this->preprocessedModel->getNumberOfStates();
    uint_fast64_t numChoices = this->preprocessedModel->getNumberOfChoices();
    uint_fast64_t numBottomStates = this->reward0EStates.getNumberOfSetBits();
    STORM_LOG_THROW(numBottomStates > 0, storm::exceptions::UnexpectedException, "No bottom states in the preprocessed model.");
    storm::expressions::Expression zero = this->expressionManager->rational(storm::utility::zero<ValueType>());
    storm::expressions::Expression one = this->expressionManager->rational(storm::utility::one<ValueType>());

    // Declare the variables for the choices and bottom states
    expectedChoiceVariables.reserve(numChoices);
    for (uint_fast64_t choice = 0; choice < numChoices; ++choice) {
        expectedChoiceVariables.push_back(this->expressionManager->declareRationalVariable("y" + std::to_string(choice)));
    }
    bottomStateVariables.reserve(numBottomStates);
    for (uint_fast64_t bottomState = 0; bottomState < numBottomStates; ++bottomState) {
        bottomStateVariables.push_back(this->expressionManager->declareRationalVariable("z" + std::to_string(bottomState)));
    }

    // assert that the values are greater zero and that the bottom state values sum up to one
    for (auto& var : expectedChoiceVariables) {
        solver->add(var.getExpression() >= zero);
    }
    std::vector<storm::expressions::Expression> bottomStateVarsAsExpression;
    bottomStateVarsAsExpression.reserve(bottomStateVariables.size());
    for (auto& var : bottomStateVariables) {
        solver->add(var.getExpression() >= zero);
        bottomStateVarsAsExpression.push_back(var.getExpression());
    }
    auto bottomStateSum = storm::expressions::sum(bottomStateVarsAsExpression).simplify();
    solver->add(bottomStateSum == one);

    // assert that the "incoming" value of each state equals the "outgoing" value
    storm::storage::SparseMatrix<ValueType> backwardsTransitions = this->preprocessedModel->getTransitionMatrix().transpose();
    auto bottomStateVariableIt = bottomStateVariables.begin();
    std::vector<storm::expressions::Expression> valueSummands;  // initialization here to avoid re-allocations
    for (uint_fast64_t state = 0; state < numStates; ++state) {
        // get the "incomming" value
        valueSummands.clear();
        if (this->preprocessedModel->getInitialStates().get(state)) {
            valueSummands.push_back(one);
        }
        for (auto const& backwardsEntry : backwardsTransitions.getRow(state)) {
            valueSummands.push_back(this->expressionManager->rational(backwardsEntry.getValue()) *
                                    expectedChoiceVariables[backwardsEntry.getColumn()].getExpression());
        }

        // subtract the "outgoing" value
        for (uint_fast64_t choice = this->preprocessedModel->getTransitionMatrix().getRowGroupIndices()[state];
             choice < this->preprocessedModel->getTransitionMatrix().getRowGroupIndices()[state + 1]; ++choice) {
            valueSummands.push_back(-expectedChoiceVariables[choice]);
        }
        if (this->reward0EStates.get(state)) {
            valueSummands.push_back(-(*bottomStateVariableIt));
            ++bottomStateVariableIt;
        }
        solver->add(storm::expressions::sum(valueSummands) == zero);
    }
    assert(bottomStateVariableIt == bottomStateVariables.end());
}

template<class SparseModelType>
void SparseCbAchievabilityQuery<SparseModelType>::addObjectiveConstraints() {
    storm::expressions::Expression zero = this->expressionManager->rational(storm::utility::zero<ValueType>());
    for (Objective<ValueType> const& obj : this->objectives) {
        STORM_LOG_THROW(obj.formula->isRewardOperatorFormula() && obj.formula->getSubformula().isTotalRewardFormula(),
                        storm::exceptions::InvalidOperationException,
                        "Constraint-based solver only supports total-reward objectives. Got " << *obj.formula << " instead.");
        STORM_LOG_THROW(obj.formula->hasBound(), storm::exceptions::InvalidOperationException,
                        "Invoked achievability query but no bound was specified for at least one objective.");
        STORM_LOG_THROW(obj.formula->asRewardOperatorFormula().hasRewardModelName(), storm::exceptions::InvalidOperationException,
                        "Expected reward operator with a reward model name. Got " << *obj.formula << " instead.");
        std::vector<ValueType> rewards = getActionBasedExpectedRewards(obj.formula->asRewardOperatorFormula().getRewardModelName());

        // Get the sum of all objective values
        std::vector<storm::expressions::Expression> objectiveValues;
        for (uint_fast64_t choice = 0; choice < rewards.size(); ++choice) {
            if (!storm::utility::isZero(rewards[choice])) {
                objectiveValues.push_back(this->expressionManager->rational(rewards[choice]) * expectedChoiceVariables[choice].getExpression());
            }
        }
        if (objectiveValues.empty()) {
            objectiveValues.push_back(this->expressionManager->rational(storm::utility::zero<storm::RationalNumber>()));
        }
        auto objValue = storm::expressions::sum(objectiveValues).simplify();

        // We need to actually evaluate the threshold as rational number. Otherwise a threshold like '<=16/9' might be considered as 1 due to integer division
        storm::expressions::Expression threshold = this->expressionManager->rational(obj.formula->getThreshold().evaluateAsRational());
        switch (obj.formula->getBound().comparisonType) {
            case storm::logic::ComparisonType::Greater:
                solver->add(objValue > threshold);
                break;
            case storm::logic::ComparisonType::GreaterEqual:
                solver->add(objValue >= threshold);
                break;
            case storm::logic::ComparisonType::Less:
                solver->add(objValue < threshold);
                break;
            case storm::logic::ComparisonType::LessEqual:
                solver->add(objValue <= threshold);
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "One or more objectives have an invalid comparison type.");
        }
    }
}

template<>
std::vector<double> SparseCbAchievabilityQuery<storm::models::sparse::Mdp<double>>::getActionBasedExpectedRewards(std::string const& rewardModelName) const {
    return this->preprocessedModel->getRewardModel(rewardModelName).getTotalRewardVector(this->preprocessedModel->getTransitionMatrix());
}

template<>
std::vector<double> SparseCbAchievabilityQuery<storm::models::sparse::MarkovAutomaton<double>>::getActionBasedExpectedRewards(
    std::string const& rewardModelName) const {
    auto const& rewModel = this->preprocessedModel->getRewardModel(rewardModelName);
    STORM_LOG_ASSERT(!rewModel.hasTransitionRewards(), "Preprocessed Reward model has transition rewards which is not expected.");
    std::vector<double> result = rewModel.hasStateActionRewards()
                                     ? rewModel.getStateActionRewardVector()
                                     : std::vector<double>(this->preprocessedModel->getNumberOfChoices(), storm::utility::zero<ValueType>());
    if (rewModel.hasStateRewards()) {
        // Note that state rewards are earned over time and thus play no role for probabilistic states
        for (auto markovianState : this->preprocessedModel->getMarkovianStates()) {
            result[this->preprocessedModel->getTransitionMatrix().getRowGroupIndices()[markovianState]] +=
                rewModel.getStateReward(markovianState) / this->preprocessedModel->getExitRate(markovianState);
        }
    }
    return result;
}

template<>
std::vector<storm::RationalNumber> SparseCbAchievabilityQuery<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::getActionBasedExpectedRewards(
    std::string const& rewardModelName) const {
    auto const& rewModel = this->preprocessedModel->getRewardModel(rewardModelName);
    STORM_LOG_ASSERT(!rewModel.hasTransitionRewards(), "Preprocessed Reward model has transition rewards which is not expected.");
    std::vector<storm::RationalNumber> result =
        rewModel.hasStateActionRewards() ? rewModel.getStateActionRewardVector()
                                         : std::vector<storm::RationalNumber>(this->preprocessedModel->getNumberOfChoices(), storm::utility::zero<ValueType>());
    if (rewModel.hasStateRewards()) {
        // Note that state rewards are earned over time and thus play no role for probabilistic states
        for (auto markovianState : this->preprocessedModel->getMarkovianStates()) {
            result[this->preprocessedModel->getTransitionMatrix().getRowGroupIndices()[markovianState]] +=
                rewModel.getStateReward(markovianState) / this->preprocessedModel->getExitRate(markovianState);
        }
    }
    return result;
}

template<>
std::vector<storm::RationalNumber> SparseCbAchievabilityQuery<storm::models::sparse::Mdp<storm::RationalNumber>>::getActionBasedExpectedRewards(
    std::string const& rewardModelName) const {
    return this->preprocessedModel->getRewardModel(rewardModelName).getTotalRewardVector(this->preprocessedModel->getTransitionMatrix());
}

template class SparseCbAchievabilityQuery<storm::models::sparse::Mdp<double>>;
template class SparseCbAchievabilityQuery<storm::models::sparse::MarkovAutomaton<double>>;

template class SparseCbAchievabilityQuery<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class SparseCbAchievabilityQuery<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
