#include "storm/transformer/SubsystemBuilder.h"

#include <boost/optional.hpp>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/builder.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {

template<typename ValueType, typename RewardModelType>
void transformModelSpecificComponents(storm::models::sparse::Model<ValueType, RewardModelType> const& originalModel, storm::storage::BitVector const& subsystem,
                                      storm::storage::sparse::ModelComponents<ValueType, RewardModelType>& components) {
    if (originalModel.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        auto const& ma = *originalModel.template as<storm::models::sparse::MarkovAutomaton<ValueType, RewardModelType>>();
        components.markovianStates = ma.getMarkovianStates() % subsystem;
        components.exitRates = storm::utility::vector::filterVector(ma.getExitRates(), subsystem);
        components.rateTransitions = false;  // Note that originalModel.getTransitionMatrix() contains probabilities
    } else if (originalModel.isOfType(storm::models::ModelType::Ctmc)) {
        auto const& ctmc = *originalModel.template as<storm::models::sparse::Ctmc<ValueType, RewardModelType>>();
        components.exitRates = storm::utility::vector::filterVector(ctmc.getExitRateVector(), subsystem);
        components.rateTransitions = true;
    } else {
        STORM_LOG_THROW(originalModel.isOfType(storm::models::ModelType::Dtmc) || originalModel.isOfType(storm::models::ModelType::Mdp),
                        storm::exceptions::UnexpectedException, "Unexpected model type.");
    }
    // Nothing to be done if the model has deadlock States
}

template<typename RewardModelType>
RewardModelType transformRewardModel(RewardModelType const& originalRewardModel, storm::storage::BitVector const& subsystem,
                                     storm::storage::BitVector const& subsystemActions, bool makeRowGroupingTrivial) {
    std::optional<std::vector<typename RewardModelType::ValueType>> stateRewardVector;
    std::optional<std::vector<typename RewardModelType::ValueType>> stateActionRewardVector;
    std::optional<storm::storage::SparseMatrix<typename RewardModelType::ValueType>> transitionRewardMatrix;
    if (originalRewardModel.hasStateRewards()) {
        stateRewardVector = storm::utility::vector::filterVector(originalRewardModel.getStateRewardVector(), subsystem);
    }
    if (originalRewardModel.hasStateActionRewards()) {
        stateActionRewardVector = storm::utility::vector::filterVector(originalRewardModel.getStateActionRewardVector(), subsystemActions);
    }
    if (originalRewardModel.hasTransitionRewards()) {
        transitionRewardMatrix = originalRewardModel.getTransitionRewardMatrix().getSubmatrix(false, subsystemActions, subsystem);
        if (makeRowGroupingTrivial) {
            STORM_LOG_ASSERT(transitionRewardMatrix.value().getColumnCount() == transitionRewardMatrix.value().getRowCount(), "Matrix should be square");
            transitionRewardMatrix.value().makeRowGroupingTrivial();
        }
    }
    return RewardModelType(std::move(stateRewardVector), std::move(stateActionRewardVector), std::move(transitionRewardMatrix));
}

