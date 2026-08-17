#include "storm-pomdp/transformer/RewardBoundUnfolder.h"

#include <queue>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/AtomicLabelFormula.h"
#include "storm/logic/BinaryBooleanStateFormula.h"
#include "storm/logic/BoundedUntilFormula.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/logic/ProbabilityOperatorFormula.h"
#include "storm/logic/UntilFormula.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/builder.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"

namespace storm::pomdp::transformer {
namespace detail {
// Typedefs for readability
using StateIdType = uint64_t;
using ChoiceIdType = uint64_t;  // global choices, i.e., each (state-action) pair has a unique id
using EpochType = std::vector<int64_t>;
using StateEpochPair = std::pair<StateIdType, EpochType>;

/*!
 * Contains information for a single dimension of the unfolding
 */
template<typename ValueType>
struct Dimension {
    enum class Relation { greater, lessEqual } const relation{Relation::lessEqual};
    int64_t const threshold;
    int64_t const levelWidth;
    std::string const freshLevelRewardOrActiveLabelName;  // A name for the newly introduced level reward or active label
    uint64_t const originalFormulaDimension;
    storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel;
};

/*!
 * Extracts the dimension information from the input formula.
 * Also performs various sanity/compatibility checks.
 */
template<typename ValueType>
std::vector<Dimension<ValueType>> extractDimensions(storm::models::sparse::Model<ValueType> const& model,
                                                    storm::logic::BoundedUntilFormula const& boundedUntilFormula, std::vector<uint64_t> const& levelWidths) {
    STORM_LOG_THROW(boundedUntilFormula.getRightSubformula().isInFragment(storm::logic::propositional()), storm::exceptions::NotSupportedException,
                    "Only propositional right subformulas are supported.");  // Temporal sub-formulas are potentially not preserved by the construction
    STORM_LOG_THROW(boundedUntilFormula.getLeftSubformula().isInFragment(storm::logic::propositional()), storm::exceptions::NotSupportedException,
                    "Only propositional left subformulas are supported.");  // Temporal sub-formulas are potentially not preserved by the construction
    std::vector<Dimension<ValueType>> dimensions;
    for (uint64_t formulaDim = 0; formulaDim < boundedUntilFormula.getDimension(); ++formulaDim) {
        int64_t const levelWidth = formulaDim < levelWidths.size() ? levelWidths[formulaDim] : 0;
        auto const& tbr = boundedUntilFormula.getTimeBoundReference(formulaDim);
        STORM_LOG_THROW(tbr.hasRewardModelName(), storm::exceptions::NotSupportedException,
                        "The reward model for bound reference " << formulaDim << " has no name.");
        STORM_LOG_THROW(
            !tbr.hasRewardAccumulation(), storm::exceptions::NotSupportedException,
            "The reward model for bound reference " << formulaDim << " has non-trivial reward accumulation which is not supported in this context.");
        auto const& rewardModel = model.getRewardModel(tbr.getRewardModelName());
        // Note: computation of successor epoch is slightly more involved with transition rewards which is why we do not support them for now
        STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                        "The reward model for bound reference " << formulaDim << " uses transition rewards. These are currently unsupported.");
        // All assigned rewards need to be integer (might support rational via scaling, but its unclear how to scale levelWidth)
        STORM_LOG_THROW(!rewardModel.hasStateRewards() || std::all_of(rewardModel.getStateRewardVector().begin(), rewardModel.getStateRewardVector().end(),
                                                                      storm::utility::isInteger<ValueType>),
                        storm::exceptions::NotSupportedException, "State rewards in reward model " << tbr.getRewardModelName() << " are not integers.");
        STORM_LOG_THROW(
            !rewardModel.hasStateActionRewards() || std::all_of(rewardModel.getStateActionRewardVector().begin(),
                                                                rewardModel.getStateActionRewardVector().end(), storm::utility::isInteger<ValueType>),
            storm::exceptions::NotSupportedException, "State action rewards in reward model " << tbr.getRewardModelName() << " are not integers.");
        // The finite epoch abstraction assumes that accumulated rewards never decrease. In particular, a negative-reward cycle would
        // increase an epoch indefinitely and therefore make the unfolding infinite.
        STORM_LOG_THROW(!rewardModel.hasNegativeRewards(), storm::exceptions::NotSupportedException,
                        "Reward model " << tbr.getRewardModelName() << " contains negative rewards. These are currently unsupported.");

        // Helper function to generate fresh identifiers (either for level reward or active label)
        auto getFreshIdentifier = [&]() {
            std::string prefix = "dim" + std::to_string(dimensions.size()) + (levelWidth == 0 ? "_active" : "_levelReward");
            auto identifier = prefix;
            for (uint64_t i = 0; (levelWidth == 0 ? model.hasLabel(identifier) : model.hasRewardModel(identifier)); ++i) {
                identifier = prefix + "_" + std::to_string(i);
            }
            return identifier;
        };

        if (boundedUntilFormula.hasUpperBound(formulaDim)) {
            STORM_LOG_THROW(boundedUntilFormula.hasIntegerUpperBound(formulaDim), storm::exceptions::NotSupportedException,
                            "Bound " << formulaDim << " is not an integer");  // might support rational via scaling (how to scale levelWidth?)
            int64_t const threshold =
                boundedUntilFormula.getUpperBound(formulaDim).evaluateAsInt() - (boundedUntilFormula.isUpperBoundStrict(formulaDim) ? 1ul : 0ul);
            STORM_LOG_THROW(threshold >= 0, storm::exceptions::NotSupportedException,
                            "Upper reward bound in dimension " << formulaDim << " is not satisfiable.");
            dimensions.push_back(
                Dimension<ValueType>{Dimension<ValueType>::Relation::lessEqual, threshold, levelWidth, getFreshIdentifier(), formulaDim, rewardModel});
        }
        if (boundedUntilFormula.hasLowerBound(formulaDim)) {
            STORM_LOG_THROW(boundedUntilFormula.hasIntegerLowerBound(formulaDim), storm::exceptions::NotSupportedException,
                            "Bound " << formulaDim << " is not an integer");  // might support rational via scaling (how to scale levelWidth?)
            int64_t const threshold =
                boundedUntilFormula.getLowerBound(formulaDim).evaluateAsInt() - (boundedUntilFormula.isLowerBoundStrict(formulaDim) ? 0ul : 1ul);
            STORM_LOG_THROW(threshold >= 0, storm::exceptions::NotSupportedException,
                            "Lower reward bound in dimension " << formulaDim << " is not satisfiable.");
            dimensions.push_back(
                Dimension<ValueType>{Dimension<ValueType>::Relation::greater, threshold, levelWidth, getFreshIdentifier(), formulaDim, rewardModel});
        }
    }
    return dimensions;
}

/*!
 * Helper function to compute the modulos
 * @note operator% is not the modulos for negative numerators. E.g. -1 % 3 = -1, but mod(-1, 3) = 2
 */
int64_t mod(int64_t a, int64_t b) {
    return (a % b + b) % b;
}

template<typename ValueType>
EpochType computeInitialEpoch(std::vector<Dimension<ValueType>> const& dimensions) {
    EpochType epoch;
    epoch.reserve(dimensions.size());
    for (auto const& dim : dimensions) {
        if (dim.levelWidth == 0) {
            epoch.push_back(dim.threshold);
        } else {
            epoch.push_back(mod(dim.threshold, dim.levelWidth));
        }
    }
    return epoch;
}

template<typename ValueType>
EpochType computeSuccessorEpoch(StateIdType currentState, EpochType const& currentEpoch, ChoiceIdType choice,
                                std::vector<Dimension<ValueType>> const& dimensions) {
    EpochType successorEpoch = currentEpoch;
    for (auto eIt = successorEpoch.begin(); auto const& dim : dimensions) {
        auto const& rew = dim.rewardModel;
        ValueType const reward = (rew.hasStateRewards() ? rew.getStateReward(currentState) : storm::utility::zero<ValueType>()) +
                                 (rew.hasStateActionRewards() ? rew.getStateActionReward(choice) : storm::utility::zero<ValueType>());
        *eIt -= storm::utility::convertNumber<uint64_t>(reward);
        ++eIt;
    }
    return successorEpoch;
}

template<typename ValueType, typename LevelRewardSetterType>
void computeLevelReward(EpochType const& successorEpoch, std::vector<Dimension<ValueType>> const& dimensions, LevelRewardSetterType const& setLevelReward) {
    for (uint64_t dimIndex = 0; dimIndex < dimensions.size(); ++dimIndex) {
        auto const lvlWidth = dimensions[dimIndex].levelWidth;
        if (lvlWidth != 0) {
            auto const reward = storm::utility::ceil<ValueType>(storm::utility::convertNumber<ValueType>(-successorEpoch[dimIndex]) /
                                                                storm::utility::convertNumber<ValueType>(lvlWidth));
            STORM_LOG_ASSERT(reward >= storm::utility::zero<ValueType>(), "Expected non-negative level reward, got "
                                                                              << reward << ". Succ epoch is " << successorEpoch[dimIndex] << " and lvlWidth is "
                                                                              << lvlWidth);
            // The level reward is the smallest number of times we can add lvlWidth to the epoch entry to get a non-negative value
            setLevelReward(dimIndex, reward);
        }
    }
}

template<typename ValueType>
void applyEpochAbstraction(EpochType& epoch, std::vector<Dimension<ValueType>> const& dimensions) {
    for (auto eIt = epoch.begin(); auto const& dim : dimensions) {
        if (*eIt < 0) {
            if (dim.levelWidth == 0) {
                // bottom epoch. If this is an upper bound, we can set all epoch entries to -1
                if (dim.relation == Dimension<ValueType>::Relation::lessEqual) {
                    epoch.assign(epoch.size(), -1);
                    return;
                }
                *eIt = -1;  // bottom epoch
            } else {
                *eIt = mod(*eIt, dim.levelWidth);  // level abstraction
            }
        }
        ++eIt;
    }
}

template<typename ValueType, typename ActiveDimensionSetterType>
void computeActiveDimensions(EpochType const& epoch, std::vector<Dimension<ValueType>> const& dimensions, ActiveDimensionSetterType const& setActiveDimension) {
    for (uint64_t dimIndex = 0; dimIndex < dimensions.size(); ++dimIndex) {
        auto const& dim = dimensions[dimIndex];
        if (dim.levelWidth == 0) {
            bool const inBottomEpoch = epoch[dimIndex] < 0;
            if ((dim.relation == Dimension<ValueType>::Relation::greater) ? inBottomEpoch : !inBottomEpoch) {
                setActiveDimension(dimIndex);
            }
        }
    }
}

struct StateEpochCollector {
    std::pair<StateIdType, bool> getOrAddStateEpoch(StateIdType state, EpochType epoch) {
        auto [idIt, isNewEntry] = stateEpochPairToId.try_emplace(std::make_pair(state, std::forward<EpochType>(epoch)), idToStateEpochPair.size());
        if (isNewEntry) {
            idToStateEpochPair.push_back(idIt->first);
        }
        return {idIt->second, isNewEntry};
    }
    std::map<StateEpochPair, StateIdType> stateEpochPairToId;
    std::vector<StateEpochPair> idToStateEpochPair;
};

template<typename ValueType>
struct ExplorationResult {
    storm::storage::SparseMatrix<ValueType> matrix;
    storm::storage::BitVector initialStates;
    std::vector<std::vector<ValueType>> levelRewardsForDimensions;
    std::vector<storm::storage::BitVector> activeDimensions;
    std::vector<StateEpochPair> stateIdToOgStateEpochPair;
};

template<typename ValueType>
ExplorationResult<ValueType> exploreUnfolding(storm::models::sparse::Model<ValueType> const& model, std::vector<Dimension<ValueType>> const& dimensions) {
    auto const& ogMatrix = model.getTransitionMatrix();
    bool const hasRowGroups = !ogMatrix.hasTrivialRowGrouping();

    // Initialize data objects that we are going to fill
    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(0, 0, 0, 0, hasRowGroups);
    storm::storage::BitVector initialStateIds;
    std::vector<std::vector<ValueType>> levelRewardsForDimensions(dimensions.size());
    std::vector<storm::storage::BitVector> activeDimensions(dimensions.size());

    // Exploration data
    std::queue<StateIdType> queue;
    StateEpochCollector stateEpochCollector;

    // Auxiliary function that is called whenever a (state-epoch) pair is found
    auto processNewStateEpoch = [&queue, &stateEpochCollector](StateIdType stateId, EpochType epoch) {
        auto [newId, isNewEntry] = stateEpochCollector.getOrAddStateEpoch(stateId, epoch);
        if (isNewEntry) {
            queue.push(newId);
        }
        return newId;
    };

    // Fill queue with initial state(s)
    auto initEpoch = computeInitialEpoch(dimensions);
    for (auto const& initState : model.getInitialStates()) {
        auto initId = processNewStateEpoch(initState, initEpoch);
        initialStateIds.grow(initId + 1);
        initialStateIds.set(initId);
    }

    // Start BFS exploration
    uint64_t numChoicesInUnfolding = 0;
    while (!queue.empty()) {
        // Pop from queue
        auto const currentStateEpochId = queue.front();
        queue.pop();
        auto [currentState, currentEpoch] = stateEpochCollector.idToStateEpochPair[currentStateEpochId];
        // Can't take currentEpoch by reference because it might be invalidated when finding new epochs

        // Compute active dimensions
        computeActiveDimensions(currentEpoch, dimensions, [&activeDimensions, &currentStateEpochId](uint64_t dimIndex) {
            activeDimensions[dimIndex].grow(currentStateEpochId + 1);
            activeDimensions[dimIndex].set(currentStateEpochId);
        });

        // Explore successors
        if (hasRowGroups) {
            matrixBuilder.newRowGroup(numChoicesInUnfolding);
        }
        for (auto const choice : ogMatrix.getRowGroupIndices(currentState)) {
            // Since we (for now) assume models without transition branch rewards, the successor epoch does not depend on the state that we reach
            auto successorEpoch = computeSuccessorEpoch(currentState, currentEpoch, choice, dimensions);
            // Compute the level rewards for the dimensions with level widths (needs to be done before abstracting away that information!)
            computeLevelReward(successorEpoch, dimensions, [&levelRewardsForDimensions](uint64_t dimIndex, ValueType levelReward) {
                levelRewardsForDimensions[dimIndex].push_back(levelReward);
            });
            // Abstract the successor epoch. Ensures that we only reach a finite set of epochs.
            applyEpochAbstraction(successorEpoch, dimensions);
            for (auto const& entry : ogMatrix.getRow(choice)) {
                auto successorInUnfolding = processNewStateEpoch(entry.getColumn(), successorEpoch);
                STORM_LOG_ASSERT(entry.getValue() > storm::utility::zero<ValueType>(), "Transition probabilities must be positive.");
                matrixBuilder.addNextValue(numChoicesInUnfolding, successorInUnfolding, entry.getValue());
            }
            ++numChoicesInUnfolding;
        }
    }
    auto matrix = matrixBuilder.build(numChoicesInUnfolding, stateEpochCollector.idToStateEpochPair.size(), stateEpochCollector.idToStateEpochPair.size());
    STORM_LOG_ASSERT(matrix.isProbabilistic(storm::utility::zero<ValueType>()), "Resulting transition matrix is not a probability matrix.");
    STORM_LOG_ASSERT(matrix.hasOnlyPositiveEntries(), "Resulting transition matrix has non-positive entries.");

    return ExplorationResult<ValueType>{std::move(matrix), std::move(initialStateIds), std::move(levelRewardsForDimensions), std::move(activeDimensions),
                                        std::move(stateEpochCollector.idToStateEpochPair)};
}

template<typename ValueType>
storm::storage::sparse::ModelComponents<ValueType> constructComponents(storm::models::sparse::Model<ValueType> const& originalModel,
                                                                       std::vector<Dimension<ValueType>> const& dimensions,
                                                                       std::set<std::string> const& preservedRewardModels,
                                                                       ExplorationResult<ValueType>&& explorationResult) {
    uint64_t const numStates = explorationResult.matrix.getColumnCount();
    uint64_t const numChoices = explorationResult.matrix.getRowCount();

    storm::storage::sparse::ModelComponents<ValueType> components(std::move(explorationResult.matrix), storm::models::sparse::StateLabeling(numStates));

    // Helper functions to iterate over all states/choices. Unfolding states/choices are called in ascending order of their ids.
    auto forEachState = [&explorationResult, &numStates](auto const& f) {
        for (uint64_t unfoldingState = 0; unfoldingState < numStates; ++unfoldingState) {
            f(unfoldingState, explorationResult.stateIdToOgStateEpochPair[unfoldingState].first);
        }
    };
    auto forEachChoice = [&originalModel, &components, &explorationResult, &numStates](auto const& f) {
        for (uint64_t unfoldingState = 0; unfoldingState < numStates; ++unfoldingState) {
            uint64_t const originalState = explorationResult.stateIdToOgStateEpochPair[unfoldingState].first;
            STORM_LOG_ASSERT(components.transitionMatrix.getRowGroupSize(unfoldingState) == originalModel.getTransitionMatrix().getRowGroupSize(originalState),
                             "Number of choices in unfolding and original model differ.");
            auto origChoice = originalModel.getTransitionMatrix().getRowGroupIndices()[originalState];
            for (auto unfoldingChoice : components.transitionMatrix.getRowGroupIndices(unfoldingState)) {
                f(unfoldingChoice, origChoice);
                ++origChoice;
            }
        }
    };

    // Create the state labeling
    explorationResult.initialStates.resize(numStates);
    components.stateLabeling.addLabel("init", std::move(explorationResult.initialStates));
    for (auto label : originalModel.getStateLabeling().getLabels()) {
        if (label == "init") {
            continue;
        }
        auto const& originalLabel = originalModel.getStateLabeling().getStates(label);
        storm::storage::BitVector newLabel(numStates, false);
        forEachState([&newLabel, &originalLabel](uint64_t unfoldingState, StateIdType originalState) {
            if (originalLabel.get(originalState)) {
                newLabel.set(unfoldingState);
            }
        });
        components.stateLabeling.addLabel(label, std::move(newLabel));
    }
    for (uint64_t dimIndex = 0; dimIndex < dimensions.size(); ++dimIndex) {
        if (dimensions[dimIndex].levelWidth == 0) {
            explorationResult.activeDimensions[dimIndex].resize(numStates);
            components.stateLabeling.addLabel(dimensions[dimIndex].freshLevelRewardOrActiveLabelName, std::move(explorationResult.activeDimensions[dimIndex]));
        }
    }

    // Create the (optional) choice labeling
    if (originalModel.hasChoiceLabeling()) {
        components.choiceLabeling.emplace(numChoices);
        for (auto label : originalModel.getChoiceLabeling().getLabels()) {
            auto const& originalLabel = originalModel.getChoiceLabeling().getChoices(label);
            storm::storage::BitVector newLabel(numChoices, false);
            forEachChoice([&newLabel, &originalLabel](uint64_t unfoldingChoice, uint64_t originalChoice) {
                if (originalLabel.get(originalChoice)) {
                    newLabel.set(unfoldingChoice);
                }
            });
            components.choiceLabeling->addLabel(label, std::move(newLabel));
        }
    }

    // Create the reward models
    for (auto const& [name, rewmodel] : originalModel.getRewardModels()) {
        if (!preservedRewardModels.contains(name)) {
            continue;
        }
        STORM_LOG_THROW(!rewmodel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                        "Transition rewards are currently not supported in this context.");
        std::optional<std::vector<ValueType>> stateRewards, stateActionRewards;
        if (rewmodel.hasStateRewards()) {
            stateRewards.emplace();
            stateRewards->reserve(numStates);
            forEachState(
                [&stateRewards, &rewmodel](uint64_t _, StateIdType originalState) { stateRewards->push_back(rewmodel.getStateReward(originalState)); });
        }
        if (rewmodel.hasStateActionRewards()) {
            stateActionRewards.emplace();
            stateActionRewards->reserve(numChoices);
            forEachChoice([&stateActionRewards, &rewmodel](auto _, StateIdType originalChoice) {
                stateActionRewards->push_back(rewmodel.getStateActionReward(originalChoice));
            });
        }
        components.rewardModels.emplace(name, storm::models::sparse::StandardRewardModel<ValueType>{std::move(stateRewards), std::move(stateActionRewards)});
    }
    for (uint64_t dimIndex = 0; dimIndex < dimensions.size(); ++dimIndex) {
        if (dimensions[dimIndex].levelWidth != 0) {
            explorationResult.levelRewardsForDimensions[dimIndex].shrink_to_fit();
            components.rewardModels.emplace(
                dimensions[dimIndex].freshLevelRewardOrActiveLabelName,
                storm::models::sparse::StandardRewardModel<ValueType>{std::nullopt, std::move(explorationResult.levelRewardsForDimensions[dimIndex])});
        }
    }

    // Create POMDP-specific components
    if (originalModel.isOfType(storm::models::ModelType::Pomdp)) {
        auto const& pomdp = static_cast<storm::models::sparse::Pomdp<ValueType> const&>(originalModel);
        std::vector<uint32_t> unfoldingStateObservations;
        unfoldingStateObservations.reserve(numStates);
        forEachState([&unfoldingStateObservations, &pomdp](auto _, StateIdType originalState) {
            unfoldingStateObservations.push_back(pomdp.getObservation(originalState));
        });
        components.observabilityClasses = std::move(unfoldingStateObservations);
        STORM_LOG_WARN_COND(!pomdp.hasObservationValuations(), "Observation valuations are dropped.");

    } else {
        STORM_LOG_THROW(originalModel.isOfType(storm::models::ModelType::Mdp), storm::exceptions::NotSupportedException,
                        "Unfolding is only supported POMDP and MDP models right now.");  // DTMCs might work, too?
    }
    return components;
}

template<typename ValueType>
std::shared_ptr<storm::logic::Formula> constructFormula(storm::logic::BoundedUntilFormula boundedUntilFormula,
                                                        std::vector<Dimension<ValueType>> const& dimensions) {
    // Construct a new (bounded or unbounded) until formula
    auto lhs = boundedUntilFormula.getLeftSubformula().clone();
    auto rhs = boundedUntilFormula.getRightSubformula().clone();
    std::vector<boost::optional<storm::logic::TimeBound>> lowerBounds, upperBounds;
    std::vector<storm::logic::TimeBoundReference> timeBoundReferences;
    STORM_LOG_ASSERT(boundedUntilFormula.getDimension() > 0, "did not expect a 0-dimensional formula.");
    auto const& exprManager =
        boundedUntilFormula.hasLowerBound(0) ? boundedUntilFormula.getLowerBound(0).getManager() : boundedUntilFormula.getUpperBound(0).getManager();
    for (auto const& dim : dimensions) {
        if (dim.levelWidth == 0) {
            // Dimension is unfolded up to the threshold. Inject the label where the bound is active
            auto activeFormula = std::make_shared<storm::logic::AtomicLabelFormula>(dim.freshLevelRewardOrActiveLabelName);
            rhs = std::make_shared<storm::logic::BinaryBooleanStateFormula>(storm::logic::BinaryBooleanOperatorType::And, std::move(rhs),
                                                                            std::move(activeFormula));
            if (dim.relation == Dimension<ValueType>::Relation::lessEqual) {
                auto activeFormula = std::make_shared<storm::logic::AtomicLabelFormula>(dim.freshLevelRewardOrActiveLabelName);
                lhs = std::make_shared<storm::logic::BinaryBooleanStateFormula>(storm::logic::BinaryBooleanOperatorType::And, std::move(lhs),
                                                                                std::move(activeFormula));
            }
        } else {
            // Dimension is unfolded up to level width. Add a reward bound addressing the level jump reward
            auto const lvlthreshold = exprManager.integer(storm::utility::convertNumber<int64_t>(storm::utility::floor<ValueType>(
                storm::utility::convertNumber<ValueType>(dim.threshold) / storm::utility::convertNumber<ValueType>(dim.levelWidth))));
            if (dim.relation == Dimension<ValueType>::Relation::lessEqual) {
                upperBounds.push_back(storm::logic::TimeBound{false, lvlthreshold});
                lowerBounds.emplace_back();
            } else {
                upperBounds.emplace_back();
                lowerBounds.push_back(storm::logic::TimeBound{true, lvlthreshold});
            }
            timeBoundReferences.emplace_back(dim.freshLevelRewardOrActiveLabelName);
        }
    }

    // Construct the new formula

    if (timeBoundReferences.empty()) {
        return std::make_shared<storm::logic::UntilFormula>(lhs, rhs);
    } else {
        return std::make_shared<storm::logic::BoundedUntilFormula>(lhs, rhs, std::move(lowerBounds), std::move(upperBounds), std::move(timeBoundReferences));
    }
}

}  // namespace detail

