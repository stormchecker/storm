#include "storm-pomdp/modelchecker/PreprocessingPomdpValueBoundsModelChecker.h"
#include <random>

#include "storm-pomdp/storage/PomdpMemory.h"
#include "storm-pomdp/transformer/PomdpMemoryUnfolder.h"

#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/storage/Scheduler.h"

#include "storm/environment/Environment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace pomdp {
namespace modelchecker {
template<typename ValueType>
PreprocessingPomdpValueBoundsModelChecker<ValueType>::PreprocessingPomdpValueBoundsModelChecker(storm::models::sparse::Pomdp<ValueType> const& pomdp)
    : pomdp(pomdp) { /* Intentionally left empty */ }

template<typename ValueType>
typename PreprocessingPomdpValueBoundsModelChecker<ValueType>::ValueBounds PreprocessingPomdpValueBoundsModelChecker<ValueType>::getValueBounds(
    storm::Environment const& env, storm::logic::Formula const& formula) {
    return getValueBounds(env, formula, storm::pomdp::analysis::getFormulaInformation(pomdp, formula));
}

template<typename ValueType>
typename PreprocessingPomdpValueBoundsModelChecker<ValueType>::ValueBounds PreprocessingPomdpValueBoundsModelChecker<ValueType>::getValueBounds(
    storm::logic::Formula const& formula) {
    storm::Environment env;
    return getValueBounds(env, formula, storm::pomdp::analysis::getFormulaInformation(pomdp, formula));
}

template<typename ValueType>
std::vector<ValueType> PreprocessingPomdpValueBoundsModelChecker<ValueType>::getChoiceValues(std::vector<ValueType> const& stateValues,
                                                                                             std::vector<ValueType>* actionBasedRewards) {
    std::vector<ValueType> choiceValues((pomdp.getNumberOfChoices()));
    pomdp.getTransitionMatrix().multiplyWithVector(stateValues, choiceValues, actionBasedRewards);
    return choiceValues;
}

template<typename ValueType>
std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>> PreprocessingPomdpValueBoundsModelChecker<ValueType>::computeValuesForGuessedScheduler(
    storm::Environment const& env, std::vector<ValueType> const& stateValues, std::vector<ValueType>* actionBasedRewards, storm::logic::Formula const& formula,
    storm::pomdp::analysis::FormulaInformation const& info, std::shared_ptr<storm::models::sparse::Mdp<ValueType>> underlyingMdp,
    ValueType const& scoreThreshold, bool relativeScore) {
    // Create some positional scheduler for the POMDP
    storm::storage::Scheduler<ValueType> pomdpScheduler(pomdp.getNumberOfStates());
    // For each state, we heuristically find a good distribution over output actions.
    auto choiceValues = getChoiceValues(stateValues, actionBasedRewards);
    auto const& choiceIndices = pomdp.getTransitionMatrix().getRowGroupIndices();
    std::vector<storm::storage::Distribution<ValueType, uint_fast64_t>> choiceDistributions(pomdp.getNrObservations());
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        auto& choiceDistribution = choiceDistributions[pomdp.getObservation(state)];
        ValueType const& stateValue = stateValues[state];
        STORM_LOG_ASSERT(stateValue >= storm::utility::zero<ValueType>(), "State value expected non-negative.");
        for (auto choice = choiceIndices[state]; choice < choiceIndices[state + 1]; ++choice) {
            ValueType const& choiceValue = choiceValues[choice];
            STORM_LOG_ASSERT(choiceValue >= storm::utility::zero<ValueType>(), "Choice value expected non-negative.");
            // Rate this choice by considering the relative difference between the choice value and the (optimal) state value
            // A high score shall mean that the choice is "good"
            if (storm::utility::isInfinity(stateValue)) {
                // For infinity states, we simply distribute uniformly.
                // This case could be handled a bit more sensible
                choiceDistribution.addProbability(choice - choiceIndices[state], scoreThreshold);
            } else {
                ValueType choiceScore = info.minimize() ? (choiceValue - stateValue) : (stateValue - choiceValue);
                if (relativeScore) {
                    ValueType avg = (stateValue + choiceValue) / storm::utility::convertNumber<ValueType, uint64_t>(2);
                    if (!storm::utility::isZero(avg)) {
                        choiceScore /= avg;
                    }
                }
                choiceScore = storm::utility::one<ValueType>() - choiceScore;
                if (choiceScore >= scoreThreshold) {
                    choiceDistribution.addProbability(choice - choiceIndices[state], choiceScore);
                }
            }
        }
        // If the distribution is empty, i.e. no choice has had a suitable score, distribute uniformly
        if (choiceDistribution.size() == 0) {
            for (auto choice = choiceIndices[state]; choice < choiceIndices[state + 1]; ++choice) {
                choiceDistribution.addProbability(choice - choiceIndices[state], scoreThreshold);
            }
        }
        STORM_LOG_ASSERT(choiceDistribution.size() > 0, "Empty choice distribution.");
    }
    // Normalize all distributions
    for (auto& choiceDistribution : choiceDistributions) {
        choiceDistribution.normalize();
    }
    // Set the scheduler for all states
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        pomdpScheduler.setChoice(choiceDistributions[pomdp.getObservation(state)], state);
    }
    STORM_LOG_ASSERT(!pomdpScheduler.isPartialScheduler(), "Expected a fully defined scheduler.");
    auto scheduledModel = underlyingMdp->applyScheduler(pomdpScheduler, false);

    auto resultPtr = storm::api::verifyWithSparseEngine<ValueType>(env, scheduledModel, storm::api::createTask<ValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<ValueType> pomdpSchedulerResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector());
    return std::make_pair(pomdpSchedulerResult, pomdpScheduler);
}

