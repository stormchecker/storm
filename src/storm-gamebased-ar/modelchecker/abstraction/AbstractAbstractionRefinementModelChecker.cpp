#include "storm-gamebased-ar/modelchecker/abstraction/AbstractAbstractionRefinementModelChecker.h"

#include "storm-gamebased-ar/abstraction/QualitativeResultMinMax.h"
#include "storm-gamebased-ar/abstraction/StateSet.h"
#include "storm-gamebased-ar/abstraction/SymbolicQualitativeGameResultMinMax.h"
#include "storm-gamebased-ar/abstraction/SymbolicQualitativeMdpResult.h"
#include "storm-gamebased-ar/abstraction/SymbolicQualitativeMdpResultMinMax.h"
#include "storm-gamebased-ar/abstraction/SymbolicStateSet.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/prctl/helper/SymbolicDtmcPrctlHelper.h"
#include "storm/modelchecker/prctl/helper/SymbolicMdpPrctlHelper.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"
#include "storm/numbers/constants.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/solver/SymbolicGameSolver.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm::gbar {
namespace modelchecker {

template<typename ModelType>
AbstractAbstractionRefinementModelChecker<ModelType>::AbstractAbstractionRefinementModelChecker()
    : storm::modelchecker::AbstractModelChecker<ModelType>(), checkTask(nullptr) {
    storm::settings::modules::AbstractionSettings::ReuseMode reuseMode =
        storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getReuseMode();
    reuseQualitativeResults = reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::All ||
                              reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::Qualitative;
    reuseQuantitativeResults = reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::All ||
                               reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::Quantitative;
}

template<typename ModelType>
AbstractAbstractionRefinementModelChecker<ModelType>::~AbstractAbstractionRefinementModelChecker() {
    // Intentionally left empty.
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::canHandle(storm::modelchecker::CheckTask<storm::logic::Formula> const& checkTask) const {
    storm::logic::Formula const& formula = checkTask.getFormula();
    bool enableRewards = this->supportsReachabilityRewards();
    storm::logic::FragmentSpecification fragment =
        storm::logic::reachability().setRewardOperatorsAllowed(enableRewards).setReachabilityRewardFormulasAllowed(enableRewards);
    return formula.isInFragment(fragment) && checkTask.isOnlyInitialStatesRelevantSet();
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::supportsReachabilityRewards() const {
    return false;
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeUntilProbabilities(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    this->setCheckTask(checkTask.template substituteFormula<storm::logic::Formula>(checkTask.getFormula()));
    return performAbstractionRefinement(env);
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeReachabilityProbabilities(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    STORM_LOG_THROW(this->supportsReachabilityRewards(), storm::exceptions::NotSupportedException,
                    "Reachability rewards are not supported by this abstraction-refinement model checker.");
    this->setCheckTask(checkTask.template substituteFormula<storm::logic::Formula>(checkTask.getFormula()));
    return performAbstractionRefinement(env);
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeReachabilityRewards(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    this->setCheckTask(checkTask.template substituteFormula<storm::logic::Formula>(checkTask.getFormula()));
    return performAbstractionRefinement(env);
}

template<typename ModelType>
void AbstractAbstractionRefinementModelChecker<ModelType>::setCheckTask(storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    this->checkTask = std::make_unique<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>>(checkTask);
}

template<typename ModelType>
storm::modelchecker::CheckTask<storm::logic::Formula, typename AbstractAbstractionRefinementModelChecker<ModelType>::ValueType> const&
AbstractAbstractionRefinementModelChecker<ModelType>::getCheckTask() const {
    return *checkTask;
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::getReuseQualitativeResults() const {
    return reuseQualitativeResults;
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::getReuseQuantitativeResults() const {
    return reuseQuantitativeResults;
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::performAbstractionRefinement(Environment const& env) {
    STORM_LOG_THROW(checkTask->isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidPropertyException,
                    "The abstraction-refinement model checkers can only compute the result for the initial states.");

    // Notify the underlying implementation that the abstraction-refinement process is being started.
    this->initializeAbstractionRefinement();

    uint64_t iterations = 0;
    std::unique_ptr<storm::modelchecker::CheckResult> result;
    auto start = std::chrono::high_resolution_clock::now();
    while (!result) {
        ++iterations;

        // Obtain the abstract model.
        auto abstractionStart = std::chrono::high_resolution_clock::now();
        std::shared_ptr<storm::models::Model<ValueType>> abstractModel = this->getAbstractModel();
        auto abstractionEnd = std::chrono::high_resolution_clock::now();
        STORM_LOG_TRACE("Model in iteration " << iterations << " has " << abstractModel->getNumberOfStates() << " states and "
                                              << abstractModel->getNumberOfTransitions() << " transitions (retrieved in "
                                              << std::chrono::duration_cast<std::chrono::milliseconds>(abstractionEnd - abstractionStart).count() << "ms).");

        // Obtain lower and upper bounds from the abstract model.
        std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>> bounds =
            computeBounds(env, *abstractModel);

        // Try to derive the final result from the obtained bounds.
        if (bounds.first || bounds.second) {
            result = tryToObtainResultFromBounds(*abstractModel, bounds);
        }
        if (!result) {
            if (bounds.first && bounds.second) {
                printBoundsInformation(bounds);
            }

            auto refinementStart = std::chrono::high_resolution_clock::now();
            this->refineAbstractModel();
            auto refinementEnd = std::chrono::high_resolution_clock::now();
            STORM_LOG_TRACE("Refinement in iteration " << iterations << " completed in "
                                                       << std::chrono::duration_cast<std::chrono::milliseconds>(refinementEnd - refinementStart).count()
                                                       << "ms.");
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_TRACE("Completed abstraction-refinement (" << this->getName() << ") in "
                                                         << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    return result;
}

template<typename ModelType>
std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>
AbstractAbstractionRefinementModelChecker<ModelType>::computeBounds(Environment const& env, storm::models::Model<ValueType> const& abstractModel) {
    // We go through two phases. In phase (1) we are solving the qualitative part and in phase (2) the quantitative part.

    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>> result;

    // Preparation: determine the constraint states and the target states of the reachability objective.
    std::pair<std::unique_ptr<storm::gbar::abstraction::StateSet>, std::unique_ptr<storm::gbar::abstraction::StateSet>> constraintAndTargetStates =
        getConstraintAndTargetStates(abstractModel);

    // Phase (1): solve qualitatively.
    lastQualitativeResults = computeQualitativeResult(env, abstractModel, *constraintAndTargetStates.first, *constraintAndTargetStates.second);

    // Check whether the answer can be given after the qualitative solution.
    result.first = checkForResultAfterQualitativeCheck(abstractModel);
    if (result.first) {
        return result;
    }

    // Check whether we should skip the quantitative solution (for example if there are initial states for which
    // the value is already known to be different at this point.
    bool doSkipQuantitativeSolution = skipQuantitativeSolution(abstractModel, *lastQualitativeResults);
    STORM_LOG_TRACE("" << (doSkipQuantitativeSolution ? "Skipping" : "Not skipping") << " quantitative solution.");

    // Phase (2): solve quantitatively.
    if (!doSkipQuantitativeSolution) {
        lastBounds =
            computeQuantitativeResult(env, abstractModel, *constraintAndTargetStates.first, *constraintAndTargetStates.second, *lastQualitativeResults);
        STORM_LOG_ASSERT(lastBounds.first && lastBounds.second, "Expected both bounds.");

        result = std::make_pair(lastBounds.first->clone(), lastBounds.second->clone());
        filterInitialStates(abstractModel, result);

        // Check whether the answer can be given after the quantitative solution.
        if (checkForResultAfterQuantitativeCheck(abstractModel, true, result.first->asQuantitativeCheckResult<ValueType>())) {
            result.second = nullptr;
        } else if (checkForResultAfterQuantitativeCheck(abstractModel, false, result.second->asQuantitativeCheckResult<ValueType>())) {
            result.first = nullptr;
        }
    }

    return result;
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::checkForResultAfterQuantitativeCheck(
    storm::models::Model<ValueType> const& abstractModel, bool lowerBounds, storm::modelchecker::QuantitativeCheckResult<ValueType> const& quantitativeResult) {
    bool result = false;
    if (checkTask->isBoundSet()) {
        storm::logic::ComparisonType comparisonType = checkTask->getBoundComparisonType();
        ValueType threshold = checkTask->getBoundThreshold();

        if (lowerBounds) {
            if (storm::logic::isLowerBound(comparisonType)) {
                ValueType minimalLowerBound = quantitativeResult.getMin();
                result = (storm::logic::isStrict(comparisonType) && minimalLowerBound > threshold) ||
                         (!storm::logic::isStrict(comparisonType) && minimalLowerBound >= threshold);
            } else {
                ValueType maximalLowerBound = quantitativeResult.getMax();
                result = (storm::logic::isStrict(comparisonType) && maximalLowerBound >= threshold) ||
                         (!storm::logic::isStrict(comparisonType) && maximalLowerBound > threshold);
            }
        } else {
            if (storm::logic::isLowerBound(comparisonType)) {
                ValueType minimalUpperBound = quantitativeResult.getMin();
                result = (storm::logic::isStrict(comparisonType) && minimalUpperBound <= threshold) ||
                         (!storm::logic::isStrict(comparisonType) && minimalUpperBound < threshold);
            } else {
                ValueType maximalUpperBound = quantitativeResult.getMax();
                result = (storm::logic::isStrict(comparisonType) && maximalUpperBound < threshold) ||
                         (!storm::logic::isStrict(comparisonType) && maximalUpperBound <= threshold);
            }
        }

        if (result) {
            STORM_LOG_TRACE("Found result after quantiative check.");
        } else {
            STORM_LOG_TRACE("Did not find result after quantiative check.");
        }
    }

    return result;
}

template<typename ModelType>
void AbstractAbstractionRefinementModelChecker<ModelType>::printBoundsInformation(
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>& bounds) {
    STORM_LOG_THROW(bounds.first->isSymbolicQuantitativeCheckResult() && bounds.second->isSymbolicQuantitativeCheckResult(),
                    storm::exceptions::NotSupportedException, "Expected symbolic bounds.");

    printBoundsInformation(bounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>(),
                           bounds.second->asSymbolicQuantitativeCheckResult<DdType, ValueType>());
}

template<typename ModelType>
void AbstractAbstractionRefinementModelChecker<ModelType>::printBoundsInformation(
    storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& lowerBounds,
    storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& upperBounds) {
    // If there is exactly one value that we stored, we print the current bounds as an interval.
    if (lowerBounds.getStates().getNonZeroCount() == 1 && upperBounds.getStates().getNonZeroCount() == 1) {
        STORM_LOG_TRACE("Obtained bounds [" << lowerBounds.getValueVector().getMax() << ", " << upperBounds.getValueVector().getMax() << "] on actual result.");
    } else {
        storm::dd::Add<DdType, ValueType> diffs = upperBounds.getValueVector() - lowerBounds.getValueVector();
        storm::dd::Bdd<DdType> maxDiffRepresentative = diffs.maxAbstractRepresentative(diffs.getContainedMetaVariables());

        std::pair<ValueType, ValueType> bounds;
        bounds.first = (lowerBounds.getValueVector() * maxDiffRepresentative.template toAdd<ValueType>()).getMax();
        bounds.second = (upperBounds.getValueVector() * maxDiffRepresentative.template toAdd<ValueType>()).getMax();

        STORM_LOG_TRACE("Largest interval over initial is [" << bounds.first << ", " << bounds.second << "], difference " << (bounds.second - bounds.first)
                                                             << ".");
    }
}

template<typename ModelType>
void AbstractAbstractionRefinementModelChecker<ModelType>::filterInitialStates(
    storm::models::Model<ValueType> const& abstractModel,
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>& bounds) {
    STORM_LOG_THROW(abstractModel.isSymbolicModel(), storm::exceptions::NotSupportedException, "Expected symbolic model.");

    filterInitialStates(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(), bounds);
}

template<typename ModelType>
void AbstractAbstractionRefinementModelChecker<ModelType>::filterInitialStates(
    storm::models::symbolic::Model<DdType, ValueType> const& abstractModel,
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>& bounds) {
    storm::modelchecker::SymbolicQualitativeCheckResult<DdType> initialStateFilter(abstractModel.getReachableStates(), abstractModel.getInitialStates());
    bounds.first->filter(initialStateFilter);
    bounds.second->filter(initialStateFilter);
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::skipQuantitativeSolution(
    storm::models::Model<ValueType> const& abstractModel, storm::gbar::abstraction::QualitativeResultMinMax const& qualitativeResults) {
    STORM_LOG_THROW(abstractModel.isSymbolicModel(), storm::exceptions::NotSupportedException, "Expected symbolic model.");

    return skipQuantitativeSolution(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(),
                                    qualitativeResults.asSymbolicQualitativeResultMinMax<DdType>());
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::skipQuantitativeSolution(
    storm::models::symbolic::Model<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
    bool isRewardFormula =
        checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
    if (isRewardFormula) {
        if ((abstractModel.getInitialStates() && qualitativeResults.getProb1Min().getStates()) !=
            (abstractModel.getInitialStates() && qualitativeResults.getProb1Max().getStates())) {
            return true;
        }
    } else {
        if ((abstractModel.getInitialStates() && qualitativeResults.getProb0Min().getStates()) !=
            (abstractModel.getInitialStates() && qualitativeResults.getProb0Max().getStates())) {
            return true;
        } else if ((abstractModel.getInitialStates() && qualitativeResults.getProb1Min().getStates()) !=
                   (abstractModel.getInitialStates() && qualitativeResults.getProb1Max().getStates())) {
            return true;
        }
    }
    return false;
}

template<typename ModelType>
std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>
AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(Environment const& env, storm::models::Model<ValueType> const& abstractModel,
                                                                                storm::gbar::abstraction::StateSet const& constraintStates,
                                                                                storm::gbar::abstraction::StateSet const& targetStates,
                                                                                storm::gbar::abstraction::QualitativeResultMinMax const& qualitativeResults) {
    STORM_LOG_ASSERT(abstractModel.isSymbolicModel(), "Expected symbolic abstract model.");

    return computeQuantitativeResult(env, *abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(),
                                     constraintStates.asSymbolicStateSet<DdType>(), targetStates.asSymbolicStateSet<DdType>(),
                                     qualitativeResults.asSymbolicQualitativeResultMinMax<DdType>());
}

template<typename ModelType>
std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>
AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates,
    storm::gbar::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
    STORM_LOG_THROW(abstractModel.isOfType(storm::models::ModelType::Dtmc) || abstractModel.isOfType(storm::models::ModelType::Mdp) ||
                        abstractModel.isOfType(storm::models::ModelType::S2pg),
                    storm::exceptions::NotSupportedException, "Abstract model type is not supported.");

    if (abstractModel.isOfType(storm::models::ModelType::Dtmc)) {
        return computeQuantitativeResult(env, *abstractModel.template as<storm::models::symbolic::Dtmc<DdType, ValueType>>(), constraintStates, targetStates,
                                         qualitativeResults);
    } else if (abstractModel.isOfType(storm::models::ModelType::Mdp)) {
        return computeQuantitativeResult(env, *abstractModel.template as<storm::models::symbolic::Mdp<DdType, ValueType>>(), constraintStates, targetStates,
                                         qualitativeResults);
    } else {
        return computeQuantitativeResult(env, *abstractModel.template as<storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType>>(),
                                         constraintStates, targetStates, qualitativeResults);
    }
}

template<typename ModelType>
std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>
AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(
    Environment const& env, storm::models::symbolic::Dtmc<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates,
    storm::gbar::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>> result;
    storm::dd::Bdd<DdType> maybe;

    bool isRewardFormula =
        checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
    if (isRewardFormula) {
        maybe = qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates();
    } else {
        maybe = !(qualitativeResults.getProb0Min().getStates() || qualitativeResults.getProb1Min().getStates()) && abstractModel.getReachableStates();
    }

    storm::dd::Add<DdType, ValueType> startValues;
    if (this->getReuseQuantitativeResults() && lastBounds.first) {
        startValues = maybe.ite(lastBounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(),
                                abstractModel.getManager().template getAddZero<ValueType>());
    } else {
        startValues = abstractModel.getManager().template getAddZero<ValueType>();
    }

    if (isRewardFormula) {
        storm::dd::Add<DdType, ValueType> values = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
            env, abstractModel, abstractModel.getTransitionMatrix(),
            checkTask->isRewardModelSet() ? abstractModel.getRewardModel(checkTask->getRewardModel()) : abstractModel.getRewardModel(""), maybe,
            targetStates.getStates(), !qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates(), startValues);

        result.first = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), values);
        result.second = result.first->clone();
    } else {
        storm::dd::Add<DdType, ValueType> values = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
            env, abstractModel, abstractModel.getTransitionMatrix(), maybe, qualitativeResults.getProb1Min().getStates(), startValues);

        result.first = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), values);
        result.second = result.first->clone();
    }

    return result;
}

template<typename ModelType>
std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>
AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(
    Environment const& env, storm::models::symbolic::Mdp<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates,
    storm::gbar::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>> result;
    storm::dd::Bdd<DdType> maybeMin;
    storm::dd::Bdd<DdType> maybeMax;

    bool isRewardFormula =
        checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
    if (isRewardFormula) {
        maybeMin = qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates();
        maybeMax = qualitativeResults.getProb1Max().getStates() && abstractModel.getReachableStates();
    } else {
        maybeMin = !(qualitativeResults.getProb0Min().getStates() || qualitativeResults.getProb1Min().getStates()) && abstractModel.getReachableStates();
        maybeMax = !(qualitativeResults.getProb0Max().getStates() || qualitativeResults.getProb1Max().getStates()) && abstractModel.getReachableStates();
    }

    storm::dd::Add<DdType, ValueType> minStartValues;
    if (this->getReuseQuantitativeResults() && lastBounds.first) {
        minStartValues = maybeMin.ite(lastBounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(),
                                      abstractModel.getManager().template getAddZero<ValueType>());
    } else {
        minStartValues = abstractModel.getManager().template getAddZero<ValueType>();
    }

    uint64_t abstractionPlayer = this->getAbstractionPlayer();
    if (isRewardFormula) {
        result.first = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
            env, abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : checkTask->getOptimizationDirection(), abstractModel,
            abstractModel.getTransitionMatrix(), abstractModel.getTransitionMatrix().notZero(),
            checkTask->isRewardModelSet() ? abstractModel.getRewardModel(checkTask->getRewardModel()) : abstractModel.getRewardModel(""), maybeMin,
            targetStates.getStates(), !qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates(), minStartValues);

        if (abstractionPlayer == 0) {
            result.second = result.first->clone();
        } else {
            result.second = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
                env, storm::OptimizationDirection::Maximize, abstractModel, abstractModel.getTransitionMatrix(), abstractModel.getTransitionMatrix().notZero(),
                checkTask->isRewardModelSet() ? abstractModel.getRewardModel(checkTask->getRewardModel()) : abstractModel.getRewardModel(""), maybeMin,
                targetStates.getStates(), !qualitativeResults.getProb1Max().getStates() && abstractModel.getReachableStates(),
                maybeMax.ite(result.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(),
                             abstractModel.getManager().template getAddZero<ValueType>()));
        }
    } else {
        result.first = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
            env, abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : checkTask->getOptimizationDirection(), abstractModel,
            abstractModel.getTransitionMatrix(), maybeMin, qualitativeResults.getProb1Min().getStates(), minStartValues);

        if (abstractionPlayer == 0) {
            result.second = result.first->clone();
        } else {
            result.second = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
                env, storm::OptimizationDirection::Maximize, abstractModel, abstractModel.getTransitionMatrix(), maybeMax,
                qualitativeResults.getProb1Max().getStates(),
                maybeMax.ite(result.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(),
                             abstractModel.getManager().template getAddZero<ValueType>()));
        }
    }

    return result;
}

template<typename ModelType>
std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>
AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(
    Environment const& env, storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates,
    storm::gbar::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>> result;
    storm::dd::Bdd<DdType> maybeMin;
    storm::dd::Bdd<DdType> maybeMax;

    bool isRewardFormula =
        checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
    if (!isRewardFormula) {
        maybeMin = !(qualitativeResults.getProb0Min().getStates() || qualitativeResults.getProb1Min().getStates()) && abstractModel.getReachableStates();
        maybeMax = !(qualitativeResults.getProb0Max().getStates() || qualitativeResults.getProb1Max().getStates()) && abstractModel.getReachableStates();
    }

    storm::dd::Add<DdType, ValueType> minStartValues;
    if (this->getReuseQuantitativeResults() && lastBounds.first) {
        minStartValues = maybeMin.ite(lastBounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(),
                                      abstractModel.getManager().template getAddZero<ValueType>());
    } else {
        minStartValues = abstractModel.getManager().template getAddZero<ValueType>();
    }

    if (isRewardFormula) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Reward properties are not supported for abstract stochastic games.");
    } else {
        result.first = computeReachabilityProbabilitiesHelper(
            env, abstractModel, this->getAbstractionPlayer() == 1 ? storm::OptimizationDirection::Minimize : checkTask->getOptimizationDirection(),
            this->getAbstractionPlayer() == 2 ? storm::OptimizationDirection::Minimize : checkTask->getOptimizationDirection(), maybeMin,
            qualitativeResults.getProb1Min().getStates(), minStartValues);
        result.second = computeReachabilityProbabilitiesHelper(
            env, abstractModel, this->getAbstractionPlayer() == 1 ? storm::OptimizationDirection::Maximize : checkTask->getOptimizationDirection(),
            this->getAbstractionPlayer() == 2 ? storm::OptimizationDirection::Maximize : checkTask->getOptimizationDirection(), maybeMin,
            qualitativeResults.getProb1Max().getStates(),
            maybeMax.ite(result.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(),
                         abstractModel.getManager().template getAddZero<ValueType>()));
    }

    return result;
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeReachabilityProbabilitiesHelper(
    Environment const& env, storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel,
    storm::OptimizationDirection const& player1Direction, storm::OptimizationDirection const& player2Direction, storm::dd::Bdd<DdType> const& maybeStates,
    storm::dd::Bdd<DdType> const& prob1States, storm::dd::Add<DdType, ValueType> const& startValues) {
    STORM_LOG_TRACE("Performing quantative solution step. Player 1: " << player1Direction << ", player 2: " << player2Direction << ".");

    // Compute the ingredients of the equation system.
    storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();
    storm::dd::Add<DdType, ValueType> submatrix = maybeStatesAdd * abstractModel.getTransitionMatrix();
    storm::dd::Add<DdType, ValueType> prob1StatesAsColumn =
        prob1States.template toAdd<ValueType>().swapVariables(abstractModel.getRowColumnMetaVariablePairs());
    storm::dd::Add<DdType, ValueType> subvector = submatrix * prob1StatesAsColumn;
    subvector = subvector.sumAbstract(abstractModel.getColumnVariables());

    // Cut away all columns targeting non-maybe states.
    submatrix *= maybeStatesAdd.swapVariables(abstractModel.getRowColumnMetaVariablePairs());

    // Cut the starting vector to the maybe states of this query.
    storm::dd::Add<DdType, ValueType> startVector = maybeStates.ite(startValues, abstractModel.getManager().template getAddZero<ValueType>());

    // Create the solver and solve the equation system.
    storm::solver::SymbolicGameSolverFactory<DdType, ValueType> solverFactory;
    std::unique_ptr<storm::solver::SymbolicGameSolver<DdType, ValueType>> solver =
        solverFactory.create(submatrix, maybeStates, abstractModel.getIllegalPlayer1Mask(), abstractModel.getIllegalPlayer2Mask(),
                             abstractModel.getRowVariables(), abstractModel.getColumnVariables(), abstractModel.getRowColumnMetaVariablePairs(),
                             abstractModel.getPlayer1Variables(), abstractModel.getPlayer2Variables());
    auto values = solver->solveGame(env, player1Direction, player2Direction, startVector, subvector);

    return std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(),
                                                                                                     prob1States.template toAdd<ValueType>() + values);
}

template<typename ModelType>
std::unique_ptr<storm::gbar::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(
    Environment const& env, storm::models::Model<ValueType> const& abstractModel, storm::gbar::abstraction::StateSet const& constraintStates,
    storm::gbar::abstraction::StateSet const& targetStates) {
    STORM_LOG_ASSERT(abstractModel.isSymbolicModel(), "Expected symbolic abstract model.");

    return computeQualitativeResult(env, *abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(),
                                    constraintStates.asSymbolicStateSet<DdType>(), targetStates.asSymbolicStateSet<DdType>());
}

template<typename ModelType>
std::unique_ptr<storm::gbar::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(
    Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates) {
    STORM_LOG_THROW(abstractModel.isOfType(storm::models::ModelType::Dtmc) || abstractModel.isOfType(storm::models::ModelType::Mdp) ||
                        abstractModel.isOfType(storm::models::ModelType::S2pg),
                    storm::exceptions::NotSupportedException, "Expected discrete-time abstract model.");

    if (abstractModel.isOfType(storm::models::ModelType::Dtmc)) {
        return computeQualitativeResult(env, *abstractModel.template as<storm::models::symbolic::Dtmc<DdType, ValueType>>(), constraintStates, targetStates);
    } else if (abstractModel.isOfType(storm::models::ModelType::Mdp)) {
        return computeQualitativeResult(env, *abstractModel.template as<storm::models::symbolic::Mdp<DdType, ValueType>>(), constraintStates, targetStates);
    } else {
        return computeQualitativeResult(env, *abstractModel.template as<storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType>>(),
                                        constraintStates, targetStates);
    }
}

template<typename ModelType>
std::unique_ptr<storm::gbar::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(
    Environment const& env, storm::models::symbolic::Dtmc<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates) {
    STORM_LOG_DEBUG("Computing qualitative solution for DTMC.");
    std::unique_ptr<storm::gbar::abstraction::SymbolicQualitativeMdpResultMinMax<DdType>> result =
        std::make_unique<storm::gbar::abstraction::SymbolicQualitativeMdpResultMinMax<DdType>>();

    auto start = std::chrono::high_resolution_clock::now();
    bool isRewardFormula =
        checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
    storm::dd::Bdd<DdType> transitionMatrixBdd = abstractModel.getTransitionMatrix().notZero();
    if (isRewardFormula) {
        auto prob1 = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
        result->prob1Min = result->prob1Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob1);
    } else {
        auto prob01 = storm::utility::graph::performProb01(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
        result->prob0Min = result->prob0Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob01.first);
        result->prob1Min = result->prob1Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob01.second);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto timeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    STORM_LOG_DEBUG("Computed qualitative solution in " << timeInMilliseconds << "ms.");

    return std::move(result);  // move() required by, e.g., clang 3.8
}

template<typename ModelType>
std::unique_ptr<storm::gbar::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(
    Environment const& env, storm::models::symbolic::Mdp<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates) {
    STORM_LOG_DEBUG("Computing qualitative solution for MDP.");
    std::unique_ptr<storm::gbar::abstraction::SymbolicQualitativeMdpResultMinMax<DdType>> result =
        std::make_unique<storm::gbar::abstraction::SymbolicQualitativeMdpResultMinMax<DdType>>();

    auto start = std::chrono::high_resolution_clock::now();
    bool isRewardFormula =
        checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
    storm::dd::Bdd<DdType> transitionMatrixBdd = abstractModel.getTransitionMatrix().notZero();
    uint64_t abstractionPlayer = this->getAbstractionPlayer();
    if (this->getReuseQualitativeResults()) {
        if (isRewardFormula) {
            bool computedMin = false;
            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Minimize) {
                auto states = storm::utility::graph::performProb1E(
                    abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                    lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates()
                                           : storm::utility::graph::performProbGreater0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(),
                                                                                         targetStates.getStates()));
                result->prob1Min = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(states);
                computedMin = true;
            }

            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Maximize) {
                auto states = storm::utility::graph::performProb1A(
                    abstractModel, transitionMatrixBdd, targetStates.getStates(),
                    storm::utility::graph::performProbGreater0A(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                result->prob1Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(states);
                if (!computedMin) {
                    result->prob1Min = result->prob1Max;
                }
            } else {
                result->prob1Max = result->prob1Min;
            }

        } else {
            bool computedMax = false;
            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Maximize) {
                auto states = storm::utility::graph::performProb0A(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                result->prob0Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(states);
                states = storm::utility::graph::performProb1E(
                    abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                    lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates()
                                           : storm::utility::graph::performProbGreater0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(),
                                                                                         targetStates.getStates()));
                result->prob1Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(states);
                computedMax = true;
            }

            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Minimize) {
                auto states = storm::utility::graph::performProb1A(
                    abstractModel, transitionMatrixBdd,
                    lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates()
                                           : targetStates.getStates(),
                    storm::utility::graph::performProbGreater0A(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                result->prob1Min = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(states);

                states = storm::utility::graph::performProb0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                result->prob0Min = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(states);

                if (!computedMax) {
                    result->prob0Max = result->prob0Min;
                    result->prob1Max = result->prob1Min;
                }
            } else {
                result->prob0Min = result->prob0Max;
                result->prob1Min = result->prob1Max;
            }
        }
    } else {
        if (isRewardFormula) {
            bool computedMin = false;
            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Minimize) {
                auto prob1 = storm::utility::graph::performProb1E(
                    abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                    storm::utility::graph::performProbGreater0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                result->prob1Min = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob1);
                computedMin = true;
            }

            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Maximize) {
                auto prob1 = storm::utility::graph::performProb1A(
                    abstractModel, transitionMatrixBdd, targetStates.getStates(),
                    storm::utility::graph::performProbGreater0A(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                result->prob1Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob1);
                if (!computedMin) {
                    result->prob1Min = result->prob1Max;
                }
            } else {
                result->prob1Max = result->prob1Min;
            }
        } else {
            bool computedMin = false;
            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Minimize) {
                auto prob01 =
                    storm::utility::graph::performProb01Min(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                result->prob0Min = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob01.first);
                result->prob1Min = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob01.second);
                computedMin = true;
            }

