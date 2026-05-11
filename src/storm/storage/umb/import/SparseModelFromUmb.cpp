#include "storm/storage/umb/import/SparseModelFromUmb.h"

#include <ranges>
#include <utility>

#include "storm/storage/umb/model/UmbModel.h"
#include "storm/storage/umb/model/Valuations.h"

#include "storm/models/ModelType.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/storage/umb/model/StringEncoding.h"
#include "storm/storage/umb/model/ValueEncoding.h"
#include "storm/utility/builder.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"

namespace storm::umb {

namespace detail {

auto csrRange(auto&& csr, uint64_t i) {
    if (csr) {
        STORM_LOG_ASSERT(i + 1 < csr->size(), "CSR index out of bounds: " << (i + 1) << " >= " << csr->size());
        return std::ranges::iota_view(csr.value()[i], csr.value()[i + 1]);
    } else {
        // assume 1:1 mapping
        return std::ranges::iota_view(i, i + 1);
    }
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> createBranchMatrix(storm::umb::UmbModel const& umbModel, std::ranges::input_range auto&& branchValues) {
    auto const& tsIndex = umbModel.index.transitionSystem;
    bool const hasRowGroups = tsIndex.numPlayers >= 1;
    storm::storage::SparseMatrixBuilder<ValueType> builder(tsIndex.numChoices, tsIndex.numStates, tsIndex.numBranches, true, hasRowGroups,
                                                           hasRowGroups ? tsIndex.numStates : 0u);
    for (uint64_t stateIndex{0}; stateIndex < tsIndex.numStates; ++stateIndex) {
        auto choices = csrRange(umbModel.stateToChoices, stateIndex);
        if (hasRowGroups) {
            builder.newRowGroup(*choices.begin());
        }
        for (auto const choiceIndex : choices) {
            STORM_LOG_ASSERT(choiceIndex < tsIndex.numChoices, "Choice index out of bounds.");
            for (auto const branchIndex : csrRange(umbModel.choiceToBranches, choiceIndex)) {
                auto const branchTarget = umbModel.branchToTarget.has_value() ? umbModel.branchToTarget.value()[branchIndex] : 0ul;
                STORM_LOG_ASSERT(branchTarget < tsIndex.numStates, "Branch target index out of bounds: " << branchTarget << " >= " << tsIndex.numStates);
                builder.addNextValue(choiceIndex, branchTarget, branchValues[branchIndex]);
            }
        }
    }
    return builder.build();
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> createBranchMatrix(storm::umb::UmbModel const& umbModel, storm::umb::GenericVector const& branchValues,
                                                           storm::umb::SizedType const& sourceType) {
    return ValueEncoding::applyDecodedVector<ValueType>([&umbModel](auto&& input) { return createBranchMatrix<ValueType>(umbModel, input); }, branchValues,
                                                        sourceType);
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> createBranchMatrix(storm::umb::UmbModel const& umbModel, ValueType const& defaultValue) {
    auto const defaultView = std::ranges::iota_view(0ull, umbModel.index.transitionSystem.numBranches) |
                             std::ranges::views::transform([&defaultValue](auto) -> ValueType { return defaultValue; });
    return createBranchMatrix<ValueType>(umbModel, defaultView);
}

storm::storage::BitVector createBitVector(storm::umb::VectorType<bool> const& umbBitVector, uint64_t size) {
    STORM_LOG_THROW(umbBitVector.size() >= size, storm::exceptions::WrongFormatException,
                    "Bit vector has unexpected size: " << umbBitVector.size() << " < " << size);
    storm::storage::BitVector result(umbBitVector);
    result.resize(size);
    return result;
}

storm::storage::BitVector createBitVector(storm::umb::OptionalVectorType<bool> const& umbBitVector, uint64_t size) {
    STORM_LOG_THROW(umbBitVector.has_value(), storm::exceptions::WrongFormatException, "BitVector is not given but expected.");
    return createBitVector(umbBitVector.value(), size);
}

storm::models::sparse::StateLabeling constructStateLabeling(storm::umb::UmbModel const& umbModel) {
    auto const& numStates = umbModel.index.transitionSystem.numStates;
    storm::models::sparse::StateLabeling stateLabelling(numStates);
    if (umbModel.stateIsInitial) {
        stateLabelling.addLabel("init", createBitVector(umbModel.stateIsInitial, numStates));
    } else {
        STORM_LOG_WARN("No initial states given in UMB model.");
        stateLabelling.addLabel("init", storm::storage::BitVector(numStates, false));  // default to all states not being initial
    }
    if (umbModel.index.aps().has_value()) {
        auto aps = umbModel.index.aps();
        for (auto const& [apName, apIndex] : aps.value()) {
            STORM_LOG_THROW(umbModel.aps().has_value() && umbModel.aps()->contains(apName), storm::exceptions::WrongFormatException,
                            "Atomic proposition '" << apName << "' mentioned in index but no files were found.");
            STORM_LOG_THROW(isBooleanType(apIndex.type.type), storm::exceptions::WrongFormatException,
                            "Atomic proposition '" << apName << "' must be of boolean type.");
            STORM_LOG_THROW(apIndex.appliesTo.size() == 1 && apIndex.appliesToStates(), storm::exceptions::WrongFormatException,
                            "Atomic proposition '" << apName << "' must apply only to states.");
            auto const& ap = umbModel.aps()->at(apName);
            auto labelName = apIndex.alias.value_or(apName);  // prefer alias as label name if it exists
            STORM_LOG_THROW(ap.states.has_value(), storm::exceptions::WrongFormatException, "Atomic proposition '" << apName << "' has no states values");
            STORM_LOG_THROW(!stateLabelling.containsLabel(labelName), storm::exceptions::WrongFormatException,
                            "Label '" << labelName << "' already exists in state labeling.");
            stateLabelling.addLabel(labelName, createBitVector(ap.states->values.template get<bool>(), numStates));
        }
    }
    return stateLabelling;
}

storm::models::sparse::ChoiceLabeling constructChoiceLabeling(storm::umb::UmbModel const& umbModel) {
    auto const& numChoices = umbModel.index.transitionSystem.numChoices;
    auto const& numActions = umbModel.index.transitionSystem.numChoiceActions;

    storm::models::sparse::ChoiceLabeling choiceLabeling(numChoices);

    auto const actionStrings = storm::umb::stringVectorView(umbModel.choiceActions->strings, umbModel.choiceActions->stringMapping);
    bool const hasActionStrings = !actionStrings.empty();
    STORM_LOG_THROW(!hasActionStrings || actionStrings.size() == numActions, storm::exceptions::WrongFormatException,
                    "Number of action strings does not match number of actions.");

    // choices where the action has the empty label will not be labeled at all. If there are no string labels, this case is not relevant.
    uint64_t const emptyActionIndex = hasActionStrings ? std::ranges::find(actionStrings, "") - actionStrings.begin() : numActions;

    // for each choice, find the corresponding action index and set the bit accordingly
    auto const& choiceToChoiceAction = umbModel.choiceActions->values.value();
    std::vector<storm::storage::BitVector> actionToLabels(numActions, storm::storage::BitVector(numChoices, false));
    for (uint64_t choiceIndex = 0; choiceIndex < numChoices; ++choiceIndex) {
        auto const actionIndex = choiceToChoiceAction[choiceIndex];
        STORM_LOG_ASSERT(actionIndex < numActions, "Choice to action mapping out of bounds.");
        if (hasActionStrings && actionIndex == emptyActionIndex) {
            continue;  // skip choices with empty action. They will not be labeled.
        }
        actionToLabels[actionIndex].set(choiceIndex);
    }

    // add the action labels to the labeling
    if (hasActionStrings) {
        for (uint64_t actionIndex = 0; actionIndex < numActions; ++actionIndex) {
            if (actionIndex == emptyActionIndex) {
                continue;
            }
            choiceLabeling.addLabel(std::string(actionStrings[actionIndex]), std::move(actionToLabels[actionIndex]));
        }
    } else {
        // use generic action names
        for (uint64_t actionIndex = 0; actionIndex < numActions; ++actionIndex) {
            choiceLabeling.addLabel("a" + std::to_string(actionIndex), std::move(actionToLabels[actionIndex]));
        }
    }
    return choiceLabeling;
}

template<typename ValueType>
auto constructRewardModels(storm::umb::UmbModel const& umbModel) {
    using RewardModel = storm::models::sparse::StandardRewardModel<ValueType>;
    std::unordered_map<std::string, RewardModel> rewardModels;
    if (umbModel.index.rewards().has_value()) {
        auto rewards = umbModel.index.rewards();
        for (auto const& [rewName, rewIndex] : rewards.value()) {
            STORM_LOG_THROW(umbModel.rewards().has_value() && umbModel.rewards()->contains(rewName), storm::exceptions::WrongFormatException,
                            "Reward " << rewName << "' mentioned in index but no files were found.");
            auto const& rew = umbModel.rewards()->at(rewName);
            auto usedRewName = rewIndex.alias.value_or(rewName);  // prefer alias as reward name if it exists
            STORM_LOG_THROW(!rewardModels.contains(usedRewName), storm::exceptions::WrongFormatException,
                            "Reward '" << usedRewName << "' already exists in reward models.");
            STORM_LOG_THROW(isNumericType(rewIndex.type.type), storm::exceptions::WrongFormatException,
                            "Reward type for reward '" << rewName << "' must be numeric.");
            std::optional<std::vector<ValueType>> stateRewards, stateActionRewards;
            std::optional<storm::storage::SparseMatrix<ValueType>> transitionRewards;
            if (rewIndex.appliesToStates() && rew.states.has_value()) {
                stateRewards = ValueEncoding::createDecodedVector<ValueType>(rew.states->values, rewIndex.type);
            }
            if (rewIndex.appliesToChoices() && rew.choices.has_value()) {
                stateActionRewards = ValueEncoding::createDecodedVector<ValueType>(rew.choices->values, rewIndex.type);
            }
            if (rewIndex.appliesToBranches() && rew.branches.has_value()) {
                transitionRewards = createBranchMatrix<ValueType>(umbModel, rew.branches->values, rewIndex.type);
            }
            STORM_LOG_THROW(!rewIndex.appliesToObservations(), storm::exceptions::NotSupportedException,
                            "Observation rewards are not supported for reward '" << rewName << "'.");
            STORM_LOG_THROW(!rewIndex.appliesToPlayers(), storm::exceptions::NotSupportedException,
                            "Player rewards are not supported for reward '" << rewName << "'.");
            rewardModels.emplace(std::move(usedRewName), RewardModel(std::move(stateRewards), std::move(stateActionRewards), std::move(transitionRewards)));
        }
    }
    return rewardModels;
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> constructTransitionMatrix(storm::umb::UmbModel const& umbModel) {
    if (umbModel.branchToProbability.hasValue()) {
        auto result = createBranchMatrix<ValueType>(umbModel, umbModel.branchToProbability, umbModel.index.transitionSystem.branchProbabilityType.value());
        if constexpr (storm::NumberTraits<ValueType>::IsExact) {
            if (umbModel.branchToProbability.isType<double>() || umbModel.branchToProbability.isType<storm::Interval>()) {
                // If the branch probabilities are imprecise, we might need to normalize the matrix rows to ensure they sum up to 1.
                uint64_t numNormalized{0};
                ValueType maxDiff{storm::utility::zero<ValueType>()};
                for (uint64_t rowIndex = 0; rowIndex < result.getRowCount(); ++rowIndex) {
                    auto const rowSum = result.getRowSum(rowIndex);
                    if (!storm::utility::isOne(rowSum)) {
                        maxDiff = std::max(maxDiff, storm::utility::abs<ValueType>(storm::utility::one<ValueType>() - rowSum));
                        ++numNormalized;
                        for (auto& entry : result.getRow(rowIndex)) {
                            entry.setValue(entry.getValue() / rowSum);
                        }
                    }
                }
                STORM_LOG_WARN_COND(numNormalized == 0,
                                    "Branch probabilities are given in an imprecise type but an exact model was requested. Probabilities for "
                                        << numNormalized << " choices were normalized to ensure they sum up to 1. Maximum diff to 1 was " << maxDiff << ".");
            }
        }
        return result;
    } else {
        return createBranchMatrix<ValueType>(umbModel, storm::utility::one<ValueType>());
    }
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> constructSparseModel(storm::umb::UmbModel const& umbModel, ImportOptions const& options) {
    umbModel.validateOrThrow();

    // transitions, labelings, rewards
    auto stateLabelling = constructStateLabeling(umbModel);
    STORM_LOG_THROW(umbModel.index.transitionSystem.branchProbabilityType.has_value(), storm::exceptions::WrongFormatException,
                    "Branch probability type must be given in the UMB model index.");
    auto transitionMatrix = constructTransitionMatrix<ValueType>(umbModel);
    storm::storage::sparse::ModelComponents<ValueType> components(std::move(transitionMatrix), std::move(stateLabelling),
                                                                  constructRewardModels<ValueType>(umbModel));
    if (options.buildChoiceLabeling && umbModel.index.transitionSystem.numChoiceActions > 0) {
        STORM_LOG_THROW(umbModel.choiceActions.has_value() && umbModel.choiceActions->values.has_value(), storm::exceptions::WrongFormatException,
                        "Choice actions mentioned in the index but no files given.");
        components.choiceLabeling = constructChoiceLabeling(umbModel);
    }
    if (options.buildStateValuations && umbModel.index.valuations.has_value() && umbModel.index.valuations->states.has_value()) {
        STORM_LOG_THROW(umbModel.valuations.states.has_value() && umbModel.valuations.states->valuations.has_value(), storm::exceptions::WrongFormatException,
                        "State valuations mentioned in the index but no files given.");
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "State valuations for UMB models are not yet supported.");
    } else {
        STORM_LOG_WARN_COND(!options.buildStateValuations, "State valuations requested but the UMB model does not have any.");
    }

    // model type-specific components
    using enum storm::models::ModelType;
    auto const modelType = deriveModelType(umbModel.index);
    if (modelType == Ctmc || modelType == MarkovAutomaton) {
        STORM_LOG_THROW(umbModel.stateToExitRate.hasValue(), storm::exceptions::WrongFormatException,
                        "Exit rates are required for CTMC and Markov automaton models but not present in the UMB model.");
        components.exitRates = ValueEncoding::createDecodedVector<ValueType>(umbModel.stateToExitRate, umbModel.index.transitionSystem.exitRateType.value());
        if (modelType == MarkovAutomaton) {
            if (umbModel.stateIsMarkovian) {
                components.markovianStates = createBitVector(umbModel.stateIsMarkovian, umbModel.index.transitionSystem.numStates);
            } else {
                // Default to no Markovian state
                components.markovianStates.emplace(umbModel.index.transitionSystem.numStates, false);
            }
        }
    } else if (modelType == Pomdp) {
        STORM_LOG_THROW(umbModel.index.transitionSystem.observationsApplyTo == storm::umb::ModelIndex::TransitionSystem::ObservationsApplyTo::States,
                        storm::exceptions::NotSupportedException, "Only state observations are currently supported for POMDP models.");
        STORM_LOG_THROW(!umbModel.index.transitionSystem.observationProbabilityType.has_value(), storm::exceptions::NotSupportedException,
                        "Only deterministic state observations are currently supported for POMDP models.");
        STORM_LOG_THROW(umbModel.stateObservations.has_value(), storm::exceptions::WrongFormatException,
                        "State observations are required for POMDP models but not present in the UMB model.");
        components.observabilityClasses.emplace(umbModel.stateObservations->values->begin(), umbModel.stateObservations->values->end());
    } else if (modelType == Smg) {
        if (umbModel.stateToPlayer.has_value()) {
            auto const& stateToPlayer = umbModel.stateToPlayer.value();
            components.statePlayerIndications.emplace(stateToPlayer.begin(), stateToPlayer.end());
        } else {
            // Default to all states belonging to player 0
            components.statePlayerIndications.emplace(umbModel.index.transitionSystem.numStates, 0);
        }
        if (umbModel.index.transitionSystem.playerNames.has_value()) {
            auto const& names = umbModel.index.transitionSystem.playerNames.value();
            STORM_LOG_THROW(names.size() == umbModel.index.transitionSystem.numPlayers, storm::exceptions::WrongFormatException,
                            "Number of player names does not match number of players in the UMB model index.");
            components.playerNameToIndexMap.emplace();
            for (uint64_t i = 0; i < names.size(); ++i) {
                components.playerNameToIndexMap->emplace(names[i], i);
            }
        }
    } else {
        STORM_LOG_THROW(modelType == Dtmc || modelType == Mdp, storm::exceptions::NotSupportedException, "Unexpected model type for UMB import: " << modelType);
    }
    return storm::utility::builder::buildModelFromComponents(deriveModelType(umbModel.index), std::move(components));
}

}  // namespace detail

storm::models::ModelType deriveModelType(storm::umb::ModelIndex const& index) {
    using enum storm::models::ModelType;

    auto const& ts = index.transitionSystem;

    STORM_LOG_THROW(ts.branchProbabilityType.has_value(), storm::exceptions::NotSupportedException, "Models without branch values are not supported.");
    switch (ts.time) {
        using enum storm::umb::ModelIndex::TransitionSystem::Time;
        case Discrete:
            switch (ts.numPlayers) {
                case 0:
                    return Dtmc;
                case 1:
                    return ts.numObservations == 0 ? Mdp : Pomdp;
                default:
                    STORM_LOG_THROW(ts.numObservations == 0, storm::exceptions::NotSupportedException,
                                    "Multiplayer partially observable models are not supported.");
                    return Smg;
            }
        case Stochastic:
            STORM_LOG_THROW(ts.numPlayers == 0, storm::exceptions::NotSupportedException, "Stochastic time models with multiple players are not supported.");
            return Ctmc;
        case UrgentStochastic:
            STORM_LOG_THROW(ts.numPlayers == 1, storm::exceptions::NotSupportedException,
                            "Urgent stochastic time models with multiple or no players are not supported.");
            return MarkovAutomaton;
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected transition system time type" << ts.time << ".");
}

template<typename ValueType>
bool deriveValueType(storm::umb::ModelIndex const& index, ImportOptions const& options) {
    STORM_LOG_THROW(index.transitionSystem.branchProbabilityType.has_value(), storm::exceptions::NotSupportedException,
                    "Models without branch values are not supported.");
    bool const haveDouble = index.transitionSystem.branchProbabilityType->type == storm::umb::Type::Double;
    bool const haveRational = index.transitionSystem.branchProbabilityType->type == storm::umb::Type::Rational;
    bool const haveInterval = index.transitionSystem.branchProbabilityType->type == storm::umb::Type::DoubleInterval;
    bool const useDefault = options.valueType == ImportOptions::ValueType::Default;
    bool const useDouble = options.valueType == ImportOptions::ValueType::Double;
    bool const useRational = options.valueType == ImportOptions::ValueType::Rational;

    STORM_LOG_ASSERT(useDefault || useDouble || useRational, "Unexpected value type option: " << static_cast<int>(options.valueType) << ".");

    if constexpr (std::is_same_v<ValueType, double>) {
        return (useDefault && haveDouble) || (useDouble && !haveInterval);
    } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        return (useDefault && haveRational) || (useRational && !haveInterval);
    } else {
        static_assert(std::is_same_v<ValueType, storm::Interval>, "Unhandled value type");
        // Rational intervals currently not supported.
        return (useDefault && haveInterval) || (useDouble && haveInterval);
    }
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> sparseModelFromUmb(storm::umb::UmbModel const& umbModel, ImportOptions const& options) {
    return detail::constructSparseModel<ValueType>(umbModel, options);
}

std::shared_ptr<storm::models::ModelBase> sparseModelFromUmb(storm::umb::UmbModel const& umbModel, ImportOptions const& options) {
    if (deriveValueType<double>(umbModel.index, options)) {
        return detail::constructSparseModel<double>(umbModel, options);
    } else if (deriveValueType<storm::RationalNumber>(umbModel.index, options)) {
        return detail::constructSparseModel<storm::RationalNumber>(umbModel, options);
    } else if (deriveValueType<storm::Interval>(umbModel.index, options)) {
        return detail::constructSparseModel<storm::Interval>(umbModel, options);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                        "Could not derive a supported value type for the UMB model with branch probabilities of type "
                            << umbModel.index.transitionSystem.branchProbabilityType->toString() << ".");
    }
}

template std::shared_ptr<storm::models::sparse::Model<double>> sparseModelFromUmb<double>(storm::umb::UmbModel const& umbModel, ImportOptions const& options);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> sparseModelFromUmb<storm::RationalNumber>(storm::umb::UmbModel const& umbModel,
                                                                                                                        ImportOptions const& options);
template std::shared_ptr<storm::models::sparse::Model<storm::Interval>> sparseModelFromUmb<storm::Interval>(storm::umb::UmbModel const& umbModel,
                                                                                                            ImportOptions const& options);

}  // namespace storm::umb