template<typename ValueType, typename RewardModelType>
SubsystemBuilderReturnType<ValueType, RewardModelType> internalBuildSubsystem(storm::models::sparse::Model<ValueType, RewardModelType> const& originalModel,
                                                                              storm::storage::BitVector const& subsystemStates,
                                                                              storm::storage::BitVector const& subsystemActions,
                                                                              SubsystemBuilderOptions const& options) {
    auto const& groupIndices = originalModel.getTransitionMatrix().getRowGroupIndices();
    SubsystemBuilderReturnType<ValueType, RewardModelType> result;
    uint_fast64_t subsystemStateCount = subsystemStates.getNumberOfSetBits();
    if (options.buildStateMapping) {
        result.newToOldStateIndexMapping.reserve(subsystemStateCount);
    }
    if (options.buildActionMapping) {
        result.newToOldActionIndexMapping.reserve(subsystemActions.getNumberOfSetBits());
    }
    storm::storage::BitVector deadlockStates;
    if (options.fixDeadlocks) {
        deadlockStates.resize(subsystemStates.size(), false);
    }
    // Get the set of actions that stay in the subsystem.
    // Also establish the mappings if requested.
    storm::storage::BitVector keptActions(originalModel.getTransitionMatrix().getRowCount(), false);
    for (auto subsysState : subsystemStates) {
        if (options.buildStateMapping) {
            result.newToOldStateIndexMapping.push_back(subsysState);
        }
        bool hasDeadlock = true;
        for (uint_fast64_t row = subsystemActions.getNextSetIndex(groupIndices[subsysState]); row < groupIndices[subsysState + 1];
             row = subsystemActions.getNextSetIndex(row + 1)) {
            bool allRowEntriesStayInSubsys = true;
            if (options.checkTransitionsOutside) {
                for (auto const& entry : originalModel.getTransitionMatrix().getRow(row)) {
                    if (!subsystemStates.get(entry.getColumn())) {
                        allRowEntriesStayInSubsys = false;
                        break;
                    }
                }
            }
            if (allRowEntriesStayInSubsys) {
                if (options.buildActionMapping) {
                    result.newToOldActionIndexMapping.push_back(row);
                }
                keptActions.set(row, true);
                hasDeadlock = false;
            }
        }
        if (hasDeadlock) {
            STORM_LOG_THROW(options.fixDeadlocks, storm::exceptions::InvalidOperationException,
                            "Expected that in each state, at least one action is selected. Got a deadlock state instead. (violated at " << subsysState << ")");
            if (options.buildActionMapping) {
                result.newToOldActionIndexMapping.push_back(std::numeric_limits<uint64_t>::max());
            }
            deadlockStates.set(subsysState);
        }
    }
    // If we have deadlock states we keep an action in each rowgroup of a deadlock states.
    bool hasDeadlockStates = !deadlockStates.empty();
    if (options.buildKeptActions) {
        // store them now, before changing them.
        result.keptActions = keptActions;
    }
    for (auto deadlockState : deadlockStates) {
        keptActions.set(groupIndices[deadlockState], true);
    }

    // Transform the components of the model
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> components;
    if (hasDeadlockStates) {
        // make deadlock choices a selfloop
        components.transitionMatrix = originalModel.getTransitionMatrix();
        components.transitionMatrix.makeRowGroupsAbsorbing(deadlockStates);
        components.transitionMatrix.dropZeroEntries();
        components.transitionMatrix = components.transitionMatrix.getSubmatrix(false, keptActions, subsystemStates);
    } else {
        components.transitionMatrix = originalModel.getTransitionMatrix().getSubmatrix(false, keptActions, subsystemStates);
    }
    if (options.makeRowGroupingTrivial) {
        STORM_LOG_ASSERT(components.transitionMatrix.getColumnCount() == components.transitionMatrix.getRowCount(), "Matrix should be square");
        components.transitionMatrix.makeRowGroupingTrivial();
    }

    components.stateLabeling = originalModel.getStateLabeling().getSubLabeling(subsystemStates);
    for (auto const& rewardModel : originalModel.getRewardModels()) {
        components.rewardModels.insert(
            std::make_pair(rewardModel.first, transformRewardModel(rewardModel.second, subsystemStates, keptActions, options.makeRowGroupingTrivial)));
    }
    if (originalModel.hasChoiceLabeling()) {
        components.choiceLabeling = originalModel.getChoiceLabeling().getSubLabeling(keptActions);
    }
    if (originalModel.hasStateValuations()) {
        components.stateValuations = originalModel.getStateValuations().selectStates(subsystemStates);
    }
    if (originalModel.hasChoiceOrigins()) {
        components.choiceOrigins = originalModel.getChoiceOrigins()->selectChoices(keptActions);
    }

    if (hasDeadlockStates) {
        auto subDeadlockStates = deadlockStates % subsystemStates;
        assert(deadlockStates.getNumberOfSetBits() == subDeadlockStates.getNumberOfSetBits());
        // erase rewards, choice labels, choice origins
        for (auto& rewModel : components.rewardModels) {
            for (auto state : subDeadlockStates) {
                rewModel.second.clearRewardAtState(state, components.transitionMatrix);
            }
        }
        if (components.choiceLabeling) {
            storm::storage::BitVector nonDeadlockChoices(components.transitionMatrix.getRowCount(), true);
            for (auto state : subDeadlockStates) {
                auto const& choice = components.transitionMatrix.getRowGroupIndices()[state];
                nonDeadlockChoices.set(choice, false);
            }
            for (auto const& label : components.choiceLabeling.value().getLabels()) {
                components.choiceLabeling->setChoices(label, components.choiceLabeling->getChoices(label) & nonDeadlockChoices);
            }
        }
        if (components.choiceOrigins) {
            for (auto state : subDeadlockStates) {
                auto const& choice = components.transitionMatrix.getRowGroupIndices()[state];
                components.choiceOrigins.value()->clearOriginOfChoice(choice);
            }
        }
        // get a unique label for the deadlock states
        result.deadlockLabel = "deadl";
        while (components.stateLabeling.containsLabel(result.deadlockLabel.get())) {
            result.deadlockLabel->push_back('_');
        }
        components.stateLabeling.addLabel(result.deadlockLabel.get(), std::move(subDeadlockStates));
    }

    transformModelSpecificComponents<ValueType, RewardModelType>(originalModel, subsystemStates, components);
    models::ModelType newModelType = originalModel.getType();
    if (components.transitionMatrix.hasTrivialRowGrouping() && originalModel.getType() == models::ModelType::Mdp) {
        newModelType = models::ModelType::Dtmc;
    }
    result.model = storm::utility::builder::buildModelFromComponents(newModelType, std::move(components));
    STORM_LOG_DEBUG("Subsystem Builder is done. Resulting model has " << result.model->getNumberOfStates() << " states.");
    return result;
}

