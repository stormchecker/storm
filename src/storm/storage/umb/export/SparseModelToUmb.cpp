#include "storm/storage/umb/export/SparseModelToUmb.h"

#include "storm/storage/SparseMatrix.h"
#include "storm/storage/umb/model/StringEncoding.h"
#include "storm/storage/umb/model/UmbModel.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/models/ModelType.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/Smg.h"
#include "storm/storage/sparse/ChoiceOrigins.h"
#include "storm/storage/valuations/ValuationsStorage.h"
#include "storm/transformer/MakePOMDPCanonic.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::umb {

namespace detail {

template<typename ValueType, typename TargetValueType>
void transitionMatrixToUmb(storm::storage::SparseMatrix<ValueType> const& matrix, storm::umb::UmbModel& umb, bool const normalize) {
    if (!matrix.hasTrivialRowGrouping()) {
        umb.stateToChoices = matrix.getRowGroupIndices();
    }
    umb.choiceToBranches = matrix.getRowIndices();
    umb.branchToTarget.emplace().reserve(matrix.getEntryCount());
    std::vector<TargetValueType> branchProbabilities;
    branchProbabilities.reserve(matrix.getEntryCount());
    for (uint64_t rowIndex = 0; rowIndex < matrix.getRowCount(); ++rowIndex) {
        auto const& row = matrix.getRow(rowIndex);
        for (auto const& entry : row) {
            umb.branchToTarget->push_back(entry.getColumn());
            branchProbabilities.push_back(storm::utility::convertNumber<TargetValueType>(entry.getValue()));
        }
        if (normalize) {
            auto rowProbs = std::span<TargetValueType>(branchProbabilities.end() - row.getNumberOfEntries(), branchProbabilities.end());
            TargetValueType const rowSum = std::accumulate(rowProbs.begin(), rowProbs.end(), storm::utility::zero<TargetValueType>());
            if (!storm::utility::isOne(rowSum)) {
                std::for_each(rowProbs.begin(), rowProbs.end(), [&rowSum](TargetValueType& entry) { entry /= rowSum; });
            }
        }
    }
    umb.branchToProbability.template set<TargetValueType>(std::move(branchProbabilities));
}

void stateLabelingToUmb(storm::models::sparse::StateLabeling const& labeling, storm::umb::UmbModel& umb) {
    for (auto const& labelName : labeling.getLabels()) {
        if (labelName == "init") {
            continue;  // skip initial state labeling. Initial states are handled separately.
        }
        STORM_LOG_ASSERT(umb.index.aps(), "Model index must have annotations to store state labels.");
        auto const name = umb.index.findAPName(labelName);
        STORM_LOG_ASSERT(name.has_value(), "Label '" << labelName << "' not found in the model index.");
        auto& aps = umb.aps(true).value();
        STORM_LOG_ASSERT(!aps.contains(*name), "Annotation for label '" << labelName << "' already exists.");
        auto& annotation = aps[*name];
        annotation.states.emplace().values.template set<bool>(labeling.getStates(labelName));
    }
}

uint64_t choiceOriginsToUmb(storm::storage::sparse::ChoiceOrigins const& choiceOrigins, storm::umb::UmbModel& umb) {
    // choice to action
    auto& choiceToAction = umb.choiceActions.emplace().values.emplace();
    choiceToAction.reserve(choiceOrigins.getNumberOfChoices());
    for (uint64_t c = 0; c < choiceOrigins.getNumberOfChoices(); ++c) {
        choiceToAction.push_back(choiceOrigins.getIdentifier(c));
    }

    // action strings
    // We always set a csr, even in cases where it could be omitted.
    auto actionStrings = StringsBuilder(umb.choiceActions->strings.emplace(), umb.choiceActions->stringMapping.emplace());
    // We use the empty action string for choices with no origin, which we want to be the first index.
    auto const emptyStringIndex = actionStrings.push_back("");
    STORM_LOG_ASSERT(emptyStringIndex == 0, "Action index for empty action string must be 0.");
    // add to the action strings by initializing the csr with {0,0}
    STORM_LOG_ASSERT(choiceOrigins.getIdentifierForChoicesWithNoOrigin() == 0, "Identifier for choices with no origin expected to be 0.");

    for (uint64_t id = 1; id < choiceOrigins.getNumberOfIdentifiers(); ++id) {  // intentionally start at 1, since we already added the empty action string
        actionStrings.push_back(choiceOrigins.getIdentifierInfo(id));
    }
    actionStrings.finalize();
    return actionStrings.size();
}

uint64_t choiceLabelingToUmb(storm::models::sparse::ChoiceLabeling const& labeling, storm::umb::UmbModel& umb) {
    // initialize umb data
    // Action 0 must be the default action and is used for choices without any label (if present).
    auto& choiceToAction = umb.choiceActions.emplace().values.emplace(labeling.getNumberOfItems(), 0);
    auto actionStrings = StringsBuilder(umb.choiceActions->strings.emplace(), umb.choiceActions->stringMapping.emplace());

    // Find out which choices have zero, at least one, or multiple labels. The former two cases can be handled more efficiently
    auto const labels = labeling.getLabels();
    storm::storage::BitVector choicesWithAtLeastOneLabel, choicesWithMultipleLabels;
    for (auto const& labelName : labels) {
        auto const& currentChoices = labeling.getChoices(labelName);
        if (choicesWithAtLeastOneLabel.size() == 0) {
            // first processed label
            choicesWithAtLeastOneLabel = currentChoices;
        } else if (choicesWithMultipleLabels.size() == 0) {
            // second processed label
            choicesWithMultipleLabels = choicesWithAtLeastOneLabel & currentChoices;
            choicesWithAtLeastOneLabel |= currentChoices;
        } else {
            // third or later processed label
            choicesWithMultipleLabels |= choicesWithAtLeastOneLabel & currentChoices;
            choicesWithAtLeastOneLabel |= currentChoices;
        }
    }

    // Handle choices without any labels.
    if (!choicesWithAtLeastOneLabel.full()) {
        // For consistency, unlabelled choices shall always have action index 0. So we add the empty action string.
        auto const emptyStringIndex = actionStrings.push_back("");
        STORM_LOG_ASSERT(emptyStringIndex == 0, "Action index for empty action string must be 0.");
        // nothing else to do for unlabeled choices: we already initialized the choiceToAction mapping with 0s
    }

    // Handle choices with exactly one label.
    auto setChoices = [&choiceToAction, &actionStrings](storm::storage::BitVector const& choices, std::string_view actionName) {
        auto choiceIt = choices.begin();
        auto const choiceItEnd = choices.end();
        if (choiceIt != choiceItEnd) {
            // there is at least one choice with this label
            auto const actionIndex = actionStrings.findOrPushBack(actionName);
            for (; choiceIt != choiceItEnd; ++choiceIt) {
                choiceToAction[*choiceIt] = actionIndex;  // set action index for this choice
            }
        }
    };
    if (choicesWithMultipleLabels.empty()) {
        for (auto const& labelName : labels) {
            setChoices(labeling.getChoices(labelName), labelName);
        }
    } else {
        choicesWithMultipleLabels.complement();  // now contains the choices with at most one label
        for (auto const& labelName : labels) {
            setChoices(labeling.getChoices(labelName) & choicesWithMultipleLabels, labelName);
        }
        choicesWithMultipleLabels.complement();  // revert above complement operation
    }

    // Handle choices with multiple labels.
    for (auto const& choice : choicesWithMultipleLabels) {
        std::string action;
        for (auto const& label : labeling.getLabelsOfChoice(choice)) {
            if (!action.empty()) {
                action += ",";  // separate multiple labels with a comma
            }
            action += label;
        }
        choiceToAction[choice] = actionStrings.findOrPushBack(action);
    }
    return actionStrings.size();
}

template<typename TargetValueType>
void setGenericVector(storm::umb::GenericVector& target, std::ranges::input_range auto&& values) {
    using ValueType = std::ranges::range_value_t<decltype(values)>;
    if constexpr (std::is_same_v<ValueType, TargetValueType>) {
        target.template set<TargetValueType>(std::forward<decltype(values)>(values));
    } else {
        target.template set<TargetValueType>(storm::utility::vector::convertNumericVector<TargetValueType>(std::forward<decltype(values)>(values)));
    }
}

template<typename ValueType, typename TargetValueType>
void rewardToUmb(std::string const& rewardModelName, storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel,
                 storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::umb::UmbModel& umb) {
    STORM_LOG_ASSERT(umb.index.rewards(), "Model index must have rewards to store state labels.");
    auto const rewardIdentifier = umb.index.findRewardName(rewardModelName);
    STORM_LOG_ASSERT(rewardIdentifier.has_value(), "Reward '" << rewardModelName << "' not found in the model index.");
    auto& umbRewards = umb.rewards(true).value();
    STORM_LOG_ASSERT(!umbRewards.contains(*rewardIdentifier), "Reward '" << *rewardIdentifier << "' already exists in the umb model.");
    auto& rewardAnnotation = umbRewards[*rewardIdentifier];
    if (rewardModel.hasStateRewards()) {
        setGenericVector<TargetValueType>(rewardAnnotation.states.emplace().values, rewardModel.getStateRewardVector());
    }
    if (rewardModel.hasStateActionRewards()) {
        setGenericVector<TargetValueType>(rewardAnnotation.choices.emplace().values, rewardModel.getStateActionRewardVector());
    }
    if (rewardModel.hasTransitionRewards()) {
        std::vector<TargetValueType> branchRewards;
        branchRewards.reserve(transitionMatrix.getEntryCount());
        STORM_LOG_ASSERT(transitionMatrix.getRowCount() == rewardModel.getTransitionRewardMatrix().getRowCount(),
                         "The number of rows in the transition matrix and the reward model do not match.");
        for (uint64_t rowIndex = 0; rowIndex < transitionMatrix.getRowCount(); ++rowIndex) {
            auto const& transitionRow = transitionMatrix.getRow(rowIndex);
            auto const& rewardRow = rewardModel.getTransitionRewardMatrix().getRow(rowIndex);
            auto rewIt = rewardRow.begin();
            // Match transition branch entries with entries in the transition reward matrix (which might not have the same entries at the same columns)
            for (auto const& entry : transitionRow) {
                while (rewIt != rewardRow.end() && rewIt->getColumn() < entry.getColumn()) {
                    ++rewIt;
                }
                if (rewIt == rewardRow.end() || rewIt->getColumn() > entry.getColumn()) {
                    branchRewards.push_back(storm::utility::zero<TargetValueType>());
                } else {
                    STORM_LOG_ASSERT(rewIt->getColumn() == entry.getColumn(), "Unexpected column in reward model.");
                    branchRewards.push_back(storm::utility::convertNumber<TargetValueType>(rewIt->getValue()));
                }
            }
        }
        rewardAnnotation.branches.emplace().values.template set<TargetValueType>(std::move(branchRewards));
    }
}

template<typename ValueType, typename TargetValueType>
void playerIndicesToUmb(storm::models::sparse::Smg<ValueType> const& smg, auto& playerNames, auto& stateToPlayerIndices) {
    STORM_LOG_ASSERT(playerNames.empty() && stateToPlayerIndices.empty(), "Expected initially empty player names and indices.");
    auto const& origPlayerNamesToIndex = smg.getPlayerNamesToIndex();
    playerNames.resize(smg.getNumberOfPlayers());
    // We might have to insert names for unnamed players
    storm::storage::BitVector unnamedIndices(playerNames.size(), true);
    for (auto const& [name, index] : origPlayerNamesToIndex) {
        playerNames[index] = name;
        unnamedIndices.set(index, false);
    }
    auto freshPlayerName = [&origPlayerNamesToIndex](uint64_t i) {
        std::string name = "unnamed_player" + std::to_string(i);
        while (origPlayerNamesToIndex.contains(name)) {
            name += "_";
        }
        return name;
    };
    for (auto const unnamedIndex : unnamedIndices) {
        playerNames[unnamedIndex] = freshPlayerName(unnamedIndex);
    }
    stateToPlayerIndices.reserve(smg.getNumberOfStates());
    // Some states might not have a player. For example, states that were not explored during model construction.
    // These states are indicated by INVALID_PLAYER_INDEX. We assign these states to a fresh player at the end.
    auto const invalPlayerIndex = smg.getNumberOfPlayers();
    bool hasInvalidIndices = false;
    for (auto const& index : smg.getStatePlayerIndications()) {
        if (index == storm::storage::INVALID_PLAYER_INDEX) {
            stateToPlayerIndices.push_back(invalPlayerIndex);
            hasInvalidIndices = true;
        } else {
            STORM_LOG_ASSERT(index < smg.getNumberOfPlayers(), "Unexpected player index.");
            stateToPlayerIndices.push_back(index);
        }
    }
    if (hasInvalidIndices) {
        playerNames.push_back(freshPlayerName(invalPlayerIndex));
    }
}

template<typename ValueType>
storm::umb::Type getExportType(ExportOptions const& options) {
    using OptionType = ExportOptions::ValueType;
    using ExportType = storm::umb::Type;
    switch (options.valueType) {
        case OptionType::Default:
            if constexpr (std::is_same_v<ValueType, double>) {
                return ExportType::Double;
            } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
                return ExportType::Rational;
            } else if constexpr (std::is_same_v<ValueType, storm::Interval>) {
                return ExportType::DoubleInterval;
            } else {
                static_assert(std::is_same_v<ValueType, storm::RationalInterval>, "Unhandled value type");
                return ExportType::RationalInterval;
            }
        case OptionType::Double:
            return ExportType::Double;
        case OptionType::Rational:
            return ExportType::Rational;
        case OptionType::DoubleInterval:
            return ExportType::DoubleInterval;
        case OptionType::RationalInterval:
            return ExportType::RationalInterval;
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected value type.");
}

template<typename ValueType>
void setIndexInformation(storm::models::sparse::Model<ValueType> const& model, storm::umb::ModelIndex& index, ExportOptions const& options) {
    // No model (meta-)data to set at this point.
    // file-data:
    index.fileData.emplace();
    index.fileData->setCreationDateToNow();
    index.fileData->tool = "Storm";
    // Note: it's difficult to get the version of the tool because the storm library is not linked against storm-version-info

    // transition-system:
    auto& ts = index.transitionSystem;
    switch (model.getType()) {
        using enum storm::models::ModelType;
        using enum storm::umb::ModelIndex::TransitionSystem::Time;
        case Dtmc:
            ts.time = Discrete;
            ts.numPlayers = 0;
            break;
        case Ctmc:
            ts.time = Stochastic;
            ts.numPlayers = 0;
            break;
        case Mdp:
        case Pomdp:
            ts.time = Discrete;
            ts.numPlayers = 1;
            break;
        case MarkovAutomaton:
            ts.time = UrgentStochastic;
            ts.numPlayers = 1;
            break;
        case Smg:
            ts.time = Discrete;
            ts.numPlayers = ModelIndex::TransitionSystem::InvalidNumber;
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unexpected model type.");
    }
    ts.numStates = model.getNumberOfStates();
    ts.numInitialStates = model.getInitialStates().getNumberOfSetBits();
    ts.numChoices = model.getNumberOfChoices();
    ts.numChoiceActions = (model.hasChoiceLabeling() || model.hasChoiceOrigins()) ? ModelIndex::TransitionSystem::InvalidNumber
                                                                                  : 0;  // action count is only known after processing choice labeling/origins.
    ts.numBranches = model.getNumberOfTransitions();
    ts.numBranchActions = 0;
    ts.numObservations = model.isPartiallyObservable() ? ModelIndex::TransitionSystem::InvalidNumber : 0;  // observation count set later

    auto const exportType = getExportType<ValueType>(options);
    ts.branchProbabilityType = {exportType, defaultBitSize(exportType)};
    if (ts.time != storm::umb::ModelIndex::TransitionSystem::Time::Discrete) {
        ts.exitRateType = ts.branchProbabilityType;
    }

    // annotations:
    bool const hasRewards = model.hasRewardModel();
    bool const hasAps = model.getStateLabeling().getNumberOfLabels() >= 2 ||
                        (model.getStateLabeling().getNumberOfLabels() == 1 && !model.getStateLabeling().containsLabel("init"));
    if (hasRewards || hasAps) {
        index.annotations.emplace();
    }

    // rewards:
    if (hasRewards) {
        auto& rewards = index.rewards(true).value();
        for (auto const& [rewardModelName, rewardModel] : model.getRewardModels()) {
            auto identifier = umb::ModelIndex::Annotation::getValidIdentifierFromAlias(rewardModelName);
            STORM_LOG_THROW(!rewards.contains(identifier), storm::exceptions::WrongFormatException, "Reward id '" << identifier << "' already exists.");
            auto& rewardIndex = rewards[identifier];
            if (!rewardModelName.empty()) {
                rewardIndex.alias = rewardModelName;  // Don't introduce an alias for unnamed rewards. They don't have a nice name.
            }
            if (rewardModel.hasNegativeRewards()) {
                if (!rewardModel.hasPositiveRewards()) {
                    rewardIndex.upper = 0;
                }
            } else {
                rewardIndex.lower = 0;
            }
            using enum storm::umb::ModelIndex::Annotation::AppliesTo;
            if (rewardModel.hasStateRewards()) {
                rewardIndex.appliesTo.push_back(States);
            }
            if (rewardModel.hasStateActionRewards()) {
                rewardIndex.appliesTo.push_back(Choices);
            }
            if (rewardModel.hasTransitionRewards()) {
                rewardIndex.appliesTo.push_back(Branches);
            }
            rewardIndex.type = {exportType, defaultBitSize(exportType)};
        }
    }

    // aps:
    if (hasAps) {
        auto& aps = index.aps(true).value();
        for (auto const& label : model.getStateLabeling().getLabels()) {
            if (label == "init") {
                continue;
            }
            auto identifier = umb::ModelIndex::Annotation::getValidIdentifierFromAlias(label);
            STORM_LOG_THROW(!aps.contains(identifier), storm::exceptions::WrongFormatException, "AP with identifier '" << identifier << "' already exists.");
            auto& apIndex = aps[identifier];
            apIndex.alias = label;
            apIndex.type = {storm::umb::Type::Bool, defaultBitSize(storm::umb::Type::Bool)};
            apIndex.appliesTo.push_back(storm::umb::ModelIndex::Annotation::AppliesTo::States);
        }
    }

    // valuations:
    auto createDescription = [](storm::storage::sparse::ValuationsStorage const& valuations) {
        storm::storage::sparse::ValuationDescription descr;
        for (uint64_t classIndex = 0; classIndex < valuations.numClasses(); ++classIndex) {
            descr.classes.push_back(valuations.getClassDescription(classIndex));
        }
        if (valuations.hasStrings()) {
            descr.numStrings = valuations.numStrings();
        }
        return descr;
    };
    if (model.hasStateValuations()) {
        index.valuations.emplace().states = createDescription(model.getStateValuations().getStorage());
    }
    if (model.isPartiallyObservable()) {
        STORM_LOG_ASSERT(model.isOfType(storm::models::ModelType::Pomdp), "Only POMDPs are supported as partially observable models.");
        auto pomdp = model.template as<storm::models::sparse::Pomdp<ValueType>>();
        if (pomdp->hasObservationValuations()) {
            if (!index.valuations.has_value()) {
                index.valuations.emplace();
            }
            index.valuations->observations = createDescription(pomdp->getObservationValuations().getStorage());
        }
    }
}

template<typename ValueType, typename TargetValueType>
void sparseModelToUmb(storm::models::sparse::Model<ValueType> const& model, UmbModel& umbModel, ExportOptions const& options) {
    // Possibly canonicize POMDP
    if (options.canonicizePomdp && model.isPartiallyObservable()) {
        STORM_LOG_ASSERT(model.isOfType(storm::models::ModelType::Pomdp), "Only POMDPs are supported as partially observable models.");
        auto pomdp = model.template as<storm::models::sparse::Pomdp<ValueType>>();
        if (!pomdp->isCanonic()) {
            STORM_LOG_INFO("Canonicizing POMDP before UMB export.");
            storm::transformer::MakePOMDPCanonic<ValueType> makeCanonic(*pomdp);
            auto newOptions = options;
            newOptions.canonicizePomdp = false;  // avoid infinite recursion
            sparseModelToUmb<ValueType, TargetValueType>(*makeCanonic.transform(), umbModel, newOptions);
            return;
        }
    }

    // index
    setIndexInformation<ValueType>(model, umbModel.index, options);

    // initial states and APs
    umbModel.stateIsInitial = model.getInitialStates();
    stateLabelingToUmb(model.getStateLabeling(), umbModel);

    // Choice Actions
    if (options.allowChoiceOriginsAsActions && model.hasChoiceOrigins()) {
        STORM_LOG_WARN_COND(!options.allowChoiceLabelingAsActions || !model.hasChoiceLabeling(),
                            "Choice origins and choice labeling are both present but only choice origins will be used as actions for UMB export.");
        uint64_t const numActions = choiceOriginsToUmb(*model.getChoiceOrigins(), umbModel);
        umbModel.index.transitionSystem.numChoiceActions = numActions;
    } else if (options.allowChoiceLabelingAsActions && model.hasChoiceLabeling()) {
        uint64_t const numActions = choiceLabelingToUmb(model.getChoiceLabeling(), umbModel);
        umbModel.index.transitionSystem.numChoiceActions = numActions;
    }

    // Valuations
    if (model.hasStateValuations()) {
        umbModel.valuations.states = model.getStateValuations().getStorage().getRawUmbData();
    }
    if (model.isPartiallyObservable()) {
        STORM_LOG_ASSERT(model.isOfType(storm::models::ModelType::Pomdp), "Only POMDPs are supported as partially observable models.");
        auto pomdp = model.template as<storm::models::sparse::Pomdp<ValueType>>();
        if (pomdp->hasObservationValuations()) {
            umbModel.valuations.observations = pomdp->getObservationValuations().getStorage().getRawUmbData();
        }
    }

    // Transition matrix
    using enum storm::models::ModelType;
    bool normalize = model.isOfType(Ctmc);
    if (!storm::NumberTraits<ValueType>::IsExact && storm::NumberTraits<TargetValueType>::IsExact) {
        STORM_LOG_WARN("Translating from non-exact to exact model representation. This may lead to rounding errors.");
        normalize = true;
    }
    transitionMatrixToUmb<ValueType, TargetValueType>(model.getTransitionMatrix(), umbModel, normalize);

    // rewards
    for (auto const& [name, rewardModel] : model.getRewardModels()) {
        rewardToUmb<ValueType, TargetValueType>(name, rewardModel, model.getTransitionMatrix(), umbModel);
    }

    // Model type specific components
    if (model.isOfType(Ctmc)) {
        auto const& ctmc = *model.template as<storm::models::sparse::Ctmc<ValueType>>();
        setGenericVector<TargetValueType>(umbModel.stateToExitRate, ctmc.getExitRateVector());
    } else if (model.isOfType(MarkovAutomaton)) {
        auto const& ma = *model.template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
        umbModel.stateIsMarkovian = ma.getMarkovianStates();
        setGenericVector<TargetValueType>(umbModel.stateToExitRate, ma.getExitRates());
    } else if (model.isOfType(Pomdp)) {
        auto const& pomdp = *model.template as<storm::models::sparse::Pomdp<ValueType>>();
        umbModel.index.transitionSystem.numObservations = pomdp.getNrObservations();
        umbModel.index.transitionSystem.observationsApplyTo = storm::umb::ModelIndex::TransitionSystem::ObservationsApplyTo::States;
        umbModel.stateObservations.emplace().values.emplace(pomdp.getObservations().begin(), pomdp.getObservations().end());
    } else if (model.isOfType(Smg)) {
        auto const& smg = *model.template as<storm::models::sparse::Smg<ValueType>>();
        playerIndicesToUmb<ValueType, TargetValueType>(smg, umbModel.index.transitionSystem.playerNames.emplace(), umbModel.stateToPlayer.emplace());
        umbModel.index.transitionSystem.numPlayers = umbModel.index.transitionSystem.playerNames->size();
        STORM_LOG_WARN_COND(umbModel.index.transitionSystem.numPlayers >= 2,
                            "Exporting SMG to UMB with zero or one players. The model will be recognized as MDP or DTMC on import.");
    } else {
        STORM_LOG_THROW(model.isOfType(Dtmc) || model.isOfType(Mdp), storm::exceptions::NotSupportedException,
                        "Unexpected model type for UMB export: " << model.getType());
    }
}

}  // namespace detail