            if (abstractionPlayer == 1 || checkTask->getOptimizationDirection() == storm::OptimizationDirection::Maximize) {
                auto prob01 =
                    storm::utility::graph::performProb01Max(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                result->prob0Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob01.first);
                result->prob1Max = storm::gbar::abstraction::SymbolicQualitativeMdpResult<DdType>(prob01.second);
                if (!computedMin) {
                    result->prob0Min = result->prob0Max;
                    result->prob1Min = result->prob1Max;
                }
            } else {
                result->prob0Max = result->prob0Min;
                result->prob1Max = result->prob1Min;
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto timeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (isRewardFormula) {
        STORM_LOG_TRACE("Min: " << result->getProb1Min().getStates().getNonZeroCount() << " states with probability 1.");
        STORM_LOG_TRACE("Max: " << result->getProb1Max().getStates().getNonZeroCount() << " states with probability 1.");
    } else {
        STORM_LOG_TRACE("Min: " << result->getProb0Min().getStates().getNonZeroCount() << " states with probability 0, "
                                << result->getProb1Min().getStates().getNonZeroCount() << " states with probability 1.");
        STORM_LOG_TRACE("Max: " << result->getProb0Max().getStates().getNonZeroCount() << " states with probability 0, "
                                << result->getProb1Max().getStates().getNonZeroCount() << " states with probability 1.");
    }
    STORM_LOG_DEBUG("Computed qualitative solution in " << timeInMilliseconds << "ms.");

    return std::move(result);  // move() required by, e.g., clang 3.8
}

template<typename ModelType>
std::unique_ptr<storm::gbar::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(
    Environment const& env, storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates) {
    STORM_LOG_DEBUG("Computing qualitative solution for S2PG.");
    std::unique_ptr<storm::gbar::abstraction::SymbolicQualitativeGameResultMinMax<DdType>> result;

    // Obtain the player optimization directions.
    uint64_t abstractionPlayer = this->getAbstractionPlayer();

    // Obtain direction for player 1 (model nondeterminism).
    storm::OptimizationDirection modelNondeterminismDirection = this->getCheckTask().getOptimizationDirection();

    // Convert the transition matrix to a BDD to use it in the qualitative algorithms.
    storm::dd::Bdd<DdType> transitionMatrixBdd = abstractModel.getTransitionMatrix().toBdd();

    // Remembers whether we need to synthesize schedulers.
    bool requiresSchedulers = this->requiresSchedulerSynthesis();

    auto start = std::chrono::high_resolution_clock::now();
    if (this->getReuseQualitativeResults()) {
        result = computeQualitativeResultReuse(abstractModel, transitionMatrixBdd, constraintStates, targetStates, abstractionPlayer,
                                               modelNondeterminismDirection, requiresSchedulers);
    } else {
        result = std::make_unique<storm::gbar::abstraction::SymbolicQualitativeGameResultMinMax<DdType>>();

        result->prob0Min = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               abstractionPlayer == 2 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               requiresSchedulers, requiresSchedulers);
        result->prob1Min = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               abstractionPlayer == 2 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               requiresSchedulers, requiresSchedulers);
        result->prob0Max = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection,
                                                               abstractionPlayer == 2 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection,
                                                               requiresSchedulers, requiresSchedulers);
        result->prob1Max = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection,
                                                               abstractionPlayer == 2 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection,
                                                               requiresSchedulers, requiresSchedulers);
    }

    STORM_LOG_TRACE("Qualitative precomputation completed.");
    if (abstractionPlayer == 1) {
        STORM_LOG_TRACE("[" << storm::OptimizationDirection::Minimize << ", " << modelNondeterminismDirection << "]: "
                            << result->prob0Min.player1States.getNonZeroCount() << " 'no', " << result->prob1Min.player1States.getNonZeroCount() << " 'yes'.");
        STORM_LOG_TRACE("[" << storm::OptimizationDirection::Maximize << ", " << modelNondeterminismDirection << "]: "
                            << result->prob0Max.player1States.getNonZeroCount() << " 'no', " << result->prob1Max.player1States.getNonZeroCount() << " 'yes'.");
    } else {
        STORM_LOG_TRACE("[" << modelNondeterminismDirection << ", " << storm::OptimizationDirection::Minimize << "]: "
                            << result->prob0Min.player1States.getNonZeroCount() << " 'no', " << result->prob1Min.player1States.getNonZeroCount() << " 'yes'.");
        STORM_LOG_TRACE("[" << modelNondeterminismDirection << ", " << storm::OptimizationDirection::Maximize << "]: "
                            << result->prob0Max.player1States.getNonZeroCount() << " 'no', " << result->prob1Max.player1States.getNonZeroCount() << " 'yes'.");
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto timeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    STORM_LOG_DEBUG("Computed qualitative solution in " << timeInMilliseconds << "ms.");

    return std::move(result);  // move() required by, e.g., clang 3.8
}

