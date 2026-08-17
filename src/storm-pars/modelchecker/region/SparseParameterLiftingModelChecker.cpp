#include "storm-pars/modelchecker/region/SparseParameterLiftingModelChecker.h"

#include "storm-pars/modelchecker/instantiation/SparseInstantiationModelChecker.h"
#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm-pars/modelchecker/region/RegionResultHypothesis.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityBackend.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityChecker.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalFunctionForward.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ConstantType>
SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::SparseParameterLiftingModelChecker() {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType>
void SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyFormula(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::Formula, typename SparseModelType::ValueType> const& checkTask) {
    currentFormula = checkTask.getFormula().asSharedPointer();
    currentCheckTask = std::make_unique<storm::modelchecker::CheckTask<storm::logic::Formula, ConstantType>>(
        checkTask.substituteFormula(*currentFormula).template convertValueType<ConstantType>());

    if (currentCheckTask->getFormula().isProbabilityOperatorFormula()) {
        auto const& probOpFormula = currentCheckTask->getFormula().asProbabilityOperatorFormula();
        if (probOpFormula.getSubformula().isBoundedUntilFormula()) {
            specifyBoundedUntilFormula(currentCheckTask->substituteFormula(probOpFormula.getSubformula().asBoundedUntilFormula()));
        } else if (probOpFormula.getSubformula().isUntilFormula()) {
            specifyUntilFormula(env, currentCheckTask->substituteFormula(probOpFormula.getSubformula().asUntilFormula()));
        } else if (probOpFormula.getSubformula().isEventuallyFormula()) {
            specifyReachabilityProbabilityFormula(env, currentCheckTask->substituteFormula(probOpFormula.getSubformula().asEventuallyFormula()));
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Parameter lifting is not supported for the given property.");
        }
    } else if (currentCheckTask->getFormula().isRewardOperatorFormula()) {
        auto const& rewOpFormula = currentCheckTask->getFormula().asRewardOperatorFormula();
        if (rewOpFormula.getSubformula().isEventuallyFormula()) {
            specifyReachabilityRewardFormula(env, currentCheckTask->substituteFormula(rewOpFormula.getSubformula().asEventuallyFormula()));
        } else if (rewOpFormula.getSubformula().isCumulativeRewardFormula()) {
            specifyCumulativeRewardFormula(currentCheckTask->substituteFormula(rewOpFormula.getSubformula().asCumulativeRewardFormula()));
        }
    }
}

template<typename SparseModelType, typename ConstantType>
bool SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::hasUniqueInitialState() const {
    return this->parametricModel->getInitialStates().getNumberOfSetBits() == 1;
}

template<typename SparseModelType, typename ConstantType>
uint64_t SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getUniqueInitialState() const {
    STORM_LOG_ASSERT(hasUniqueInitialState(), "Model does not have a unique initial state.");
    return *this->parametricModel->getInitialStates().begin();
}

template<typename RegionType>
auto getOptimalValuationForMonotonicity(RegionType const& region,
                                        std::map<typename RegionType::VariableType, storm::analysis::MonotonicityKind> const& monotonicityResult,
                                        storm::OptimizationDirection dir) {
    typename RegionType::Valuation result;
    for (auto const& [var, mon] : monotonicityResult) {
        if (mon == storm::analysis::MonotonicityKind::Constant) {
            result.emplace(var, region.getLowerBoundary(var));
        } else if (mon == storm::analysis::MonotonicityKind::Incr) {
            result.emplace(var, storm::solver::maximize(dir) ? region.getUpperBoundary(var) : region.getLowerBoundary(var));
        } else if (mon == storm::analysis::MonotonicityKind::Decr) {
            result.emplace(var, storm::solver::minimize(dir) ? region.getUpperBoundary(var) : region.getLowerBoundary(var));
        }
    }
    return result;
}

template<typename SparseModelType, typename ConstantType>
RegionResult SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::analyzeRegion(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                                              RegionResultHypothesis const& hypothesis,
                                                                                              bool sampleVerticesOfRegion) {
    STORM_LOG_THROW(this->currentCheckTask->isOnlyInitialStatesRelevantSet(), storm::exceptions::NotSupportedException,
                    "Analyzing regions with parameter lifting requires a property where only the value in the initial states is relevant.");
    STORM_LOG_THROW(this->currentCheckTask->isBoundSet(), storm::exceptions::NotSupportedException,
                    "Analyzing regions with parameter lifting requires a bounded property.");
    STORM_LOG_THROW(hasUniqueInitialState(), storm::exceptions::NotSupportedException,
                    "Analyzing regions with parameter lifting requires a model with a single initial state.");

    RegionResult result = region.result;
    if (result == RegionResult::AllSat || result == RegionResult::AllViolated || result == RegionResult::ExistsBoth) {
        return result;  // Result is already known, nothing to do.
    }

    // Check if we need to check the formula on one point to decide whether to show AllSat or AllViolated
    if (hypothesis == RegionResultHypothesis::Unknown &&
        (result == RegionResult::Unknown || result == RegionResult::ExistsIllDefined || result == RegionResult::CenterIllDefined)) {
        auto const center = region.region.getCenterPoint();
        if (getInstantiationChecker(false).isWellDefined(center)) {
            result = getInstantiationChecker(false).check(env, center)->template asExplicitQualitativeCheckResult<ConstantType>()[getUniqueInitialState()]
                         ? RegionResult::CenterSat
                         : RegionResult::CenterViolated;
        } else {
            auto const lowerCorner = region.region.getLowerBoundaries();
            if (getInstantiationChecker(false).isWellDefined(lowerCorner)) {
                result =
                    getInstantiationChecker(false).check(env, lowerCorner)->template asExplicitQualitativeCheckResult<ConstantType>()[getUniqueInitialState()]
                        ? RegionResult::ExistsSat
                        : RegionResult::ExistsViolated;
            } else {
                result = RegionResult::CenterIllDefined;
            }
        }
    }

    bool const existsSat = (hypothesis == RegionResultHypothesis::AllSat || result == RegionResult::ExistsSat || result == RegionResult::CenterSat);
    bool const existsIllDefined = (result == RegionResult::ExistsIllDefined || result == RegionResult::CenterIllDefined);
    {
        [[maybe_unused]] bool const existsViolated =
            (hypothesis == RegionResultHypothesis::AllViolated || result == RegionResult::ExistsViolated || result == RegionResult::CenterViolated);
        STORM_LOG_ASSERT(existsSat + existsViolated + existsIllDefined == 1,
                         "Invalid state of region analysis.");  // At this point, exactly one of the three cases must be true
    }
    auto const dirForSat =
        isLowerBound(this->currentCheckTask->getBound().comparisonType) ? storm::OptimizationDirection::Minimize : storm::OptimizationDirection::Maximize;

    std::vector<storm::OptimizationDirection> dirsToCheck;
    if (existsSat) {
        dirsToCheck = {dirForSat};
    } else if (existsIllDefined) {
        dirsToCheck = {dirForSat, storm::solver::invert(dirForSat)};
    } else {
        dirsToCheck = {storm::solver::invert(dirForSat)};
    }

    for (auto const& dirToCheck : dirsToCheck) {
        // Try solving through global monotonicity
        if (auto globalMonotonicity = region.monotonicityAnnotation.getGlobalMonotonicityResult();
            globalMonotonicity.has_value() && globalMonotonicity->isDone() && globalMonotonicity->isAllMonotonicity()) {
            auto const valuation = getOptimalValuationForMonotonicity(region.region, globalMonotonicity->getMonotonicityResult(), dirToCheck);
            STORM_LOG_ASSERT(valuation.size() == region.region.getVariables().size(), "Not all parameters seem to be monotonic.");
            auto& checker = existsSat ? getInstantiationCheckerSAT(false) : getInstantiationCheckerVIO(false);
            bool const monCheckResult = checker.check(env, valuation)->template asExplicitQualitativeCheckResult<ConstantType>()[getUniqueInitialState()];
            if (existsSat == monCheckResult) {
                result = existsSat ? RegionResult::AllSat : RegionResult::AllViolated;
                STORM_LOG_INFO("Region " << region.region << " is " << result << ", discovered with instantiation checker on " << valuation
                                         << " and help of monotonicity\n");
                region.resultKnownThroughMonotonicity = true;
            } else if (result == RegionResult::ExistsSat || result == RegionResult::CenterSat || result == RegionResult::ExistsViolated ||
                       result == RegionResult::CenterViolated) {
                // We found a satisfying and a violating point
                result = RegionResult::ExistsBoth;
            } else {
                STORM_LOG_ASSERT(result == RegionResult::Unknown,
                                 "This case should only be reached if the initial region result is unknown, but it is " << result << ".");
                result = monCheckResult ? RegionResult::ExistsSat : RegionResult::ExistsViolated;
                if (sampleVerticesOfRegion) {
                    result = sampleVertices(env, region.region, result);
                }
            }
        } else {
            // Try to prove AllSat or AllViolated through parameterLifting
            auto const checkResult = this->check(env, region, dirToCheck);
            if (checkResult) {
                bool const value = checkResult->template asExplicitQualitativeCheckResult<ConstantType>()[getUniqueInitialState()];
                if ((dirToCheck == dirForSat) == value) {
                    result = (dirToCheck == dirForSat) ? RegionResult::AllSat : RegionResult::AllViolated;
                } else if (sampleVerticesOfRegion) {
                    result = sampleVertices(env, region.region, result);
                }
            } else {
                result = RegionResult::AllIllDefined;
            }
        }
    }
    return result;
}

template<typename SparseModelType, typename ConstantType>
RegionResult SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::sampleVertices(Environment const& env,
                                                                                               storm::storage::ParameterRegion<ParametricType> const& region,
                                                                                               RegionResult const& initialResult) {
    RegionResult result = initialResult;

    if (result == RegionResult::AllSat || result == RegionResult::AllViolated) {
        return result;
    }

    bool hasSatPoint = result == RegionResult::ExistsSat || result == RegionResult::CenterSat;
    bool hasViolatedPoint = result == RegionResult::ExistsViolated || result == RegionResult::CenterViolated;

    // Check if there is a point in the region for which the property is satisfied
    auto vertices = region.getVerticesOfRegion(region.getVariables());
    auto vertexIt = vertices.begin();
    while (vertexIt != vertices.end() && !(hasSatPoint && hasViolatedPoint)) {
        if (getInstantiationChecker(false).check(env, *vertexIt)->template asExplicitQualitativeCheckResult<ConstantType>()[getUniqueInitialState()]) {
            hasSatPoint = true;
        } else {
            hasViolatedPoint = true;
        }
        ++vertexIt;
    }

    if (hasSatPoint) {
        if (hasViolatedPoint) {
            result = RegionResult::ExistsBoth;
        } else if (result != RegionResult::CenterSat) {
            result = RegionResult::ExistsSat;
        }
    } else if (hasViolatedPoint && result != RegionResult::CenterViolated) {
        result = RegionResult::ExistsViolated;
    }

    return result;
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::check(
    Environment const& env, AnnotatedRegion<ParametricType>& region, storm::solver::OptimizationDirection const& dirForParameters) {
    auto quantitativeResult = computeQuantitativeValues(env, region, dirForParameters);
    if (quantitativeResult.size() == 0) {
        return nullptr;
    }
    auto quantitativeCheckResult = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ConstantType>>(std::move(quantitativeResult));
    if (currentCheckTask->getFormula().hasQuantitativeResult()) {
        return quantitativeCheckResult;
    } else {
        return quantitativeCheckResult->compareAgainstBound(this->currentCheckTask->getFormula().asOperatorFormula().getComparisonType(),
                                                            this->currentCheckTask->getFormula().asOperatorFormula().template getThresholdAs<ConstantType>());
    }
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<QuantitativeCheckResult<ConstantType>> SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getBound(
    Environment const& env, AnnotatedRegion<ParametricType>& region, storm::solver::OptimizationDirection const& dirForParameters) {
    STORM_LOG_WARN_COND(this->currentCheckTask->getFormula().hasQuantitativeResult(), "Computing quantitative bounds for a qualitative formula...");
    return std::make_unique<ExplicitQuantitativeCheckResult<ConstantType>>(computeQuantitativeValues(env, region, dirForParameters));
}

template<typename SparseModelType, typename ConstantType>
typename SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::CoefficientType
SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getBoundAtInitState(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                                       storm::solver::OptimizationDirection const& dirForParameters) {
    STORM_LOG_THROW(hasUniqueInitialState(), storm::exceptions::NotSupportedException,
                    "Getting a bound at the initial state requires a model with a single initial state.");
    auto result = computeQuantitativeValues(env, region, dirForParameters).at(getUniqueInitialState());
    return storm::utility::isInfinity(result) ? storm::utility::infinity<CoefficientType>() : storm::utility::convertNumber<CoefficientType>(result);
}

template<typename SparseModelType, typename ConstantType>
storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>&
SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getInstantiationCheckerSAT(bool quantitative) {
    return getInstantiationChecker(quantitative);
}

template<typename SparseModelType, typename ConstantType>
storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>&
SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getInstantiationCheckerVIO(bool quantitative) {
    return getInstantiationChecker(quantitative);
}

template<typename SparseModelType, typename ConstantType>
SparseModelType const& SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getConsideredParametricModel() const {
    return *parametricModel;
}

template<typename SparseModelType, typename ConstantType>
CheckTask<storm::logic::Formula, ConstantType> const& SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getCurrentCheckTask() const {
    return *currentCheckTask;
}

template<typename SparseModelType, typename ConstantType>
void SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyBoundedUntilFormula(
    const CheckTask<logic::BoundedUntilFormula, ConstantType>& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Parameter lifting is not supported for the given property.");
}

template<typename SparseModelType, typename ConstantType>
void SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyUntilFormula(Environment const& env,
                                                                                            CheckTask<logic::UntilFormula, ConstantType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Parameter lifting is not supported for the given property.");
}

template<typename SparseModelType, typename ConstantType>
void SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyReachabilityProbabilityFormula(
    Environment const& env, CheckTask<logic::EventuallyFormula, ConstantType> const& checkTask) {
    // transform to until formula
    auto untilFormula =
        std::make_shared<storm::logic::UntilFormula const>(storm::logic::Formula::getTrueFormula(), checkTask.getFormula().getSubformula().asSharedPointer());
    specifyUntilFormula(env, currentCheckTask->substituteFormula(*untilFormula));
}

template<typename SparseModelType, typename ConstantType>
void SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyReachabilityRewardFormula(
    Environment const& env, CheckTask<logic::EventuallyFormula, ConstantType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Parameter lifting is not supported for the given property.");
}

template<typename SparseModelType, typename ConstantType>
void SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyCumulativeRewardFormula(
    const CheckTask<logic::CumulativeRewardFormula, ConstantType>& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Parameter lifting is not supported for the given property.");
}

template<typename SparseModelType, typename ConstantType>
std::pair<typename SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::CoefficientType,
          typename SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::Valuation>
SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::getAndEvaluateGoodPoint(Environment const& env, AnnotatedRegion<ParametricType>& region,
                                                                                           OptimizationDirection const& dirForParameters) {
    // Take region boundaries for parameters that are known to be monotonic or where there is hope for monotonicity
    auto point = getOptimalValuationForMonotonicity(region.region, this->monotonicityBackend->getOptimisticMonotonicityApproximation(region), dirForParameters);
    // Fill in missing parameters with the center point
    for (auto const& var : region.region.getVariables()) {
        point.emplace(var, region.region.getCenter(var));  // does not overwrite existing values
    }
    auto value = getInstantiationChecker(true).check(env, point)->template asExplicitQuantitativeCheckResult<ConstantType>()[getUniqueInitialState()];

    return std::make_pair(storm::utility::convertNumber<CoefficientType>(value), std::move(point));
}

template<typename SparseModelType, typename ConstantType>
void SparseParameterLiftingModelChecker<SparseModelType, ConstantType>::updateKnownValueBoundInRegion(AnnotatedRegion<ParametricType>& region,
                                                                                                      storm::solver::OptimizationDirection dir,
                                                                                                      std::vector<ConstantType> const& newValues) {
    if (hasUniqueInitialState()) {
        // Catch the infinity case since conversion might fail otherwise
        auto const& newValue = newValues.at(getUniqueInitialState());
        CoefficientType convertedValue =
            storm::utility::isInfinity(newValue) ? storm::utility::infinity<CoefficientType>() : storm::utility::convertNumber<CoefficientType>(newValue);
        region.updateValueBound(convertedValue, dir);
    }
}

template class SparseParameterLiftingModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double>;
template class SparseParameterLiftingModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, double>;
template class SparseParameterLiftingModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber>;
template class SparseParameterLiftingModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, storm::RationalNumber>;
}  // namespace modelchecker
}  // namespace storm