template<typename ValueType, typename RewardModelType>
SubsystemBuilderReturnType<ValueType, RewardModelType> buildSubsystem(storm::models::sparse::Model<ValueType, RewardModelType> const& originalModel,
                                                                      storm::storage::BitVector const& subsystemStates,
                                                                      storm::storage::BitVector const& subsystemActions, bool keepUnreachableStates,
                                                                      SubsystemBuilderOptions options) {
    STORM_LOG_DEBUG("Invoked subsystem builder on model with " << originalModel.getNumberOfStates() << " states.");
    storm::storage::BitVector initialStates = originalModel.getInitialStates() & subsystemStates;
    STORM_LOG_THROW(!initialStates.empty(), storm::exceptions::InvalidArgumentException, "The subsystem would not contain any initial states");

    STORM_LOG_THROW(!subsystemStates.empty(), storm::exceptions::InvalidArgumentException, "Invoked SubsystemBuilder for an empty subsystem.");
    if (keepUnreachableStates) {
        return internalBuildSubsystem(originalModel, subsystemStates, subsystemActions, options);
    } else {
        auto actualSubsystem = storm::utility::graph::getReachableStates(originalModel.getTransitionMatrix(), initialStates, subsystemStates,
                                                                         storm::storage::BitVector(subsystemStates.size(), false), false, 0, subsystemActions);
        return internalBuildSubsystem(originalModel, actualSubsystem, subsystemActions, options);
    }
}

template SubsystemBuilderReturnType<double> buildSubsystem(storm::models::sparse::Model<double> const& originalModel,
                                                           storm::storage::BitVector const& subsystemStates, storm::storage::BitVector const& subsystemActions,
                                                           bool keepUnreachableStates = true, SubsystemBuilderOptions options = SubsystemBuilderOptions());
template SubsystemBuilderReturnType<double, storm::models::sparse::StandardRewardModel<storm::Interval>> buildSubsystem(
    storm::models::sparse::Model<double, storm::models::sparse::StandardRewardModel<storm::Interval>> const& originalModel,
    storm::storage::BitVector const& subsystemStates, storm::storage::BitVector const& subsystemActions, bool keepUnreachableStates = true,
    SubsystemBuilderOptions options = SubsystemBuilderOptions());
template SubsystemBuilderReturnType<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>> buildSubsystem(
    storm::models::sparse::Model<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>> const& originalModel,
    storm::storage::BitVector const& subsystemStates, storm::storage::BitVector const& subsystemActions, bool keepUnreachableStates = true,
    SubsystemBuilderOptions options = SubsystemBuilderOptions());
template SubsystemBuilderReturnType<storm::RationalNumber> buildSubsystem(storm::models::sparse::Model<storm::RationalNumber> const& originalModel,
                                                                          storm::storage::BitVector const& subsystemStates,
                                                                          storm::storage::BitVector const& subsystemActions, bool keepUnreachableStates = true,
                                                                          SubsystemBuilderOptions options = SubsystemBuilderOptions());
template SubsystemBuilderReturnType<storm::RationalFunction> buildSubsystem(storm::models::sparse::Model<storm::RationalFunction> const& originalModel,
                                                                            storm::storage::BitVector const& subsystemStates,
                                                                            storm::storage::BitVector const& subsystemActions,
                                                                            bool keepUnreachableStates = true,
                                                                            SubsystemBuilderOptions options = SubsystemBuilderOptions());
template SubsystemBuilderReturnType<storm::Interval> buildSubsystem(storm::models::sparse::Model<storm::Interval> const& originalModel,
                                                                    storm::storage::BitVector const& subsystemStates,
                                                                    storm::storage::BitVector const& subsystemActions, bool keepUnreachableStates = true,
                                                                    SubsystemBuilderOptions options = SubsystemBuilderOptions());
template SubsystemBuilderReturnType<storm::RationalInterval> buildSubsystem(storm::models::sparse::Model<storm::RationalInterval> const& originalModel,
                                                                            storm::storage::BitVector const& subsystemStates,
                                                                            storm::storage::BitVector const& subsystemActions,
                                                                            bool keepUnreachableStates = true,
                                                                            SubsystemBuilderOptions options = SubsystemBuilderOptions());

}  // namespace transformer
}  // namespace storm
