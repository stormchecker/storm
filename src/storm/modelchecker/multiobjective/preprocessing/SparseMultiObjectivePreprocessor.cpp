#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessor.h"

#include <algorithm>
#include <set>

#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/memorystructure/SparseModelMemoryProductReverseData.h"
#include "storm/transformer/MemoryIncorporation.h"
#include "storm/transformer/SubsystemBuilder.h"
#include "storm/utility/FilteredRewardModel.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {
namespace preprocessing {

template<typename SparseModelType>
typename SparseMultiObjectivePreprocessor<SparseModelType>::ReturnType SparseMultiObjectivePreprocessor<SparseModelType>::preprocess(
    Environment const& env, SparseModelType const& originalModel, storm::logic::MultiObjectiveFormula const& originalFormula, bool produceScheduler) {
    std::shared_ptr<SparseModelType> model;
    std::optional<storm::storage::SparseModelMemoryProductReverseData> memoryIncorporationReverseData;

    // Incorporate the necessary memory
    if (env.modelchecker().multi().isSchedulerRestrictionSet()) {
        auto const& schedRestr = env.modelchecker().multi().getSchedulerRestriction();
        if (schedRestr.getMemoryPattern() == storm::storage::SchedulerClass::MemoryPattern::GoalMemory) {
            if (produceScheduler) {
                std::tie(model, memoryIncorporationReverseData) =
                    storm::transformer::MemoryIncorporation<SparseModelType>::incorporateGoalMemoryWithReverseData(originalModel,
                                                                                                                   originalFormula.getSubformulas());
            } else {
                model = storm::transformer::MemoryIncorporation<SparseModelType>::incorporateGoalMemory(originalModel, originalFormula.getSubformulas());
            }
        } else if (schedRestr.getMemoryPattern() == storm::storage::SchedulerClass::MemoryPattern::Arbitrary && schedRestr.getMemoryStates() > 1) {
            STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotImplementedException, "Cannot produce schedulers for the provided memory pattern.");
            model = storm::transformer::MemoryIncorporation<SparseModelType>::incorporateFullMemory(originalModel, schedRestr.getMemoryStates());
        } else if (schedRestr.getMemoryPattern() == storm::storage::SchedulerClass::MemoryPattern::Counter && schedRestr.getMemoryStates() > 1) {
            STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotImplementedException, "Cannot produce schedulers for the provided memory pattern.");
            model = storm::transformer::MemoryIncorporation<SparseModelType>::incorporateCountingMemory(originalModel, schedRestr.getMemoryStates());
        } else if (schedRestr.isPositional()) {
            model = std::make_shared<SparseModelType>(originalModel);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "The given scheduler restriction has not been implemented.");
        }
    } else {
        if (produceScheduler) {
            std::tie(model, memoryIncorporationReverseData) =
                storm::transformer::MemoryIncorporation<SparseModelType>::incorporateGoalMemoryWithReverseData(originalModel, originalFormula.getSubformulas());
        } else {
            model = storm::transformer::MemoryIncorporation<SparseModelType>::incorporateGoalMemory(originalModel, originalFormula.getSubformulas());
        }
    }

    // Remove states that are irrelevant for all properties (e.g. because they are only reachable via goal states
    boost::optional<std::string> deadlockLabel;
    if (!produceScheduler) {
        // When producing schedulers, removing irrelevant states requires additional bookkeeping.
        removeIrrelevantStates(model, deadlockLabel, originalFormula);
    }

    PreprocessorData data(model);
    data.deadlockLabel = deadlockLabel;
    data.memoryIncorporationReverseData = std::move(memoryIncorporationReverseData);

    // Invoke preprocessing on the individual objectives
    for (auto const& subFormula : originalFormula.getSubformulas()) {
        STORM_LOG_INFO("Preprocessing objective " << *subFormula << ".");
        data.objectives.push_back(std::make_shared<Objective<ValueType>>());
        data.objectives.back()->originalFormula = subFormula;
        data.finiteRewardCheckObjectives.resize(data.objectives.size(), false);
        STORM_LOG_THROW(data.objectives.back()->originalFormula->isOperatorFormula(), storm::exceptions::InvalidPropertyException,
                        "Could not preprocess the subformula " << *subFormula << " of " << originalFormula << " because it is not supported");
        preprocessOperatorFormula(data.objectives.back()->originalFormula->asOperatorFormula(), data);
    }

    // Remove reward models that are not needed anymore
    std::set<std::string> relevantRewardModels;
    for (auto const& obj : data.objectives) {
        obj->formula->gatherReferencedRewardModels(relevantRewardModels);
    }
    data.model->restrictRewardModels(relevantRewardModels);

    // Build the actual result
    return buildResult(originalModel, originalFormula, data);
}

