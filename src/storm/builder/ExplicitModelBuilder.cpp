#include "storm/builder/ExplicitModelBuilder.h"

#include <map>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/builder/RewardModelBuilder.h"
#include "storm/builder/StateAndChoiceInformationBuilder.h"
#include "storm/exceptions/AbortException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/generator/JaniNextStateGenerator.h"
#include "storm/generator/PrismNextStateGenerator.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Model.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/builder.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace builder {

template<typename StateType>
StateType ExplicitStateLookup<StateType>::lookup(std::map<storm::expressions::Variable, storm::expressions::Expression> const& stateDescription) const {
    auto cs = storm::generator::createCompressedState(this->varInfo, stateDescription, true);
    // TODO search once
    if (!stateToId.contains(cs)) {
        return static_cast<StateType>(this->size());
    }
    return this->stateToId.getValue(cs);
}

template<typename StateType>
uint64_t ExplicitStateLookup<StateType>::size() const {
    return this->stateToId.size();
}

template<typename ValueType, typename RewardModelType, typename StateType>
ExplicitModelBuilder<ValueType, RewardModelType, StateType>::Options::Options() {
    auto const& buildSettings = storm::settings::getModule<storm::settings::modules::BuildSettings>();
    explorationOrder = buildSettings.getExplorationOrder();
    fixDeadlocks = !buildSettings.isDontFixDeadlocksSet();
    if (buildSettings.isExplorationStateLimitSet()) {
        explorationStateLimit = buildSettings.getExplorationStateLimit();
    }
}

template<typename ValueType, typename RewardModelType, typename StateType>
ExplicitModelBuilder<ValueType, RewardModelType, StateType>::ExplicitModelBuilder(
    std::shared_ptr<storm::generator::NextStateGenerator<ValueType, StateType>> const& generator, Options const& options)
    : generator(generator), options(options), stateStorage(generator->getStateSize()) {
    // Intentionally left empty.
}

template<typename ValueType, typename RewardModelType, typename StateType>
ExplicitModelBuilder<ValueType, RewardModelType, StateType>::ExplicitModelBuilder(storm::prism::Program const& program,
                                                                                  storm::generator::NextStateGeneratorOptions const& generatorOptions,
                                                                                  Options const& builderOptions)
    : ExplicitModelBuilder(std::make_shared<storm::generator::PrismNextStateGenerator<ValueType, StateType>>(program, generatorOptions), builderOptions) {
    // Intentionally left empty.
}

template<typename ValueType, typename RewardModelType, typename StateType>
ExplicitModelBuilder<ValueType, RewardModelType, StateType>::ExplicitModelBuilder(storm::jani::Model const& model,
                                                                                  storm::generator::NextStateGeneratorOptions const& generatorOptions,
                                                                                  Options const& builderOptions)
    requires(!storm::IsIntervalType<ValueType>)
    : ExplicitModelBuilder(std::make_shared<storm::generator::JaniNextStateGenerator<ValueType, StateType>>(model, generatorOptions), builderOptions) {
    // Intentionally left empty.
}

template<typename ValueType, typename RewardModelType, typename StateType>
std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> ExplicitModelBuilder<ValueType, RewardModelType, StateType>::build() {
    STORM_LOG_DEBUG("Exploration order is: " << options.explorationOrder);

    switch (generator->getModelType()) {
        case storm::generator::ModelType::DTMC:
            return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::Dtmc, buildModelComponents());
        case storm::generator::ModelType::CTMC:
            return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::Ctmc, buildModelComponents());
        case storm::generator::ModelType::MDP:
            return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::Mdp, buildModelComponents());
        case storm::generator::ModelType::POMDP:
            return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::Pomdp, buildModelComponents());
        case storm::generator::ModelType::MA:
            return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::MarkovAutomaton, buildModelComponents());
        case storm::generator::ModelType::SMG:
            return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::Smg, buildModelComponents());
        default:
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Error while creating model: cannot handle this model type.");
    }

    return nullptr;
}