template<typename ValueType>
storm::umb::UmbModel sparseModelToUmb(storm::models::sparse::Model<ValueType> const& model, ExportOptions const& options) {
    storm::umb::UmbModel umbModel;
    using enum ExportOptions::ValueType;
    switch (options.valueType) {
        case Default:
            detail::sparseModelToUmb<ValueType, ValueType>(model, umbModel, options);
            break;
        case Double:
            detail::sparseModelToUmb<ValueType, double>(model, umbModel, options);
            break;
        case Rational:
            detail::sparseModelToUmb<ValueType, storm::RationalNumber>(model, umbModel, options);
            break;
        case DoubleInterval:
            detail::sparseModelToUmb<ValueType, storm::Interval>(model, umbModel, options);
            break;
        case RationalInterval:
            detail::sparseModelToUmb<ValueType, storm::RationalInterval>(model, umbModel, options);
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unexpected value type.");
    }
    STORM_LOG_ASSERT(umbModel.validate(std::cout), "Created umb model is not valid.");
    return umbModel;
}

template storm::umb::UmbModel sparseModelToUmb<double>(storm::models::sparse::Model<double> const& model, ExportOptions const& options);
template storm::umb::UmbModel sparseModelToUmb<storm::RationalNumber>(storm::models::sparse::Model<storm::RationalNumber> const& model,
                                                                      ExportOptions const& options);
template storm::umb::UmbModel sparseModelToUmb<storm::Interval>(storm::models::sparse::Model<storm::Interval> const& model, ExportOptions const& options);
template storm::umb::UmbModel sparseModelToUmb<storm::RationalInterval>(storm::models::sparse::Model<storm::RationalInterval> const& model,
                                                                        ExportOptions const& options);
}  // namespace storm::umb