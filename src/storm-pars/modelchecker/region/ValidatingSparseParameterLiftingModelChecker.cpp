#include "storm-pars/modelchecker/region/ValidatingSparseParameterLiftingModelChecker.h"

#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityBackend.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-pars/transformer/SparseParametricMdpSimplifier.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::ValidatingSparseParameterLiftingModelChecker()
    : numOfWrongRegions(0) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::~ValidatingSparseParameterLiftingModelChecker() {
    STORM_LOG_STATISTICS("Validating Parameter Lifting Model Checker detected " << numOfWrongRegions << " regions where the imprecise method was wrong.\n");
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
bool ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::canHandle(
    std::shared_ptr<storm::models::ModelBase> parametricModel, CheckTask<storm::logic::Formula, typename SparseModelType::ValueType> const& checkTask) const {
    return impreciseChecker.canHandle(parametricModel, checkTask) && preciseChecker.canHandle(parametricModel, checkTask);
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
void ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::specify(
    Environment const& env, std::shared_ptr<storm::models::ModelBase> parametricModel, CheckTask<storm::logic::Formula, ParametricType> const& checkTask,
    std::optional<RegionSplitEstimateKind> generateRegionSplitEstimates, std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend,
    bool allowModelSimplifications, bool graphPreserving) {
    STORM_LOG_THROW(this->canHandle(parametricModel, checkTask), storm::exceptions::NotSupportedException,
                    "Combination of model " << parametricModel->getType() << " and formula '" << checkTask.getFormula() << "' is not supported.");
    STORM_LOG_THROW(graphPreserving, storm::exceptions::NotImplementedException, "Non-graph-preserving regions not implemented for validating PLA.");
    this->specifySplitEstimates(generateRegionSplitEstimates, checkTask);
    this->specifyMonotonicity(monotonicityBackend, checkTask);

    auto specifyUnderlyingCheckers = [&](auto pm, auto const& ct) {
        // TODO: Consider taking split estimates from the imprecise checker?
        // Do not perform simplification (again) in the underlying model checkers
        impreciseChecker.specify(env, pm, ct, std::nullopt, monotonicityBackend, false);
        preciseChecker.specify(env, pm, ct, std::nullopt, monotonicityBackend, false);
    };

    if (allowModelSimplifications) {
        auto dtmcOrMdp = parametricModel->template as<SparseModelType>();
        if constexpr (IsMDP) {
            auto simplifier = storm::transformer::SparseParametricMdpSimplifier<SparseModelType>(*dtmcOrMdp);
            STORM_LOG_THROW(simplifier.simplify(checkTask.getFormula()), storm::exceptions::UnexpectedException, "Simplifying the model was not successfull.");
            auto simplifiedTask = checkTask.substituteFormula(*simplifier.getSimplifiedFormula());
            specifyUnderlyingCheckers(simplifier.getSimplifiedModel(), simplifiedTask);
        } else {
            auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<SparseModelType>(*dtmcOrMdp);
            STORM_LOG_THROW(simplifier.simplify(checkTask.getFormula()), storm::exceptions::UnexpectedException, "Simplifying the model was not successfull.");
            auto simplifiedTask = checkTask.substituteFormula(*simplifier.getSimplifiedFormula());
            specifyUnderlyingCheckers(simplifier.getSimplifiedModel(), simplifiedTask);
        }
    } else {
        specifyUnderlyingCheckers(parametricModel, checkTask);
    }
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
RegionResult ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::analyzeRegion(Environment const& env,
                                                                                                                      AnnotatedRegion<ParametricType>& region,
                                                                                                                      RegionResultHypothesis const& hypothesis,
                                                                                                                      bool sampleVerticesOfRegion) {
    // Do not add annotations from the potentiallty imprecise model checker
    auto impreciseAnnotatedRegion = region;
    RegionResult currentResult = impreciseChecker.analyzeRegion(env, impreciseAnnotatedRegion, hypothesis, false);

    if (currentResult == RegionResult::AllSat || currentResult == RegionResult::AllViolated) {
        applyHintsToPreciseChecker();

        storm::solver::OptimizationDirection parameterOptDir = preciseChecker.getCurrentCheckTask().getOptimizationDirection();
        if (currentResult == RegionResult::AllViolated) {
            parameterOptDir = storm::solver::invert(parameterOptDir);
        }

        bool preciseResult =
            preciseChecker.check(env, region, parameterOptDir)
                ->template asExplicitQualitativeCheckResult<PreciseType>()[*preciseChecker.getConsideredParametricModel().getInitialStates().begin()];
        bool preciseResultAgrees = preciseResult == (currentResult == RegionResult::AllSat);

        if (!preciseResultAgrees) {
            // Imprecise result is wrong!
            currentResult = RegionResult::Unknown;
            ++numOfWrongRegions;

            // Check the other direction in case no hypothesis was given
            if (hypothesis == RegionResultHypothesis::Unknown) {
                parameterOptDir = storm::solver::invert(parameterOptDir);
                preciseResult =
                    preciseChecker.check(env, region, parameterOptDir)
                        ->template asExplicitQualitativeCheckResult<PreciseType>()[*preciseChecker.getConsideredParametricModel().getInitialStates().begin()];
                if (preciseResult && parameterOptDir == preciseChecker.getCurrentCheckTask().getOptimizationDirection()) {
                    currentResult = RegionResult::AllSat;
                } else if (!preciseResult && parameterOptDir == storm::solver::invert(preciseChecker.getCurrentCheckTask().getOptimizationDirection())) {
                    currentResult = RegionResult::AllViolated;
                }
            }
        }
    }

    if (sampleVerticesOfRegion && currentResult != RegionResult::AllSat && currentResult != RegionResult::AllViolated) {
        currentResult = preciseChecker.sampleVertices(env, region.region, currentResult);
    }

    return currentResult;
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
typename ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::CoefficientType
ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::getBoundAtInitState(
    Environment const& env, AnnotatedRegion<ParametricType>& region, storm::solver::OptimizationDirection const& dirForParameters) {
    return preciseChecker.getBoundAtInitState(env, region, dirForParameters);
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
std::pair<typename ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::CoefficientType,
          typename ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::Valuation>
ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::getAndEvaluateGoodPoint(
    Environment const& env, AnnotatedRegion<ParametricType>& region, storm::solver::OptimizationDirection const& dirForParameters) {
    return preciseChecker.getAndEvaluateGoodPoint(env, region, dirForParameters);
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
bool ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::isMonotonicitySupported(
    MonotonicityBackend<ParametricType> const& backend, CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const {
    // Currently, we do not support any interaction with the monotonicity backend
    return !backend.requiresInteractionWithRegionModelChecker() && impreciseChecker.isMonotonicitySupported(backend, checkTask) &&
           preciseChecker.isMonotonicitySupported(backend, checkTask);
}

template<typename SparseModelType, typename ImpreciseType, typename PreciseType>
void ValidatingSparseParameterLiftingModelChecker<SparseModelType, ImpreciseType, PreciseType>::applyHintsToPreciseChecker() {
    if (impreciseChecker.getCurrentMaxScheduler()) {
        preciseChecker.getCurrentMaxScheduler() = impreciseChecker.getCurrentMaxScheduler()->template toValueType<PreciseType>();
    }
    if (impreciseChecker.getCurrentMinScheduler()) {
        preciseChecker.getCurrentMinScheduler() = impreciseChecker.getCurrentMinScheduler()->template toValueType<PreciseType>();
    }
    if constexpr (IsMDP) {
        if (impreciseChecker.getCurrentPlayer1Scheduler()) {
            preciseChecker.getCurrentPlayer1Scheduler() = impreciseChecker.getCurrentPlayer1Scheduler()->template toValueType<PreciseType>();
        }
    }
}

template class ValidatingSparseParameterLiftingModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double, storm::RationalNumber>;
template class ValidatingSparseParameterLiftingModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, double, storm::RationalNumber>;

}  // namespace modelchecker
}  // namespace storm