template<typename ValueType>
std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>> PreprocessingPomdpValueBoundsModelChecker<ValueType>::computeValuesForRandomFMPolicy(
    storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info, uint64_t memoryBound) {
    // Consider memoryless policy on memory-unfolded POMDP
    storm::storage::Scheduler<ValueType> pomdpScheduler(pomdp.getNumberOfStates() * memoryBound);

    STORM_LOG_DEBUG("Computing the unfolding for memory bound " << memoryBound);
    storm::storage::PomdpMemory memory = storm::storage::PomdpMemoryBuilder().build(storm::storage::PomdpMemoryPattern::Full, memoryBound);
    storm::transformer::PomdpMemoryUnfolder<ValueType> memoryUnfolder(pomdp, memory);
    // We keep unreachable states to not mess with the state ordering and capture potential better choices
    auto memPomdp = memoryUnfolder.transform(false);

    // Determine an observation-based policy by choosing any of the enabled actions uniformly at random
    std::vector<uint64_t> obsChoiceVector(memPomdp->getNrObservations());
    std::random_device rd;
    auto engine = std::mt19937(rd());
    for (uint64_t obs = 0; obs < memPomdp->getNrObservations(); ++obs) {
        uint64_t nrChoices = memPomdp->getNumberOfChoices(memPomdp->getStatesWithObservation(obs).front());
        std::uniform_int_distribution<uint64_t> uniform_dist(0, nrChoices - 1);
        obsChoiceVector[obs] = uniform_dist(engine);
    }
    for (uint64_t state = 0; state < memPomdp->getNumberOfStates(); ++state) {
        pomdpScheduler.setChoice(obsChoiceVector[memPomdp->getObservation(state)], state);
    }

    // Model check the DTMC resulting from the policy
    auto underlyingMdp =
        std::make_shared<storm::models::sparse::Mdp<ValueType>>(memPomdp->getTransitionMatrix(), memPomdp->getStateLabeling(), memPomdp->getRewardModels());
    auto scheduledModel = underlyingMdp->applyScheduler(pomdpScheduler, false);
    auto resultPtr = storm::api::verifyWithSparseEngine<ValueType>(env, scheduledModel, storm::api::createTask<ValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<ValueType> pomdpSchedulerResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector());

    // Take the optimal value in ANY of the unfolded states for a POMDP state as the resulting state value
    std::vector<ValueType> res(pomdp.getNumberOfStates(), info.minimize() ? storm::utility::infinity<ValueType>() : -storm::utility::infinity<ValueType>());
    for (uint64_t memPomdpState = 0; memPomdpState < pomdpSchedulerResult.size(); ++memPomdpState) {
        uint64_t modelState = memPomdpState / memoryBound;
        if ((info.minimize() && pomdpSchedulerResult[memPomdpState] < res[modelState]) ||
            (!info.minimize() && pomdpSchedulerResult[memPomdpState] > res[modelState])) {
            res[modelState] = pomdpSchedulerResult[memPomdpState];
        }
    }
    return std::make_pair(res, pomdpScheduler);
}

