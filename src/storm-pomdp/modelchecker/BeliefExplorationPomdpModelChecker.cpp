#include "BeliefExplorationPomdpModelChecker.h"

#include <tuple>

#include "storm-pomdp/analysis/FiniteBeliefMdpDetection.h"
#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/transformer/MakeStateSetObservationClosed.h"

#include "storm/logic/Formulas.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/NumberTraits.h"

#include "storm-pomdp/builder/BeliefMdpExplorer.h"
#include "storm-pomdp/modelchecker/PreprocessingPomdpValueBoundsModelChecker.h"
#include "storm/utility/vector.h"

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {
namespace pomdp {
namespace modelchecker {

/* Struct Functions */

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result::Result(ValueType lower, ValueType upper)
    : lowerBound(lower), upperBound(upper) {
    // Intentionally left empty
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
typename BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::ValueType
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result::diff(bool relative) const {
    ValueType diff = upperBound - lowerBound;
    if (diff < storm::utility::zero<ValueType>()) {
        STORM_LOG_WARN_COND(diff >= storm::utility::convertNumber<ValueType>(1e-6),
                            "Upper bound '" << upperBound << "' is smaller than lower bound '" << lowerBound << "': Difference is " << diff << ".");
        diff = storm::utility::zero<ValueType>();
    }
    if (relative && !storm::utility::isZero(upperBound)) {
        diff /= upperBound;
    }
    return diff;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result::updateLowerBound(ValueType const& value) {
    if (value > lowerBound) {
        lowerBound = value;
        return true;
    }
    return false;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result::updateUpperBound(ValueType const& value) {
    if (value < upperBound) {
        upperBound = value;
        return true;
    }
    return false;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Statistics::Statistics()
    : beliefMdpDetectedToBeFinite(false),
      refinementFixpointDetected(false),
      overApproximationBuildAborted(false),
      underApproximationBuildAborted(false),
      aborted(false) {
    // intentionally left empty;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::BeliefExplorationPomdpModelChecker(std::shared_ptr<PomdpModelType> pomdp,
                                                                                                                       Options options)
    : options(options),
      inputPomdp(pomdp),
      beliefTypeCC(storm::utility::convertNumber<BeliefValueType>(this->options.numericPrecision), false),
      valueTypeCC(this->options.numericPrecision, false) {
    STORM_LOG_ASSERT(inputPomdp, "The given POMDP is not initialized.");
    STORM_LOG_ERROR_COND(inputPomdp->isCanonic(), "Input Pomdp is not known to be canonic. This might lead to unexpected verification results.");
}

/* Public Functions */

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::precomputeValueBounds(storm::logic::Formula const& formula,
                                                                                                               storm::Environment const& preProcEnv) {
    auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(pomdp(), formula);

    // Compute some initial bounds on the values for each state of the pomdp
    // We work with the Belief MDP value type, so if the POMDP is exact, but the belief MDP is not, we need to convert
    auto preProcessingMC = PreprocessingPomdpValueBoundsModelChecker<ValueType>(pomdp());
    auto initialPomdpValueBounds = preProcessingMC.getValueBounds(preProcEnv, formula);
    pomdpValueBounds.trivialPomdpValueBounds = initialPomdpValueBounds;

    // If we clip and compute rewards, compute the values necessary for the correction terms
    if (options.useClipping && formula.isRewardOperatorFormula()) {
        pomdpValueBounds.extremePomdpValueBound = preProcessingMC.getExtremeValueBound(preProcEnv, formula);
    }
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
typename BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::check(
    storm::Environment const& env, storm::logic::Formula const& formula,
    std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> const& additionalUnderApproximationBounds) {
    return check(env, formula, env, additionalUnderApproximationBounds);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
typename BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::check(
    storm::logic::Formula const& formula, std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> const& additionalUnderApproximationBounds) {
    storm::Environment env;
    return check(env, formula, env, additionalUnderApproximationBounds);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
typename BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::check(
    storm::logic::Formula const& formula, storm::Environment const& preProcEnv,
    std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> const& additionalUnderApproximationBounds) {
    storm::Environment env;
    return check(env, formula, preProcEnv, additionalUnderApproximationBounds);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
typename BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::check(
    storm::Environment const& env, storm::logic::Formula const& formula, storm::Environment const& preProcEnv,
    std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> const& additionalUnderApproximationBounds) {
    STORM_LOG_ASSERT(options.unfold || options.discretize || options.interactiveUnfolding,
                     "Invoked belief exploration but no task (unfold or discretize) given.");
    // Potentially reset preprocessed model from previous call
    preprocessedPomdp.reset();

    // Reset all collected statistics
    statistics = Statistics();
    statistics.totalTime.start();
    // Extract the relevant information from the formula
    auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(pomdp(), formula);

    precomputeValueBounds(formula, preProcEnv);
    if (!additionalUnderApproximationBounds.empty()) {
        pomdpValueBounds.fmSchedulerValueList = additionalUnderApproximationBounds;
    }
    uint64_t initialPomdpState = pomdp().getInitialStates().getNextSetIndex(0);
    Result result(pomdpValueBounds.trivialPomdpValueBounds.getHighestLowerBound(initialPomdpState),
                  pomdpValueBounds.trivialPomdpValueBounds.getSmallestUpperBound(initialPomdpState));
    STORM_LOG_INFO("Initial value bounds are [" << result.lowerBound << ", " << result.upperBound << "]");

    std::optional<std::string> rewardModelName;
    std::set<uint32_t> targetObservations;
    if (formulaInfo.isNonNestedReachabilityProbability() || formulaInfo.isNonNestedExpectedRewardFormula()) {
        if (formulaInfo.getTargetStates().observationClosed) {
            targetObservations = formulaInfo.getTargetStates().observations;
        } else {
            storm::transformer::MakeStateSetObservationClosed<ValueType> obsCloser(inputPomdp);
            std::tie(preprocessedPomdp, targetObservations) = obsCloser.transform(formulaInfo.getTargetStates().states);
        }
        if (formulaInfo.isNonNestedReachabilityProbability()) {
            if (!formulaInfo.getSinkStates().empty()) {
                storm::storage::sparse::ModelComponents<ValueType> components;
                components.stateLabeling = pomdp().getStateLabeling();
                components.rewardModels = pomdp().getRewardModels();
                auto matrix = pomdp().getTransitionMatrix();
                matrix.makeRowGroupsAbsorbing(formulaInfo.getSinkStates().states);
                components.transitionMatrix = matrix;
                components.observabilityClasses = pomdp().getObservations();
                if (pomdp().hasChoiceLabeling()) {
                    components.choiceLabeling = pomdp().getChoiceLabeling();
                }
                if (pomdp().hasObservationValuations()) {
                    components.observationValuations = pomdp().getObservationValuations();
                }
                preprocessedPomdp = std::make_shared<storm::models::sparse::Pomdp<ValueType>>(std::move(components), true);
                auto reachableFromSinkStates = storm::utility::graph::getReachableStates(
                    pomdp().getTransitionMatrix(), formulaInfo.getSinkStates().states, formulaInfo.getSinkStates().states, ~formulaInfo.getSinkStates().states);
                reachableFromSinkStates &= ~formulaInfo.getSinkStates().states;
                STORM_LOG_THROW(reachableFromSinkStates.empty(), storm::exceptions::NotSupportedException,
                                "There are sink states that can reach non-sink states. This is currently not supported.");
            }
        } else {
            // Expected reward formula!
            rewardModelName = formulaInfo.getRewardModelName();
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unsupported formula '" << formula << "'.");
    }
    if (storm::pomdp::detectFiniteBeliefMdp(pomdp(), formulaInfo.getTargetStates().states)) {
        STORM_LOG_INFO("Detected that the belief MDP is finite.");
        statistics.beliefMdpDetectedToBeFinite = true;
    }
    if (options.interactiveUnfolding) {
        unfoldInteractively(env, targetObservations, formulaInfo.minimize(), rewardModelName, pomdpValueBounds, result);
    } else {
        refineReachability(env, targetObservations, formulaInfo.minimize(), rewardModelName, pomdpValueBounds, result);
    }
    // "clear" results in case they were actually not requested (this will make the output a bit more clear)
    if ((formulaInfo.minimize() && !options.discretize) || (formulaInfo.maximize() && !options.unfold)) {
        result.lowerBound = -storm::utility::infinity<ValueType>();
    }
    if ((formulaInfo.maximize() && !options.discretize) || (formulaInfo.minimize() && !options.unfold)) {
        result.upperBound = storm::utility::infinity<ValueType>();
    }

    if (storm::utility::resources::isTerminate()) {
        statistics.aborted = true;
    }
    statistics.totalTime.stop();
    return result;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::printStatisticsToStream(std::ostream& stream) const {
    stream << "##### POMDP Approximation Statistics ######\n";
    stream << "# Input model: \n";
    pomdp().printModelInformationToStream(stream);
    stream << "# Max. Number of states with same observation: " << pomdp().getMaxNrStatesWithSameObservation() << '\n';
    if (statistics.beliefMdpDetectedToBeFinite) {
        stream << "# Pre-computations detected that the belief MDP is finite.\n";
    }
    if (statistics.aborted) {
        stream << "# Computation aborted early\n";
    }

    stream << "# Total check time: " << statistics.totalTime << '\n';
    // Refinement information:
    if (statistics.refinementSteps) {
        stream << "# Number of refinement steps: " << statistics.refinementSteps.value() << '\n';
    }
    if (statistics.refinementFixpointDetected) {
        stream << "# Detected a refinement fixpoint.\n";
    }

    // The overapproximation MDP:
    if (statistics.overApproximationStates) {
        stream << "# Number of states in the ";
        if (options.refine) {
            stream << "final ";
        }
        stream << "grid MDP for the over-approximation: ";
        if (statistics.overApproximationBuildAborted) {
            stream << ">=";
        }
        stream << statistics.overApproximationStates.value() << '\n';
        stream << "# Maximal resolution for over-approximation: " << statistics.overApproximationMaxResolution.value() << '\n';
        stream << "# Time spend for building the over-approx grid MDP(s): " << statistics.overApproximationBuildTime << '\n';
        stream << "# Time spend for checking the over-approx grid MDP(s): " << statistics.overApproximationCheckTime << '\n';
    }

    // The underapproximation MDP:
    if (statistics.underApproximationStates) {
        stream << "# Number of states in the ";
        if (options.refine) {
            stream << "final ";
        }
        stream << "belief MDP for the under-approximation: ";
        if (statistics.underApproximationBuildAborted) {
            stream << ">=";
        }
        stream << statistics.underApproximationStates.value() << '\n';
        if (statistics.nrClippingAttempts) {
            stream << "# Clipping attempts (clipped states) for the under-approximation: ";
            if (statistics.underApproximationBuildAborted) {
                stream << ">=";
            }
            stream << statistics.nrClippingAttempts.value() << " (" << statistics.nrClippedStates.value() << ")\n";
            stream << "# Total clipping preprocessing time: " << statistics.clippingPreTime << "\n";
            stream << "# Total clipping time: " << statistics.clipWatch << "\n";
        } else if (statistics.nrTruncatedStates) {
            stream << "# Truncated states for the under-approximation: ";
            if (statistics.underApproximationBuildAborted) {
                stream << ">=";
            }
            stream << statistics.nrTruncatedStates.value() << "\n";
        }
        if (statistics.underApproximationStateLimit) {
            stream << "# Exploration state limit for under-approximation: " << statistics.underApproximationStateLimit.value() << '\n';
        }
        stream << "# Time spend for building the under-approx grid MDP(s): " << statistics.underApproximationBuildTime << '\n';
        stream << "# Time spend for checking the under-approx grid MDP(s): " << statistics.underApproximationCheckTime << '\n';
    }

    stream << "##########################################\n";
}

/* Private Functions */

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
PomdpModelType const& BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::pomdp() const {
    if (preprocessedPomdp) {
        return *preprocessedPomdp;
    } else {
        return *inputPomdp;
    }
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::refineReachability(
    storm::Environment const& env, std::set<uint32_t> const& targetObservations, bool min, std::optional<std::string> rewardModelName,
    storm::pomdp::modelchecker::POMDPValueBounds<ValueType> const& valueBounds, Result& result) {
    statistics.refinementSteps = 0;
    auto trivialPOMDPBounds = valueBounds.trivialPomdpValueBounds;
    // Set up exploration data
    std::vector<BeliefValueType> observationResolutionVector;
    std::shared_ptr<BeliefManagerType> overApproxBeliefManager;
    std::shared_ptr<ExplorerType> overApproximation;
    HeuristicParameters overApproxHeuristicPar{};
    if (options.discretize) {  // Setup and build first OverApproximation
        observationResolutionVector =
            std::vector<BeliefValueType>(pomdp().getNrObservations(), storm::utility::convertNumber<BeliefValueType>(options.resolutionInit));
        overApproxBeliefManager = std::make_shared<BeliefManagerType>(
            pomdp(), storm::utility::convertNumber<BeliefValueType>(options.numericPrecision),
            options.dynamicTriangulation ? BeliefManagerType::TriangulationMode::Dynamic : BeliefManagerType::TriangulationMode::Static);
        if (rewardModelName) {
            overApproxBeliefManager->setRewardModel(rewardModelName);
        }
        overApproximation = std::make_shared<ExplorerType>(overApproxBeliefManager, trivialPOMDPBounds, storm::builder::ExplorationHeuristic::BreadthFirst);
        overApproxHeuristicPar.gapThreshold = options.gapThresholdInit;
        overApproxHeuristicPar.observationThreshold = options.obsThresholdInit;
        overApproxHeuristicPar.sizeThreshold = options.sizeThresholdInit == 0 ? std::numeric_limits<uint64_t>::max() : options.sizeThresholdInit;
        overApproxHeuristicPar.optimalChoiceValueEpsilon = options.optimalChoiceValueThresholdInit;

        buildOverApproximation(env, targetObservations, min, rewardModelName.has_value(), false, overApproxHeuristicPar, observationResolutionVector,
                               overApproxBeliefManager, overApproximation);
        if (!overApproximation->hasComputedValues() || storm::utility::resources::isTerminate()) {
            return;
        }
        ValueType const& newValue = overApproximation->getComputedValueAtInitialState();
        bool betterBound = min ? result.updateLowerBound(newValue) : result.updateUpperBound(newValue);
        if (betterBound) {
            STORM_LOG_INFO("Initial Over-approx result obtained after " << statistics.totalTime << ". Value is '" << newValue << "'.\n");
        }
    }

    std::shared_ptr<BeliefManagerType> underApproxBeliefManager;
    std::shared_ptr<ExplorerType> underApproximation;
    HeuristicParameters underApproxHeuristicPar{};
    if (options.unfold) {  // Setup and build first UnderApproximation
        underApproxBeliefManager = std::make_shared<BeliefManagerType>(
            pomdp(), storm::utility::convertNumber<BeliefValueType>(options.numericPrecision),
            options.dynamicTriangulation ? BeliefManagerType::TriangulationMode::Dynamic : BeliefManagerType::TriangulationMode::Static);
        if (rewardModelName) {
            underApproxBeliefManager->setRewardModel(rewardModelName);
        }
        underApproximation = std::make_shared<ExplorerType>(underApproxBeliefManager, trivialPOMDPBounds, options.explorationHeuristic);
        underApproxHeuristicPar.gapThreshold = options.gapThresholdInit;
        underApproxHeuristicPar.optimalChoiceValueEpsilon = options.optimalChoiceValueThresholdInit;
        underApproxHeuristicPar.sizeThreshold = options.sizeThresholdInit;
        if (underApproxHeuristicPar.sizeThreshold == 0) {
            if (!options.refine && options.explorationTimeLimit != 0) {
                underApproxHeuristicPar.sizeThreshold = std::numeric_limits<uint64_t>::max();
            } else {
                underApproxHeuristicPar.sizeThreshold = pomdp().getNumberOfStates() * pomdp().getMaxNrStatesWithSameObservation();
                STORM_LOG_INFO("Heuristically selected an under-approximation MDP size threshold of " << underApproxHeuristicPar.sizeThreshold << ".\n");
            }
        }

        if (options.useClipping && rewardModelName.has_value()) {
            underApproximation->setExtremeValueBound(valueBounds.extremePomdpValueBound);
        }
        if (!valueBounds.fmSchedulerValueList.empty()) {
            underApproximation->setFMSchedValueList(valueBounds.fmSchedulerValueList);
        }
        buildUnderApproximation(env, targetObservations, min, rewardModelName.has_value(), false, underApproxHeuristicPar, underApproxBeliefManager,
                                underApproximation, false);
        if (!underApproximation->hasComputedValues() || storm::utility::resources::isTerminate()) {
            return;
        }
        ValueType const& newValue = underApproximation->getComputedValueAtInitialState();
        bool betterBound = min ? result.updateUpperBound(newValue) : result.updateLowerBound(newValue);
        if (betterBound) {
            STORM_LOG_INFO("Initial Under-approx result obtained after " << statistics.totalTime << ". Value is '" << newValue << "'.\n");
        }
    }

    // Do some output
    STORM_LOG_INFO("Completed (initial) computation. Current checktime is " << statistics.totalTime << ".");
    bool computingLowerBound = false;
    bool computingUpperBound = false;
    if (options.discretize) {
        STORM_LOG_INFO("\tOver-approx MDP has size " << overApproximation->getExploredMdp()->getNumberOfStates() << ".");
        (min ? computingLowerBound : computingUpperBound) = true;
    }
    if (options.unfold) {
        STORM_LOG_INFO("\tUnder-approx MDP has size " << underApproximation->getExploredMdp()->getNumberOfStates() << ".");
        (min ? computingUpperBound : computingLowerBound) = true;
    }
    if (computingLowerBound && computingUpperBound) {
        STORM_LOG_INFO("\tObtained result is [" << result.lowerBound << ", " << result.upperBound << "].");
    } else if (computingLowerBound) {
        STORM_LOG_INFO("\tObtained result is ≥" << result.lowerBound << ".");
    } else if (computingUpperBound) {
        STORM_LOG_INFO("\tObtained result is ≤" << result.upperBound << ".");
    }

    // Start refinement
    if (options.refine) {
        STORM_LOG_WARN_COND(options.refineStepLimit != 0 || !storm::utility::isZero(options.refinePrecision),
                            "No termination criterion for refinement given. Consider to specify a steplimit, a non-zero precisionlimit, or a timeout");
        STORM_LOG_WARN_COND(storm::utility::isZero(options.refinePrecision) || (options.unfold && options.discretize),
                            "Refinement goal precision is given, but only one bound is going to be refined.");
        while ((options.refineStepLimit == 0 || statistics.refinementSteps.value() < options.refineStepLimit) && result.diff() > options.refinePrecision) {
            bool overApproxFixPoint = true;
            bool underApproxFixPoint = true;
            if (options.discretize) {
                // Refine over-approximation
                if (min) {
                    overApproximation->takeCurrentValuesAsLowerBounds();
                } else {
                    overApproximation->takeCurrentValuesAsUpperBounds();
                }
                overApproxHeuristicPar.gapThreshold *= options.gapThresholdFactor;
                overApproxHeuristicPar.sizeThreshold = storm::utility::convertNumber<uint64_t, ValueType>(
                    storm::utility::convertNumber<ValueType, uint64_t>(overApproximation->getExploredMdp()->getNumberOfStates()) * options.sizeThresholdFactor);
                overApproxHeuristicPar.observationThreshold +=
                    options.obsThresholdIncrementFactor * (storm::utility::one<ValueType>() - overApproxHeuristicPar.observationThreshold);
                overApproxHeuristicPar.optimalChoiceValueEpsilon *= options.optimalChoiceValueThresholdFactor;
                overApproxFixPoint = buildOverApproximation(env, targetObservations, min, rewardModelName.has_value(), true, overApproxHeuristicPar,
                                                            observationResolutionVector, overApproxBeliefManager, overApproximation);
                if (overApproximation->hasComputedValues() && !storm::utility::resources::isTerminate()) {
                    ValueType const& newValue = overApproximation->getComputedValueAtInitialState();
                    bool betterBound = min ? result.updateLowerBound(newValue) : result.updateUpperBound(newValue);
                    if (betterBound) {
                        STORM_LOG_INFO("Over-approx result for refinement improved after " << statistics.totalTime << " in refinement step #"
                                                                                           << (statistics.refinementSteps.value() + 1) << ". New value is '"
                                                                                           << newValue << "'.");
                    }
                } else {
                    break;
                }
            }

            if (options.unfold && result.diff() > options.refinePrecision) {
                // Refine under-approximation
                underApproxHeuristicPar.gapThreshold *= options.gapThresholdFactor;
                underApproxHeuristicPar.sizeThreshold = storm::utility::convertNumber<uint64_t, ValueType>(
                    storm::utility::convertNumber<ValueType, uint64_t>(underApproximation->getExploredMdp()->getNumberOfStates()) *
                    options.sizeThresholdFactor);
                underApproxHeuristicPar.optimalChoiceValueEpsilon *= options.optimalChoiceValueThresholdFactor;
                underApproxFixPoint = buildUnderApproximation(env, targetObservations, min, rewardModelName.has_value(), true, underApproxHeuristicPar,
                                                              underApproxBeliefManager, underApproximation, true);
                if (underApproximation->hasComputedValues() && !storm::utility::resources::isTerminate()) {
                    ValueType const& newValue = underApproximation->getComputedValueAtInitialState();
                    bool betterBound = min ? result.updateUpperBound(newValue) : result.updateLowerBound(newValue);
                    if (betterBound) {
                        STORM_LOG_INFO("Under-approx result for refinement improved after " << statistics.totalTime << " in refinement step #"
                                                                                            << (statistics.refinementSteps.value() + 1) << ". New value is '"
                                                                                            << newValue << "'.");
                    }
                } else {
                    break;
                }
            }

            if (storm::utility::resources::isTerminate()) {
                break;
            } else {
                ++statistics.refinementSteps.value();
                // Don't make too many outputs (to avoid logfile clutter)
                if (statistics.refinementSteps.value() <= 1000) {
                    STORM_LOG_INFO("Completed iteration #" << statistics.refinementSteps.value() << ". Current checktime is " << statistics.totalTime << ".");
                    computingLowerBound = false;
                    computingUpperBound = false;
                    if (options.discretize) {
                        STORM_LOG_INFO("\tOver-approx MDP has size " << overApproximation->getExploredMdp()->getNumberOfStates() << ".");
                        (min ? computingLowerBound : computingUpperBound) = true;
                    }
                    if (options.unfold) {
                        STORM_LOG_INFO("\tUnder-approx MDP has size " << underApproximation->getExploredMdp()->getNumberOfStates() << ".");
                        (min ? computingUpperBound : computingLowerBound) = true;
                    }
                    if (computingLowerBound && computingUpperBound) {
                        STORM_LOG_INFO("\tCurrent result is [" << result.lowerBound << ", " << result.upperBound << "].");
                    } else if (computingLowerBound) {
                        STORM_LOG_INFO("\tCurrent result is ≥" << result.lowerBound << ".");
                    } else if (computingUpperBound) {
                        STORM_LOG_INFO("\tCurrent result is ≤" << result.upperBound << ".");
                    }
                    STORM_LOG_WARN_COND(statistics.refinementSteps.value() < 1000, "Refinement requires  more than 1000 iterations.");
                }
            }
            if (overApproxFixPoint && underApproxFixPoint) {
                STORM_LOG_INFO("Refinement fixpoint reached after " << statistics.refinementSteps.value() << " iterations.\n");
                statistics.refinementFixpointDetected = true;
                break;
            }
        }
    }
    // Print model information of final over- / under-approximation MDP
    if (options.discretize && overApproximation->hasComputedValues()) {
        auto printOverInfo = [&overApproximation]() {
            std::stringstream str;
            str << "Explored and checked Over-Approximation MDP:\n";
            overApproximation->getExploredMdp()->printModelInformationToStream(str);
            return str.str();
        };
        STORM_LOG_INFO(printOverInfo());
    }
    if (options.unfold && underApproximation->hasComputedValues()) {
        auto printUnderInfo = [&underApproximation]() {
            std::stringstream str;
            str << "Explored and checked Under-Approximation MDP:\n";
            underApproximation->getExploredMdp()->printModelInformationToStream(str);
            return str.str();
        };
        STORM_LOG_INFO(printUnderInfo());
        std::shared_ptr<storm::models::sparse::Model<ValueType>> scheduledModel = underApproximation->getExploredMdp();
        if (!options.useStateEliminationCutoff) {
            storm::models::sparse::StateLabeling newLabeling(scheduledModel->getStateLabeling());
            auto nrPreprocessingScheds = min ? underApproximation->getNrSchedulersForUpperBounds() : underApproximation->getNrSchedulersForLowerBounds();
            for (uint64_t i = 0; i < nrPreprocessingScheds; ++i) {
                newLabeling.addLabel("sched_" + std::to_string(i));
            }
            newLabeling.addLabel("cutoff");
            newLabeling.addLabel("clipping");
            newLabeling.addLabel("finite_mem");

            auto transMatrix = scheduledModel->getTransitionMatrix();
            for (uint64_t i = 0; i < scheduledModel->getNumberOfStates(); ++i) {
                if (newLabeling.getStateHasLabel("truncated", i)) {
                    uint64_t localChosenActionIndex = underApproximation->getSchedulerForExploredMdp()->getChoice(i).getDeterministicChoice();
                    auto rowIndex = scheduledModel->getTransitionMatrix().getRowGroupIndices()[i];
                    if (scheduledModel->getChoiceLabeling().getLabelsOfChoice(rowIndex + localChosenActionIndex).size() > 0) {
                        auto label = *(scheduledModel->getChoiceLabeling().getLabelsOfChoice(rowIndex + localChosenActionIndex).begin());
                        if (label.rfind("clip", 0) == 0) {
                            newLabeling.addLabelToState("clipping", i);
                            auto chosenRow = transMatrix.getRow(i, 0);
                            auto candidateIndex = (chosenRow.end() - 1)->getColumn();
                            transMatrix.makeRowDirac(transMatrix.getRowGroupIndices()[i], candidateIndex);
                        } else if (label.rfind("mem_node", 0) == 0) {
                            if (!newLabeling.containsLabel("finite_mem_" + label.substr(9, 1))) {
                                newLabeling.addLabel("finite_mem_" + label.substr(9, 1));
                            }
                            newLabeling.addLabelToState("finite_mem_" + label.substr(9, 1), i);
                            newLabeling.addLabelToState("cutoff", i);
                        } else {
                            newLabeling.addLabelToState(label, i);
                            newLabeling.addLabelToState("cutoff", i);
                        }
                    }
                }
            }
            newLabeling.removeLabel("truncated");

            transMatrix.dropZeroEntries();
            storm::storage::sparse::ModelComponents<ValueType> modelComponents(transMatrix, newLabeling);
            if (scheduledModel->hasChoiceLabeling()) {
                modelComponents.choiceLabeling = scheduledModel->getChoiceLabeling();
            }
            storm::models::sparse::Mdp<ValueType> newMDP(modelComponents);
            auto inducedMC = newMDP.applyScheduler(*(underApproximation->getSchedulerForExploredMdp()), true);
            scheduledModel = std::static_pointer_cast<storm::models::sparse::Model<ValueType>>(inducedMC);
        } else {
            auto inducedMC = underApproximation->getExploredMdp()->applyScheduler(*(underApproximation->getSchedulerForExploredMdp()), true);
            scheduledModel = std::static_pointer_cast<storm::models::sparse::Model<ValueType>>(inducedMC);
        }
        result.schedulerAsMarkovChain = scheduledModel;
        if (min) {
            result.cutoffSchedulers = underApproximation->getUpperValueBoundSchedulers();
        } else {
            result.cutoffSchedulers = underApproximation->getLowerValueBoundSchedulers();
        }
    }
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::unfoldInteractively(
    storm::Environment const& env, std::set<uint32_t> const& targetObservations, bool min, std::optional<std::string> rewardModelName,
    storm::pomdp::modelchecker::POMDPValueBounds<ValueType> const& valueBounds, Result& result) {
    statistics.refinementSteps = 0;
    interactiveResult = result;
    unfoldingStatus = Status::Uninitialized;
    unfoldingControl = UnfoldingControl::Run;
    auto trivialPOMDPBounds = valueBounds.trivialPomdpValueBounds;
    // Set up exploration data
    std::shared_ptr<BeliefManagerType> underApproxBeliefManager;
    HeuristicParameters underApproxHeuristicPar{};
    bool firstIteration = true;
    // Set up belief manager
    underApproxBeliefManager = std::make_shared<BeliefManagerType>(
        pomdp(), storm::utility::convertNumber<BeliefValueType>(options.numericPrecision),
        options.dynamicTriangulation ? BeliefManagerType::TriangulationMode::Dynamic : BeliefManagerType::TriangulationMode::Static);
    if (rewardModelName) {
        underApproxBeliefManager->setRewardModel(rewardModelName);
    }

    // set up belief MDP explorer
    interactiveUnderApproximationExplorer = std::make_shared<ExplorerType>(underApproxBeliefManager, trivialPOMDPBounds, options.explorationHeuristic);
    underApproxHeuristicPar.gapThreshold = options.gapThresholdInit;
    underApproxHeuristicPar.optimalChoiceValueEpsilon = options.optimalChoiceValueThresholdInit;
    underApproxHeuristicPar.sizeThreshold = std::numeric_limits<uint64_t>::max() - 1;  // we don't set a size threshold

    if (options.useClipping && rewardModelName.has_value()) {
        interactiveUnderApproximationExplorer->setExtremeValueBound(valueBounds.extremePomdpValueBound);
    }

    if (!valueBounds.fmSchedulerValueList.empty()) {
        interactiveUnderApproximationExplorer->setFMSchedValueList(valueBounds.fmSchedulerValueList);
    }

    // Start iteration
    while (!(unfoldingControl ==
             storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::UnfoldingControl::Terminate)) {
        bool underApproxFixPoint = true;
        bool hasTruncatedStates = false;
        if (unfoldingStatus != Status::Converged) {
            // Continue unfolding underapproximation
            underApproxFixPoint = buildUnderApproximation(env, targetObservations, min, rewardModelName.has_value(), false, underApproxHeuristicPar,
                                                          underApproxBeliefManager, interactiveUnderApproximationExplorer, firstIteration);
            if (interactiveUnderApproximationExplorer->hasComputedValues() && !storm::utility::resources::isTerminate()) {
                ValueType const& newValue = interactiveUnderApproximationExplorer->getComputedValueAtInitialState();
                bool betterBound = min ? interactiveResult.updateUpperBound(newValue) : interactiveResult.updateLowerBound(newValue);
                if (betterBound) {
                    STORM_LOG_INFO("Under-approximation result improved after " << statistics.totalTime << " in step #"
                                                                                << (statistics.refinementSteps.value() + 1) << ". New value is '" << newValue
                                                                                << "'.");
                }
                std::shared_ptr<storm::models::sparse::Model<ValueType>> scheduledModel = interactiveUnderApproximationExplorer->getExploredMdp();
                if (!options.useStateEliminationCutoff) {
                    storm::models::sparse::StateLabeling newLabeling(scheduledModel->getStateLabeling());
                    auto nrPreprocessingScheds = min ? interactiveUnderApproximationExplorer->getNrSchedulersForUpperBounds()
                                                     : interactiveUnderApproximationExplorer->getNrSchedulersForLowerBounds();
                    for (uint64_t i = 0; i < nrPreprocessingScheds; ++i) {
                        newLabeling.addLabel("sched_" + std::to_string(i));
                    }
                    newLabeling.addLabel("cutoff");
                    newLabeling.addLabel("clipping");
                    newLabeling.addLabel("finite_mem");

                    auto transMatrix = scheduledModel->getTransitionMatrix();
                    for (uint64_t i = 0; i < scheduledModel->getNumberOfStates(); ++i) {
                        if (newLabeling.getStateHasLabel("truncated", i)) {
                            hasTruncatedStates = true;
                            uint64_t localChosenActionIndex =
                                interactiveUnderApproximationExplorer->getSchedulerForExploredMdp()->getChoice(i).getDeterministicChoice();
                            auto rowIndex = scheduledModel->getTransitionMatrix().getRowGroupIndices()[i];
                            if (scheduledModel->getChoiceLabeling().getLabelsOfChoice(rowIndex + localChosenActionIndex).size() > 0) {
                                auto label = *(scheduledModel->getChoiceLabeling().getLabelsOfChoice(rowIndex + localChosenActionIndex).begin());
                                if (label.rfind("clip", 0) == 0) {
                                    newLabeling.addLabelToState("clipping", i);
                                    auto chosenRow = transMatrix.getRow(i, 0);
                                    auto candidateIndex = (chosenRow.end() - 1)->getColumn();
                                    transMatrix.makeRowDirac(transMatrix.getRowGroupIndices()[i], candidateIndex);
                                } else if (label.rfind("mem_node", 0) == 0) {
                                    if (!newLabeling.containsLabel("finite_mem_" + label.substr(9, 1))) {
                                        newLabeling.addLabel("finite_mem_" + label.substr(9, 1));
                                    }
                                    newLabeling.addLabelToState("finite_mem_" + label.substr(9, 1), i);
                                    newLabeling.addLabelToState("cutoff", i);
                                } else {
                                    newLabeling.addLabelToState(label, i);
                                    newLabeling.addLabelToState("cutoff", i);
                                }
                            }
                        }
                    }
                    newLabeling.removeLabel("truncated");

                    transMatrix.dropZeroEntries();
                    storm::storage::sparse::ModelComponents<ValueType> modelComponents(transMatrix, newLabeling);
                    if (scheduledModel->hasChoiceLabeling()) {
                        modelComponents.choiceLabeling = scheduledModel->getChoiceLabeling();
                    }
                    storm::models::sparse::Mdp<ValueType> newMDP(modelComponents);
                    auto inducedMC = newMDP.applyScheduler(*(interactiveUnderApproximationExplorer->getSchedulerForExploredMdp()), true);
                    scheduledModel = std::static_pointer_cast<storm::models::sparse::Model<ValueType>>(inducedMC);
                }
                interactiveResult.schedulerAsMarkovChain = scheduledModel;
                if (min) {
                    interactiveResult.cutoffSchedulers = interactiveUnderApproximationExplorer->getUpperValueBoundSchedulers();
                } else {
                    interactiveResult.cutoffSchedulers = interactiveUnderApproximationExplorer->getLowerValueBoundSchedulers();
                }
                if (firstIteration) {
                    firstIteration = false;
                }
                unfoldingStatus = Status::ResultAvailable;
            } else {
                break;
            }

            if (storm::utility::resources::isTerminate()) {
                break;
            } else {
                ++statistics.refinementSteps.value();
                // Don't make too many outputs (to avoid logfile clutter)
                if (statistics.refinementSteps.value() <= 1000) {
                    STORM_LOG_INFO("Completed iteration #" << statistics.refinementSteps.value() << ". Current checktime is " << statistics.totalTime << ".");
                    bool computingLowerBound = false;
                    bool computingUpperBound = false;
                    if (options.unfold) {
                        STORM_LOG_INFO("\tUnder-approx MDP has size " << interactiveUnderApproximationExplorer->getExploredMdp()->getNumberOfStates() << ".");
                        (min ? computingUpperBound : computingLowerBound) = true;
                    }
                    if (computingLowerBound && computingUpperBound) {
                        STORM_LOG_INFO("\tCurrent result is [" << interactiveResult.lowerBound << ", " << interactiveResult.upperBound << "].");
                    } else if (computingLowerBound) {
                        STORM_LOG_INFO("\tCurrent result is ≥" << interactiveResult.lowerBound << ".");
                    } else if (computingUpperBound) {
                        STORM_LOG_INFO("\tCurrent result is ≤" << interactiveResult.upperBound << ".");
                    }
                }
            }
            if (underApproxFixPoint) {
                STORM_LOG_INFO("Fixpoint reached after " << statistics.refinementSteps.value() << " iterations.\n");
                statistics.refinementFixpointDetected = true;
                unfoldingStatus = Status::Converged;
                unfoldingControl = UnfoldingControl::Pause;
            }
            if (!hasTruncatedStates) {
                STORM_LOG_INFO("No states have been truncated, so continued iteration does not yield new results.\n");
                unfoldingStatus = Status::Converged;
                unfoldingControl = UnfoldingControl::Pause;
            }
        }
        // While we tell the procedure to be paused, idle
        while (unfoldingControl ==
                   storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::UnfoldingControl::Pause &&
               !storm::utility::resources::isTerminate()) {
            // Intentionally left empty
        }
    }
    STORM_LOG_INFO("\tInteractive Unfolding terminated.\n");
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::unfoldInteractively(
    std::set<uint32_t> const& targetObservations, bool min, std::optional<std::string> rewardModelName,
    storm::pomdp::modelchecker::POMDPValueBounds<ValueType> const& valueBounds, Result& result) {
    storm::Environment env;
    unfoldInteractively(env, targetObservations, min, rewardModelName, valueBounds, result);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
typename BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::Result
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::getInteractiveResult() {
    return interactiveResult;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
int64_t BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::getStatus() {
    if (unfoldingStatus == Status::Uninitialized) {
        return 0;
    }
    if (unfoldingStatus == Status::Exploring) {
        return 1;
    }
    if (unfoldingStatus == Status::ModelExplorationFinished) {
        return 2;
    }
    if (unfoldingStatus == Status::ResultAvailable) {
        return 3;
    }
    if (unfoldingStatus == Status::Terminated) {
        return 4;
    }

    return -1;
}

template<typename ValueType>
ValueType getGap(ValueType const& l, ValueType const& u) {
    STORM_LOG_ASSERT(l >= storm::utility::zero<ValueType>() && u >= storm::utility::zero<ValueType>(),
                     "Gap computation currently does not handle negative values.");
    if (storm::utility::isInfinity(u)) {
        if (storm::utility::isInfinity(l)) {
            return storm::utility::zero<ValueType>();
        } else {
            return u;
        }
    } else if (storm::utility::isZero(u)) {
        STORM_LOG_ASSERT(storm::utility::isZero(l), "Upper bound is zero but lower bound is " << l << ".");
        return u;
    } else {
        STORM_LOG_ASSERT(!storm::utility::isInfinity(l), "Lower bound is infinity, but upper bound is " << u << ".");
        // get the relative gap
        return storm::utility::abs<ValueType>(u - l) * storm::utility::convertNumber<ValueType, uint64_t>(2) / (l + u);
    }
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::buildOverApproximation(
    storm::Environment const& env, std::set<uint32_t> const& targetObservations, bool min, bool computeRewards, bool refine,
    HeuristicParameters const& heuristicParameters, std::vector<BeliefValueType>& observationResolutionVector,
    std::shared_ptr<BeliefManagerType>& beliefManager, std::shared_ptr<ExplorerType>& overApproximation) {
    // Detect whether the refinement reached a fixpoint.
    bool fixPoint = true;

    statistics.overApproximationBuildTime.start();
    storm::storage::BitVector refinedObservations;
    if (!refine) {
        // If we build the model from scratch, we first have to set up the explorer for the overApproximation.
        if (computeRewards) {
            overApproximation->startNewExploration(storm::utility::zero<ValueType>());
        } else {
            overApproximation->startNewExploration(storm::utility::one<ValueType>(), storm::utility::zero<ValueType>());
        }
    } else {
        // If we refine the existing overApproximation, our heuristic also wants to know which states are reachable under an optimal policy
        overApproximation->computeOptimalChoicesAndReachableMdpStates(heuristicParameters.optimalChoiceValueEpsilon, true);
        // We also need to find out which observation resolutions needs refinement.
        // current maximal resolution (needed for refinement heuristic)
        auto obsRatings = getObservationRatings(overApproximation, observationResolutionVector);
        // If there is a score < 1, we have not reached a fixpoint, yet
        auto numericPrecision = storm::utility::convertNumber<BeliefValueType>(options.numericPrecision);
        if (std::any_of(obsRatings.begin(), obsRatings.end(),
                        [&numericPrecision](BeliefValueType const& value) { return value + numericPrecision < storm::utility::one<BeliefValueType>(); })) {
            STORM_LOG_INFO_COND(!fixPoint, "Not reaching a refinement fixpoint because there are still observations to refine.");
            fixPoint = false;
        }
        refinedObservations = storm::utility::vector::filter<BeliefValueType>(obsRatings, [&heuristicParameters](BeliefValueType const& r) {
            return r <= storm::utility::convertNumber<BeliefValueType>(heuristicParameters.observationThreshold);
        });
        STORM_LOG_DEBUG("Refining the resolution of " << refinedObservations.getNumberOfSetBits() << "/" << refinedObservations.size() << " observations.");
        for (auto const obs : refinedObservations) {
            // Increment the resolution at the refined observations.
            // Use storm's rational number to detect overflows properly.
            storm::RationalNumber newObsResolutionAsRational = storm::utility::convertNumber<storm::RationalNumber>(observationResolutionVector[obs]) *
                                                               storm::utility::convertNumber<storm::RationalNumber>(options.resolutionFactor);
            static_assert(storm::NumberTraits<BeliefValueType>::IsExact || std::is_same<BeliefValueType, double>::value, "Unhandled belief value type");
            if (!storm::NumberTraits<BeliefValueType>::IsExact &&
                newObsResolutionAsRational > storm::utility::convertNumber<storm::RationalNumber>(std::numeric_limits<double>::max())) {
                observationResolutionVector[obs] = storm::utility::convertNumber<BeliefValueType>(std::numeric_limits<double>::max());
            } else {
                observationResolutionVector[obs] = storm::utility::convertNumber<BeliefValueType>(newObsResolutionAsRational);
            }
        }
        overApproximation->restartExploration();
    }
    statistics.overApproximationMaxResolution = storm::utility::ceil(*std::max_element(observationResolutionVector.begin(), observationResolutionVector.end()));

    // Start exploration
    storm::utility::Stopwatch explorationTime;
    if (options.explorationTimeLimit != 0) {
        explorationTime.start();
    }
    bool timeLimitExceeded = false;
    std::map<uint32_t, typename ExplorerType::SuccessorObservationInformation> gatheredSuccessorObservations;  // Declare here to avoid reallocations
    uint64_t numRewiredOrExploredStates = 0;
    while (overApproximation->hasUnexploredState()) {
        if (!timeLimitExceeded && options.explorationTimeLimit != 0 &&
            static_cast<uint64_t>(explorationTime.getTimeInSeconds()) > options.explorationTimeLimit) {
            STORM_LOG_INFO("Exploration time limit exceeded.");
            timeLimitExceeded = true;
            STORM_LOG_INFO_COND(!fixPoint, "Not reaching a refinement fixpoint because the exploration time limit is exceeded.");
            fixPoint = false;
        }

        uint64_t currId = overApproximation->exploreNextState();
        bool hasOldBehavior = refine && overApproximation->currentStateHasOldBehavior();
        if (!hasOldBehavior) {
            STORM_LOG_INFO_COND(!fixPoint, "Not reaching a refinement fixpoint because a new state is explored");
            fixPoint = false;  // Exploring a new state!
        }
        uint32_t currObservation = beliefManager->getBeliefObservation(currId);
        if (targetObservations.count(currObservation) != 0) {
            overApproximation->setCurrentStateIsTarget();
            overApproximation->addSelfloopTransition();
        } else {
            // We need to decide how to treat this state (and each individual enabled action). There are the following cases:
            // 1 The state has no old behavior and
            //   1.1 we explore all actions or
            //   1.2 we truncate all actions
            // 2 The state has old behavior and was truncated in the last iteration and
            //   2.1 we explore all actions or
            //   2.2 we truncate all actions (essentially restoring old behavior, but we do the truncation step again to benefit from updated bounds)
            // 3 The state has old behavior and was not truncated in the last iteration and the current action
            //   3.1 should be rewired or
            //   3.2 should get the old behavior but either
            //       3.2.1 none of the successor observation has been refined since the last rewiring or exploration of this action
            //       3.2.2 rewiring is only delayed as it could still have an effect in a later refinement step

            // Find out in which case we are
            bool exploreAllActions = false;
            bool truncateAllActions = false;
            bool restoreAllActions = false;
            bool checkRewireForAllActions = false;
            // Get the relative gap
            ValueType gap = getGap(overApproximation->getLowerValueBoundAtCurrentState(), overApproximation->getUpperValueBoundAtCurrentState());
            if (!hasOldBehavior) {
                // Case 1
                // If we explore this state and if it has no old behavior, it is clear that an "old" optimal scheduler can be extended to a scheduler that
                // reaches this state
                if (!timeLimitExceeded && gap >= heuristicParameters.gapThreshold && numRewiredOrExploredStates < heuristicParameters.sizeThreshold) {
                    exploreAllActions = true;  // Case 1.1
                } else {
                    truncateAllActions = true;  // Case 1.2
                    overApproximation->setCurrentStateIsTruncated();
                }
            } else if (overApproximation->getCurrentStateWasTruncated()) {
                // Case 2
                if (!timeLimitExceeded && overApproximation->currentStateIsOptimalSchedulerReachable() && gap > heuristicParameters.gapThreshold &&
                    numRewiredOrExploredStates < heuristicParameters.sizeThreshold) {
                    exploreAllActions = true;  // Case 2.1
                    STORM_LOG_INFO_COND(!fixPoint, "Not reaching a refinement fixpoint because a previously truncated state is now explored.");
                    fixPoint = false;
                } else {
                    truncateAllActions = true;  // Case 2.2
                    overApproximation->setCurrentStateIsTruncated();
                    if (fixPoint) {
                        // Properly check whether this can still be a fixpoint
                        if (overApproximation->currentStateIsOptimalSchedulerReachable() && !storm::utility::isZero(gap)) {
                            STORM_LOG_INFO_COND(!fixPoint, "Not reaching a refinement fixpoint because we truncate a state with non-zero gap "
                                                               << gap << " that is reachable via an optimal sched.");
                            fixPoint = false;
                        }
                        // else {}
                        // In this case we truncated a state that is not reachable under optimal schedulers.
                        // If no other state is explored (i.e. fixPoint remains true), these states should still not be reachable in subsequent iterations
                    }
                }
            } else {
                // Case 3
                // The decision for rewiring also depends on the corresponding action, but we have some criteria that lead to case 3.2 (independent of the
                // action)
                if (!timeLimitExceeded && overApproximation->currentStateIsOptimalSchedulerReachable() && gap > heuristicParameters.gapThreshold &&
                    numRewiredOrExploredStates < heuristicParameters.sizeThreshold) {
                    checkRewireForAllActions = true;  // Case 3.1 or Case 3.2
                } else {
                    restoreAllActions = true;  // Definitely Case 3.2
                    // We still need to check for each action whether rewiring makes sense later
                    checkRewireForAllActions = true;
                }
            }
            bool expandedAtLeastOneAction = false;
            for (uint64_t action = 0, numActions = beliefManager->getBeliefNumberOfChoices(currId); action < numActions; ++action) {
                bool expandCurrentAction = exploreAllActions || truncateAllActions;
                if (checkRewireForAllActions) {
                    STORM_LOG_ASSERT(refine, "Expected refine to be true.");
                    // In this case, we still need to check whether this action needs to be expanded
                    STORM_LOG_ASSERT(!expandCurrentAction, "Action should not be expanded.");
                    // Check the action dependent conditions for rewiring
                    // First, check whether this action has been rewired since the last refinement of one of the successor observations (i.e. whether rewiring
                    // would actually change the successor states)
                    STORM_LOG_ASSERT(overApproximation->currentStateHasOldBehavior(), "Expected old behavior.");
                    if (overApproximation->getCurrentStateActionExplorationWasDelayed(action) ||
                        overApproximation->currentStateHasSuccessorObservationInObservationSet(action, refinedObservations)) {
                        // Then, check whether the other criteria for rewiring are satisfied
                        if (!restoreAllActions && overApproximation->actionAtCurrentStateWasOptimal(action)) {
                            // Do the rewiring now! (Case 3.1)
                            expandCurrentAction = true;
                            STORM_LOG_INFO_COND(!fixPoint, "Not reaching a refinement fixpoint because we rewire a state.");
                            fixPoint = false;
                        } else {
                            // Delay the rewiring (Case 3.2.2)
                            overApproximation->setCurrentChoiceIsDelayed(action);
                            if (fixPoint) {
                                // Check whether this delay means that a fixpoint has not been reached
                                if (!overApproximation->getCurrentStateActionExplorationWasDelayed(action) ||
                                    (overApproximation->currentStateIsOptimalSchedulerReachable() &&
                                     overApproximation->actionAtCurrentStateWasOptimal(action) && !storm::utility::isZero(gap))) {
                                    STORM_LOG_INFO_COND(!fixPoint,
                                                        "Not reaching a refinement fixpoint because we delay a rewiring of a state with non-zero gap "
                                                            << gap << " that is reachable via an optimal scheduler.");
                                    fixPoint = false;
                                }
                            }
                        }
                    }  // else { Case 3.2.1 }
                }

                if (expandCurrentAction) {
                    expandedAtLeastOneAction = true;
                    if (!truncateAllActions) {
                        // Cases 1.1, 2.1, or 3.1
                        auto successorGridPoints = beliefManager->expandAndTriangulate(env, currId, action, observationResolutionVector);
                        for (auto const& successor : successorGridPoints) {
                            overApproximation->addTransitionToBelief(action, successor.first, successor.second, false);
                        }
                        if (computeRewards) {
                            overApproximation->computeRewardAtCurrentState(action);
                        }
                    } else {
                        // Cases 1.2 or 2.2
                        auto truncationProbability = storm::utility::zero<ValueType>();
                        auto truncationValueBound = storm::utility::zero<ValueType>();
                        auto successorGridPoints = beliefManager->expandAndTriangulate(env, currId, action, observationResolutionVector);
                        for (auto const& successor : successorGridPoints) {
                            bool added = overApproximation->addTransitionToBelief(action, successor.first, successor.second, true);
                            if (!added) {
                                // We did not explore this successor state. Get a bound on the "missing" value
                                truncationProbability += successor.second;
                                truncationValueBound += successor.second * (min ? overApproximation->computeLowerValueBoundAtBelief(successor.first)
                                                                                : overApproximation->computeUpperValueBoundAtBelief(successor.first));
                            }
                        }
                        if (computeRewards) {
                            // The truncationValueBound will be added on top of the reward introduced by the current belief state.
                            overApproximation->addTransitionsToExtraStates(action, truncationProbability);
                            overApproximation->computeRewardAtCurrentState(action, truncationValueBound);
                        } else {
                            overApproximation->addTransitionsToExtraStates(action, truncationValueBound, truncationProbability - truncationValueBound);
                        }
                    }
                } else {
                    // Case 3.2
                    overApproximation->restoreOldBehaviorAtCurrentState(action);
                }
            }
            if (expandedAtLeastOneAction) {
                ++numRewiredOrExploredStates;
            }
        }

        for (uint64_t action = 0, numActions = beliefManager->getBeliefNumberOfChoices(currId); action < numActions; ++action) {
            if (pomdp().hasChoiceLabeling()) {
                auto rowIndex = pomdp().getTransitionMatrix().getRowGroupIndices()[beliefManager->getRepresentativeState(currId)];
                if (pomdp().getChoiceLabeling().getLabelsOfChoice(rowIndex + action).size() > 0) {
                    overApproximation->addChoiceLabelToCurrentState(action, *(pomdp().getChoiceLabeling().getLabelsOfChoice(rowIndex + action).begin()));
                }
            }
        }

        if (storm::utility::resources::isTerminate()) {
            break;
        }
    }

    if (storm::utility::resources::isTerminate()) {
        // don't overwrite statistics of a previous, successful computation
        if (!statistics.overApproximationStates) {
            statistics.overApproximationBuildAborted = true;
            statistics.overApproximationStates = overApproximation->getCurrentNumberOfMdpStates();
        }
        statistics.overApproximationBuildTime.stop();
        return false;
    }

    overApproximation->finishExploration();
    statistics.overApproximationBuildTime.stop();

    statistics.overApproximationCheckTime.start();
    overApproximation->computeValuesOfExploredMdp(env, min ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize);
    statistics.overApproximationCheckTime.stop();

    // don't overwrite statistics of a previous, successful computation
    if (!storm::utility::resources::isTerminate() || !statistics.overApproximationStates) {
        statistics.overApproximationStates = overApproximation->getExploredMdp()->getNumberOfStates();
    }
    return fixPoint;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::buildUnderApproximation(
    storm::Environment const& env, std::set<uint32_t> const& targetObservations, bool min, bool computeRewards, bool refine,
    HeuristicParameters const& heuristicParameters, std::shared_ptr<BeliefManagerType>& beliefManager, std::shared_ptr<ExplorerType>& underApproximation,
    bool firstIteration) {
    statistics.underApproximationBuildTime.start();

    unfoldingStatus = Status::Exploring;
    if (options.useClipping) {
        STORM_LOG_INFO("Use Belief Clipping with grid beliefs \n");
        statistics.nrClippingAttempts = 0;
        statistics.nrClippedStates = 0;
    }

    uint64_t nrCutoffStrategies = min ? underApproximation->getNrSchedulersForUpperBounds() : underApproximation->getNrSchedulersForLowerBounds();

    bool fixPoint = true;
    if (heuristicParameters.sizeThreshold != std::numeric_limits<uint64_t>::max()) {
        statistics.underApproximationStateLimit = heuristicParameters.sizeThreshold;
    }
    if (!refine) {
        if (options.interactiveUnfolding && !firstIteration) {
            underApproximation->restoreExplorationState();
        } else if (computeRewards) {  // Build a new under approximation
            // We use the sink state for infinite cut-off/clipping values
            underApproximation->startNewExploration(storm::utility::zero<ValueType>(), storm::utility::infinity<ValueType>());
        } else {
            underApproximation->startNewExploration(storm::utility::one<ValueType>(), storm::utility::zero<ValueType>());
        }
    } else {
        // Restart the building process
        underApproximation->restartExploration();
    }

    // Expand the beliefs
    storm::utility::Stopwatch explorationTime;
    storm::utility::Stopwatch printUpdateStopwatch;
    printUpdateStopwatch.start();
    if (options.explorationTimeLimit != 0) {
        explorationTime.start();
    }
    bool timeLimitExceeded = false;
    bool stateStored = false;
    while (underApproximation->hasUnexploredState()) {
        if (!timeLimitExceeded && options.explorationTimeLimit != 0 &&
            static_cast<uint64_t>(explorationTime.getTimeInSeconds()) > options.explorationTimeLimit) {
            STORM_LOG_INFO("Exploration time limit exceeded.");
            timeLimitExceeded = true;
        }
        if (printUpdateStopwatch.getTimeInSeconds() >= 60) {
            printUpdateStopwatch.restart();
            STORM_LOG_INFO("### " << underApproximation->getCurrentNumberOfMdpStates() << " beliefs in underapproximation MDP"
                                  << " ##### " << underApproximation->getUnexploredStates().size() << " beliefs queued\n");
            if (underApproximation->getCurrentNumberOfMdpStates() > heuristicParameters.sizeThreshold && options.useClipping) {
                STORM_LOG_INFO("##### Clipping Attempts: " << statistics.nrClippingAttempts.value() << " ##### "
                                                           << "Clipped States: " << statistics.nrClippedStates.value() << "\n");
            }
        }
        if (unfoldingControl == UnfoldingControl::Pause && !stateStored) {
            underApproximation->storeExplorationState();
            stateStored = true;
        }
        uint64_t currId = underApproximation->exploreNextState();
        uint32_t currObservation = beliefManager->getBeliefObservation(currId);
        uint64_t addedActions = 0;
        bool stateAlreadyExplored = refine && underApproximation->currentStateHasOldBehavior() && !underApproximation->getCurrentStateWasTruncated();
        if (!stateAlreadyExplored || timeLimitExceeded) {
            fixPoint = false;
        }
        if (targetObservations.count(beliefManager->getBeliefObservation(currId)) != 0) {
            underApproximation->setCurrentStateIsTarget();
            underApproximation->addSelfloopTransition();
            underApproximation->addChoiceLabelToCurrentState(0, "loop");
        } else {
            bool stopExploration = false;
            bool clipBelief = false;
            if (timeLimitExceeded || (options.interactiveUnfolding && unfoldingControl != UnfoldingControl::Run)) {
                clipBelief = options.useClipping;
                stopExploration = !underApproximation->isMarkedAsGridBelief(currId);
            } else if (!stateAlreadyExplored) {
                // Check whether we want to explore the state now!
                ValueType gap = getGap(underApproximation->getLowerValueBoundAtCurrentState(), underApproximation->getUpperValueBoundAtCurrentState());
                if ((gap < heuristicParameters.gapThreshold) || (gap == 0 && options.cutZeroGap)) {
                    stopExploration = true;
                } else if (underApproximation->getCurrentNumberOfMdpStates() >=
                           heuristicParameters.sizeThreshold /*&& !statistics.beliefMdpDetectedToBeFinite*/) {
                    clipBelief = options.useClipping;
                    stopExploration = !underApproximation->isMarkedAsGridBelief(currId);
                }
            }

            if (clipBelief && !underApproximation->isMarkedAsGridBelief(currId)) {
                // Use a belief grid as clipping candidates
                if (!options.useStateEliminationCutoff) {
                    bool successfulClip = clipToGridExplicitly(env, currId, computeRewards, beliefManager, underApproximation, 0);
                    // Set again as the current belief might have been detected to be a grid belief
                    stopExploration = !underApproximation->isMarkedAsGridBelief(currId);
                    if (successfulClip) {
                        addedActions += 1;
                    }
                } else {
                    clipToGrid(env, currId, computeRewards, min, beliefManager, underApproximation);
                    addedActions += beliefManager->getBeliefNumberOfChoices(currId);
                }
            }  // end Clipping Procedure

            if (stopExploration) {
                underApproximation->setCurrentStateIsTruncated();
            }
            if (options.useStateEliminationCutoff || !stopExploration) {
                // Add successor transitions or cut-off transitions when exploration is stopped
                uint64_t numActions = beliefManager->getBeliefNumberOfChoices(currId);
                if (underApproximation->needsActionAdjustment(numActions)) {
                    underApproximation->adjustActions(numActions);
                }
                for (uint64_t action = 0; action < numActions; ++action) {
                    // Always restore old behavior if available
                    if (pomdp().hasChoiceLabeling()) {
                        auto rowIndex = pomdp().getTransitionMatrix().getRowGroupIndices()[beliefManager->getRepresentativeState(currId)];
                        if (pomdp().getChoiceLabeling().getLabelsOfChoice(rowIndex + action).size() > 0) {
                            underApproximation->addChoiceLabelToCurrentState(addedActions + action,
                                                                             *(pomdp().getChoiceLabeling().getLabelsOfChoice(rowIndex + action).begin()));
                        }
                    }
                    if (stateAlreadyExplored) {
                        underApproximation->restoreOldBehaviorAtCurrentState(action);
                    } else {
                        auto truncationProbability = storm::utility::zero<ValueType>();
                        auto truncationValueBound = storm::utility::zero<ValueType>();
                        auto successors = beliefManager->expand(env, currId, action);
                        for (auto const& successor : successors) {
                            bool added = underApproximation->addTransitionToBelief(addedActions + action, successor.first, successor.second, stopExploration);
                            if (!added) {
                                STORM_LOG_ASSERT(stopExploration, "Didn't add a transition although exploration shouldn't be stopped.");
                                // We did not explore this successor state. Get a bound on the "missing" value
                                truncationProbability += successor.second;
                                // Some care has to be taken here: Essentially, we are triangulating a value for the under-approximation out of
                                // other under-approximation values. In general, this does not yield a sound underapproximation anymore as the
                                // values might be achieved by different schedulers. However, in our case this is still the case as the
                                // under-approximation values are based on a single memory-less scheduler.
                                truncationValueBound += successor.second * (min ? underApproximation->computeUpperValueBoundAtBelief(successor.first)
                                                                                : underApproximation->computeLowerValueBoundAtBelief(successor.first));
                            }
                        }
                        if (stopExploration) {
                            if (computeRewards) {
                                if (truncationValueBound == storm::utility::infinity<ValueType>()) {
                                    underApproximation->addTransitionsToExtraStates(addedActions + action, storm::utility::zero<ValueType>(),
                                                                                    truncationProbability);
                                } else {
                                    underApproximation->addTransitionsToExtraStates(addedActions + action, truncationProbability);
                                }
                            } else {
                                underApproximation->addTransitionsToExtraStates(addedActions + action, truncationValueBound,
                                                                                truncationProbability - truncationValueBound);
                            }
                        }
                        if (computeRewards) {
                            // The truncationValueBound will be added on top of the reward introduced by the current belief state.
                            if (truncationValueBound != storm::utility::infinity<ValueType>()) {
                                if (!clipBelief) {
                                    underApproximation->computeRewardAtCurrentState(action, truncationValueBound);
                                } else {
                                    underApproximation->addRewardToCurrentState(addedActions + action,
                                                                                beliefManager->getBeliefActionReward(currId, action) + truncationValueBound);
                                }
                            }
                        }
                    }
                }
            } else {
                for (uint64_t i = 0; i < nrCutoffStrategies && !options.skipHeuristicSchedulers; ++i) {
                    auto cutOffValue = min ? underApproximation->computeUpperValueBoundForScheduler(currId, i)
                                           : underApproximation->computeLowerValueBoundForScheduler(currId, i);
                    if (computeRewards) {
                        if (cutOffValue != storm::utility::infinity<ValueType>()) {
                            underApproximation->addTransitionsToExtraStates(addedActions, storm::utility::one<ValueType>());
                            underApproximation->addRewardToCurrentState(addedActions, cutOffValue);
                        } else {
                            underApproximation->addTransitionsToExtraStates(addedActions, storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
                        }
                    } else {
                        underApproximation->addTransitionsToExtraStates(addedActions, cutOffValue, storm::utility::one<ValueType>() - cutOffValue);
                    }
                    if (pomdp().hasChoiceLabeling()) {
                        underApproximation->addChoiceLabelToCurrentState(addedActions, "sched_" + std::to_string(i));
                    }
                    addedActions++;
                }
                if (underApproximation->hasFMSchedulerValues()) {
                    uint64_t transitionNr = 0;
                    for (uint64_t i = 0; i < underApproximation->getNrOfMemoryNodesForObservation(currObservation); ++i) {
                        auto resPair = underApproximation->computeFMSchedulerValueForMemoryNode(currId, i);
                        ValueType cutOffValue;
                        if (resPair.first) {
                            cutOffValue = resPair.second;
                        } else {
                            STORM_LOG_DEBUG("Skipped cut-off of belief with ID " << currId << " with finite memory scheduler in memory node " << i
                                                                                 << ". Missing values.");
                            continue;
                        }
                        if (computeRewards) {
                            if (cutOffValue != storm::utility::infinity<ValueType>()) {
                                underApproximation->addTransitionsToExtraStates(addedActions + transitionNr, storm::utility::one<ValueType>());
                                underApproximation->addRewardToCurrentState(addedActions + transitionNr, cutOffValue);
                            } else {
                                underApproximation->addTransitionsToExtraStates(addedActions + transitionNr, storm::utility::zero<ValueType>(),
                                                                                storm::utility::one<ValueType>());
                            }
                        } else {
                            underApproximation->addTransitionsToExtraStates(addedActions + transitionNr, cutOffValue,
                                                                            storm::utility::one<ValueType>() - cutOffValue);
                        }
                        if (pomdp().hasChoiceLabeling()) {
                            underApproximation->addChoiceLabelToCurrentState(addedActions + transitionNr, "mem_node_" + std::to_string(i));
                        }
                        ++transitionNr;
                    }
                }
            }
        }
        if (storm::utility::resources::isTerminate()) {
            break;
        }
    }

    if (storm::utility::resources::isTerminate()) {
        // don't overwrite statistics of a previous, successful computation
        if (!statistics.underApproximationStates) {
            statistics.underApproximationBuildAborted = true;
            statistics.underApproximationStates = underApproximation->getCurrentNumberOfMdpStates();
        }
        statistics.underApproximationBuildTime.stop();
        return false;
    }

    underApproximation->finishExploration();
    statistics.underApproximationBuildTime.stop();
    printUpdateStopwatch.stop();
    STORM_LOG_INFO("Finished exploring under-approximation MDP.\nStart analysis...\n");
    unfoldingStatus = Status::ModelExplorationFinished;
    statistics.underApproximationCheckTime.start();
    underApproximation->computeValuesOfExploredMdp(env, min ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize);
    statistics.underApproximationCheckTime.stop();
    if (underApproximation->getExploredMdp()->getStateLabeling().getStates("truncated").getNumberOfSetBits() > 0) {
        statistics.nrTruncatedStates = underApproximation->getExploredMdp()->getStateLabeling().getStates("truncated").getNumberOfSetBits();
    }
    // don't overwrite statistics of a previous, successful computation
    if (!storm::utility::resources::isTerminate() || !statistics.underApproximationStates) {
        statistics.underApproximationStates = underApproximation->getExploredMdp()->getNumberOfStates();
    }
    return fixPoint;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::clipToGrid(storm::Environment const& env, uint64_t clippingStateId,
                                                                                                    bool computeRewards, bool min,
                                                                                                    std::shared_ptr<BeliefManagerType>& beliefManager,
                                                                                                    std::shared_ptr<ExplorerType>& beliefExplorer) {
    // Add all transitions to states which are already in the MDP, clip all others to a grid
    // To make the resulting MDP smaller, we eliminate intermediate successor states when clipping is applied
    for (uint64_t action = 0, numActions = beliefManager->getBeliefNumberOfChoices(clippingStateId); action < numActions; ++action) {
        auto rewardBound = utility::zero<BeliefValueType>();
        auto successors = beliefManager->expand(env, clippingStateId, action);
        auto absDelta = utility::zero<BeliefValueType>();
        for (auto const& successor : successors) {
            // Add transition if successor is in explored space.
            // We can directly add the transitions as there is at most one successor for each observation
            // Therefore no belief can be clipped to an already added successor
            bool added = beliefExplorer->addTransitionToBelief(action, successor.first, successor.second, true);
            if (!added) {
                // The successor is not in the explored space. Clip it
                statistics.nrClippingAttempts = statistics.nrClippingAttempts.value() + 1;
                auto clipping =
                    beliefManager->clipBeliefToGrid(env, successor.first, options.clippingGridRes,
                                                    computeRewards ? beliefExplorer->getStateExtremeBoundIsInfinite() : storm::storage::BitVector());
                if (clipping.isClippable) {
                    // The belief is not on the grid and there is a candidate with finite reward
                    statistics.nrClippedStates = statistics.nrClippedStates.value() + 1;
                    // Transition probability to candidate is (probability to successor) * (clipping transition probability)
                    BeliefValueType transitionProb =
                        (utility::one<BeliefValueType>() - clipping.delta) * utility::convertNumber<BeliefValueType>(successor.second);
                    beliefExplorer->addTransitionToBelief(action, clipping.targetBelief, utility::convertNumber<BeliefMDPType>(transitionProb), false);
                    // Collect weighted clipping values
                    absDelta += clipping.delta * utility::convertNumber<BeliefValueType>(successor.second);
                    if (computeRewards) {
                        // collect cumulative reward bounds
                        auto localRew = utility::zero<BeliefValueType>();
                        for (auto const& deltaValue : clipping.deltaValues) {
                            localRew += deltaValue.second *
                                        utility::convertNumber<BeliefValueType>((beliefExplorer->getExtremeValueBoundAtPOMDPState(deltaValue.first)));
                        }
                        if (localRew == utility::infinity<BeliefValueType>()) {
                            STORM_LOG_WARN("Infinite reward in clipping!");
                        }
                        rewardBound += localRew * utility::convertNumber<BeliefValueType>(successor.second);
                    }
                } else if (clipping.onGrid) {
                    // If the belief is not clippable, but on the grid, it may need to be explored, too
                    beliefExplorer->addTransitionToBelief(action, successor.first, successor.second, false);
                } else {
                    // Otherwise, the reward for all candidates is infinite, clipping does not make sense. Cut it off instead
                    absDelta += utility::convertNumber<BeliefValueType>(successor.second);
                    rewardBound += utility::convertNumber<BeliefValueType>(successor.second) *
                                   utility::convertNumber<BeliefValueType>(min ? beliefExplorer->computeUpperValueBoundAtBelief(successor.first)
                                                                               : beliefExplorer->computeLowerValueBoundAtBelief(successor.first));
                }
            }
        }
        // Add the collected clipping transition if necessary
        if (absDelta != utility::zero<BeliefValueType>()) {
            if (computeRewards) {
                if (rewardBound == utility::infinity<BeliefValueType>()) {
                    // If the reward is infinite, add a transition to the sink state to collect infinite reward
                    beliefExplorer->addTransitionsToExtraStates(action, utility::zero<BeliefMDPType>(), utility::convertNumber<BeliefMDPType>(absDelta));
                } else {
                    beliefExplorer->addTransitionsToExtraStates(action, utility::convertNumber<BeliefMDPType>(absDelta));
                    BeliefValueType totalRewardVal = rewardBound / absDelta;
                    beliefExplorer->addClippingRewardToCurrentState(action, utility::convertNumber<BeliefMDPType>(totalRewardVal));
                }
            } else {
                beliefExplorer->addTransitionsToExtraStates(action, utility::zero<BeliefMDPType>(), utility::convertNumber<BeliefMDPType>(absDelta));
            }
        }
        if (computeRewards) {
            beliefExplorer->computeRewardAtCurrentState(action);
        }
    }
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::clipToGridExplicitly(storm::Environment const& env,
                                                                                                              uint64_t clippingStateId, bool computeRewards,
                                                                                                              std::shared_ptr<BeliefManagerType>& beliefManager,
                                                                                                              std::shared_ptr<ExplorerType>& beliefExplorer,
                                                                                                              uint64_t localActionIndex) {
    statistics.nrClippingAttempts = statistics.nrClippingAttempts.value() + 1;
    auto clipping = beliefManager->clipBeliefToGrid(env, clippingStateId, options.clippingGridRes,
                                                    computeRewards ? beliefExplorer->getStateExtremeBoundIsInfinite() : storm::storage::BitVector());
    if (clipping.isClippable) {
        // The belief is not on the grid and there is a candidate with finite reward
        statistics.nrClippedStates = statistics.nrClippedStates.value() + 1;
        // Transition probability to candidate is clipping value
        BeliefValueType transitionProb = (utility::one<BeliefValueType>() - clipping.delta);
        beliefExplorer->addTransitionToBelief(localActionIndex, clipping.targetBelief, utility::convertNumber<BeliefMDPType>(transitionProb), false);
        beliefExplorer->markAsGridBelief(clipping.targetBelief);
        if (computeRewards) {
            // collect cumulative reward bounds
            auto reward = utility::zero<BeliefValueType>();
            for (auto const& deltaValue : clipping.deltaValues) {
                reward += deltaValue.second * utility::convertNumber<BeliefValueType>((beliefExplorer->getExtremeValueBoundAtPOMDPState(deltaValue.first)));
            }
            if (reward == utility::infinity<BeliefValueType>()) {
                STORM_LOG_WARN("Infinite reward in clipping!");
                // If the reward is infinite, add a transition to the sink state to collect infinite reward in our semantics
                beliefExplorer->addTransitionsToExtraStates(localActionIndex, utility::zero<BeliefMDPType>(),
                                                            utility::convertNumber<BeliefMDPType>(clipping.delta));
            } else {
                beliefExplorer->addTransitionsToExtraStates(localActionIndex, utility::convertNumber<BeliefMDPType>(clipping.delta));
                BeliefValueType totalRewardVal = reward / clipping.delta;
                beliefExplorer->addClippingRewardToCurrentState(localActionIndex, utility::convertNumber<BeliefMDPType>(totalRewardVal));
            }
        } else {
            beliefExplorer->addTransitionsToExtraStates(localActionIndex, utility::zero<BeliefMDPType>(),
                                                        utility::convertNumber<BeliefMDPType>(clipping.delta));
        }
        beliefExplorer->addChoiceLabelToCurrentState(localActionIndex, "clip");
        return true;
    } else {
        if (clipping.onGrid) {
            // If the belief is not clippable, but on the grid, it may need to be explored, too
            beliefExplorer->markAsGridBelief(clippingStateId);
        }
    }
    return false;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::setUnfoldingControl(
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::UnfoldingControl newUnfoldingControl) {
    unfoldingControl = newUnfoldingControl;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::pauseUnfolding() {
    STORM_LOG_TRACE("PAUSE COMMAND ISSUED");
    setUnfoldingControl(UnfoldingControl::Pause);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::continueUnfolding() {
    STORM_LOG_TRACE("CONTINUATION COMMAND ISSUED");
    setUnfoldingControl(UnfoldingControl::Run);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::terminateUnfolding() {
    STORM_LOG_TRACE("TERMINATION COMMAND ISSUED");
    setUnfoldingControl(UnfoldingControl::Terminate);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::isResultReady() {
    return unfoldingStatus == Status::ResultAvailable || unfoldingStatus == Status::Converged;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::hasConverged() {
    return unfoldingStatus == Status::Converged;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
bool BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::isExploring() {
    return unfoldingStatus == Status::Exploring;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
std::shared_ptr<storm::builder::BeliefMdpExplorer<PomdpModelType, BeliefValueType>>
BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::getInteractiveBeliefExplorer() {
    return interactiveUnderApproximationExplorer;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
void BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::setFMSchedValueList(
    std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> valueList) {
    interactiveUnderApproximationExplorer->setFMSchedValueList(valueList);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
BeliefValueType BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::rateObservation(
    typename ExplorerType::SuccessorObservationInformation const& info, BeliefValueType const& observationResolution, BeliefValueType const& maxResolution) {
    auto n = storm::utility::convertNumber<BeliefValueType, uint64_t>(info.support.size());
    auto one = storm::utility::one<BeliefValueType>();
    if (storm::utility::isOne(n)) {
        // If the belief is Dirac, it has to be approximated precisely.
        // In this case, we return the best possible rating
        return one;
    } else {
        // Create the rating for this observation at this choice from the given info
        auto obsChoiceRating = storm::utility::convertNumber<BeliefValueType, ValueType>(info.maxProbabilityToSuccessorWithObs / info.observationProbability);
        // At this point, obsRating is the largest triangulation weight (which ranges from 1/n to 1)
        // Normalize the rating so that it ranges from 0 to 1, where
        // 0 means that the actual belief lies in the middle of the triangulating simplex (i.e. a "bad" approximation) and 1 means that the belief is precisely
        // approximated.
        obsChoiceRating = (obsChoiceRating * n - one) / (n - one);
        // Scale the ratings with the resolutions, so that low resolutions get a lower rating (and are thus more likely to be refined)
        obsChoiceRating *= observationResolution / maxResolution;
        return obsChoiceRating;
    }
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
std::vector<BeliefValueType> BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::getObservationRatings(
    std::shared_ptr<ExplorerType> const& overApproximation, std::vector<BeliefValueType> const& observationResolutionVector) {
    uint64_t numMdpStates = overApproximation->getExploredMdp()->getNumberOfStates();
    auto const& choiceIndices = overApproximation->getExploredMdp()->getNondeterministicChoiceIndices();
    BeliefValueType maxResolution = *std::max_element(observationResolutionVector.begin(), observationResolutionVector.end());

    std::vector<BeliefValueType> resultingRatings(pomdp().getNrObservations(), storm::utility::one<BeliefValueType>());

    std::map<uint32_t, typename ExplorerType::SuccessorObservationInformation> gatheredSuccessorObservations;  // Declare here to avoid reallocations
    for (uint64_t mdpState = 0; mdpState < numMdpStates; ++mdpState) {
        // Check whether this state is reached under an optimal scheduler.
        // The heuristic assumes that the remaining states are not relevant for the observation score.
        if (overApproximation->stateIsOptimalSchedulerReachable(mdpState)) {
            for (uint64_t mdpChoice = choiceIndices[mdpState]; mdpChoice < choiceIndices[mdpState + 1]; ++mdpChoice) {
                // Similarly, only optimal actions are relevant
                if (overApproximation->actionIsOptimal(mdpChoice)) {
                    // score the observations for this choice
                    gatheredSuccessorObservations.clear();
                    overApproximation->gatherSuccessorObservationInformationAtMdpChoice(mdpChoice, gatheredSuccessorObservations);
                    for (auto const& obsInfo : gatheredSuccessorObservations) {
                        auto const& obs = obsInfo.first;
                        BeliefValueType obsChoiceRating = rateObservation(obsInfo.second, observationResolutionVector[obs], maxResolution);

                        // The rating of the observation will be the minimum over all choice-based observation ratings
                        resultingRatings[obs] = std::min(resultingRatings[obs], obsChoiceRating);
                    }
                }
            }
        }
    }
    return resultingRatings;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMDPType>
typename PomdpModelType::ValueType BeliefExplorationPomdpModelChecker<PomdpModelType, BeliefValueType, BeliefMDPType>::getGap(
    typename PomdpModelType::ValueType const& l, typename PomdpModelType::ValueType const& u) {
    STORM_LOG_ASSERT(l >= storm::utility::zero<typename PomdpModelType::ValueType>() && u >= storm::utility::zero<typename PomdpModelType::ValueType>(),
                     "Gap computation currently does not handle negative values.");
    if (storm::utility::isInfinity(u)) {
        if (storm::utility::isInfinity(l)) {
            return storm::utility::zero<typename PomdpModelType::ValueType>();
        } else {
            return u;
        }
    } else if (storm::utility::isZero(u)) {
        STORM_LOG_ASSERT(storm::utility::isZero(l), "Upper bound is zero but lower bound is " << l << ".");
        return u;
    } else {
        STORM_LOG_ASSERT(!storm::utility::isInfinity(l), "Lower bound is infinity, but upper bound is " << u << ".");
        // get the relative gap
        return storm::utility::abs<typename PomdpModelType::ValueType>(u - l) * storm::utility::convertNumber<typename PomdpModelType::ValueType, uint64_t>(2) /
               (l + u);
    }
}

/* Template Instantiations */

template class BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<double>>;

template class BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<double>, storm::RationalNumber>;

template class BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>, double>;

template class BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>>;

}  // namespace modelchecker
}  // namespace pomdp
}  // namespace storm
