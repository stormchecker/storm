#pragma once

#include <cstdint>
#include <set>
#include <vector>

#include "storm-pars/modelchecker/region/RegionCheckEngine.h"
#include "storm-pars/utility/parametric.h"
#include "storm/logic/Formula.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/logic/RewardOperatorFormula.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace utility {
namespace parameterlifting {

/*!
 * Checks whether the parameter lifting approach is sound on the given model with respect to the provided property
 *
 * This method is taylored to an efficient but incomplete check, i.e., if false is returned,
 * parameter lifting might still be applicable.
 *
 * More precisely, we only check if the occurring functions are multilinear polynomials.
 * However, Parameter Lifting also works for fractions of multilinear polynomials where for every state s,
 * every probability and reward occurring at s have the same denominator.
 *
 * @param model
 * @param formula
 * @return true iff it was successfully validated that parameter lifting is sound on the provided model.
 */
template<typename ValueType>
static bool validateParameterLiftingSound(storm::models::sparse::Model<ValueType> const& model, storm::logic::Formula const& formula,
                                          storm::modelchecker::RegionCheckEngine engine) {
    // Check whether all numbers occurring in the model are multilinear

    // Transition matrix
    if (model.isOfType(storm::models::ModelType::Dtmc) || model.isOfType(storm::models::ModelType::Mdp) || model.isOfType(storm::models::ModelType::Ctmc)) {
        if (engine == modelchecker::RegionCheckEngine::RobustParameterLifting) {
            if (!model.isOfType(storm::models::ModelType::Dtmc)) {
                STORM_LOG_ERROR("Robust parameter lifting is only supported for DTMCs.");
                return false;
            }
        } else {
            for (auto const& entry : model.getTransitionMatrix()) {
                if (!storm::utility::parametric::isMultiLinearPolynomial(entry.getValue())) {
                    STORM_LOG_WARN("The input model contains a non-linear polynomial as transition: '"
                                   << entry.getValue() << "'. Can not validate that parameter lifting is sound on this model.");
                    return false;
                }
            }
        }
    } else if (model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        auto const& ma = dynamic_cast<storm::models::sparse::MarkovAutomaton<ValueType> const&>(model);
        // Markov Automata store the probability matrix and the exit rate vector. However, we need to considert the rate matrix.
        if (!ma.isClosed()) {
            STORM_LOG_ERROR("parameter lifting requires a closed Markov automaton.");
            return false;
        }
        auto const& rateVector = ma.getExitRates();
        auto const& markovianStates = ma.getMarkovianStates();
        for (uint_fast64_t state = 0; state < model.getNumberOfStates(); ++state) {
            if (markovianStates.get(state)) {
                auto const& exitRate = rateVector[state];
                for (auto const& entry : model.getTransitionMatrix().getRowGroup(state)) {
                    if (!storm::utility::parametric::isMultiLinearPolynomial(storm::utility::simplify(entry.getValue() * exitRate))) {
                        STORM_LOG_WARN("The input model contains a non-linear polynomial as transition rate: '"
                                       << storm::utility::simplify(entry.getValue() * exitRate)
                                       << "'. Can not validate that parameter lifting is sound on this model.");
                        return false;
                    }
                }
            } else {
                for (auto const& entry : model.getTransitionMatrix().getRowGroup(state)) {
                    if (!storm::utility::parametric::isMultiLinearPolynomial(entry.getValue())) {
                        STORM_LOG_WARN("The input model contains a non-linear polynomial as transition: '"
                                       << entry.getValue() << "'. Can not validate that parameter lifting is sound on this model.");
                        return false;
                    }
                }
            }
        }
    } else {
        STORM_LOG_ERROR("Unsupported model type for parameter lifting.");
        return false;
    }

    // Rewards
    if (formula.isRewardOperatorFormula()) {
        storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel =
            formula.asRewardOperatorFormula().hasRewardModelName() ? model.getRewardModel(formula.asRewardOperatorFormula().getRewardModelName())
                                                                   : model.getUniqueRewardModel();
        if (rewardModel.hasStateRewards()) {
            for (auto const& rew : rewardModel.getStateRewardVector()) {
                if (!storm::utility::parametric::isMultiLinearPolynomial(rew)) {
                    STORM_LOG_WARN("The input model contains a non-linear polynomial as state reward: '"
                                   << rew << "'. Can not validate that parameter lifting is sound on this model.");
                    return false;
                }
            }
        }
        // Parameters in transition rewards (and for continuous time models also action rewards) have to be disjoint from the probability/rate parameters.
        // Note: This check could also be done action-wise.
        std::set<typename storm::utility::parametric::VariableType<ValueType>::type> collectedRewardParameters;
        if (rewardModel.hasStateActionRewards()) {
            if (model.isOfType(storm::models::ModelType::Ctmc) || model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
                for (auto const& rew : rewardModel.getStateActionRewardVector()) {
                    if (!storm::utility::parametric::isMultiLinearPolynomial(rew)) {
                        STORM_LOG_WARN("The input model contains a non-linear polynomial as action reward: '"
                                       << rew << "'. Can not validate that parameter lifting is sound on this model.");
                        return false;
                    }
                    storm::utility::parametric::gatherOccurringVariables(rew, collectedRewardParameters);
                }
            } else {
                for (auto const& rew : rewardModel.getStateActionRewardVector()) {
                    if (!storm::utility::parametric::isMultiLinearPolynomial(rew)) {
                        STORM_LOG_WARN("The input model contains a non-linear polynomial as action reward: '"
                                       << rew << "'. Can not validate that parameter lifting is sound on this model.");
                        return false;
                    }
                }
            }
        }

        if (rewardModel.hasTransitionRewards()) {
            for (auto const& rewEntry : rewardModel.getTransitionRewardMatrix()) {
                if (!storm::utility::parametric::isMultiLinearPolynomial(rewEntry.getValue())) {
                    STORM_LOG_WARN("The input model contains a non-linear polynomial as transition reward: '"
                                   << rewEntry.getValue() << "'. Can not validate that parameter lifting is sound on this model.");
                    return false;
                }
                storm::utility::parametric::gatherOccurringVariables(rewEntry.getValue(), collectedRewardParameters);
            }
        }

        if (!collectedRewardParameters.empty()) {
            std::set<typename storm::utility::parametric::VariableType<ValueType>::type> transitionParameters =
                storm::models::sparse::getProbabilityParameters(model);
            auto rewParIt = collectedRewardParameters.begin();
            auto trParIt = transitionParameters.begin();
            while (rewParIt != collectedRewardParameters.end() && trParIt != transitionParameters.end()) {
                if (*rewParIt == *trParIt) {
                    STORM_LOG_WARN(
                        "Parameter "
                        << *trParIt
                        << " occurs in a transition probability/rate and in a transition/action reward. Parameter lifting might not be sound on this model.");
                    return false;
                }
                if (*rewParIt < *trParIt) {
                    ++rewParIt;
                } else {
                    ++trParIt;
                }
            }
        }
    }
    return true;
}

}  // namespace parameterlifting
}  // namespace utility
}  // namespace storm