template<typename ValueType>
[[maybe_unused]] std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>>
PreprocessingPomdpValueBoundsModelChecker<ValueType>::computeValuesForRandomMemorylessPolicy(
    storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info,
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> underlyingMdp) {
    storm::storage::Scheduler<ValueType> pomdpScheduler(pomdp.getNumberOfStates());
    std::vector<uint64_t> obsChoiceVector(pomdp.getNrObservations());

    std::random_device rd;
    auto engine = std::mt19937(rd());
    for (uint64_t obs = 0; obs < pomdp.getNrObservations(); ++obs) {
        uint64_t nrChoices = pomdp.getNumberOfChoices(pomdp.getStatesWithObservation(obs).front());
        std::uniform_int_distribution<uint64_t> uniform_dist(0, nrChoices - 1);
        obsChoiceVector[obs] = uniform_dist(engine);
    }

    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        STORM_LOG_DEBUG("State " << state << " -- Random Choice " << obsChoiceVector[pomdp.getObservation(state)]);
        pomdpScheduler.setChoice(obsChoiceVector[pomdp.getObservation(state)], state);
    }

    auto scheduledModel = underlyingMdp->applyScheduler(pomdpScheduler, false);

    auto resultPtr = storm::api::verifyWithSparseEngine<ValueType>(env, scheduledModel, storm::api::createTask<ValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<ValueType> pomdpSchedulerResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector());

    STORM_LOG_DEBUG("Initial Value for guessed Policy: " << pomdpSchedulerResult[pomdp.getInitialStates().getNextSetIndex(0)]);

    return std::make_pair(pomdpSchedulerResult, pomdpScheduler);
}

