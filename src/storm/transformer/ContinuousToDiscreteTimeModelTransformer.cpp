#include "storm/transformer/ContinuousToDiscreteTimeModelTransformer.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/logic/ExpectedTimeToExpectedRewardVisitor.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Dtmc<ValueType, RewardModelType>> ContinuousToDiscreteTimeModelTransformer<ValueType, RewardModelType>::transform(
    storm::models::sparse::Ctmc<ValueType, RewardModelType> const& ctmc, boost::optional<std::string> const& timeRewardModelName) {
    // Init the dtmc components
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> dtmcComponents(ctmc.getTransitionMatrix(), ctmc.getStateLabeling(),
                                                                                       ctmc.getRewardModels());
    dtmcComponents.choiceLabeling = ctmc.getOptionalChoiceLabeling();
    dtmcComponents.stateValuations = ctmc.getOptionalStateValuations();
    dtmcComponents.choiceOrigins = ctmc.getOptionalChoiceOrigins();

    // Turn the rates into probabilities by dividing each row of the transition matrix with the exit rate
    std::vector<ValueType> const& exitRates = ctmc.getExitRateVector();
    dtmcComponents.transitionMatrix.divideRowsInPlace(exitRates);

    // Transform the reward models
    for (auto& rewardModel : dtmcComponents.rewardModels) {
        if (rewardModel.second.hasStateRewards()) {
            storm::utility::vector::divideVectorsPointwise(rewardModel.second.getStateRewardVector(), exitRates, rewardModel.second.getStateRewardVector());
        }
    }

    if (timeRewardModelName) {
        // Invert the exit rate vector in place
        std::vector<ValueType> timeRewardVector;
        timeRewardVector.reserve(exitRates.size());
        for (auto const& r : exitRates) {
            timeRewardVector.push_back(storm::utility::one<ValueType>() / r);
        }
        RewardModelType timeRewards(std::move(timeRewardVector));
        auto insertRes = dtmcComponents.rewardModels.insert(std::make_pair(*timeRewardModelName, std::move(timeRewards)));
        STORM_LOG_THROW(insertRes.second, storm::exceptions::InvalidArgumentException,
                        "Could not insert auxiliary reward model " << *timeRewardModelName << " because a model with this name already exists.");
    }

    return std::make_shared<storm::models::sparse::Dtmc<ValueType, RewardModelType>>(std::move(dtmcComponents));
}

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Dtmc<ValueType, RewardModelType>> ContinuousToDiscreteTimeModelTransformer<ValueType, RewardModelType>::transform(
    storm::models::sparse::Ctmc<ValueType, RewardModelType>&& ctmc, boost::optional<std::string> const& timeRewardModelName) {
    // Init the dtmc components
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> dtmcComponents(std::move(ctmc.getTransitionMatrix()),
                                                                                       std::move(ctmc.getStateLabeling()), std::move(ctmc.getRewardModels()));
    dtmcComponents.choiceLabeling = std::move(ctmc.getOptionalChoiceLabeling());
    dtmcComponents.stateValuations = std::move(ctmc.getOptionalStateValuations());
    dtmcComponents.choiceOrigins = std::move(ctmc.getOptionalChoiceOrigins());

    // Turn the rates into probabilities by dividing each row of the transition matrix with the exit rate
    std::vector<ValueType>& exitRates = ctmc.getExitRateVector();
    dtmcComponents.transitionMatrix.divideRowsInPlace(exitRates);

    // Transform the reward models
    for (auto& rewardModel : dtmcComponents.rewardModels) {
        if (rewardModel.second.hasStateRewards()) {
            storm::utility::vector::divideVectorsPointwise(rewardModel.second.getStateRewardVector(), exitRates, rewardModel.second.getStateRewardVector());
        }
    }

    if (timeRewardModelName) {
        // Invert the exit rate vector in place
        storm::utility::vector::applyPointwise<ValueType, ValueType>(exitRates, exitRates,
                                                                     [&](ValueType const& r) -> ValueType { return storm::utility::one<ValueType>() / r; });
        RewardModelType timeRewards(std::move(exitRates));
        auto insertRes = dtmcComponents.rewardModels.insert(std::make_pair(*timeRewardModelName, std::move(timeRewards)));
        STORM_LOG_THROW(insertRes.second, storm::exceptions::InvalidArgumentException,
                        "Could not insert auxiliary reward model " << *timeRewardModelName << " because a model with this name already exists.");
    }
    // Note: exitRates might be invalidated at this point.

    return std::make_shared<storm::models::sparse::Dtmc<ValueType, RewardModelType>>(std::move(dtmcComponents));
}

template<typename ValueType, typename RewardModelType>
bool ContinuousToDiscreteTimeModelTransformer<ValueType, RewardModelType>::preservesFormula(storm::logic::Formula const& formula) {
    storm::logic::FragmentSpecification fragment = storm::logic::propositional();
    fragment.setProbabilityOperatorsAllowed(true);
    fragment.setGloballyFormulasAllowed(true);
    fragment.setReachabilityProbabilityFormulasAllowed(true);
    fragment.setNextFormulasAllowed(true);
    fragment.setUntilFormulasAllowed(true);
    fragment.setRewardOperatorsAllowed(true);
    fragment.setReachabilityRewardFormulasAllowed(true);
    fragment.setTotalRewardFormulasAllowed(true);
    fragment.setMultiObjectiveFormulasAllowed(true);
    return formula.isInFragment(fragment);
}