template<typename ModelType>
std::unique_ptr<storm::gbar::abstraction::SymbolicQualitativeGameResultMinMax<AbstractAbstractionRefinementModelChecker<ModelType>::DdType>>
AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResultReuse(
    storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel, storm::dd::Bdd<DdType> const& transitionMatrixBdd,
    storm::gbar::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::gbar::abstraction::SymbolicStateSet<DdType> const& targetStates,
    uint64_t abstractionPlayer, storm::OptimizationDirection const& modelNondeterminismDirection, bool requiresSchedulers) {
    std::unique_ptr<storm::gbar::abstraction::SymbolicQualitativeGameResultMinMax<DdType>> result =
        std::make_unique<storm::gbar::abstraction::SymbolicQualitativeGameResultMinMax<DdType>>();

    // Depending on the model nondeterminism direction, we choose a different order of operations.
    if (modelNondeterminismDirection == storm::OptimizationDirection::Minimize) {
        // (1) min/min: compute prob0 using the game functions
        result->prob0Min = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize,
                                                               requiresSchedulers, requiresSchedulers);

        // (2) min/min: compute prob1 using the MDP functions
        storm::dd::Bdd<DdType> candidates = abstractModel.getReachableStates() && !result->getProb0Min().getStates();
        storm::dd::Bdd<DdType> prob1MinMinMdp = storm::utility::graph::performProb1A(
            abstractModel, transitionMatrixBdd,
            lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates() : targetStates.getStates(),
            candidates);

        // (3) min/min: compute prob1 using the game functions
        result->prob1Min = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize,
                                                               requiresSchedulers, requiresSchedulers, boost::make_optional(prob1MinMinMdp));

        // (4) min/max, max/min: compute prob 0 using the game functions
        result->prob0Max = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection,
                                                               abstractionPlayer == 2 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection,
                                                               requiresSchedulers, requiresSchedulers);

        // (5) min/max, max/min: compute prob 1 using the game functions
        // We know that only previous prob1 states can now be prob 1 states again, because the upper bound
        // values can only decrease over iterations.
        result->prob1Max = storm::utility::graph::performProb1(
            abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
            abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection,
            abstractionPlayer == 2 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers,
            lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Max().getStates()
                                   : boost::optional<storm::dd::Bdd<DdType>>());
    } else {
        // (1) max/max: compute prob0 using the game functions
        result->prob0Max = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize,
                                                               requiresSchedulers, requiresSchedulers);

        // (2) max/max: compute prob1 using the MDP functions, reuse prob1 states of last iteration to constrain the candidate states.
        storm::dd::Bdd<DdType> candidates = abstractModel.getReachableStates() && !result->getProb0Max().getStates();
        if (this->getReuseQualitativeResults() && lastQualitativeResults) {
            candidates &= lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Max().getStates();
        }
        storm::dd::Bdd<DdType> prob1MaxMaxMdp =
            storm::utility::graph::performProb1E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), candidates);

        // (3) max/max: compute prob1 using the game functions, reuse prob1 states from the MDP precomputation
        result->prob1Max = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize,
                                                               requiresSchedulers, requiresSchedulers, boost::make_optional(prob1MaxMaxMdp));

        // (4) max/min, min/max: compute prob0 using the game functions
        result->prob0Min = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               abstractionPlayer == 2 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               requiresSchedulers, requiresSchedulers);

        // (5) max/min:, max/min compute prob1 using the game functions, use prob1 from max/max as the candidate set
        result->prob1Min = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(),
                                                               abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               abstractionPlayer == 2 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection,
                                                               requiresSchedulers, requiresSchedulers, boost::make_optional(prob1MaxMaxMdp));
    }

    return result;
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::checkForResultAfterQualitativeCheck(
    storm::models::Model<ValueType> const& abstractModel) {
    STORM_LOG_THROW(abstractModel.isSymbolicModel(), storm::exceptions::NotSupportedException, "Expected symbolic model.");

    return checkForResultAfterQualitativeCheck(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>());
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::checkForResultAfterQualitativeCheck(
    storm::models::symbolic::Model<DdType, ValueType> const& abstractModel) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    auto const& symbolicQualitativeResultMinMax = lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>();

    bool isRewardFormula =
        checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
    if (isRewardFormula) {
        // In the reachability reward case, we can give an answer if all initial states of the system are infinity
        // states in the min result.
        if ((abstractModel.getInitialStates() && !symbolicQualitativeResultMinMax.getProb1Min().getStates()) == abstractModel.getInitialStates()) {
            result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(
                abstractModel.getReachableStates(), abstractModel.getInitialStates(),
                abstractModel.getInitialStates().ite(abstractModel.getManager().getConstant(storm::numbers::infinity<ValueType>()),
                                                     abstractModel.getManager().template getAddZero<ValueType>()));
        }
    } else {
        // In the reachability probability case, we can give the answer if all initial states are prob1 states
        // in the min result or if all initial states are prob0 in the max case.
        // Furthermore, we can give the answer if there are initial states with probability > 0 in the min case
        // and the probability bound was 0 or if there are initial states with probability < 1 in the max case
        // and the probability bound was 1.
        if ((abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb1Min().getStates()) == abstractModel.getInitialStates()) {
            result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(
                abstractModel.getReachableStates(), abstractModel.getInitialStates(), abstractModel.getManager().template getAddOne<ValueType>());
        } else if ((abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb0Max().getStates()) == abstractModel.getInitialStates()) {
            result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(
                abstractModel.getReachableStates(), abstractModel.getInitialStates(), abstractModel.getManager().template getAddZero<ValueType>());
        } else if (checkTask->isBoundSet() && checkTask->getBoundThreshold() == storm::numbers::zero<ValueType>() &&
                   (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb0Min().getStates()) != abstractModel.getInitialStates()) {
            result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(
                abstractModel.getReachableStates(), abstractModel.getInitialStates(),
                (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb0Min().getStates())
                    .ite(abstractModel.getManager().template getConstant<ValueType>(0.5), abstractModel.getManager().template getAddZero<ValueType>()));
        } else if (checkTask->isBoundSet() && checkTask->getBoundThreshold() == storm::numbers::one<ValueType>() &&
                   (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb1Max().getStates()) != abstractModel.getInitialStates()) {
            result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(
                abstractModel.getReachableStates(), abstractModel.getInitialStates(),
                (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb1Max().getStates())
                        .ite(abstractModel.getManager().template getConstant<ValueType>(0.5), abstractModel.getManager().template getAddZero<ValueType>()) +
                    symbolicQualitativeResultMinMax.getProb1Max().getStates().template toAdd<ValueType>());
        }
    }

    if (result) {
        STORM_LOG_TRACE("Found result after qualitative check.");
    } else {
        STORM_LOG_TRACE("Did not find result after qualitative check.");
    }

    return result;
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::tryToObtainResultFromBounds(
    storm::models::Model<ValueType> const& abstractModel,
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>>& bounds) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    if (bounds.first == nullptr || bounds.second == nullptr) {
        STORM_LOG_ASSERT(bounds.first || bounds.second, "Expected at least one bound.");

        if (bounds.first) {
            return std::move(bounds.first);
        } else {
            return std::move(bounds.second);
        }
    } else {
        if (boundsAreSufficientlyClose(bounds)) {
            STORM_LOG_TRACE("Obtained bounds are sufficiently close.");
            result = getAverageOfBounds(bounds);
        }
    }

    if (result) {
        abstractModel.printModelInformationToStream(std::cout);
    }

    return result;
}