template<typename ValueType>
typename PreprocessingPomdpValueBoundsModelChecker<ValueType>::ValueBounds PreprocessingPomdpValueBoundsModelChecker<ValueType>::getValueBounds(
    storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info) {
    STORM_LOG_THROW(info.isNonNestedReachabilityProbability() || info.isNonNestedExpectedRewardFormula(), storm::exceptions::NotSupportedException,
                    "The property type is not supported for this analysis.");

    // Compute the values on the fully observable MDP
    // We need an actual MDP so that we can apply schedulers below.
    // Also, the api call in the next line will require a copy anyway.
    auto underlyingMdp =
        std::make_shared<storm::models::sparse::Mdp<ValueType>>(pomdp.getTransitionMatrix(), pomdp.getStateLabeling(), pomdp.getRewardModels());
    auto resultPtr = storm::api::verifyWithSparseEngine<ValueType>(env, underlyingMdp, storm::api::createTask<ValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<ValueType> fullyObservableResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector());

    std::vector<ValueType> actionBasedRewards;
    std::vector<ValueType>* actionBasedRewardsPtr = nullptr;
    if (info.isNonNestedExpectedRewardFormula()) {
        actionBasedRewards = pomdp.getRewardModel(info.getRewardModelName()).getTotalRewardVector(pomdp.getTransitionMatrix());
        actionBasedRewardsPtr = &actionBasedRewards;
    }
    std::vector<std::vector<ValueType>> guessedSchedulerValues;
    std::vector<storm::storage::Scheduler<ValueType>> guessedSchedulers;
    std::shared_ptr<std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>>> guessedSchedulerPair;
    std::vector<std::pair<double, bool>> guessParameters({{0.875, false}, {0.875, true}, {0.75, false}, {0.75, true}});
    for (auto const& pars : guessParameters) {
        guessedSchedulerPair = std::make_shared<std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>>>(
            computeValuesForGuessedScheduler(env, fullyObservableResult, actionBasedRewardsPtr, formula, info, underlyingMdp,
                                             storm::utility::convertNumber<ValueType>(pars.first), pars.second));
        guessedSchedulerValues.push_back(guessedSchedulerPair->first);
        guessedSchedulers.push_back(guessedSchedulerPair->second);
    }

    // compute the 'best' guess and do a few iterations on it
    uint64_t bestGuess = 0;
    ValueType bestGuessSum = std::accumulate(guessedSchedulerValues.front().begin(), guessedSchedulerValues.front().end(), storm::utility::zero<ValueType>());
    for (uint64_t guess = 1; guess < guessedSchedulerValues.size(); ++guess) {
        ValueType guessSum = std::accumulate(guessedSchedulerValues[guess].begin(), guessedSchedulerValues[guess].end(), storm::utility::zero<ValueType>());
        if ((info.minimize() && guessSum < bestGuessSum) || (info.maximize() && guessSum > bestGuessSum)) {
            bestGuess = guess;
            bestGuessSum = guessSum;
        }
    }
    guessedSchedulerPair = std::make_shared<std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>>>(
        computeValuesForGuessedScheduler(env, guessedSchedulerValues[bestGuess], actionBasedRewardsPtr, formula, info, underlyingMdp,
                                         storm::utility::convertNumber<ValueType>(guessParameters[bestGuess].first), guessParameters[bestGuess].second));
    guessedSchedulerValues.push_back(guessedSchedulerPair->first);
    guessedSchedulers.push_back(guessedSchedulerPair->second);
    guessedSchedulerPair = std::make_shared<std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>>>(
        computeValuesForGuessedScheduler(env, guessedSchedulerValues.back(), actionBasedRewardsPtr, formula, info, underlyingMdp,
                                         storm::utility::convertNumber<ValueType>(guessParameters[bestGuess].first), guessParameters[bestGuess].second));
    guessedSchedulerValues.push_back(guessedSchedulerPair->first);
    guessedSchedulers.push_back(guessedSchedulerPair->second);
    guessedSchedulerPair = std::make_shared<std::pair<std::vector<ValueType>, storm::storage::Scheduler<ValueType>>>(
        computeValuesForGuessedScheduler(env, guessedSchedulerValues.back(), actionBasedRewardsPtr, formula, info, underlyingMdp,
                                         storm::utility::convertNumber<ValueType>(guessParameters[bestGuess].first), guessParameters[bestGuess].second));
    guessedSchedulerValues.push_back(guessedSchedulerPair->first);
    guessedSchedulers.push_back(guessedSchedulerPair->second);

    // Check if one of the guesses is worse than one of the others (and potentially delete it)
    // Avoid deleting entries during the loop to ensure that indices remain valid
    storm::storage::BitVector keptGuesses(guessedSchedulerValues.size(), true);
    for (uint64_t i = 0; i < guessedSchedulerValues.size() - 1; ++i) {
        if (!keptGuesses.get(i)) {
            continue;
        }
        for (uint64_t j = i + 1; j < guessedSchedulerValues.size(); ++j) {
            if (!keptGuesses.get(j)) {
                continue;
            }
            if (storm::utility::vector::compareElementWise(guessedSchedulerValues[i], guessedSchedulerValues[j], std::less_equal<ValueType>())) {
                if (info.minimize()) {
                    // In this case we are guessing upper bounds (and smaller upper bounds are better)
                    keptGuesses.set(j, false);
                } else {
                    // In this case we are guessing lower bounds (and larger lower bounds are better)
                    keptGuesses.set(i, false);
                    break;
                }
            } else if (storm::utility::vector::compareElementWise(guessedSchedulerValues[j], guessedSchedulerValues[i], std::less_equal<ValueType>())) {
                if (info.minimize()) {
                    keptGuesses.set(i, false);
                    break;
                } else {
                    keptGuesses.set(j, false);
                }
            }
        }
    }
    STORM_LOG_INFO("Keeping scheduler guesses " << keptGuesses);
    storm::utility::vector::filterVectorInPlace(guessedSchedulerValues, keptGuesses);
    std::vector<storm::storage::Scheduler<ValueType>> filteredSchedulers;
    for (uint64_t i = 0; i < guessedSchedulers.size(); ++i) {
        if (keptGuesses[i]) {
            filteredSchedulers.push_back(guessedSchedulers[i]);
        }
    }

    // Finally prepare the result
    ValueBounds result;
    if (info.minimize()) {
        result.lower.push_back(std::move(fullyObservableResult));
        result.upper = std::move(guessedSchedulerValues);
        result.upperSchedulers = filteredSchedulers;
    } else {
        result.lower = std::move(guessedSchedulerValues);
        result.upper.push_back(std::move(fullyObservableResult));
        result.lowerSchedulers = filteredSchedulers;
    }
    STORM_LOG_WARN_COND_DEBUG(storm::utility::vector::compareElementWise(result.lower.front(), result.upper.front(), std::less_equal<ValueType>()),
                              "Lower bound is larger than upper bound");
    return result;
}