template<typename ValueType>
RewardBoundUnfolder<ValueType>::ReturnType RewardBoundUnfolder<ValueType>::transform(storm::models::sparse::Model<ValueType> const& model,
                                                                                     storm::logic::Formula const& formula, UnfoldingOptions const& options) {
    if (formula.isProbabilityOperatorFormula()) {
        // Recursive call with subformula
        auto const& opFormula = formula.asProbabilityOperatorFormula();
        auto result = transform(model, opFormula.getSubformula(), options);
        result.formula = std::make_shared<storm::logic::ProbabilityOperatorFormula>(std::move(result.formula), opFormula.getOperatorInformation());
        return result;
    }

    // Process with boundedUntilFormula
    STORM_LOG_THROW(formula.isBoundedUntilFormula(), storm::exceptions::InvalidPropertyException, "Unexpected formula type." << formula << ".");
    auto const& boundedUntilFormula = formula.asBoundedUntilFormula();
    auto const dimensions = detail::extractDimensions(model, boundedUntilFormula, options.levelWidths);
    auto explorationResult = detail::exploreUnfolding(model, dimensions);
    auto components = detail::constructComponents(model, dimensions, options.preservedRewardModels, std::move(explorationResult));
    auto unfoldedModel = storm::utility::builder::buildModelFromComponents(model.getType(), std::move(components));

    if (unfoldedModel->isOfType(storm::models::ModelType::Pomdp)) {
        auto& unfoldedPomdp = static_cast<storm::models::sparse::Pomdp<ValueType>&>(*unfoldedModel);
        auto const& originalPomdp = static_cast<storm::models::sparse::Pomdp<ValueType> const&>(model);
        unfoldedPomdp.setIsCanonic(originalPomdp.isCanonic());
    }

    return {std::move(unfoldedModel), std::move(detail::constructFormula(boundedUntilFormula, dimensions))};
}

template class RewardBoundUnfolder<double>;
template class RewardBoundUnfolder<storm::RationalNumber>;

}  // namespace storm::pomdp::transformer