template<typename ValueType, typename RewardModelType, typename StateType>
StateType ExplicitModelBuilder<ValueType, RewardModelType, StateType>::getOrAddStateIndex(CompressedState const& state) {
    StateType newIndex = static_cast<StateType>(stateStorage.getNumberOfStates());

    // Check, if the state was already registered.
    std::pair<StateType, std::size_t> actualIndexBucketPair = stateStorage.stateToId.findOrAddAndGetBucket(state, newIndex);

    StateType actualIndex = actualIndexBucketPair.first;

    if (actualIndex == newIndex) {
        if (options.explorationOrder == ExplorationOrder::Dfs) {
            statesToExplore.emplace_front(state, actualIndex);

            // Reserve one slot for the new state in the remapping.
            stateRemapping.get().push_back(storm::utility::zero<StateType>());
        } else if (options.explorationOrder == ExplorationOrder::Bfs) {
            statesToExplore.emplace_back(state, actualIndex);
        } else {
            STORM_LOG_ASSERT(false, "Invalid exploration order.");
        }
    }

    return actualIndex;
}

template<typename ValueType, typename RewardModelType, typename StateType>
ExplicitStateLookup<StateType> ExplicitModelBuilder<ValueType, RewardModelType, StateType>::exportExplicitStateLookup() const {
    return ExplicitStateLookup<StateType>(this->generator->getVariableInformation(), this->stateStorage.stateToId);
}