template<typename ValueType>
typename PreprocessingPomdpValueBoundsModelChecker<ValueType>::ExtremeValueBound PreprocessingPomdpValueBoundsModelChecker<ValueType>::getExtremeValueBound(
    storm::logic::Formula const& formula) {
    storm::Environment env;
    return getExtremeValueBound(env, formula);
}

template<typename ValueType>
typename PreprocessingPomdpValueBoundsModelChecker<ValueType>::ExtremeValueBound PreprocessingPomdpValueBoundsModelChecker<ValueType>::getExtremeValueBound(
    storm::Environment const& env, storm::logic::Formula const& formula) {
    return getExtremeValueBound(env, formula, storm::pomdp::analysis::getFormulaInformation(pomdp, formula));
}

template<typename ValueType>
typename PreprocessingPomdpValueBoundsModelChecker<ValueType>::ExtremeValueBound PreprocessingPomdpValueBoundsModelChecker<ValueType>::getExtremeValueBound(
    storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info) {
    STORM_LOG_THROW(info.isNonNestedExpectedRewardFormula(), storm::exceptions::NotSupportedException, "The property type is not supported for this analysis.");

    // Compute the values for the opposite direction on the fully observable MDP
    // We need an actual MDP so that we can apply schedulers below.
    // Also, the api call in the next line will require a copy anyway.
    storm::logic::RewardOperatorFormula newFormula(formula.asRewardOperatorFormula());
    if (formula.asOperatorFormula().getOptimalityType() == storm::solver::OptimizationDirection::Maximize) {
        newFormula.setOptimalityType(storm::solver::OptimizationDirection::Minimize);
    } else {
        newFormula.setOptimalityType(storm::solver::OptimizationDirection::Maximize);
    }
    auto formulaPtr = std::make_shared<storm::logic::RewardOperatorFormula>(newFormula);
    auto underlyingMdp =
        std::make_shared<storm::models::sparse::Mdp<ValueType>>(pomdp.getTransitionMatrix(), pomdp.getStateLabeling(), pomdp.getRewardModels());
    auto resultPtr = storm::api::verifyWithSparseEngine<ValueType>(env, underlyingMdp, storm::api::createTask<ValueType>(formulaPtr, false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<ValueType> resultVec = std::move(resultPtr->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector());
    ExtremeValueBound res;
    if (info.minimize()) {
        res.min = false;
    } else {
        res.min = true;
    }
    res.isInfinite = storm::utility::vector::filterInfinity(resultVec);
    res.values = std::move(resultVec);
    return res;
}

template class PreprocessingPomdpValueBoundsModelChecker<double>;

template class PreprocessingPomdpValueBoundsModelChecker<storm::RationalNumber>;
}  // namespace modelchecker
}  // namespace pomdp
}  // namespace storm