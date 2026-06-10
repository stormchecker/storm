#include "storm/modelchecker/AbstractModelChecker.h"

#include <boost/core/typeinfo.hpp>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/exceptions/InternalTypeErrorException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/FormulaInformation.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "storm/models/ModelRepresentation.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/Smg.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"
#include "storm/storage/dd/Add.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename ModelType>
std::string AbstractModelChecker<ModelType>::getClassName() const {
    return std::string(boost::core::demangled_name(BOOST_CORE_TYPEID(*this)));
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::check(CheckTask<storm::logic::Formula, SolutionType> const& checkTask) {
    Environment env;
    return this->check(env, checkTask);
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::check(Environment const& env, CheckTask<storm::logic::Formula, SolutionType> const& checkTask) {
    storm::logic::Formula const& formula = checkTask.getFormula();
    STORM_LOG_THROW(this->canHandle(checkTask), storm::exceptions::InvalidArgumentException,
                    "The model checker (" << getClassName() << ") is not able to check the formula '" << formula << "'.");
    if (formula.isStateFormula()) {
        return this->checkStateFormula(env, checkTask.substituteFormula(formula.asStateFormula()));
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The given formula '" << formula << "' is invalid.");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeProbabilities(Environment const& env,
                                                                                   CheckTask<storm::logic::Formula, SolutionType> const& checkTask) {
    storm::logic::Formula const& formula = checkTask.getFormula();

    if (formula.isStateFormula() || formula.hasQualitativeResult()) {
        return this->computeStateFormulaProbabilities(env, checkTask.substituteFormula(formula));
    }

    if (formula.isHOAPathFormula()) {
        return this->computeHOAPathProbabilities(env, checkTask.substituteFormula(formula.asHOAPathFormula()));
    } else if (formula.isConditionalProbabilityFormula()) {
        return this->computeConditionalProbabilities(env, checkTask.substituteFormula(formula.asConditionalFormula()));
    } else if (formula.info(false).containsComplexPathFormula()) {
        // we need to do LTL model checking
        return this->computeLTLProbabilities(env, checkTask.substituteFormula(formula.asPathFormula()));
    } else if (formula.isBoundedUntilFormula()) {
        return this->computeBoundedUntilProbabilities(env, checkTask.substituteFormula(formula.asBoundedUntilFormula()));
    } else if (formula.isReachabilityProbabilityFormula()) {
        return this->computeReachabilityProbabilities(env, checkTask.substituteFormula(formula.asReachabilityProbabilityFormula()));
    } else if (formula.isGloballyFormula()) {
        return this->computeGloballyProbabilities(env, checkTask.substituteFormula(formula.asGloballyFormula()));
    } else if (formula.isUntilFormula()) {
        return this->computeUntilProbabilities(env, checkTask.substituteFormula(formula.asUntilFormula()));
    } else if (formula.isNextFormula()) {
        return this->computeNextProbabilities(env, checkTask.substituteFormula(formula.asNextFormula()));
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The given formula '" << formula << "' is invalid.");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeBoundedUntilProbabilities(
    Environment const&, CheckTask<storm::logic::BoundedUntilFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeConditionalProbabilities(
    Environment const&, CheckTask<storm::logic::ConditionalFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeReachabilityProbabilities(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) {
    storm::logic::EventuallyFormula const& pathFormula = checkTask.getFormula();
    storm::logic::UntilFormula newFormula(storm::logic::Formula::getTrueFormula(), pathFormula.getSubformula().asSharedPointer());
    return this->computeUntilProbabilities(env, checkTask.substituteFormula(newFormula));
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeGloballyProbabilities(
    Environment const&, CheckTask<storm::logic::GloballyFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeNextProbabilities(Environment const&,
                                                                                       CheckTask<storm::logic::NextFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeUntilProbabilities(Environment const&,
                                                                                        CheckTask<storm::logic::UntilFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeHOAPathProbabilities(
    Environment const&, CheckTask<storm::logic::HOAPathFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "This model checker does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeLTLProbabilities(Environment const&,
                                                                                      CheckTask<storm::logic::PathFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "This model checker does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeStateFormulaProbabilities(
    Environment const& env, CheckTask<storm::logic::Formula, SolutionType> const& checkTask) {
    // recurse
    std::unique_ptr<CheckResult> resultPointer = this->check(env, checkTask.getFormula());
    if (resultPointer->isExplicitQualitativeCheckResult()) {
        STORM_LOG_ASSERT(ModelType::Representation == storm::models::ModelRepresentation::Sparse, "Unexpected model type.");
        return std::make_unique<ExplicitQuantitativeCheckResult<SolutionType>>(resultPointer->template asExplicitQualitativeCheckResult<SolutionType>());
    } else {
        STORM_LOG_ASSERT(resultPointer->isSymbolicQualitativeCheckResult(), "Unexpected result type.");
        STORM_LOG_ASSERT(ModelType::Representation != storm::models::ModelRepresentation::Sparse, "Unexpected model type.");
        auto const& qualRes = resultPointer->asSymbolicQualitativeCheckResult<storm::models::GetDdType<ModelType::Representation>::ddType>();
        return std::make_unique<SymbolicQuantitativeCheckResult<storm::models::GetDdType<ModelType::Representation>::ddType, SolutionType>>(qualRes);
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeRewards(Environment const& env,
                                                                             CheckTask<storm::logic::Formula, SolutionType> const& checkTask) {
    storm::logic::Formula const& rewardFormula = checkTask.getFormula();
    if (rewardFormula.isCumulativeRewardFormula()) {
        return this->computeCumulativeRewards(env, checkTask.substituteFormula(rewardFormula.asCumulativeRewardFormula()));
    } else if (rewardFormula.isDiscountedCumulativeRewardFormula()) {
        return this->computeDiscountedCumulativeRewards(env, checkTask.substituteFormula(rewardFormula.asDiscountedCumulativeRewardFormula()));
    } else if (rewardFormula.isInstantaneousRewardFormula()) {
        return this->computeInstantaneousRewards(env, checkTask.substituteFormula(rewardFormula.asInstantaneousRewardFormula()));
    } else if (rewardFormula.isReachabilityRewardFormula()) {
        return this->computeReachabilityRewards(env, checkTask.substituteFormula(rewardFormula.asReachabilityRewardFormula()));
    } else if (rewardFormula.isTotalRewardFormula()) {
        return this->computeTotalRewards(env, checkTask.substituteFormula(rewardFormula.asTotalRewardFormula()));
    } else if (rewardFormula.isDiscountedTotalRewardFormula()) {
        return this->computeDiscountedTotalRewards(env, checkTask.substituteFormula(rewardFormula.asDiscountedTotalRewardFormula()));
    } else if (rewardFormula.isLongRunAverageRewardFormula()) {
        return this->computeLongRunAverageRewards(env, checkTask.substituteFormula(rewardFormula.asLongRunAverageRewardFormula()));
    } else if (rewardFormula.isConditionalRewardFormula()) {
        return this->computeConditionalRewards(env, checkTask.substituteFormula(rewardFormula.asConditionalFormula()));
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The given formula '" << rewardFormula << "' is invalid.");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeConditionalRewards(
    Environment const&, CheckTask<storm::logic::ConditionalFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeCumulativeRewards(
    Environment const&, CheckTask<storm::logic::CumulativeRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeInstantaneousRewards(
    Environment const&, CheckTask<storm::logic::InstantaneousRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeReachabilityRewards(
    Environment const&, CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeTotalRewards(Environment const&,
                                                                                  CheckTask<storm::logic::TotalRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeLongRunAverageRewards(
    Environment const&, CheckTask<storm::logic::LongRunAverageRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeLongRunAverageProbabilities(
    Environment const&, CheckTask<storm::logic::StateFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeDiscountedCumulativeRewards(
    Environment const&, CheckTask<storm::logic::DiscountedCumulativeRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeDiscountedTotalRewards(
    Environment const&, CheckTask<storm::logic::DiscountedTotalRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeTimes(Environment const& env,
                                                                           CheckTask<storm::logic::Formula, SolutionType> const& checkTask) {
    storm::logic::Formula const& timeFormula = checkTask.getFormula();
    if (timeFormula.isReachabilityTimeFormula()) {
        return this->computeReachabilityTimes(env, checkTask.substituteFormula(timeFormula.asReachabilityTimeFormula()));
    }
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::computeReachabilityTimes(
    Environment const&, CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkStateFormula(Environment const& env,
                                                                                CheckTask<storm::logic::StateFormula, SolutionType> const& checkTask) {
    storm::logic::StateFormula const& stateFormula = checkTask.getFormula();
    if (stateFormula.isBinaryBooleanStateFormula()) {
        return this->checkBinaryBooleanStateFormula(env, checkTask.substituteFormula(stateFormula.asBinaryBooleanStateFormula()));
    } else if (stateFormula.isUnaryBooleanStateFormula()) {
        return this->checkUnaryBooleanStateFormula(env, checkTask.substituteFormula(stateFormula.asUnaryBooleanStateFormula()));
    } else if (stateFormula.isBooleanLiteralFormula()) {
        return this->checkBooleanLiteralFormula(env, checkTask.substituteFormula(stateFormula.asBooleanLiteralFormula()));
    } else if (stateFormula.isProbabilityOperatorFormula()) {
        return this->checkProbabilityOperatorFormula(env, checkTask.substituteFormula(stateFormula.asProbabilityOperatorFormula()));
    } else if (stateFormula.isRewardOperatorFormula()) {
        return this->checkRewardOperatorFormula(env, checkTask.substituteFormula(stateFormula.asRewardOperatorFormula()));
    } else if (stateFormula.isTimeOperatorFormula()) {
        return this->checkTimeOperatorFormula(env, checkTask.substituteFormula(stateFormula.asTimeOperatorFormula()));
    } else if (stateFormula.isLongRunAverageOperatorFormula()) {
        return this->checkLongRunAverageOperatorFormula(env, checkTask.substituteFormula(stateFormula.asLongRunAverageOperatorFormula()));
    } else if (stateFormula.isAtomicExpressionFormula()) {
        return this->checkAtomicExpressionFormula(env, checkTask.substituteFormula(stateFormula.asAtomicExpressionFormula()));
    } else if (stateFormula.isAtomicLabelFormula()) {
        return this->checkAtomicLabelFormula(env, checkTask.substituteFormula(stateFormula.asAtomicLabelFormula()));
    } else if (stateFormula.isBooleanLiteralFormula()) {
        return this->checkBooleanLiteralFormula(env, checkTask.substituteFormula(stateFormula.asBooleanLiteralFormula()));
    } else if (stateFormula.isGameFormula()) {
        return this->checkGameFormula(env, checkTask.substituteFormula(stateFormula.asGameFormula()));
    } else if (stateFormula.isMultiObjectiveFormula()) {
        auto const& mof = stateFormula.asMultiObjectiveFormula();
        if (mof.isLexicographic() || env.modelchecker().multi().isLexicographicModelCheckingSet()) {
            return this->checkLexObjectiveFormula(env, checkTask.substituteFormula(mof));
        } else {
            STORM_LOG_ASSERT(mof.isTradeoff(), "Unexpected multi-objective formula type.");
            return this->checkMultiObjectiveFormula(env, checkTask.substituteFormula(mof));
        }
    } else if (stateFormula.isQuantileFormula()) {
        return this->checkQuantileFormula(env, checkTask.substituteFormula(stateFormula.asQuantileFormula()));
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The given formula '" << stateFormula << "' is invalid.");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkAtomicExpressionFormula(
    Environment const& env, CheckTask<storm::logic::AtomicExpressionFormula, SolutionType> const& checkTask) {
    storm::logic::AtomicExpressionFormula const& stateFormula = checkTask.getFormula();
    std::stringstream stream;
    stream << stateFormula.getExpression();
    return this->checkAtomicLabelFormula(env, checkTask.substituteFormula(storm::logic::AtomicLabelFormula(stream.str())));
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkAtomicLabelFormula(
    Environment const&, CheckTask<storm::logic::AtomicLabelFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkBinaryBooleanStateFormula(
    Environment const& env, CheckTask<storm::logic::BinaryBooleanStateFormula, SolutionType> const& checkTask) {
    storm::logic::BinaryBooleanStateFormula const& stateFormula = checkTask.getFormula();
    STORM_LOG_THROW(stateFormula.getLeftSubformula().isStateFormula() && stateFormula.getRightSubformula().isStateFormula(),
                    storm::exceptions::InvalidArgumentException, "The given formula is invalid.");

    std::unique_ptr<CheckResult> leftResult =
        this->check(env, checkTask.template substituteFormula<storm::logic::Formula>(stateFormula.getLeftSubformula().asStateFormula()));
    std::unique_ptr<CheckResult> rightResult =
        this->check(env, checkTask.template substituteFormula<storm::logic::Formula>(stateFormula.getRightSubformula().asStateFormula()));

    STORM_LOG_THROW(leftResult->isQualitative() && rightResult->isQualitative(), storm::exceptions::InternalTypeErrorException,
                    "Expected qualitative results.");

    if (stateFormula.isAnd()) {
        leftResult->asQualitativeCheckResult() &= rightResult->asQualitativeCheckResult();
    } else if (stateFormula.isOr()) {
        leftResult->asQualitativeCheckResult() |= rightResult->asQualitativeCheckResult();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The given formula '" << stateFormula << "' is invalid.");
    }

    return leftResult;
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkBooleanLiteralFormula(
    Environment const&, CheckTask<storm::logic::BooleanLiteralFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkProbabilityOperatorFormula(
    Environment const& env, CheckTask<storm::logic::ProbabilityOperatorFormula, SolutionType> const& checkTask) {
    storm::logic::ProbabilityOperatorFormula const& stateFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> result = this->computeProbabilities(env, checkTask.substituteFormula(stateFormula.getSubformula()));

    if (checkTask.isBoundSet()) {
        STORM_LOG_THROW(result->isQuantitative(), storm::exceptions::InvalidOperationException,
                        "Unable to perform comparison operation on non-quantitative result.");
        return result->asQuantitativeCheckResult<SolutionType>().compareAgainstBound(checkTask.getBoundComparisonType(), checkTask.getBoundThreshold());
    } else {
        return result;
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkRewardOperatorFormula(
    Environment const& env, CheckTask<storm::logic::RewardOperatorFormula, SolutionType> const& checkTask) {
    storm::logic::RewardOperatorFormula const& stateFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> result = this->computeRewards(env, checkTask.substituteFormula(stateFormula.getSubformula()));

    if (checkTask.isBoundSet()) {
        STORM_LOG_THROW(result->isQuantitative(), storm::exceptions::InvalidOperationException,
                        "Unable to perform comparison operation on non-quantitative result.");
        return result->asQuantitativeCheckResult<SolutionType>().compareAgainstBound(checkTask.getBoundComparisonType(), checkTask.getBoundThreshold());
    } else {
        return result;
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkTimeOperatorFormula(
    Environment const& env, CheckTask<storm::logic::TimeOperatorFormula, SolutionType> const& checkTask) {
    storm::logic::TimeOperatorFormula const& stateFormula = checkTask.getFormula();
    STORM_LOG_THROW(stateFormula.getSubformula().isReachabilityTimeFormula(), storm::exceptions::InvalidArgumentException, "The given formula is invalid.");

    std::unique_ptr<CheckResult> result = this->computeTimes(env, checkTask.substituteFormula(stateFormula.getSubformula()));

    if (checkTask.isBoundSet()) {
        STORM_LOG_THROW(result->isQuantitative(), storm::exceptions::InvalidOperationException,
                        "Unable to perform comparison operation on non-quantitative result.");
        return result->asQuantitativeCheckResult<SolutionType>().compareAgainstBound(checkTask.getBoundComparisonType(), checkTask.getBoundThreshold());
    } else {
        return result;
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkLongRunAverageOperatorFormula(
    Environment const& env, CheckTask<storm::logic::LongRunAverageOperatorFormula, SolutionType> const& checkTask) {
    storm::logic::LongRunAverageOperatorFormula const& stateFormula = checkTask.getFormula();
    STORM_LOG_THROW(stateFormula.getSubformula().isStateFormula(), storm::exceptions::InvalidArgumentException, "The given formula is invalid.");

    std::unique_ptr<CheckResult> result =
        this->computeLongRunAverageProbabilities(env, checkTask.substituteFormula(stateFormula.getSubformula().asStateFormula()));

    if (checkTask.isBoundSet()) {
        STORM_LOG_THROW(result->isQuantitative(), storm::exceptions::InvalidOperationException,
                        "Unable to perform comparison operation on non-quantitative result.");
        return result->asQuantitativeCheckResult<SolutionType>().compareAgainstBound(checkTask.getBoundComparisonType(), checkTask.getBoundThreshold());
    } else {
        return result;
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkUnaryBooleanStateFormula(
    Environment const& env, CheckTask<storm::logic::UnaryBooleanStateFormula, SolutionType> const& checkTask) {
    storm::logic::UnaryBooleanStateFormula const& stateFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResult = this->check(env, checkTask.template substituteFormula<storm::logic::Formula>(stateFormula.getSubformula()));
    STORM_LOG_THROW(subResult->isQualitative(), storm::exceptions::InternalTypeErrorException, "Expected qualitative result.");
    if (stateFormula.isNot()) {
        subResult->asQualitativeCheckResult().complement();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The given formula '" << stateFormula << "' is invalid.");
    }
    return subResult;
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkLexObjectiveFormula(
    Environment const&, CheckTask<storm::logic::MultiObjectiveFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkMultiObjectiveFormula(
    Environment const&, CheckTask<storm::logic::MultiObjectiveFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkQuantileFormula(Environment const&,
                                                                                   CheckTask<storm::logic::QuantileFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

template<typename ModelType>
std::unique_ptr<CheckResult> AbstractModelChecker<ModelType>::checkGameFormula(Environment const&,
                                                                               CheckTask<storm::logic::GameFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                    "This model checker (" << getClassName() << ") does not support the formula: " << checkTask.getFormula() << ".");
}

///////////////////////////////////////////////
// Explicitly instantiate the template class.
///////////////////////////////////////////////
// Sparse
template class AbstractModelChecker<storm::models::sparse::Model<double>>;
template class AbstractModelChecker<storm::models::sparse::Dtmc<double>>;
template class AbstractModelChecker<storm::models::sparse::Ctmc<double>>;
template class AbstractModelChecker<storm::models::sparse::Mdp<double>>;
template class AbstractModelChecker<storm::models::sparse::Pomdp<double>>;
template class AbstractModelChecker<storm::models::sparse::MarkovAutomaton<double>>;
template class AbstractModelChecker<storm::models::sparse::Smg<double>>;

template class AbstractModelChecker<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<storm::Interval>>>;
template class AbstractModelChecker<storm::models::sparse::Smg<double, storm::models::sparse::StandardRewardModel<storm::Interval>>>;

template class AbstractModelChecker<storm::models::sparse::Mdp<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>>;
template class AbstractModelChecker<storm::models::sparse::Smg<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>>;

template class AbstractModelChecker<storm::models::sparse::Model<storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::sparse::Dtmc<storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::sparse::Ctmc<storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::sparse::Smg<storm::RationalNumber>>;

template class AbstractModelChecker<storm::models::sparse::Model<storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::sparse::MarkovAutomaton<storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::sparse::Smg<storm::RationalFunction>>;

template class AbstractModelChecker<storm::models::sparse::Mdp<storm::Interval>>;
template class AbstractModelChecker<storm::models::sparse::Dtmc<storm::Interval>>;

template class AbstractModelChecker<storm::models::sparse::Mdp<storm::RationalInterval>>;
template class AbstractModelChecker<storm::models::sparse::Dtmc<storm::RationalInterval>>;

// DD
template class AbstractModelChecker<storm::models::symbolic::Model<storm::dd::DdType::CUDD, double>>;
template class AbstractModelChecker<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>>;
template class AbstractModelChecker<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::CUDD, double>>;
template class AbstractModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, double>>;
template class AbstractModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::CUDD, double>>;
template class AbstractModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, double>>;
template class AbstractModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double>>;
template class AbstractModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double>>;
template class AbstractModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::CUDD, double>>;
template class AbstractModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, double>>;
template class AbstractModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class AbstractModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::CUDD, double>>;
template class AbstractModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, double>>;
template class AbstractModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class AbstractModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
}  // namespace modelchecker
}  // namespace storm