template<typename ValueType, typename RewardModelType, typename StateType>
void ExplicitModelBuilder<ValueType, RewardModelType, StateType>::buildMatrices(
    storm::storage::SparseMatrixBuilder<ValueType>& transitionMatrixBuilder,
    std::vector<RewardModelBuilder<typename RewardModelType::ValueType>>& rewardModelBuilders,
    StateAndChoiceInformationBuilder& stateAndChoiceInformationBuilder) {
    // Initialize building state valuations (if necessary)
    if (stateAndChoiceInformationBuilder.isBuildStateValuations()) {
        stateAndChoiceInformationBuilder.stateValuationsBuilder() = generator->initializeStateValuationsBuilder();
    }

    // Create a callback for the next-state generator to enable it to request the index of states.
    std::function<StateType(CompressedState const&)> stateToIdCallback =
        std::bind(&ExplicitModelBuilder<ValueType, RewardModelType, StateType>::getOrAddStateIndex, this, std::placeholders::_1);

    // If the exploration order is something different from breadth-first, we need to keep track of the remapping
    // from state ids to row groups. For this, we actually store the reversed mapping of row groups to state-ids
    // and later reverse it.
    if (options.explorationOrder != ExplorationOrder::Bfs) {
        stateRemapping = std::vector<uint_fast64_t>();
    }

    // Let the generator create all initial states.
    this->stateStorage.initialStateIndices = generator->getInitialStates(stateToIdCallback);
    STORM_LOG_THROW(!this->stateStorage.initialStateIndices.empty(), storm::exceptions::WrongFormatException,
                    "The model does not have a single initial state.");

    // Now explore the current state until there is no more reachable state.
    uint_fast64_t currentRowGroup = 0;
    uint_fast64_t currentRow = 0;

    auto timeOfStart = std::chrono::high_resolution_clock::now();
    auto timeOfLastMessage = std::chrono::high_resolution_clock::now();
    uint64_t numberOfExploredStates = 0;
    uint64_t numberOfExploredStatesSinceLastMessage = 0;

    // Perform a search through the model.
    while (!statesToExplore.empty()) {
        // Get the first state in the queue.
        CompressedState currentState = statesToExplore.front().first;
        StateType currentIndex = statesToExplore.front().second;
        statesToExplore.pop_front();

        // If the exploration order differs from breadth-first, we remember that this row group was actually
        // filled with the transitions of a different state.
        if (options.explorationOrder != ExplorationOrder::Bfs) {
            stateRemapping.get()[currentIndex] = currentRowGroup;
        }

        if (currentIndex % 100000 == 0) {
            STORM_LOG_TRACE("Exploring state with id " << currentIndex << ".");
        }

        generator->load(currentState);
        if (stateAndChoiceInformationBuilder.isBuildStateValuations()) {
            generator->addStateValuation(currentIndex, stateAndChoiceInformationBuilder.stateValuationsBuilder());
        }

        storm::generator::StateBehavior<ValueType, StateType> behavior;
        // If the exploration state limit is set and the limit is reached, we stop the exploration.
        bool const stateLimitExceeded = options.explorationStateLimit.has_value() && stateStorage.getNumberOfStates() >= options.explorationStateLimit.value();
        if (!stateLimitExceeded) {
            behavior = generator->expand(stateToIdCallback);
        }

        if (behavior.empty()) {
            // There are three possible cases for missing behavior:
            if (behavior.wasExpanded()) {
                // (a) The state is a deadlock state, i.e. there is no behavior even though the state was expanded
                STORM_LOG_THROW(options.fixDeadlocks, storm::exceptions::WrongFormatException,
                                "Error while creating sparse matrix from probabilistic program: found deadlock state ("
                                    << generator->stateToString(currentState) << "). For fixing these, please provide the appropriate option.");
                this->stateStorage.deadlockStateIndices.push_back(currentIndex);
            } else {
                if (stateLimitExceeded) {
                    // (b) The state was not expanded because the state limit is reached
                    this->stateStorage.unexploredStateIndices.push_back(currentIndex);
                }
                // (c) the state was not expanded because it is terminal, i.e., exploration from that state is not required for the given property/ies
            }

            // In all cases, we need to add a self-loop to the transition matrix.

            if (!generator->isDeterministicModel()) {
                transitionMatrixBuilder.newRowGroup(currentRow);
            }

            transitionMatrixBuilder.addNextValue(currentRow, currentIndex, storm::utility::one<ValueType>());

            for (auto& rewardModelBuilder : rewardModelBuilders) {
                if (rewardModelBuilder.hasStateRewards()) {
                    rewardModelBuilder.addStateReward(storm::utility::zero<ValueType>());
                }

                if (rewardModelBuilder.hasStateActionRewards()) {
                    rewardModelBuilder.addStateActionReward(storm::utility::zero<ValueType>());
                }
            }

            // This state shall be Markovian (to not introduce Zeno behavior)
            if (stateAndChoiceInformationBuilder.isBuildMarkovianStates()) {
                stateAndChoiceInformationBuilder.addMarkovianState(currentRowGroup);
            }
            // Other state-based information does not need to be treated, in particular:
            // * StateValuations have already been set above
            // * The associated player shall be the "default" player, i.e. INVALID_PLAYER_INDEX

            ++currentRow;
            ++currentRowGroup;
        } else {
            // Add the state rewards to the corresponding reward models.
            auto stateRewardIt = behavior.getStateRewards().begin();
            for (auto& rewardModelBuilder : rewardModelBuilders) {
                if (rewardModelBuilder.hasStateRewards()) {
                    rewardModelBuilder.addStateReward(*stateRewardIt);
                }
                ++stateRewardIt;
            }

            // If the model is nondeterministic, we need to open a row group.
            if (!generator->isDeterministicModel()) {
                transitionMatrixBuilder.newRowGroup(currentRow);
            }

            // Now add all choices.
            bool firstChoiceOfState = true;
            for (auto const& choice : behavior) {
                // add the generated choice information
                if (stateAndChoiceInformationBuilder.isBuildChoiceLabels() && choice.hasLabels()) {
                    for (auto const& label : choice.getLabels()) {
                        stateAndChoiceInformationBuilder.addChoiceLabel(label, currentRow);
                    }
                }
                if (stateAndChoiceInformationBuilder.isBuildChoiceOrigins() && choice.hasOriginData()) {
                    stateAndChoiceInformationBuilder.addChoiceOriginData(choice.getOriginData(), currentRow);
                }
                if (stateAndChoiceInformationBuilder.isBuildStatePlayerIndications() && choice.hasPlayerIndex()) {
                    STORM_LOG_ASSERT(
                        firstChoiceOfState || stateAndChoiceInformationBuilder.hasStatePlayerIndicationBeenSet(choice.getPlayerIndex(), currentRowGroup),
                        "There is a state where different players have an enabled choice.");  // Should have been detected in generator, already
                    if (firstChoiceOfState) {
                        stateAndChoiceInformationBuilder.addStatePlayerIndication(choice.getPlayerIndex(), currentRowGroup);
                    }
                }
                if (stateAndChoiceInformationBuilder.isBuildMarkovianStates() && choice.isMarkovian()) {
                    stateAndChoiceInformationBuilder.addMarkovianState(currentRowGroup);
                }

                // Add the probabilistic behavior to the matrix.
                for (auto const& stateProbabilityPair : choice) {
                    transitionMatrixBuilder.addNextValue(currentRow, stateProbabilityPair.first, stateProbabilityPair.second);
                }

                // Add the rewards to the reward models.
                auto choiceRewardIt = choice.getRewards().begin();
                for (auto& rewardModelBuilder : rewardModelBuilders) {
                    if (rewardModelBuilder.hasStateActionRewards()) {
                        rewardModelBuilder.addStateActionReward(*choiceRewardIt);
                    }
                    ++choiceRewardIt;
                }
                ++currentRow;
                firstChoiceOfState = false;
            }

            ++currentRowGroup;
        }

        ++numberOfExploredStates;
        if (generator->getOptions().isShowProgressSet()) {
            ++numberOfExploredStatesSinceLastMessage;

            auto now = std::chrono::high_resolution_clock::now();
            auto durationSinceLastMessage = std::chrono::duration_cast<std::chrono::seconds>(now - timeOfLastMessage).count();
            if (static_cast<uint64_t>(durationSinceLastMessage) >= generator->getOptions().getShowProgressDelay()) {
                auto statesPerSecond = numberOfExploredStatesSinceLastMessage / durationSinceLastMessage;
                auto durationSinceStart = std::chrono::duration_cast<std::chrono::seconds>(now - timeOfStart).count();
                std::cout << "Explored " << numberOfExploredStates << " states in " << durationSinceStart << " seconds (currently " << statesPerSecond
                          << " states per second).\n";
                timeOfLastMessage = std::chrono::high_resolution_clock::now();
                numberOfExploredStatesSinceLastMessage = 0;
            }
        }

        if (storm::utility::resources::isTerminate()) {
            auto durationSinceStart = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - timeOfStart).count();
            std::cout << "Explored " << numberOfExploredStates << " states in " << durationSinceStart << " seconds before abort.\n";
            STORM_LOG_THROW(false, storm::exceptions::AbortException, "Aborted in state space exploration.");
            break;
        }
    }

    // If the exploration order was not breadth-first, we need to fix the entries in the matrix according to
    // (reversed) mapping of row groups to indices.
    if (options.explorationOrder != ExplorationOrder::Bfs) {
        STORM_LOG_ASSERT(stateRemapping, "Unable to fix columns without mapping.");
        std::vector<uint_fast64_t> const& remapping = stateRemapping.get();

        // We need to fix the following entities:
        // (a) the transition matrix
        // (b) the initial states
        // (c) the hash map storing the mapping states -> ids
        // (d) fix remapping for state-generation labels

        // Fix (a).
        transitionMatrixBuilder.replaceColumns(remapping, 0);

        // Fix (b).
        std::vector<StateType> newInitialStateIndices(this->stateStorage.initialStateIndices.size());
        std::transform(this->stateStorage.initialStateIndices.begin(), this->stateStorage.initialStateIndices.end(), newInitialStateIndices.begin(),
                       [&remapping](StateType const& state) { return remapping[state]; });
        std::sort(newInitialStateIndices.begin(), newInitialStateIndices.end());
        this->stateStorage.initialStateIndices = std::move(newInitialStateIndices);

        // Fix (c).
        this->stateStorage.stateToId.remap([&remapping](StateType const& state) { return remapping[state]; });

        this->generator->remapStateIds([&remapping](StateType const& state) { return remapping[state]; });
    }
}