template<typename ModelType>
bool AbstractAbstractionRefinementModelChecker<ModelType>::boundsAreSufficientlyClose(
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>> const& bounds) {
    STORM_LOG_ASSERT(bounds.first->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
    storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& lowerBounds =
        bounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>();
    STORM_LOG_ASSERT(bounds.second->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
    storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& upperBounds =
        bounds.second->asSymbolicQuantitativeCheckResult<DdType, ValueType>();

    return lowerBounds.getValueVector().equalModuloPrecision(upperBounds.getValueVector(),
                                                             storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getPrecision(), false);
}

template<typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::getAverageOfBounds(
    std::pair<std::unique_ptr<storm::modelchecker::CheckResult>, std::unique_ptr<storm::modelchecker::CheckResult>> const& bounds) {
    STORM_LOG_ASSERT(bounds.first->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
    storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& lowerBounds =
        bounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>();
    STORM_LOG_ASSERT(bounds.second->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
    storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& upperBounds =
        bounds.second->asSymbolicQuantitativeCheckResult<DdType, ValueType>();

    return std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(
        lowerBounds.getReachableStates(), lowerBounds.getStates(),
        (lowerBounds.getValueVector() + upperBounds.getValueVector()) /
            lowerBounds.getValueVector().getDdManager().getConstant(storm::numbers::convert<ValueType>(std::string("2.0"))));
}

template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::CUDD, double>>;
template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::CUDD, double>>;
template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, double>>;
template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, double>>;

}  // namespace modelchecker
}  // namespace storm::gbar
