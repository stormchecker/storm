#include "storm-pomdp/transformer/ToStateBasedObservationTransformer.h"

#include <algorithm>
#include <map>
#include <optional>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidModelException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/builder.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::transformer {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> ToStateBasedObservationTransformer<ValueType>::transform(
    storm::models::sparse::Mdp<ValueType> const& mdp, TransitionObservationFunction const& transitionObservationFunction, ObservationType initialObservation) {
    auto const& transitionMatrix = mdp.getTransitionMatrix();

    // Create a vector that for each state contains the set of observations with which we may enter that state.
    std::vector<std::vector<ObservationType>> stateObservations(mdp.getNumberOfStates());
    // Start with the initial observations
    for (auto const initState : mdp.getInitialStates()) {
        stateObservations[initState].push_back(initialObservation);
    }
    // Now run over all transitions.
    // Also gather all transition observations so that we do not have to query them twice
    std::vector<ObservationType> transitionTargetObservations;
    transitionTargetObservations.reserve(transitionMatrix.getEntryCount());
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        for (auto choice : transitionMatrix.getRowGroupIndices(state)) {
            for (auto const& entry : transitionMatrix.getRow(choice)) {
                auto const obs = transitionObservationFunction(state, choice, entry.getColumn());
                transitionTargetObservations.push_back(obs);
                auto& obsSet = stateObservations[entry.getColumn()];
                if (std::find(obsSet.begin(), obsSet.end(), obs) == obsSet.end()) {
                    obsSet.push_back(obs);
                }
            }
        }
    }

    // Create state offsets. The entry for a given input model state is the id of the first copy of that state.
    std::vector<uint64_t> stateOffsets;
    stateOffsets.reserve(mdp.getNumberOfStates() + 1);
    stateOffsets.push_back(0);
    for (uint64_t offset = 0; auto const& obsSet : stateObservations) {
        STORM_LOG_THROW(!obsSet.empty(), storm::exceptions::InvalidModelException,
                        "There are states that are neither initial nor have an incoming transition.");
        offset += obsSet.size();
        stateOffsets.push_back(offset);
    }

    // Create the transition matrix of the resulting model.
    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(0, stateOffsets.back(), 0, true, true, stateOffsets.back());
    auto transTargetObsIt = transitionTargetObservations.begin();
    uint64_t rowInResultMatrix = 0;
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        auto const transTargetObsBegin = transTargetObsIt;
        for (auto _ [[maybe_unused]] : stateObservations[state]) {
            transTargetObsIt = transTargetObsBegin;
            matrixBuilder.newRowGroup(rowInResultMatrix);
            for (auto choice : transitionMatrix.getRowGroupIndices(state)) {
                for (auto const& entry : transitionMatrix.getRow(choice)) {
                    auto const targetObs = *transTargetObsIt;
                    auto const& targetObsSet = stateObservations[entry.getColumn()];
                    auto const findIt = std::find(targetObsSet.begin(), targetObsSet.end(), targetObs);
                    STORM_LOG_ASSERT(findIt != targetObsSet.end(), "Transition target observation not found in target state.");
                    auto transitionTarget = stateOffsets[entry.getColumn()] + std::distance(targetObsSet.begin(), findIt);
                    matrixBuilder.addNextValue(rowInResultMatrix, transitionTarget, entry.getValue());
                    ++transTargetObsIt;
                }
                ++rowInResultMatrix;
            }
        }
    }

    uint64_t const numResultStates = stateOffsets.back();
    uint64_t const numResultChoices = rowInResultMatrix;

    storm::storage::sparse::ModelComponents<ValueType> components(matrixBuilder.build(numResultChoices, numResultStates, numResultStates),
                                                                  storm::models::sparse::StateLabeling(numResultStates));

    // Helper functions to iterate over all states/choices. Unfolding states/choices are called in ascending order of their ids.
    auto forEachState = [&stateOffsets](auto const& f) {
        for (uint64_t ogState = 0; ogState < stateOffsets.size() - 1; ++ogState) {
            for (uint64_t resultState = stateOffsets[ogState]; resultState < stateOffsets[ogState + 1]; ++resultState) {
                f(resultState, ogState);
            }
        }
    };
    auto forEachChoice = [&stateOffsets, &transitionMatrix, &components](auto const& f) {
        for (uint64_t ogState = 0; ogState < stateOffsets.size() - 1; ++ogState) {
            for (uint64_t resultState = stateOffsets[ogState]; resultState < stateOffsets[ogState + 1]; ++resultState) {
                STORM_LOG_ASSERT(components.transitionMatrix.getRowGroupSize(resultState) == transitionMatrix.getRowGroupSize(ogState),
                                 "Number of choices in unfolding and original model differ.");
                auto ogChoice = transitionMatrix.getRowGroupIndices()[ogState];
                for (auto resultChoice : components.transitionMatrix.getRowGroupIndices(resultState)) {
                    f(resultChoice, ogChoice);
                    ++ogChoice;
                }
            }
        }
    };

    // Create the state labeling
    storm::storage::BitVector initialStates(numResultStates, false);
    for (auto ogInitState : mdp.getInitialStates()) {
        auto const& obsSet = stateObservations[ogInitState];
        auto const findIt = std::find(obsSet.begin(), obsSet.end(), initialObservation);
        STORM_LOG_ASSERT(findIt != obsSet.end(), "Initial observation not found in initial state.");
        auto const resultInitState = stateOffsets[ogInitState] + std::distance(obsSet.begin(), findIt);
        initialStates.set(resultInitState);
    }
    components.stateLabeling.addLabel("init", std::move(initialStates));
    for (auto label : mdp.getStateLabeling().getLabels()) {
        if (label == "init") {
            continue;
        }
        auto const& originalLabel = mdp.getStateLabeling().getStates(label);
        storm::storage::BitVector newLabel(numResultStates, false);
        forEachState([&newLabel, &originalLabel](uint64_t resultState, StateIdType ogState) {
            if (originalLabel.get(ogState)) {
                newLabel.set(resultState);
            }
        });
        components.stateLabeling.addLabel(label, std::move(newLabel));
    }

    // Create the (optional) choice labeling
    if (mdp.hasChoiceLabeling()) {
        components.choiceLabeling.emplace(numResultChoices);
        for (auto label : mdp.getChoiceLabeling().getLabels()) {
            auto const& originalLabel = mdp.getChoiceLabeling().getChoices(label);
            storm::storage::BitVector newLabel(numResultChoices, false);
            forEachChoice([&newLabel, &originalLabel](uint64_t unfoldingChoice, uint64_t originalChoice) {
                if (originalLabel.get(originalChoice)) {
                    newLabel.set(unfoldingChoice);
                }
            });
            components.choiceLabeling->addLabel(label, std::move(newLabel));
        }
    }

    // Create the reward models
    for (auto const& [name, rewmodel] : mdp.getRewardModels()) {
        STORM_LOG_THROW(!rewmodel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                        "Transition rewards are currently not supported in this context.");
        std::optional<std::vector<ValueType>> stateRewards, stateActionRewards;
        if (rewmodel.hasStateRewards()) {
            stateRewards.emplace();
            stateRewards->reserve(numResultStates);
            forEachState(
                [&stateRewards, &rewmodel](uint64_t _, StateIdType originalState) { stateRewards->push_back(rewmodel.getStateReward(originalState)); });
        }
        if (rewmodel.hasStateActionRewards()) {
            stateActionRewards.emplace();
            stateActionRewards->reserve(numResultChoices);
            forEachChoice([&stateActionRewards, &rewmodel](auto _, StateIdType originalChoice) {
                stateActionRewards->push_back(rewmodel.getStateActionReward(originalChoice));
            });
        }
        components.rewardModels.emplace(name, storm::models::sparse::StandardRewardModel<ValueType>{std::move(stateRewards), std::move(stateActionRewards)});
    }

    // Create POMDP-specific components
    std::vector<ObservationType> flatStateObservations;
    flatStateObservations.reserve(numResultStates);
    for (auto const& obsSet : stateObservations) {
        flatStateObservations.insert(flatStateObservations.end(), obsSet.begin(), obsSet.end());
    }
    components.observabilityClasses = std::move(flatStateObservations);

    return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::Pomdp, std::move(components))
        ->template as<storm::models::sparse::Pomdp<ValueType>>();
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> ToStateBasedObservationTransformer<ValueType>::transformRewardAware(
    storm::models::sparse::Pomdp<ValueType> const& pomdp, std::set<std::string> const& observableRewardModels) {
    STORM_LOG_THROW(pomdp.getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "The model must have exactly one initial state.");
    auto const initialObservation = pomdp.getObservation(pomdp.getInitialStates().getNextSetIndex(0));

    struct TransitionObservation {
        ObservationType targetStateObservation;
        std::vector<ValueType> rewards;
        bool operator<(TransitionObservation const& other) const {
            if (targetStateObservation != other.targetStateObservation) {
                return targetStateObservation < other.targetStateObservation;
            }
            return rewards < other.rewards;
        }
    };
    std::map<TransitionObservation, ObservationType> observationIndexStorage;
    // Generated observations must not collide with the original observations retained by initial-state copies.
    ObservationType freshObservation = pomdp.getNrObservations();
    auto getOrAddObservationIndex = [&observationIndexStorage, &freshObservation](TransitionObservation const& obs) {
        auto [it, inserted] = observationIndexStorage.try_emplace(obs, freshObservation);
        if (inserted) {
            ++freshObservation;
        }
        return it->second;
    };
    auto result = transform(
        pomdp,
        [&pomdp, &getOrAddObservationIndex, &observableRewardModels](StateIdType srcState, ActionIdType action, StateIdType targetState) {
            TransitionObservation obs{pomdp.getObservation(targetState), {}};
            for (auto const& rewName : observableRewardModels) {
                auto const& rewModel = pomdp.getRewardModel(rewName);
                obs.rewards.push_back(storm::utility::zero<ValueType>());
                if (rewModel.hasStateRewards()) {
                    obs.rewards.back() += rewModel.getStateReward(srcState);
                }
                if (rewModel.hasStateActionRewards()) {
                    obs.rewards.back() += rewModel.getStateActionReward(action);
                }
                STORM_LOG_THROW(!rewModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                                "Transition rewards are currently not supported in this context.");
            }
            return getOrAddObservationIndex(obs);
        },
        initialObservation);
    result->setIsCanonic(pomdp.isCanonic());
    return result;
}

template class ToStateBasedObservationTransformer<double>;
template class ToStateBasedObservationTransformer<storm::RationalNumber>;
}  // namespace storm::pomdp::transformer