template<typename ValueType, typename RewardModelType, typename StateType>
storm::storage::sparse::ModelComponents<ValueType, RewardModelType> ExplicitModelBuilder<ValueType, RewardModelType, StateType>::buildModelComponents() {
    // Determine whether we have to combine different choices to one or whether this model can have more than
    // one choice per state.
    bool deterministicModel = generator->isDeterministicModel();

    // Prepare the component builders
    storm::storage::SparseMatrixBuilder<ValueType> transitionMatrixBuilder(0, 0, 0, false, !deterministicModel, 0);
    std::vector<RewardModelBuilder<typename RewardModelType::ValueType>> rewardModelBuilders;
    for (uint64_t i = 0; i < generator->getNumberOfRewardModels(); ++i) {
        rewardModelBuilders.emplace_back(generator->getRewardModelInformation(i));
    }
    StateAndChoiceInformationBuilder stateAndChoiceInformationBuilder;
    stateAndChoiceInformationBuilder.setBuildChoiceLabels(generator->getOptions().isBuildChoiceLabelsSet());
    stateAndChoiceInformationBuilder.setBuildChoiceOrigins(generator->getOptions().isBuildChoiceOriginsSet());
    stateAndChoiceInformationBuilder.setBuildStatePlayerIndications(generator->getModelType() == storm::generator::ModelType::SMG);
    stateAndChoiceInformationBuilder.setBuildMarkovianStates(generator->getModelType() == storm::generator::ModelType::MA);
    stateAndChoiceInformationBuilder.setBuildStateValuations(generator->getOptions().isBuildStateValuationsSet());

    buildMatrices(transitionMatrixBuilder, rewardModelBuilders, stateAndChoiceInformationBuilder);

    // Initialize the model components with the obtained information.
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> modelComponents(
        transitionMatrixBuilder.build(0, transitionMatrixBuilder.getCurrentRowGroupCount()), buildStateLabeling(),
        std::unordered_map<std::string, RewardModelType>(), !generator->isDiscreteTimeModel());

    uint_fast64_t numStates = modelComponents.transitionMatrix.getColumnCount();
    uint_fast64_t numChoices = modelComponents.transitionMatrix.getRowCount();

    // Now finalize all reward models.
    for (auto& rewardModelBuilder : rewardModelBuilders) {
        modelComponents.rewardModels.emplace(rewardModelBuilder.getName(),
                                             rewardModelBuilder.build(numChoices, modelComponents.transitionMatrix.getColumnCount(), numStates));
    }
    // Build the player assignment
    if (stateAndChoiceInformationBuilder.isBuildStatePlayerIndications()) {
        modelComponents.statePlayerIndications = stateAndChoiceInformationBuilder.buildStatePlayerIndications(numStates);
        modelComponents.playerNameToIndexMap = generator->getPlayerNameToIndexMap();
    }
    // Build Markovian states
    if (stateAndChoiceInformationBuilder.isBuildMarkovianStates()) {
        modelComponents.markovianStates = stateAndChoiceInformationBuilder.buildMarkovianStates(numStates);
    }
    // Build the choice labeling
    if (stateAndChoiceInformationBuilder.isBuildChoiceLabels()) {
        modelComponents.choiceLabeling = stateAndChoiceInformationBuilder.buildChoiceLabeling(numChoices);
    }
    // If requested, build the state valuations and choice origins
    if (stateAndChoiceInformationBuilder.isBuildStateValuations()) {
        modelComponents.stateValuations = stateAndChoiceInformationBuilder.stateValuationsBuilder().build();
    }
    if (stateAndChoiceInformationBuilder.isBuildChoiceOrigins()) {
        auto originData = stateAndChoiceInformationBuilder.buildDataOfChoiceOrigins(numChoices);
        modelComponents.choiceOrigins = generator->generateChoiceOrigins(originData);
    }
    if (generator->isPartiallyObservable()) {
        std::vector<uint32_t> classes(stateStorage.getNumberOfStates());
        std::unordered_map<uint32_t, std::vector<std::pair<std::vector<std::string>, uint32_t>>> observationActions;
        for (auto const& bitVectorIndexPair : stateStorage.stateToId) {
            uint32_t varObservation = generator->observabilityClass(bitVectorIndexPair.first);
            classes[bitVectorIndexPair.second] = varObservation;
        }

        modelComponents.observabilityClasses = classes;
        if (generator->getOptions().isBuildObservationValuationsSet()) {
            modelComponents.observationValuations = generator->makeObservationValuation();
        }
    }
    return modelComponents;
}

