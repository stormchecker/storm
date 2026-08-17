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

namespace storm::pomdp::modelchecker {
template<typename PomdpType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PreprocessingPomdpValueBoundsModelChecker(PomdpType const& pomdp)
    : pomdp(pomdp) { /* Intentionally left empty */ }

template<typename PomdpType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::ValueBounds PreprocessingPomdpValueBoundsModelChecker<PomdpType>::getValueBounds(
    storm::Environment const& env, storm::logic::Formula const& formula) {
    return getValueBounds(env, formula, storm::pomdp::analysis::getFormulaInformation(pomdp, formula));
}

template<typename PomdpType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::ValueBounds PreprocessingPomdpValueBoundsModelChecker<PomdpType>::getValueBounds(
    storm::logic::Formula const& formula) {
    storm::Environment env;
    return getValueBounds(env, formula, storm::pomdp::analysis::getFormulaInformation(pomdp, formula));
}

template<typename PomdpType>
std::vector<typename PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PomdpValueType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::getChoiceValues(std::vector<PomdpValueType> const& stateValues,
                                                                      std::vector<PomdpValueType>* actionBasedRewards) {
    std::vector<PomdpValueType> choiceValues((pomdp.getNumberOfChoices()));
    pomdp.getTransitionMatrix().multiplyWithVector(stateValues, choiceValues, actionBasedRewards);
    return choiceValues;
}

template<typename PomdpType>
std::pair<std::vector<typename PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PomdpValueType>,
          storm::storage::Scheduler<typename PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PomdpValueType>>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::computeValuesForGuessedScheduler(
    storm::Environment const& env, std::vector<PomdpValueType> const& stateValues, std::vector<PomdpValueType>* actionBasedRewards,
    storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info,
    std::shared_ptr<storm::models::sparse::Mdp<PomdpValueType>> underlyingMdp, PomdpValueType const& scoreThreshold, bool relativeScore) {
    // Create some positional scheduler for the POMDP
    storm::storage::Scheduler<PomdpValueType> pomdpScheduler(pomdp.getNumberOfStates());
    // For each state, we heuristically find a good distribution over output actions.
    auto choiceValues = getChoiceValues(stateValues, actionBasedRewards);
    auto const& choiceIndices = pomdp.getTransitionMatrix().getRowGroupIndices();
    std::vector<storm::storage::Distribution<PomdpValueType, uint_fast64_t>> choiceDistributions(pomdp.getNrObservations());
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        auto& choiceDistribution = choiceDistributions[pomdp.getObservation(state)];
        PomdpValueType const& stateValue = stateValues[state];
        STORM_LOG_ASSERT(stateValue >= storm::utility::zero<PomdpValueType>(), "State value expected non-negative.");
        for (auto choice = choiceIndices[state]; choice < choiceIndices[state + 1]; ++choice) {
            PomdpValueType const& choiceValue = choiceValues[choice];
            STORM_LOG_ASSERT(choiceValue >= storm::utility::zero<PomdpValueType>(), "Choice value expected non-negative.");
            // Rate this choice by considering the relative difference between the choice value and the (optimal) state value
            // A high score shall mean that the choice is "good"
            if (storm::utility::isInfinity(stateValue)) {
                // For infinity states, we simply distribute uniformly.
                // This case could be handled a bit more sensible
                choiceDistribution.addProbability(choice - choiceIndices[state], scoreThreshold);
            } else {
                PomdpValueType choiceScore = info.minimize() ? (choiceValue - stateValue) : (stateValue - choiceValue);
                if (relativeScore) {
                    PomdpValueType avg = (stateValue + choiceValue) / storm::utility::convertNumber<PomdpValueType, uint64_t>(2);
                    if (!storm::utility::isZero(avg)) {
                        choiceScore /= avg;
                    }
                }
                choiceScore = storm::utility::one<PomdpValueType>() - choiceScore;
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

    auto resultPtr =
        storm::api::verifyWithSparseEngine<PomdpValueType>(env, scheduledModel, storm::api::createTask<PomdpValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<PomdpValueType> pomdpSchedulerResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<PomdpValueType>().getValueVector());
    return std::make_pair(pomdpSchedulerResult, pomdpScheduler);
}

template<typename PomdpType>
std::pair<std::vector<typename PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PomdpValueType>,
          storm::storage::Scheduler<typename PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PomdpValueType>>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::computeValuesForRandomFMPolicy(storm::Environment const& env, storm::logic::Formula const& formula,
                                                                                     storm::pomdp::analysis::FormulaInformation const& info,
                                                                                     uint64_t memoryBound) {
    // Consider memoryless policy on memory-unfolded POMDP
    storm::storage::Scheduler<PomdpValueType> pomdpScheduler(pomdp.getNumberOfStates() * memoryBound);

    STORM_LOG_DEBUG("Computing the unfolding for memory bound " << memoryBound);
    storm::storage::PomdpMemory memory = storm::storage::PomdpMemoryBuilder().build(storm::storage::PomdpMemoryPattern::Full, memoryBound);
    storm::transformer::PomdpMemoryUnfolder<PomdpValueType> memoryUnfolder(pomdp, memory);
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
    auto underlyingMdp = std::make_shared<storm::models::sparse::Mdp<PomdpValueType>>(memPomdp->getTransitionMatrix(), memPomdp->getStateLabeling(),
                                                                                      memPomdp->getRewardModels());
    auto scheduledModel = underlyingMdp->applyScheduler(pomdpScheduler, false);
    auto resultPtr =
        storm::api::verifyWithSparseEngine<PomdpValueType>(env, scheduledModel, storm::api::createTask<PomdpValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<PomdpValueType> pomdpSchedulerResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<PomdpValueType>().getValueVector());

    // Take the optimal value in ANY of the unfolded states for a POMDP state as the resulting state value
    std::vector<PomdpValueType> res(pomdp.getNumberOfStates(),
                                    info.minimize() ? storm::utility::infinity<PomdpValueType>() : -storm::utility::infinity<PomdpValueType>());
    for (uint64_t memPomdpState = 0; memPomdpState < pomdpSchedulerResult.size(); ++memPomdpState) {
        uint64_t modelState = memPomdpState / memoryBound;
        if ((info.minimize() && pomdpSchedulerResult[memPomdpState] < res[modelState]) ||
            (!info.minimize() && pomdpSchedulerResult[memPomdpState] > res[modelState])) {
            res[modelState] = pomdpSchedulerResult[memPomdpState];
        }
    }
    return std::make_pair(res, pomdpScheduler);
}

template<typename PomdpType>
[[maybe_unused]] std::pair<std::vector<typename PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PomdpValueType>,
                           storm::storage::Scheduler<typename PreprocessingPomdpValueBoundsModelChecker<PomdpType>::PomdpValueType>>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::computeValuesForRandomMemorylessPolicy(
    storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info,
    std::shared_ptr<storm::models::sparse::Mdp<PomdpValueType>> underlyingMdp) {
    storm::storage::Scheduler<PomdpValueType> pomdpScheduler(pomdp.getNumberOfStates());
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

    auto resultPtr =
        storm::api::verifyWithSparseEngine<PomdpValueType>(env, scheduledModel, storm::api::createTask<PomdpValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<PomdpValueType> pomdpSchedulerResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<PomdpValueType>().getValueVector());

    STORM_LOG_DEBUG("Initial Value for guessed Policy: " << pomdpSchedulerResult[pomdp.getInitialStates().getNextSetIndex(0)]);

    return std::make_pair(pomdpSchedulerResult, pomdpScheduler);
}

template<typename PomdpType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::ValueBounds PreprocessingPomdpValueBoundsModelChecker<PomdpType>::getValueBounds(
    storm::Environment const& env, storm::logic::Formula const& formula, storm::pomdp::analysis::FormulaInformation const& info) {
    STORM_LOG_THROW(info.isNonNestedReachabilityProbability() || info.isNonNestedExpectedRewardFormula(), storm::exceptions::NotSupportedException,
                    "The property type is not supported for this analysis.");

    // Compute the values on the fully observable MDP
    // We need an actual MDP so that we can apply schedulers below.
    // Also, the api call in the next line will require a copy anyway.
    auto underlyingMdp =
        std::make_shared<storm::models::sparse::Mdp<PomdpValueType>>(pomdp.getTransitionMatrix(), pomdp.getStateLabeling(), pomdp.getRewardModels());
    auto resultPtr =
        storm::api::verifyWithSparseEngine<PomdpValueType>(env, underlyingMdp, storm::api::createTask<PomdpValueType>(formula.asSharedPointer(), false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<PomdpValueType> fullyObservableResult = std::move(resultPtr->template asExplicitQuantitativeCheckResult<PomdpValueType>().getValueVector());

    std::vector<PomdpValueType> actionBasedRewards;
    std::vector<PomdpValueType>* actionBasedRewardsPtr = nullptr;
    if (info.isNonNestedExpectedRewardFormula()) {
        actionBasedRewards = pomdp.getRewardModel(info.getRewardModelName()).getTotalRewardVector(pomdp.getTransitionMatrix());
        actionBasedRewardsPtr = &actionBasedRewards;
    }
    std::vector<std::vector<PomdpValueType>> guessedSchedulerValues;
    std::vector<storm::storage::Scheduler<PomdpValueType>> guessedSchedulers;
    std::shared_ptr<std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>>> guessedSchedulerPair;
    std::vector<std::pair<double, bool>> guessParameters({{0.875, false}, {0.875, true}, {0.75, false}, {0.75, true}});
    for (auto const& pars : guessParameters) {
        guessedSchedulerPair = std::make_shared<std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>>>(
            computeValuesForGuessedScheduler(env, fullyObservableResult, actionBasedRewardsPtr, formula, info, underlyingMdp,
                                             storm::utility::convertNumber<PomdpValueType>(pars.first), pars.second));
        guessedSchedulerValues.push_back(guessedSchedulerPair->first);
        guessedSchedulers.push_back(guessedSchedulerPair->second);
    }

    // compute the 'best' guess and do a few iterations on it
    uint64_t bestGuess = 0;
    PomdpValueType bestGuessSum =
        std::accumulate(guessedSchedulerValues.front().begin(), guessedSchedulerValues.front().end(), storm::utility::zero<PomdpValueType>());
    for (uint64_t guess = 1; guess < guessedSchedulerValues.size(); ++guess) {
        PomdpValueType guessSum =
            std::accumulate(guessedSchedulerValues[guess].begin(), guessedSchedulerValues[guess].end(), storm::utility::zero<PomdpValueType>());
        if ((info.minimize() && guessSum < bestGuessSum) || (info.maximize() && guessSum > bestGuessSum)) {
            bestGuess = guess;
            bestGuessSum = guessSum;
        }
    }
    guessedSchedulerPair = std::make_shared<std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>>>(
        computeValuesForGuessedScheduler(env, guessedSchedulerValues[bestGuess], actionBasedRewardsPtr, formula, info, underlyingMdp,
                                         storm::utility::convertNumber<PomdpValueType>(guessParameters[bestGuess].first), guessParameters[bestGuess].second));
    guessedSchedulerValues.push_back(guessedSchedulerPair->first);
    guessedSchedulers.push_back(guessedSchedulerPair->second);
    guessedSchedulerPair = std::make_shared<std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>>>(
        computeValuesForGuessedScheduler(env, guessedSchedulerValues.back(), actionBasedRewardsPtr, formula, info, underlyingMdp,
                                         storm::utility::convertNumber<PomdpValueType>(guessParameters[bestGuess].first), guessParameters[bestGuess].second));
    guessedSchedulerValues.push_back(guessedSchedulerPair->first);
    guessedSchedulers.push_back(guessedSchedulerPair->second);
    guessedSchedulerPair = std::make_shared<std::pair<std::vector<PomdpValueType>, storm::storage::Scheduler<PomdpValueType>>>(
        computeValuesForGuessedScheduler(env, guessedSchedulerValues.back(), actionBasedRewardsPtr, formula, info, underlyingMdp,
                                         storm::utility::convertNumber<PomdpValueType>(guessParameters[bestGuess].first), guessParameters[bestGuess].second));
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
            if (storm::utility::vector::compareElementWise(guessedSchedulerValues[i], guessedSchedulerValues[j], std::less_equal<PomdpValueType>())) {
                if (info.minimize()) {
                    // In this case we are guessing upper bounds (and smaller upper bounds are better)
                    keptGuesses.set(j, false);
                } else {
                    // In this case we are guessing lower bounds (and larger lower bounds are better)
                    keptGuesses.set(i, false);
                    break;
                }
            } else if (storm::utility::vector::compareElementWise(guessedSchedulerValues[j], guessedSchedulerValues[i], std::less_equal<PomdpValueType>())) {
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
    std::vector<storm::storage::Scheduler<PomdpValueType>> filteredSchedulers;
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
#ifndef NDEBUG
    bool boundsValid = true;
    auto maxDifference = storm::utility::zero<PomdpValueType>();
    for (auto const& lower : result.lower) {
        for (auto const& upper : result.upper) {
            for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
                if (storm::utility::min<PomdpValueType>(upper.at(state), lower.at(state)) == upper.at(state) && upper.at(state) != lower.at(state)) {
                    boundsValid = false;
                    maxDifference = storm::utility::max<PomdpValueType>(maxDifference, lower.at(state) - upper.at(state));
                    STORM_LOG_TRACE("Lower bound " << lower.at(state) << " at state " << state << " is larger than upper bound " << upper.at(state));
                }
            }
        }
    }

    STORM_LOG_WARN_COND_DEBUG(boundsValid, "At least one lower bound is not smaller than an upper bound (max. difference: "
                                               << maxDifference
                                               << "). This might be due to floating point imprecisions. Enable TRACE output for more details.");
#endif

    return result;
}

template<typename PomdpType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::ExtremeValueBound PreprocessingPomdpValueBoundsModelChecker<PomdpType>::getExtremeValueBound(
    storm::logic::Formula const& formula) {
    storm::Environment env;
    return getExtremeValueBound(env, formula);
}

template<typename PomdpType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::ExtremeValueBound PreprocessingPomdpValueBoundsModelChecker<PomdpType>::getExtremeValueBound(
    storm::Environment const& env, storm::logic::Formula const& formula) {
    return getExtremeValueBound(env, formula, storm::pomdp::analysis::getFormulaInformation(pomdp, formula));
}

template<typename PomdpType>
PreprocessingPomdpValueBoundsModelChecker<PomdpType>::ExtremeValueBound PreprocessingPomdpValueBoundsModelChecker<PomdpType>::getExtremeValueBound(
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
        std::make_shared<storm::models::sparse::Mdp<PomdpValueType>>(pomdp.getTransitionMatrix(), pomdp.getStateLabeling(), pomdp.getRewardModels());
    auto resultPtr = storm::api::verifyWithSparseEngine<PomdpValueType>(env, underlyingMdp, storm::api::createTask<PomdpValueType>(formulaPtr, false));
    STORM_LOG_THROW(resultPtr, storm::exceptions::UnexpectedException, "No check result obtained.");
    STORM_LOG_THROW(resultPtr->isExplicitQuantitativeCheckResult(), storm::exceptions::UnexpectedException, "Unexpected Check result Type.");
    std::vector<PomdpValueType> resultVec = std::move(resultPtr->template asExplicitQuantitativeCheckResult<PomdpValueType>().getValueVector());
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

template class PreprocessingPomdpValueBoundsModelChecker<storm::models::sparse::Pomdp<double>>;

template class PreprocessingPomdpValueBoundsModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>>;
}  // namespace storm::pomdp::modelchecker