template<typename SparseModelType>
storm::storage::BitVector getOnlyReachableViaPhi(SparseModelType const& model, storm::storage::BitVector const& phi) {
    // Get the complement of the states that are reachable without visiting phi
    auto result =
        storm::utility::graph::getReachableStates(model.getTransitionMatrix(), model.getInitialStates(), ~phi, storm::storage::BitVector(phi.size(), false));
    result.complement();
    assert(phi.isSubsetOf(result));
    return result;
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::removeIrrelevantStates(std::shared_ptr<SparseModelType>& model,
                                                                               boost::optional<std::string>& deadlockLabel,
                                                                               storm::logic::MultiObjectiveFormula const& originalFormula) {
    storm::storage::BitVector absorbingStates(model->getNumberOfStates(), true);

    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> mc(*model);
    storm::storage::SparseMatrix<ValueType> backwardTransitions = model->getBackwardTransitions();

    for (auto const& opFormula : originalFormula.getSubformulas()) {
        // Compute a set of states from which we can make any subset absorbing without affecting this subformula
        storm::storage::BitVector absorbingStatesForSubformula;
        STORM_LOG_THROW(opFormula->isOperatorFormula(), storm::exceptions::InvalidPropertyException,
                        "Could not preprocess the subformula " << *opFormula << " of " << originalFormula << " because it is not supported");
        auto const& pathFormula = opFormula->asOperatorFormula().getSubformula();
        if (opFormula->isProbabilityOperatorFormula()) {
            if (pathFormula.isUntilFormula()) {
                auto lhs =
                    mc.check(pathFormula.asUntilFormula().getLeftSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                auto rhs =
                    mc.check(pathFormula.asUntilFormula().getRightSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                absorbingStatesForSubformula = storm::utility::graph::performProb0A(backwardTransitions, lhs, rhs);
                absorbingStatesForSubformula |= getOnlyReachableViaPhi(*model, ~lhs | rhs);
            } else if (pathFormula.isBoundedUntilFormula()) {
                if (pathFormula.asBoundedUntilFormula().hasMultiDimensionalSubformulas()) {
                    absorbingStatesForSubformula = storm::storage::BitVector(model->getNumberOfStates(), true);
                    storm::storage::BitVector absorbingStatesForSubSubformula;
                    for (uint64_t i = 0; i < pathFormula.asBoundedUntilFormula().getDimension(); ++i) {
                        auto subPathFormula = pathFormula.asBoundedUntilFormula().restrictToDimension(i);
                        auto lhs = mc.check(pathFormula.asBoundedUntilFormula().getLeftSubformula(i))
                                       ->template asExplicitQualitativeCheckResult<ValueType>()
                                       .getTruthValuesVector();
                        auto rhs = mc.check(pathFormula.asBoundedUntilFormula().getRightSubformula(i))
                                       ->template asExplicitQualitativeCheckResult<ValueType>()
                                       .getTruthValuesVector();
                        absorbingStatesForSubSubformula = storm::utility::graph::performProb0A(backwardTransitions, lhs, rhs);
                        if (pathFormula.asBoundedUntilFormula().hasLowerBound(i)) {
                            absorbingStatesForSubSubformula |= getOnlyReachableViaPhi(*model, ~lhs);
                        } else {
                            absorbingStatesForSubSubformula |= getOnlyReachableViaPhi(*model, ~lhs | rhs);
                        }
                        absorbingStatesForSubformula &= absorbingStatesForSubSubformula;
                    }
                } else {
                    auto lhs = mc.check(pathFormula.asBoundedUntilFormula().getLeftSubformula())
                                   ->template asExplicitQualitativeCheckResult<ValueType>()
                                   .getTruthValuesVector();
                    auto rhs = mc.check(pathFormula.asBoundedUntilFormula().getRightSubformula())
                                   ->template asExplicitQualitativeCheckResult<ValueType>()
                                   .getTruthValuesVector();
                    absorbingStatesForSubformula = storm::utility::graph::performProb0A(backwardTransitions, lhs, rhs);
                    if (pathFormula.asBoundedUntilFormula().hasLowerBound()) {
                        absorbingStatesForSubformula |= getOnlyReachableViaPhi(*model, ~lhs);
                    } else {
                        absorbingStatesForSubformula |= getOnlyReachableViaPhi(*model, ~lhs | rhs);
                    }
                }
            } else if (pathFormula.isGloballyFormula()) {
                auto phi =
                    mc.check(pathFormula.asGloballyFormula().getSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                auto notPhi = ~phi;
                absorbingStatesForSubformula = storm::utility::graph::performProb0A(backwardTransitions, phi, notPhi);
                absorbingStatesForSubformula |= getOnlyReachableViaPhi(*model, notPhi);
            } else if (pathFormula.isEventuallyFormula()) {
                auto phi =
                    mc.check(pathFormula.asEventuallyFormula().getSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                absorbingStatesForSubformula = storm::utility::graph::performProb0A(backwardTransitions, ~phi, phi);
                absorbingStatesForSubformula |= getOnlyReachableViaPhi(*model, phi);
            } else {
                STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "The subformula of " << pathFormula << " is not supported.");
            }
        } else if (opFormula->isRewardOperatorFormula()) {
            auto const& baseRewardModel = opFormula->asRewardOperatorFormula().hasRewardModelName()
                                              ? model->getRewardModel(opFormula->asRewardOperatorFormula().getRewardModelName())
                                              : model->getUniqueRewardModel();
            if (pathFormula.isEventuallyFormula()) {
                auto rewardModel = storm::utility::createFilteredRewardModel(baseRewardModel, model->isDiscreteTimeModel(), pathFormula.asEventuallyFormula());
                storm::storage::BitVector statesWithoutReward = rewardModel.get().getStatesWithZeroReward(model->getTransitionMatrix());
                // Make states that can not reach a state with non-zero reward absorbing
                absorbingStatesForSubformula = storm::utility::graph::performProb0A(backwardTransitions, statesWithoutReward, ~statesWithoutReward);
                auto phi =
                    mc.check(pathFormula.asEventuallyFormula().getSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                // Make states that reach phi with prob 1 while only visiting states with reward 0 absorbing
                absorbingStatesForSubformula |= storm::utility::graph::performProb1A(
                    model->getTransitionMatrix(), model->getTransitionMatrix().getRowGroupIndices(), backwardTransitions, statesWithoutReward, phi);
                // Make states that are only reachable via phi absorbing
                absorbingStatesForSubformula |= getOnlyReachableViaPhi(*model, phi);
            } else if (pathFormula.isCumulativeRewardFormula()) {
                auto rewardModel =
                    storm::utility::createFilteredRewardModel(baseRewardModel, model->isDiscreteTimeModel(), pathFormula.asCumulativeRewardFormula());
                storm::storage::BitVector statesWithoutReward = rewardModel.get().getStatesWithZeroReward(model->getTransitionMatrix());
                absorbingStatesForSubformula = storm::utility::graph::performProb0A(backwardTransitions, statesWithoutReward, ~statesWithoutReward);
            } else if (pathFormula.isTotalRewardFormula()) {
                auto rewardModel = storm::utility::createFilteredRewardModel(baseRewardModel, model->isDiscreteTimeModel(), pathFormula.asTotalRewardFormula());
                storm::storage::BitVector statesWithoutReward = rewardModel.get().getStatesWithZeroReward(model->getTransitionMatrix());
                absorbingStatesForSubformula = storm::utility::graph::performProb0A(backwardTransitions, statesWithoutReward, ~statesWithoutReward);
            } else if (pathFormula.isLongRunAverageRewardFormula()) {
                auto rewardModel =
                    storm::utility::createFilteredRewardModel(baseRewardModel, model->isDiscreteTimeModel(), pathFormula.asLongRunAverageRewardFormula());
                storm::storage::BitVector statesWithoutReward = rewardModel.get().getStatesWithZeroReward(model->getTransitionMatrix());
                // Compute Sat(Forall F (Forall G "statesWithoutReward"))
                auto forallGloballyStatesWithoutReward = storm::utility::graph::performProb0A(backwardTransitions, statesWithoutReward, ~statesWithoutReward);
                absorbingStatesForSubformula =
                    storm::utility::graph::performProb1A(model->getTransitionMatrix(), model->getNondeterministicChoiceIndices(), backwardTransitions,
                                                         storm::storage::BitVector(model->getNumberOfStates(), true), forallGloballyStatesWithoutReward);
            } else {
                STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "The subformula of " << pathFormula << " is not supported.");
            }
        } else if (opFormula->isTimeOperatorFormula()) {
            if (pathFormula.isEventuallyFormula()) {
                auto phi =
                    mc.check(pathFormula.asEventuallyFormula().getSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                absorbingStatesForSubformula = getOnlyReachableViaPhi(*model, phi);
            } else {
                STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "The subformula of " << pathFormula << " is not supported.");
            }
        } else if (opFormula->isLongRunAverageOperatorFormula()) {
            auto lraStates = mc.check(pathFormula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
            // Compute Sat(Forall F (Forall G not "lraStates"))
            auto forallGloballyNotLraStates = storm::utility::graph::performProb0A(backwardTransitions, ~lraStates, lraStates);
            absorbingStatesForSubformula = storm::utility::graph::performProb1A(model->getTransitionMatrix(), model->getNondeterministicChoiceIndices(),
                                                                                backwardTransitions, ~lraStates, forallGloballyNotLraStates);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException,
                            "Could not preprocess the subformula " << *opFormula << " of " << originalFormula << " because it is not supported");
        }
        absorbingStates &= absorbingStatesForSubformula;
        if (absorbingStates.empty()) {
            break;
        }
    }

    if (!absorbingStates.empty()) {
        // We can make the states absorbing and delete unreachable states.
        storm::storage::BitVector subsystemActions(model->getNumberOfChoices(), true);
        for (auto absorbingState : absorbingStates) {
            for (uint64_t action = model->getTransitionMatrix().getRowGroupIndices()[absorbingState];
                 action < model->getTransitionMatrix().getRowGroupIndices()[absorbingState + 1]; ++action) {
                subsystemActions.set(action, false);
            }
        }
        storm::transformer::SubsystemBuilderOptions options;
        options.fixDeadlocks = true;
        auto const& submodel =
            storm::transformer::buildSubsystem(*model, storm::storage::BitVector(model->getNumberOfStates(), true), subsystemActions, false, options);
        STORM_LOG_INFO("Making states absorbing reduced the state space from " << model->getNumberOfStates() << " to " << submodel.model->getNumberOfStates()
                                                                               << ".");
        model = submodel.model->template as<SparseModelType>();
        deadlockLabel = submodel.deadlockLabel;
    }
}

template<typename SparseModelType>
SparseMultiObjectivePreprocessor<SparseModelType>::PreprocessorData::PreprocessorData(std::shared_ptr<SparseModelType> model) : model(model) {
    // The rewardModelNamePrefix should not be a prefix of a reward model name of the given model to ensure uniqueness of new reward model names
    rewardModelNamePrefix = "obj";
    while (true) {
        bool prefixIsUnique = true;
        for (auto const& rewardModels : model->getRewardModels()) {
            if (rewardModelNamePrefix.size() <= rewardModels.first.size()) {
                if (std::mismatch(rewardModelNamePrefix.begin(), rewardModelNamePrefix.end(), rewardModels.first.begin()).first ==
                    rewardModelNamePrefix.end()) {
                    prefixIsUnique = false;
                    rewardModelNamePrefix = "_" + rewardModelNamePrefix;
                    break;
                }
            }
        }
        if (prefixIsUnique) {
            break;
        }
    }
}

storm::logic::OperatorInformation getOperatorInformation(storm::logic::OperatorFormula const& formula, bool considerComplementaryEvent) {
    storm::logic::OperatorInformation opInfo;
    if (formula.hasBound()) {
        opInfo.bound = formula.getBound();
        // Invert the bound (if necessary)
        if (considerComplementaryEvent) {
            opInfo.bound->threshold = opInfo.bound->threshold.getManager().rational(storm::utility::one<storm::RationalNumber>()) - opInfo.bound->threshold;
            switch (opInfo.bound->comparisonType) {
                case storm::logic::ComparisonType::Greater:
                    opInfo.bound->comparisonType = storm::logic::ComparisonType::Less;
                    break;
                case storm::logic::ComparisonType::GreaterEqual:
                    opInfo.bound->comparisonType = storm::logic::ComparisonType::LessEqual;
                    break;
                case storm::logic::ComparisonType::Less:
                    opInfo.bound->comparisonType = storm::logic::ComparisonType::Greater;
                    break;
                case storm::logic::ComparisonType::LessEqual:
                    opInfo.bound->comparisonType = storm::logic::ComparisonType::GreaterEqual;
                    break;
                default:
                    STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "Current objective " << formula << " has unexpected comparison type");
            }
        }
        if (storm::logic::isLowerBound(opInfo.bound->comparisonType)) {
            opInfo.optimalityType = storm::solver::OptimizationDirection::Maximize;
        } else {
            opInfo.optimalityType = storm::solver::OptimizationDirection::Minimize;
        }
        STORM_LOG_WARN_COND(!formula.hasOptimalityType(),
                            "Optimization direction of formula " << formula << " ignored as the formula also specifies a threshold.");
    } else if (formula.hasOptimalityType()) {
        opInfo.optimalityType = formula.getOptimalityType();
        // Invert the optimality type (if necessary)
        if (considerComplementaryEvent) {
            opInfo.optimalityType = storm::solver::invert(opInfo.optimalityType.get());
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "Objective " << formula << " does not specify whether to minimize or maximize");
    }
    return opInfo;
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessOperatorFormula(storm::logic::OperatorFormula const& formula, PreprocessorData& data) {
    Objective<ValueType>& objective = *data.objectives.back();

    // Check whether the complementary event is considered
    objective.considersComplementaryEvent = formula.isProbabilityOperatorFormula() && formula.getSubformula().isGloballyFormula();

    // Extract the operator information from the formula and potentially invert it for the complementary event
    storm::logic::OperatorInformation opInfo = getOperatorInformation(formula, objective.considersComplementaryEvent);

    if (formula.isProbabilityOperatorFormula()) {
        preprocessProbabilityOperatorFormula(formula.asProbabilityOperatorFormula(), opInfo, data);
    } else if (formula.isRewardOperatorFormula()) {
        preprocessRewardOperatorFormula(formula.asRewardOperatorFormula(), opInfo, data);
    } else if (formula.isTimeOperatorFormula()) {
        preprocessTimeOperatorFormula(formula.asTimeOperatorFormula(), opInfo, data);
    } else if (formula.isLongRunAverageOperatorFormula()) {
        preprocessLongRunAverageOperatorFormula(formula.asLongRunAverageOperatorFormula(), opInfo, data);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "Could not preprocess the objective " << formula << " because it is not supported");
    }
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessProbabilityOperatorFormula(storm::logic::ProbabilityOperatorFormula const& formula,
                                                                                             storm::logic::OperatorInformation const& opInfo,
                                                                                             PreprocessorData& data) {
    // Probabilities are between zero and one
    data.objectives.back()->lowerResultBound = storm::utility::zero<ValueType>();
    data.objectives.back()->upperResultBound = storm::utility::one<ValueType>();

    if (formula.getSubformula().isUntilFormula()) {
        preprocessUntilFormula(formula.getSubformula().asUntilFormula(), opInfo, data);
    } else if (formula.getSubformula().isBoundedUntilFormula()) {
        preprocessBoundedUntilFormula(formula.getSubformula().asBoundedUntilFormula(), opInfo, data);
    } else if (formula.getSubformula().isGloballyFormula()) {
        preprocessGloballyFormula(formula.getSubformula().asGloballyFormula(), opInfo, data);
    } else if (formula.getSubformula().isEventuallyFormula()) {
        preprocessEventuallyFormula(formula.getSubformula().asEventuallyFormula(), opInfo, data);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "The subformula of " << formula << " is not supported.");
    }
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessRewardOperatorFormula(storm::logic::RewardOperatorFormula const& formula,
                                                                                        storm::logic::OperatorInformation const& opInfo,
                                                                                        PreprocessorData& data) {
    std::string rewardModelName;
    if (formula.hasRewardModelName()) {
        rewardModelName = formula.getRewardModelName();
        STORM_LOG_THROW(data.model->hasRewardModel(rewardModelName), storm::exceptions::InvalidPropertyException,
                        "The reward model specified by formula " << formula << " does not exist in the model");
    } else {
        // We have to assert that a unique reward model exists, and we need to find its name.
        // However, we might have added auxiliary reward models for other objectives which we have to filter out here.
        auto prefixOf = [](std::string const& left, std::string const& right) {
            return std::mismatch(left.begin(), left.end(), right.begin()).first == left.end();
        };
        bool uniqueRewardModelFound = false;
        for (auto const& rewModel : data.model->getRewardModels()) {
            if (prefixOf(data.rewardModelNamePrefix, rewModel.first)) {
                // Skip auxiliary reward model
                continue;
            }
            STORM_LOG_THROW(!uniqueRewardModelFound, storm::exceptions::InvalidOperationException,
                            "The formula " << formula << " does not specify a reward model name and the reward model is not unique.");
            uniqueRewardModelFound = true;
            rewardModelName = rewModel.first;
        }
        STORM_LOG_THROW(uniqueRewardModelFound, storm::exceptions::InvalidOperationException,
                        "The formula " << formula << " refers to an unnamed reward model but no reward model has been defined.");
    }

    if (formula.getSubformula().isEventuallyFormula()) {
        preprocessEventuallyFormula(formula.getSubformula().asEventuallyFormula(), opInfo, data, rewardModelName);
    } else if (formula.getSubformula().isCumulativeRewardFormula()) {
        preprocessCumulativeRewardFormula(formula.getSubformula().asCumulativeRewardFormula(), opInfo, data, rewardModelName);
    } else if (formula.getSubformula().isTotalRewardFormula()) {
        preprocessTotalRewardFormula(formula.getSubformula().asTotalRewardFormula(), opInfo, data, rewardModelName);
    } else if (formula.getSubformula().isLongRunAverageRewardFormula()) {
        preprocessLongRunAverageRewardFormula(formula.getSubformula().asLongRunAverageRewardFormula(), opInfo, data, rewardModelName);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "The subformula of " << formula << " is not supported.");
    }
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessTimeOperatorFormula(storm::logic::TimeOperatorFormula const& formula,
                                                                                      storm::logic::OperatorInformation const& opInfo, PreprocessorData& data) {
    data.objectives.back()->lowerResultBound = storm::utility::zero<ValueType>();

    if (formula.getSubformula().isEventuallyFormula()) {
        preprocessEventuallyFormula(formula.getSubformula().asEventuallyFormula(), opInfo, data);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "The subformula of " << formula << " is not supported.");
    }
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessLongRunAverageOperatorFormula(storm::logic::LongRunAverageOperatorFormula const& formula,
                                                                                                storm::logic::OperatorInformation const& opInfo,
                                                                                                PreprocessorData& data) {
    data.objectives.back()->lowerResultBound = storm::utility::zero<ValueType>();
    data.objectives.back()->upperResultBound = storm::utility::one<ValueType>();

    // Convert to a long run average reward formula
    // Create and add the new formula
    std::string rewardModelName = data.rewardModelNamePrefix + std::to_string(data.objectives.size());
    auto lraRewardFormula = std::make_shared<storm::logic::LongRunAverageRewardFormula>();
    data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(lraRewardFormula, rewardModelName, opInfo);

    // Create and add the new reward model that only gives one reward for goal states
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> mc(*data.model);
    storm::storage::BitVector subFormulaResult =
        mc.check(formula.getSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    std::vector<typename SparseModelType::ValueType> lraRewards(data.model->getNumberOfStates(), storm::utility::zero<typename SparseModelType::ValueType>());
    storm::utility::vector::setVectorValues(lraRewards, subFormulaResult, storm::utility::one<typename SparseModelType::ValueType>());
    data.model->addRewardModel(rewardModelName, typename SparseModelType::RewardModelType(std::move(lraRewards)));
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessUntilFormula(storm::logic::UntilFormula const& formula,
                                                                               storm::logic::OperatorInformation const& opInfo, PreprocessorData& data,
                                                                               std::shared_ptr<storm::logic::Formula const> subformula) {
    // Try to transform the formula to expected total (or cumulative) rewards

    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> mc(*data.model);
    storm::storage::BitVector rightSubformulaResult =
        mc.check(formula.getRightSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    // Check if the formula is already satisfied in the initial state because then the transformation to expected rewards will fail.
    // TODO: Handle this case more properly
    STORM_LOG_THROW((data.model->getInitialStates() & rightSubformulaResult).empty(), storm::exceptions::NotImplementedException,
                    "The Probability for the objective "
                        << *data.objectives.back()->originalFormula
                        << " is always one as the rhs of the until formula is true in the initial state. This (trivial) case is currently not implemented.");

    // Whenever a state that violates the left subformula or satisfies the right subformula is reached, the objective is 'decided', i.e., no more reward should
    // be collected from there
    storm::storage::BitVector notLeftOrRight =
        mc.check(formula.getLeftSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    notLeftOrRight.complement();
    notLeftOrRight |= rightSubformulaResult;

    // Get the states that are reachable from a notLeftOrRight state
    storm::storage::BitVector allStates(data.model->getNumberOfStates(), true), noStates(data.model->getNumberOfStates(), false);
    storm::storage::BitVector reachableFromGoal =
        storm::utility::graph::getReachableStates(data.model->getTransitionMatrix(), notLeftOrRight, allStates, noStates);
    // Get the states that are reachable from an initial state, stopping at the states reachable from goal
    storm::storage::BitVector reachableFromInit =
        storm::utility::graph::getReachableStates(data.model->getTransitionMatrix(), data.model->getInitialStates(), ~notLeftOrRight, reachableFromGoal);
    // Exclude the actual notLeftOrRight states from the states that are reachable from init
    reachableFromInit &= ~notLeftOrRight;
    // If we can reach a state that is reachable from goal, but which is not a goal state, it means that the transformation to expected rewards is not possible.
    if ((reachableFromInit & reachableFromGoal).empty()) {
        STORM_LOG_INFO("Objective " << *data.objectives.back()->originalFormula << " is transformed to an expected total/cumulative reward property.");
        // Transform to expected total rewards:
        // build stateAction reward vector that gives (one*transitionProbability) reward whenever a transition leads from a reachableFromInit state to a
        // goalState
        std::vector<typename SparseModelType::ValueType> objectiveRewards(data.model->getTransitionMatrix().getRowCount(),
                                                                          storm::utility::zero<typename SparseModelType::ValueType>());
        for (auto state : reachableFromInit) {
            for (uint_fast64_t row = data.model->getTransitionMatrix().getRowGroupIndices()[state];
                 row < data.model->getTransitionMatrix().getRowGroupIndices()[state + 1]; ++row) {
                objectiveRewards[row] = data.model->getTransitionMatrix().getConstrainedRowSum(row, rightSubformulaResult);
            }
        }
        std::string rewardModelName = data.rewardModelNamePrefix + std::to_string(data.objectives.size());
        data.model->addRewardModel(rewardModelName, typename SparseModelType::RewardModelType(std::nullopt, std::move(objectiveRewards)));
        if (subformula == nullptr) {
            subformula = std::make_shared<storm::logic::TotalRewardFormula>();
        }
        data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(subformula, rewardModelName, opInfo);
    } else {
        STORM_LOG_INFO("Objective " << *data.objectives.back()->originalFormula << " can not be transformed to an expected total/cumulative reward property.");
        data.objectives.back()->formula = std::make_shared<storm::logic::ProbabilityOperatorFormula>(formula.asSharedPointer(), opInfo);
    }
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessBoundedUntilFormula(storm::logic::BoundedUntilFormula const& formula,
                                                                                      storm::logic::OperatorInformation const& opInfo, PreprocessorData& data) {
    // Check how to handle this query
    if (formula.isMultiDimensional() || formula.getTimeBoundReference().isRewardBound()) {
        // multidimensional and/or reward-bounded formulas are kept as they are. No preprocessing is done for them.
        data.objectives.back()->formula = std::make_shared<storm::logic::ProbabilityOperatorFormula>(formula.asSharedPointer(), opInfo);
    } else if (!formula.hasLowerBound() || (!formula.isLowerBoundStrict() && storm::utility::isZero(formula.template getLowerBound<storm::RationalNumber>()))) {
        std::shared_ptr<storm::logic::Formula const> subformula;
        if (!formula.hasUpperBound()) {
            // The formula is actually unbounded
            subformula = std::make_shared<storm::logic::TotalRewardFormula>();
        } else {
            STORM_LOG_THROW(!data.model->isOfType(storm::models::ModelType::MarkovAutomaton) || formula.getTimeBoundReference().isTimeBound(),
                            storm::exceptions::InvalidPropertyException,
                            "Bounded until formulas for Markov Automata are only allowed when time bounds are considered.");
            storm::logic::TimeBound bound(formula.isUpperBoundStrict(), formula.getUpperBound());
            subformula = std::make_shared<storm::logic::CumulativeRewardFormula>(bound, formula.getTimeBoundReference());
        }
        preprocessUntilFormula(storm::logic::UntilFormula(formula.getLeftSubformula().asSharedPointer(), formula.getRightSubformula().asSharedPointer()),
                               opInfo, data, subformula);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "Property " << formula << "is not supported");
    }
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessGloballyFormula(storm::logic::GloballyFormula const& formula,
                                                                                  storm::logic::OperatorInformation const& opInfo, PreprocessorData& data) {
    // The formula is transformed to an until formula for the complementary event.
    auto negatedSubformula = std::make_shared<storm::logic::UnaryBooleanStateFormula>(storm::logic::UnaryBooleanStateFormula::OperatorType::Not,
                                                                                      formula.getSubformula().asSharedPointer());

    preprocessUntilFormula(storm::logic::UntilFormula(storm::logic::Formula::getTrueFormula(), negatedSubformula), opInfo, data);
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessEventuallyFormula(storm::logic::EventuallyFormula const& formula,
                                                                                    storm::logic::OperatorInformation const& opInfo, PreprocessorData& data,
                                                                                    boost::optional<std::string> const& optionalRewardModelName) {
    if (formula.isReachabilityProbabilityFormula()) {
        preprocessUntilFormula(
            *std::make_shared<storm::logic::UntilFormula>(storm::logic::Formula::getTrueFormula(), formula.getSubformula().asSharedPointer()), opInfo, data);
        return;
    }

    // Analyze the subformula
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> mc(*data.model);
    storm::storage::BitVector subFormulaResult =
        mc.check(formula.getSubformula())->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();

    // Get the states that are reachable from a goal state
    storm::storage::BitVector allStates(data.model->getNumberOfStates(), true), noStates(data.model->getNumberOfStates(), false);
    storm::storage::BitVector reachableFromGoal =
        storm::utility::graph::getReachableStates(data.model->getTransitionMatrix(), subFormulaResult, allStates, noStates);
    // Get the states that are reachable from an initial state, stopping at the states reachable from goal
    storm::storage::BitVector reachableFromInit =
        storm::utility::graph::getReachableStates(data.model->getTransitionMatrix(), data.model->getInitialStates(), allStates, reachableFromGoal);
    // Exclude the actual goal states from the states that are reachable from an initial state
    reachableFromInit &= ~subFormulaResult;
    // If we can reach a state that is reachable from goal but which is not a goal state, it means that the transformation to expected total rewards is not
    // possible.
    if ((reachableFromInit & reachableFromGoal).empty()) {
        // Transform to expected total rewards.
        STORM_LOG_INFO("Objective " << *data.objectives.back()->originalFormula << " is transformed to an expected total reward property.");
        std::string const rewardModelName = data.rewardModelNamePrefix + std::to_string(data.objectives.size());
        auto const totalRewardFormula = std::make_shared<storm::logic::TotalRewardFormula>();
        data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(totalRewardFormula, rewardModelName, opInfo);

        if (formula.isReachabilityRewardFormula()) {
            // The reachableFromGoal-states are those that are *only* reachable via goal.
            // This is true because we applied the goal unfolding before, and we are in the (reachableFromInit & reachableFromGoal).empty() case).
            // We therefore clear all the rewards collected at reachableFromGoal-states.
            assert(optionalRewardModelName.is_initialized());
            auto objectiveRewards =
                storm::utility::createFilteredRewardModel(data.model->getRewardModel(optionalRewardModelName.get()), data.model->isDiscreteTimeModel(), formula)
                    .extract();
            // Reduce potential transition branch rewards to state-action rewards.
            objectiveRewards.reduceToStateBasedRewards(data.model->getTransitionMatrix(), false);
            STORM_LOG_ASSERT(!objectiveRewards.hasTransitionRewards(), "Expected no transition rewards after reducing to state-based rewards");
            // clear state-rewards
            if (objectiveRewards.hasStateRewards()) {
                storm::utility::vector::setVectorValues(objectiveRewards.getStateRewardVector(), reachableFromGoal,
                                                        storm::utility::zero<typename SparseModelType::ValueType>());
            }
            // clear state-action rewards
            if (objectiveRewards.hasStateActionRewards()) {
                for (auto state : reachableFromGoal) {
                    std::fill_n(objectiveRewards.getStateActionRewardVector().begin() + data.model->getTransitionMatrix().getRowGroupIndices()[state],
                                data.model->getTransitionMatrix().getRowGroupSize(state), storm::utility::zero<typename SparseModelType::ValueType>());
                }
            }
            // add the new reward model
            data.model->addRewardModel(rewardModelName, std::move(objectiveRewards));
        } else if (formula.isReachabilityTimeFormula()) {
            // build state reward vector that only gives reward for relevant states
            std::vector<typename SparseModelType::ValueType> timeRewards(data.model->getNumberOfStates(),
                                                                         storm::utility::zero<typename SparseModelType::ValueType>());
            if (data.model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
                storm::utility::vector::setVectorValues(
                    timeRewards,
                    dynamic_cast<storm::models::sparse::MarkovAutomaton<typename SparseModelType::ValueType> const&>(*data.model).getMarkovianStates() &
                        reachableFromInit,
                    storm::utility::one<typename SparseModelType::ValueType>());
            } else {
                storm::utility::vector::setVectorValues(timeRewards, reachableFromInit, storm::utility::one<typename SparseModelType::ValueType>());
            }
            data.model->addRewardModel(rewardModelName, typename SparseModelType::RewardModelType(std::move(timeRewards)));
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException,
                            "The formula " << formula << " neither considers reachability probabilities nor reachability rewards "
                                           << (data.model->isOfType(storm::models::ModelType::MarkovAutomaton) ? "nor reachability time" : "")
                                           << ". This is not supported.");
        }
    } else {
        STORM_LOG_INFO("Objective " << *data.objectives.back()->originalFormula << " can not be transformed to an expected total/cumulative reward property.");
        if (formula.isReachabilityRewardFormula()) {
            // TODO: this probably needs some better treatment regarding schedulers that do not reach the goal state allmost surely
            assert(optionalRewardModelName.is_initialized());
            if (data.deadlockLabel) {
                // We made some states absorbing and created a new deadlock state. To make sure that this deadlock state gets value zero, we add it to the set
                // of goal states of the formula.
                std::shared_ptr<storm::logic::Formula const> newSubSubformula =
                    std::make_shared<storm::logic::AtomicLabelFormula const>(data.deadlockLabel.get());
                std::shared_ptr<storm::logic::Formula const> newSubformula = std::make_shared<storm::logic::BinaryBooleanStateFormula const>(
                    storm::logic::BinaryBooleanStateFormula::OperatorType::Or, formula.getSubformula().asSharedPointer(), newSubSubformula);
                boost::optional<storm::logic::RewardAccumulation> newRewardAccumulation;
                if (formula.hasRewardAccumulation()) {
                    newRewardAccumulation = formula.getRewardAccumulation();
                }
                std::shared_ptr<storm::logic::Formula const> newFormula =
                    std::make_shared<storm::logic::EventuallyFormula const>(newSubformula, formula.getContext(), newRewardAccumulation);
                data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(newFormula, optionalRewardModelName.get(), opInfo);
            } else {
                data.objectives.back()->formula =
                    std::make_shared<storm::logic::RewardOperatorFormula>(formula.asSharedPointer(), optionalRewardModelName.get(), opInfo);
            }
        } else if (formula.isReachabilityTimeFormula()) {
            // Reduce to reachability rewards so that time formulas do not have to be treated seperately later.
            std::string rewardModelName = data.rewardModelNamePrefix + std::to_string(data.objectives.size());
            std::shared_ptr<storm::logic::Formula const> newSubformula = formula.getSubformula().asSharedPointer();
            if (data.deadlockLabel) {
                // We made some states absorbing and created a new deadlock state. To make sure that this deadlock state gets value zero, we add it to the set
                // of goal states of the formula.
                std::shared_ptr<storm::logic::Formula const> newSubSubformula =
                    std::make_shared<storm::logic::AtomicLabelFormula const>(data.deadlockLabel.get());
                newSubformula = std::make_shared<storm::logic::BinaryBooleanStateFormula const>(storm::logic::BinaryBooleanStateFormula::OperatorType::Or,
                                                                                                formula.getSubformula().asSharedPointer(), newSubSubformula);
            }
            auto newFormula = std::make_shared<storm::logic::EventuallyFormula>(newSubformula, storm::logic::FormulaContext::Reward);
            data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(newFormula, rewardModelName, opInfo);
            std::vector<typename SparseModelType::ValueType> timeRewards;
            if (data.model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
                timeRewards.assign(data.model->getNumberOfStates(), storm::utility::zero<typename SparseModelType::ValueType>());
                storm::utility::vector::setVectorValues(
                    timeRewards,
                    dynamic_cast<storm::models::sparse::MarkovAutomaton<typename SparseModelType::ValueType> const&>(*data.model).getMarkovianStates(),
                    storm::utility::one<typename SparseModelType::ValueType>());
            } else {
                timeRewards.assign(data.model->getNumberOfStates(), storm::utility::one<typename SparseModelType::ValueType>());
            }
            data.model->addRewardModel(rewardModelName, typename SparseModelType::RewardModelType(std::move(timeRewards)));
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException,
                            "The formula " << formula << " neither considers reachability probabilities nor reachability rewards "
                                           << (data.model->isOfType(storm::models::ModelType::MarkovAutomaton) ? "nor reachability time" : "")
                                           << ". This is not supported.");
        }
    }
    data.finiteRewardCheckObjectives.set(data.objectives.size() - 1, true);
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessCumulativeRewardFormula(storm::logic::CumulativeRewardFormula const& formula,
                                                                                          storm::logic::OperatorInformation const& opInfo,
                                                                                          PreprocessorData& data,
                                                                                          boost::optional<std::string> const& optionalRewardModelName) {
    STORM_LOG_THROW(data.model->isOfType(storm::models::ModelType::Mdp), storm::exceptions::InvalidPropertyException,
                    "Cumulative reward formulas are not supported for the given model type.");
    std::string rewardModelName = optionalRewardModelName.get();
    // Strip away potential RewardAccumulations in the formula itself but also in reward bounds
    auto filteredRewards = storm::utility::createFilteredRewardModel(data.model->getRewardModel(rewardModelName), data.model->isDiscreteTimeModel(), formula);
    if (filteredRewards.isDifferentFromUnfilteredModel()) {
        rewardModelName = data.rewardModelNamePrefix + std::to_string(data.objectives.size());
        data.model->addRewardModel(rewardModelName, std::move(filteredRewards.extract()));
    }
    // Clear potential transition rewards.
    auto& rewardModel = data.model->getRewardModel(rewardModelName);
    rewardModel.reduceToStateBasedRewards(data.model->getTransitionMatrix(), false);
    STORM_LOG_ASSERT(!rewardModel.hasTransitionRewards(), "Expected no transition rewards after reducing to state-based rewards");

    std::vector<storm::logic::TimeBoundReference> newTimeBoundReferences;
    bool onlyRewardBounds = true;
    for (uint64_t i = 0; i < formula.getDimension(); ++i) {
        auto oldTbr = formula.getTimeBoundReference(i);
        if (oldTbr.isRewardBound()) {
            if (oldTbr.hasRewardAccumulation()) {
                auto filteredBoundRewards = storm::utility::createFilteredRewardModel(data.model->getRewardModel(oldTbr.getRewardName()),
                                                                                      oldTbr.getRewardAccumulation(), data.model->isDiscreteTimeModel());
                if (filteredBoundRewards.isDifferentFromUnfilteredModel()) {
                    std::string freshRewardModelName =
                        data.rewardModelNamePrefix + std::to_string(data.objectives.size()) + std::string("_" + std::to_string(i));
                    data.model->addRewardModel(freshRewardModelName, std::move(filteredBoundRewards.extract()));
                    newTimeBoundReferences.emplace_back(freshRewardModelName);
                } else {
                    // Strip away the reward accumulation
                    newTimeBoundReferences.emplace_back(oldTbr.getRewardName());
                }
            } else {
                newTimeBoundReferences.push_back(oldTbr);
            }
        } else {
            onlyRewardBounds = false;
            newTimeBoundReferences.push_back(oldTbr);
        }
    }

    auto newFormula = std::make_shared<storm::logic::CumulativeRewardFormula>(formula.getBounds(), newTimeBoundReferences);
    data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(newFormula, rewardModelName, opInfo);

    if (onlyRewardBounds) {
        data.finiteRewardCheckObjectives.set(data.objectives.size() - 1, true);
    }
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessTotalRewardFormula(storm::logic::TotalRewardFormula const& formula,
                                                                                     storm::logic::OperatorInformation const& opInfo, PreprocessorData& data,
                                                                                     boost::optional<std::string> const& optionalRewardModelName) {
    std::string rewardModelName = optionalRewardModelName.get();
    auto filteredRewards = storm::utility::createFilteredRewardModel(data.model->getRewardModel(rewardModelName), data.model->isDiscreteTimeModel(), formula);
    if (filteredRewards.isDifferentFromUnfilteredModel()) {
        rewardModelName = data.rewardModelNamePrefix + std::to_string(data.objectives.size());
        data.model->addRewardModel(rewardModelName, filteredRewards.extract());
    }
    // Reduce potential transition branch rewards to state-action rewards
    auto& rewardModel = data.model->getRewardModel(rewardModelName);
    rewardModel.reduceToStateBasedRewards(data.model->getTransitionMatrix(), false);
    STORM_LOG_ASSERT(!rewardModel.hasTransitionRewards(), "Expected no transition rewards after reducing to state-based rewards");

    data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(formula.stripRewardAccumulation(), rewardModelName, opInfo);
    data.finiteRewardCheckObjectives.set(data.objectives.size() - 1, true);
}

template<typename SparseModelType>
void SparseMultiObjectivePreprocessor<SparseModelType>::preprocessLongRunAverageRewardFormula(storm::logic::LongRunAverageRewardFormula const& formula,
                                                                                              storm::logic::OperatorInformation const& opInfo,
                                                                                              PreprocessorData& data,
                                                                                              boost::optional<std::string> const& optionalRewardModelName) {
    std::string rewardModelName = optionalRewardModelName.get();
    auto filteredRewards = storm::utility::createFilteredRewardModel(data.model->getRewardModel(rewardModelName), data.model->isDiscreteTimeModel(), formula);
    if (filteredRewards.isDifferentFromUnfilteredModel()) {
        std::string rewardModelName = data.rewardModelNamePrefix + std::to_string(data.objectives.size());
        data.model->addRewardModel(rewardModelName, std::move(filteredRewards.extract()));
    }
    // Reduce potential transition branch rewards to state-action rewards
    auto& rewardModel = data.model->getRewardModel(rewardModelName);
    rewardModel.reduceToStateBasedRewards(data.model->getTransitionMatrix(), false);
    STORM_LOG_ASSERT(!rewardModel.hasTransitionRewards(), "Expected no transition rewards after reducing to state-based rewards");

    data.objectives.back()->formula = std::make_shared<storm::logic::RewardOperatorFormula>(formula.stripRewardAccumulation(), rewardModelName, opInfo);
}

template<typename SparseModelType>
typename SparseMultiObjectivePreprocessor<SparseModelType>::ReturnType SparseMultiObjectivePreprocessor<SparseModelType>::buildResult(
    SparseModelType const& originalModel, storm::logic::MultiObjectiveFormula const& originalFormula, PreprocessorData& data) {
    ReturnType result(originalFormula, originalModel);
    auto backwardTransitions = data.model->getBackwardTransitions();
    result.preprocessedModel = data.model;
    result.memoryIncorporationReverseData = data.memoryIncorporationReverseData;

    for (auto& obj : data.objectives) {
        result.objectives.push_back(std::move(*obj));
    }
    result.queryType = getQueryType(result.objectives);
    result.maybeInfiniteRewardObjectives = std::move(data.finiteRewardCheckObjectives);

    return result;
}

template<typename SparseModelType>
typename SparseMultiObjectivePreprocessor<SparseModelType>::ReturnType::QueryType SparseMultiObjectivePreprocessor<SparseModelType>::getQueryType(
    std::vector<Objective<ValueType>> const& objectives) {
    uint_fast64_t numOfObjectivesWithThreshold = 0;
    for (auto& obj : objectives) {
        if (obj.formula->hasBound()) {
            ++numOfObjectivesWithThreshold;
        }
    }
    if (numOfObjectivesWithThreshold == objectives.size()) {
        return ReturnType::QueryType::Achievability;
    } else if (numOfObjectivesWithThreshold + 1 == objectives.size()) {
        // Note: We do not want to consider a Pareto query when the total number of objectives is one.
        return ReturnType::QueryType::Quantitative;
    } else if (numOfObjectivesWithThreshold == 0) {
        return ReturnType::QueryType::Pareto;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException,
                        "Invalid Multi-objective query: The numer of qualitative objectives should be either 0 (Pareto query), 1 (quantitative query), or "
                        "#objectives (achievability query).");
    }
}

template class SparseMultiObjectivePreprocessor<storm::models::sparse::Mdp<double>>;
template class SparseMultiObjectivePreprocessor<storm::models::sparse::MarkovAutomaton<double>>;

template class SparseMultiObjectivePreprocessor<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class SparseMultiObjectivePreprocessor<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
}  // namespace preprocessing
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