template<typename ValueType, typename RewardModelType, typename StateType>
storm::models::sparse::StateLabeling ExplicitModelBuilder<ValueType, RewardModelType, StateType>::buildStateLabeling() {
    return generator->label(stateStorage, stateStorage.initialStateIndices, stateStorage.deadlockStateIndices, stateStorage.unexploredStateIndices);
}

// Explicitly instantiate the class.
template class ExplicitModelBuilder<double, storm::models::sparse::StandardRewardModel<double>, uint32_t>;
template class ExplicitStateLookup<uint32_t>;

template class ExplicitModelBuilder<RationalNumber, storm::models::sparse::StandardRewardModel<RationalNumber>, uint32_t>;
template class ExplicitModelBuilder<RationalFunction, storm::models::sparse::StandardRewardModel<RationalFunction>, uint32_t>;
template class ExplicitModelBuilder<double, storm::models::sparse::StandardRewardModel<storm::Interval>, uint32_t>;  // TODO: where is this used?
template class ExplicitModelBuilder<storm::Interval, storm::models::sparse::StandardRewardModel<storm::Interval>, uint32_t>;
template class ExplicitModelBuilder<storm::RationalInterval, storm::models::sparse::StandardRewardModel<storm::RationalInterval>, uint32_t>;

}  // namespace builder
}  // namespace storm