template<typename ValueType, typename RewardModelType>
std::vector<std::shared_ptr<storm::logic::Formula const>> ContinuousToDiscreteTimeModelTransformer<ValueType, RewardModelType>::checkAndTransformFormulas(
    std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas, std::string const& timeRewardName) {
    std::vector<std::shared_ptr<storm::logic::Formula const>> result;
    storm::logic::ExpectedTimeToExpectedRewardVisitor v(timeRewardName);
    for (auto const& f : formulas) {
        // Translate expected time formulas
        auto newF = v.substitute(*f);
        if (preservesFormula(*newF)) {
            result.push_back(newF);
        } else {
            STORM_LOG_INFO("Continuous to discrete time transformation does not preserve formula " << *f);
        }
    }
    return result;
}

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Mdp<ValueType, RewardModelType>> ContinuousToDiscreteTimeModelTransformer<ValueType, RewardModelType>::transform(
    storm::models::sparse::MarkovAutomaton<ValueType, RewardModelType> const& ma, boost::optional<std::string> const& timeRewardModelName) {
    STORM_LOG_THROW(ma.isClosed(), storm::exceptions::InvalidArgumentException, "Transformation of MA to its underlying MDP is only possible for closed MAs.");

    // Init the mdp components
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> mdpComponents(ma.getTransitionMatrix(), ma.getStateLabeling(), ma.getRewardModels());
    mdpComponents.choiceLabeling = ma.getOptionalChoiceLabeling();
    mdpComponents.stateValuations = ma.getOptionalStateValuations();
    mdpComponents.choiceOrigins = ma.getOptionalChoiceOrigins();

    // Markov automata already store the probability matrix

    // Transform the reward models
    std::vector<ValueType> const& exitRates = ma.getExitRates();
    for (auto& rewardModel : mdpComponents.rewardModels) {
        if (rewardModel.second.hasStateRewards()) {
            auto& stateRewards = rewardModel.second.getStateRewardVector();
            for (uint_fast64_t state = 0; state < stateRewards.size(); ++state) {
                if (ma.getMarkovianStates().get(state)) {
                    stateRewards[state] /= exitRates[state];
                } else {
                    stateRewards[state] = storm::utility::zero<ValueType>();
                }
            }
        }
    }

    if (timeRewardModelName) {
        // Invert the exit rate vector. Avoid division by zero at probabilistic states
        std::vector<ValueType> timeRewardVector(exitRates.size(), storm::utility::zero<ValueType>());
        for (auto state : ma.getMarkovianStates()) {
            timeRewardVector[state] = storm::utility::one<ValueType>() / exitRates[state];
        }
        RewardModelType timeRewards(std::move(timeRewardVector));
        auto insertRes = mdpComponents.rewardModels.insert(std::make_pair(*timeRewardModelName, std::move(timeRewards)));
        STORM_LOG_THROW(insertRes.second, storm::exceptions::InvalidArgumentException,
                        "Could not insert auxiliary reward model " << *timeRewardModelName << " because a model with this name already exists.");
    }

    return std::make_shared<storm::models::sparse::Mdp<ValueType, RewardModelType>>(std::move(mdpComponents));
}

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Mdp<ValueType, RewardModelType>> ContinuousToDiscreteTimeModelTransformer<ValueType, RewardModelType>::transform(
    storm::models::sparse::MarkovAutomaton<ValueType, RewardModelType>&& ma, boost::optional<std::string> const& timeRewardModelName) {
    STORM_LOG_THROW(ma.isClosed(), storm::exceptions::InvalidArgumentException, "Transformation of MA to its underlying MDP is only possible for closed MAs.");
    std::vector<ValueType>& exitRates = ma.getExitRates();

    // Init the mdp components
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> mdpComponents(std::move(ma.getTransitionMatrix()), std::move(ma.getStateLabeling()),
                                                                                      std::move(ma.getRewardModels()));
    mdpComponents.choiceLabeling = std::move(ma.getOptionalChoiceLabeling());
    mdpComponents.stateValuations = std::move(ma.getOptionalStateValuations());
    mdpComponents.choiceOrigins = std::move(ma.getOptionalChoiceOrigins());

    // Markov automata already store the probability matrix

    // Transform the reward models
    for (auto& rewardModel : mdpComponents.rewardModels) {
        if (rewardModel.second.hasStateRewards()) {
            auto& stateRewards = rewardModel.second.getStateRewardVector();
            for (uint_fast64_t state = 0; state < stateRewards.size(); ++state) {
                if (ma.getMarkovianStates().get(state)) {
                    stateRewards[state] /= exitRates[state];
                } else {
                    stateRewards[state] = storm::utility::zero<ValueType>();
                }
            }
        }
    }

    if (timeRewardModelName) {
        // Invert the exit rate vector. Avoid division by zero at probabilistic states
        std::vector<ValueType> timeRewardVector(exitRates.size(), storm::utility::zero<ValueType>());
        for (auto state : ma.getMarkovianStates()) {
            timeRewardVector[state] = storm::utility::one<ValueType>() / exitRates[state];
        }
        RewardModelType timeRewards(std::move(timeRewardVector));
        auto insertRes = mdpComponents.rewardModels.insert(std::make_pair(*timeRewardModelName, std::move(timeRewards)));
        STORM_LOG_THROW(insertRes.second, storm::exceptions::InvalidArgumentException,
                        "Could not insert auxiliary reward model " << *timeRewardModelName << " because a model with this name already exists.");
    }

    return std::make_shared<storm::models::sparse::Mdp<ValueType, RewardModelType>>(std::move(mdpComponents));
}

template class ContinuousToDiscreteTimeModelTransformer<double>;
template class ContinuousToDiscreteTimeModelTransformer<storm::RationalNumber>;
template class ContinuousToDiscreteTimeModelTransformer<storm::RationalFunction>;
}  // namespace transformer
}  // namespace storm
