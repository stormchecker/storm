#include "storm-gamebased-ar/abstraction/MenuGameRefiner.h"

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm-gamebased-ar/abstraction/ExplicitQualitativeGameResultMinMax.h"
#include "storm-gamebased-ar/abstraction/ExplicitQuantitativeResultMinMax.h"
#include "storm-gamebased-ar/abstraction/MenuGameAbstractor.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/solver/MathsatSmtSolver.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/ExplicitGameStrategyPair.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/Odd.h"
#include "storm/utility/dd.h"
#include "storm/utility/shortestPaths.h"

namespace storm::gbar {
namespace abstraction {

using storm::settings::modules::AbstractionSettings;

RefinementPredicates::RefinementPredicates(Source const& source, std::vector<storm::expressions::Expression> const& predicates)
    : source(source), predicates(predicates) {
    // Intentionally left empty.
}

RefinementPredicates::Source RefinementPredicates::getSource() const {
    return source;
}

std::vector<storm::expressions::Expression> const& RefinementPredicates::getPredicates() const {
    return predicates;
}

void RefinementPredicates::addPredicates(std::vector<storm::expressions::Expression> const& newPredicates) {
    this->predicates.insert(this->predicates.end(), newPredicates.begin(), newPredicates.end());
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicMostProbablePathsResult<Type, ValueType>::SymbolicMostProbablePathsResult(storm::dd::Add<Type, ValueType> const& maxProbabilities,
                                                                                  storm::dd::Bdd<Type> const& spanningTree)
    : maxProbabilities(maxProbabilities), spanningTree(spanningTree) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type>
struct PivotStateCandidatesResult {
    storm::dd::Bdd<Type> reachableTransitionsMin;
    storm::dd::Bdd<Type> reachableTransitionsMax;
    storm::dd::Bdd<Type> pivotStates;
};

template<storm::dd::DdType Type, typename ValueType>
SymbolicPivotStateResult<Type, ValueType>::SymbolicPivotStateResult(
    storm::dd::Bdd<Type> const& pivotState, storm::OptimizationDirection fromDirection,
    boost::optional<SymbolicMostProbablePathsResult<Type, ValueType>> const& symbolicMostProbablePathsResult)
    : pivotState(pivotState), fromDirection(fromDirection), symbolicMostProbablePathsResult(symbolicMostProbablePathsResult) {
    // Intentionally left empty.
}

template<storm::dd::DdType Type, typename ValueType>
MenuGameRefiner<Type, ValueType>::MenuGameRefiner(MenuGameAbstractor<Type, ValueType>& abstractor, std::unique_ptr<storm::solver::SmtSolver>&& smtSolver,
                                                  MenuGameRefinerOptions const& options)
    : abstractor(abstractor),
      useInterpolation(storm::settings::getModule<storm::settings::modules::AbstractionSettings>().isUseInterpolationSet()),
      splitAll(false),
      splitPredicates(false),
      rankPredicates(false),
      addedAllGuardsFlag(false),
      refinementPredicatesToInject(options.refinementPredicates),
      pivotSelectionHeuristic(),
      splitter(),
      equivalenceChecker(std::move(smtSolver)) {
    auto const& abstractionSettings = storm::settings::getModule<storm::settings::modules::AbstractionSettings>();

    pivotSelectionHeuristic = abstractionSettings.getPivotSelectionHeuristic();
    AbstractionSettings::SplitMode splitMode = storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getSplitMode();
    splitAll = splitMode == AbstractionSettings::SplitMode::All;
    splitPredicates = splitMode == AbstractionSettings::SplitMode::NonGuard;
    rankPredicates = abstractionSettings.isRankRefinementPredicatesSet();
    addPredicatesEagerly = abstractionSettings.isUseEagerRefinementSet();

    equivalenceChecker.addConstraints(abstractor.getAbstractionInformation().getConstraints());
    if (abstractionSettings.isAddAllGuardsSet()) {
        std::vector<storm::expressions::Expression> guards;

        std::pair<uint64_t, uint64_t> player1Choices = this->abstractor.get().getPlayer1ChoiceRange();
        for (uint64_t index = player1Choices.first; index < player1Choices.second; ++index) {
            storm::expressions::Expression guard = this->abstractor.get().getGuard(index);
            if (!guard.isTrue() && !guard.isFalse()) {
                guards.push_back(guard);
            }
        }
        performRefinement(createGlobalRefinement(preprocessPredicates(guards, RefinementPredicates::Source::InitialGuard)));

        this->abstractor.get().notifyGuardsArePredicates();
        addedAllGuardsFlag = true;
    }
    if (abstractionSettings.isAddAllInitialExpressionsSet()) {
        storm::expressions::Expression initialExpression = this->abstractor.get().getInitialExpression();
        performRefinement(createGlobalRefinement(preprocessPredicates({initialExpression}, RefinementPredicates::Source::InitialExpression)));
    }
}

template<storm::dd::DdType Type, typename ValueType>
void MenuGameRefiner<Type, ValueType>::refine(std::vector<storm::expressions::Expression> const& predicates, bool allowInjection) const {
    performRefinement(createGlobalRefinement(preprocessPredicates(predicates, RefinementPredicates::Source::Manual)), allowInjection);
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicMostProbablePathsResult<Type, ValueType> getMostProbablePathSpanningTree(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
                                                                                 storm::dd::Bdd<Type> const& transitionFilter) {
    storm::dd::Add<Type, ValueType> maxProbabilities = game.getInitialStates().template toAdd<ValueType>();

    storm::dd::Bdd<Type> border = game.getInitialStates();
    storm::dd::Bdd<Type> spanningTree = game.getManager().getBddZero();

    storm::dd::Add<Type, ValueType> transitionMatrix =
        ((transitionFilter && game.getExtendedTransitionMatrix().maxAbstractRepresentative(game.getProbabilisticBranchingVariables()))
             .template toAdd<ValueType>() *
         game.getExtendedTransitionMatrix())
            .sumAbstract(game.getPlayer2Variables());

    std::set<storm::expressions::Variable> variablesToAbstract(game.getRowVariables());
    variablesToAbstract.insert(game.getPlayer1Variables().begin(), game.getPlayer1Variables().end());
    variablesToAbstract.insert(game.getProbabilisticBranchingVariables().begin(), game.getProbabilisticBranchingVariables().end());
    while (!border.isZero()) {
        // Determine the new maximal probabilities to all states.
        storm::dd::Add<Type, ValueType> tmp = border.template toAdd<ValueType>() * transitionMatrix * maxProbabilities;
        storm::dd::Bdd<Type> newMaxProbabilityChoices = tmp.maxAbstractRepresentative(variablesToAbstract);
        storm::dd::Add<Type, ValueType> newMaxProbabilities = tmp.maxAbstract(variablesToAbstract).swapVariables(game.getRowColumnMetaVariablePairs());

        // Determine the probability values for which states strictly increased.
        storm::dd::Bdd<Type> updateStates = newMaxProbabilities.greater(maxProbabilities);
        maxProbabilities = updateStates.ite(newMaxProbabilities, maxProbabilities);

        // Delete all edges in the spanning tree that lead to states that need to be updated.
        spanningTree &= ((!updateStates).swapVariables(game.getRowColumnMetaVariablePairs()));

        // Add all edges that achieve the new maximal value to the spanning tree.
        spanningTree |= updateStates.swapVariables(game.getRowColumnMetaVariablePairs()) && newMaxProbabilityChoices;

        // Continue exploration from states that have been updated.
        border = updateStates;
    }

    return SymbolicMostProbablePathsResult<Type, ValueType>(maxProbabilities, spanningTree);
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicPivotStateResult<Type, ValueType> pickPivotState(AbstractionSettings::PivotSelectionHeuristic const& heuristic,
                                                         storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
                                                         PivotStateCandidatesResult<Type> const& pivotStateCandidateResult,
                                                         boost::optional<SymbolicQualitativeGameResultMinMax<Type>> const& qualitativeResult,
                                                         boost::optional<SymbolicQuantitativeGameResultMinMax<Type, ValueType>> const& quantitativeResult) {
    // Get easy access to strategies.
    storm::dd::Bdd<Type> minPlayer1Strategy;
    storm::dd::Bdd<Type> minPlayer2Strategy;
    storm::dd::Bdd<Type> maxPlayer1Strategy;
    storm::dd::Bdd<Type> maxPlayer2Strategy;
    if (qualitativeResult) {
        minPlayer1Strategy = qualitativeResult.get().prob0Min.getPlayer1Strategy();
        minPlayer2Strategy = qualitativeResult.get().prob0Min.getPlayer2Strategy();
        maxPlayer1Strategy = qualitativeResult.get().prob1Max.getPlayer1Strategy();
        maxPlayer2Strategy = qualitativeResult.get().prob1Max.getPlayer2Strategy();
    } else if (quantitativeResult) {
        minPlayer1Strategy = quantitativeResult.get().min.getPlayer1Strategy();
        minPlayer2Strategy = quantitativeResult.get().min.getPlayer2Strategy();
        maxPlayer1Strategy = quantitativeResult.get().max.getPlayer1Strategy();
        maxPlayer2Strategy = quantitativeResult.get().max.getPlayer2Strategy();
    } else {
        STORM_LOG_ASSERT(false, "Either qualitative or quantitative result is required.");
    }

    storm::dd::Bdd<Type> pivotStates = pivotStateCandidateResult.pivotStates;

    if (heuristic == AbstractionSettings::PivotSelectionHeuristic::NearestMaximalDeviation) {
        // Set up used variables.
        storm::dd::Bdd<Type> initialStates = game.getInitialStates();
        std::set<storm::expressions::Variable> const& rowVariables = game.getRowVariables();
        std::set<storm::expressions::Variable> const& columnVariables = game.getColumnVariables();
        storm::dd::Bdd<Type> transitionsMin = pivotStateCandidateResult.reachableTransitionsMin;
        storm::dd::Bdd<Type> transitionsMax = pivotStateCandidateResult.reachableTransitionsMax;
        storm::dd::Bdd<Type> frontierMin = initialStates;
        storm::dd::Bdd<Type> frontierMax = initialStates;
        storm::dd::Bdd<Type> frontierPivotStates = frontierMin && pivotStates;

        // Check whether we have pivot states on the very first level.
        uint64_t level = 0;
        bool foundPivotState = !frontierPivotStates.isZero();
        if (foundPivotState) {
            STORM_LOG_TRACE("Picked pivot state from " << frontierPivotStates.getNonZeroCount() << " candidates on level " << level << ", "
                                                       << pivotStates.getNonZeroCount() << " candidates in total.");
            return SymbolicPivotStateResult<Type, ValueType>(frontierPivotStates.existsAbstractRepresentative(rowVariables),
                                                             storm::OptimizationDirection::Minimize);
        } else {
            // Otherwise, we perform a simulatenous BFS in the sense that we make one step in both the min and max
            // transitions and check for pivot states we encounter.
            while (!foundPivotState) {
                frontierMin = frontierMin.relationalProduct(transitionsMin, rowVariables, columnVariables);
                frontierMax = frontierMax.relationalProduct(transitionsMax, rowVariables, columnVariables);
                ++level;

                storm::dd::Bdd<Type> frontierMinPivotStates = frontierMin && pivotStates;
                storm::dd::Bdd<Type> frontierMaxPivotStates = frontierMax && pivotStates;
                uint64_t numberOfPivotStateCandidatesOnLevel = frontierMinPivotStates.getNonZeroCount() + frontierMaxPivotStates.getNonZeroCount();

                if (!frontierMinPivotStates.isZero() || !frontierMaxPivotStates.isZero()) {
                    if (quantitativeResult) {
                        storm::dd::Add<Type, ValueType> frontierMinPivotStatesAdd = frontierMinPivotStates.template toAdd<ValueType>();
                        storm::dd::Add<Type, ValueType> frontierMaxPivotStatesAdd = frontierMaxPivotStates.template toAdd<ValueType>();

                        storm::dd::Add<Type, ValueType> diffMin =
                            frontierMinPivotStatesAdd * quantitativeResult.get().max.values - frontierMinPivotStatesAdd * quantitativeResult.get().min.values;
                        storm::dd::Add<Type, ValueType> diffMax =
                            frontierMaxPivotStatesAdd * quantitativeResult.get().max.values - frontierMaxPivotStatesAdd * quantitativeResult.get().min.values;

                        ValueType diffValue;
                        storm::OptimizationDirection direction;
                        if (diffMin.getMax() >= diffMax.getMax()) {
                            direction = storm::OptimizationDirection::Minimize;
                            diffValue = diffMin.getMax();
                        } else {
                            direction = storm::OptimizationDirection::Maximize;
                            diffValue = diffMax.getMax();
                        }

                        STORM_LOG_TRACE("Picked pivot state with difference " << diffValue << " from " << numberOfPivotStateCandidatesOnLevel
                                                                              << " candidates on level " << level << ", " << pivotStates.getNonZeroCount()
                                                                              << " candidates in total.");
                        return SymbolicPivotStateResult<Type, ValueType>(direction == storm::OptimizationDirection::Minimize
                                                                             ? diffMin.maxAbstractRepresentative(rowVariables)
                                                                             : diffMax.maxAbstractRepresentative(rowVariables),
                                                                         direction);
                    } else {
                        STORM_LOG_TRACE("Picked pivot state from " << numberOfPivotStateCandidatesOnLevel << " candidates on level " << level << ", "
                                                                   << pivotStates.getNonZeroCount() << " candidates in total.");

                        storm::OptimizationDirection direction;
                        if (!frontierMinPivotStates.isZero()) {
                            direction = storm::OptimizationDirection::Minimize;
                        } else {
                            direction = storm::OptimizationDirection::Maximize;
                        }

                        return SymbolicPivotStateResult<Type, ValueType>(direction == storm::OptimizationDirection::Minimize
                                                                             ? frontierMinPivotStates.existsAbstractRepresentative(rowVariables)
                                                                             : frontierMaxPivotStates.existsAbstractRepresentative(rowVariables),
                                                                         direction);
                    }
                }
            }
        }
    } else {
        // Compute the most probable paths to the reachable states and the corresponding spanning trees.
        SymbolicMostProbablePathsResult<Type, ValueType> minSymbolicMostProbablePathsResult =
            getMostProbablePathSpanningTree(game, minPlayer1Strategy && minPlayer2Strategy);
        SymbolicMostProbablePathsResult<Type, ValueType> maxSymbolicMostProbablePathsResult =
            getMostProbablePathSpanningTree(game, maxPlayer1Strategy && maxPlayer2Strategy);
        storm::dd::Bdd<Type> selectedPivotState;

        storm::dd::Add<Type, ValueType> score = pivotStates.template toAdd<ValueType>() * minSymbolicMostProbablePathsResult.maxProbabilities.maximum(
                                                                                              maxSymbolicMostProbablePathsResult.maxProbabilities);

        if (heuristic == AbstractionSettings::PivotSelectionHeuristic::MaxWeightedDeviation && quantitativeResult) {
            score = score * (quantitativeResult.get().max.values - quantitativeResult.get().min.values);
        }
        selectedPivotState = score.maxAbstractRepresentative(game.getRowVariables());
        STORM_LOG_TRACE("Selected pivot state with score " << score.getMax() << ".");

        storm::OptimizationDirection fromDirection = storm::OptimizationDirection::Minimize;
        storm::dd::Add<Type, ValueType> selectedPivotStateAsAdd = selectedPivotState.template toAdd<ValueType>();
        if ((selectedPivotStateAsAdd * maxSymbolicMostProbablePathsResult.maxProbabilities).getMax() >
            (selectedPivotStateAsAdd * minSymbolicMostProbablePathsResult.maxProbabilities).getMax()) {
            fromDirection = storm::OptimizationDirection::Maximize;
        }

        return SymbolicPivotStateResult<Type, ValueType>(
            selectedPivotState, fromDirection,
            fromDirection == storm::OptimizationDirection::Minimize ? minSymbolicMostProbablePathsResult : maxSymbolicMostProbablePathsResult);
    }

    STORM_LOG_ASSERT(false, "This point must not be reached, because then no pivot state could be found.");
    return SymbolicPivotStateResult<Type, ValueType>(storm::dd::Bdd<Type>(), storm::OptimizationDirection::Minimize);
}

template<storm::dd::DdType Type, typename ValueType>
RefinementPredicates MenuGameRefiner<Type, ValueType>::derivePredicatesFromDifferingChoices(storm::dd::Bdd<Type> const& player1Choice,
                                                                                            storm::dd::Bdd<Type> const& lowerChoice,
                                                                                            storm::dd::Bdd<Type> const& upperChoice) const {
    // Prepare result.
    storm::expressions::Expression newPredicate;
    bool fromGuard = false;

    // Get abstraction information for easier access.
    AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();

    // Decode the index of the command chosen by player 1.
    storm::dd::Add<Type, ValueType> player1ChoiceAsAdd = player1Choice.template toAdd<ValueType>();
    auto pl1It = player1ChoiceAsAdd.begin();
    uint_fast64_t player1Index = abstractionInformation.decodePlayer1Choice((*pl1It).first, abstractionInformation.getPlayer1VariableCount());

    // Check whether there are bottom states in the game and whether one of the choices actually picks the
    // bottom state as the successor.
    bool buttomStateSuccessor =
        !((abstractionInformation.getBottomStateBdd(false, false) && lowerChoice) || (abstractionInformation.getBottomStateBdd(false, false) && upperChoice))
             .isZero();

    // If one of the choices picks the bottom state, the new predicate is based on the guard of the appropriate
    // command (that is the player 1 choice).
    if (buttomStateSuccessor) {
        STORM_LOG_TRACE("One of the successors is a bottom state, taking a guard as a new predicate.");
        newPredicate = abstractor.get().getGuard(player1Index);
        fromGuard = true;
        STORM_LOG_DEBUG("Derived new predicate (based on guard): " << newPredicate);
    } else {
        STORM_LOG_TRACE("No bottom state successor. Deriving a new predicate using weakest precondition.");

        // Decode both choices to explicit mappings.
        std::map<uint64_t, std::pair<storm::storage::BitVector, ValueType>> lowerChoiceUpdateToSuccessorMapping =
            abstractionInformation.template decodeChoiceToUpdateSuccessorMapping<ValueType>(lowerChoice);
        std::map<uint64_t, std::pair<storm::storage::BitVector, ValueType>> upperChoiceUpdateToSuccessorMapping =
            abstractionInformation.template decodeChoiceToUpdateSuccessorMapping<ValueType>(upperChoice);
        STORM_LOG_ASSERT(
            lowerChoiceUpdateToSuccessorMapping.size() == upperChoiceUpdateToSuccessorMapping.size(),
            "Mismatching sizes after decode (" << lowerChoiceUpdateToSuccessorMapping.size() << " vs. " << upperChoiceUpdateToSuccessorMapping.size() << ").");

        // First, sort updates according to probability mass.
        std::vector<std::pair<uint64_t, ValueType>> updateIndicesAndMasses;
        for (auto const& entry : lowerChoiceUpdateToSuccessorMapping) {
            updateIndicesAndMasses.emplace_back(entry.first, entry.second.second);
        }
        std::sort(updateIndicesAndMasses.begin(), updateIndicesAndMasses.end(),
                  [](std::pair<uint64_t, ValueType> const& a, std::pair<uint64_t, ValueType> const& b) { return a.second > b.second; });

        // Now find the update with the highest probability mass among all deviating updates. More specifically,
        // we determine the set of predicate indices for which there is a deviation.
        std::set<uint64_t> deviationPredicates;
        uint64_t orderedUpdateIndex = 0;
        std::vector<storm::expressions::Expression> possibleRefinementPredicates;
        for (; orderedUpdateIndex < updateIndicesAndMasses.size(); ++orderedUpdateIndex) {
            storm::storage::BitVector const& lower = lowerChoiceUpdateToSuccessorMapping.at(updateIndicesAndMasses[orderedUpdateIndex].first).first;
            storm::storage::BitVector const& upper = upperChoiceUpdateToSuccessorMapping.at(updateIndicesAndMasses[orderedUpdateIndex].first).first;

            bool deviates = lower != upper;
            if (deviates) {
                std::map<storm::expressions::Variable, storm::expressions::Expression> variableUpdates =
                    abstractor.get().getVariableUpdates(player1Index, updateIndicesAndMasses[orderedUpdateIndex].first);

                for (uint64_t predicateIndex = 0; predicateIndex < lower.size(); ++predicateIndex) {
                    if (lower[predicateIndex] != upper[predicateIndex]) {
                        possibleRefinementPredicates.push_back(
                            abstractionInformation.getPredicateByIndex(predicateIndex).substitute(variableUpdates).simplify());
                        if (!rankPredicates) {
                            break;
                        }
                    }
                }
                break;
            }
        }

        STORM_LOG_ASSERT(!possibleRefinementPredicates.empty(), "Expected refinement predicates.");

        STORM_LOG_TRACE("Possible refinement predicates:");
        for (auto const& pred : possibleRefinementPredicates) {
            STORM_LOG_TRACE(pred);
        }

        if (rankPredicates) {
            // Since we can choose any of the deviation predicates to perform the split, we go through the remaining
            // updates and build all deviation predicates. We can then check whether any of the possible refinement
            // predicates also eliminates another deviation.
            std::vector<storm::expressions::Expression> otherRefinementPredicates;
            for (; orderedUpdateIndex < updateIndicesAndMasses.size(); ++orderedUpdateIndex) {
                storm::storage::BitVector const& lower = lowerChoiceUpdateToSuccessorMapping.at(updateIndicesAndMasses[orderedUpdateIndex].first).first;
                storm::storage::BitVector const& upper = upperChoiceUpdateToSuccessorMapping.at(updateIndicesAndMasses[orderedUpdateIndex].first).first;

                bool deviates = lower != upper;
                if (deviates) {
                    std::map<storm::expressions::Variable, storm::expressions::Expression> newVariableUpdates =
                        abstractor.get().getVariableUpdates(player1Index, updateIndicesAndMasses[orderedUpdateIndex].first);
                    for (uint64_t predicateIndex = 0; predicateIndex < lower.size(); ++predicateIndex) {
                        if (lower[predicateIndex] != upper[predicateIndex]) {
                            otherRefinementPredicates.push_back(
                                abstractionInformation.getPredicateByIndex(predicateIndex).substitute(newVariableUpdates).simplify());
                        }
                    }
                }
            }

            // Finally, go through the refinement predicates and see how many deviations they cover.
            std::vector<uint64_t> refinementPredicateIndexToCount(possibleRefinementPredicates.size(), 0);
            for (uint64_t index = 0; index < possibleRefinementPredicates.size(); ++index) {
                refinementPredicateIndexToCount[index] = 1;
            }
            for (auto const& otherPredicate : otherRefinementPredicates) {
                for (uint64_t index = 0; index < possibleRefinementPredicates.size(); ++index) {
                    if (equivalenceChecker.areEquivalentModuloNegation(otherPredicate, possibleRefinementPredicates[index])) {
                        ++refinementPredicateIndexToCount[index];
                    }
                }
            }

            // Find predicate that covers the most deviations.
            uint64_t chosenPredicateIndex = 0;
            for (uint64_t index = 0; index < possibleRefinementPredicates.size(); ++index) {
                if (refinementPredicateIndexToCount[index] > refinementPredicateIndexToCount[chosenPredicateIndex]) {
                    chosenPredicateIndex = index;
                }
            }
            newPredicate = possibleRefinementPredicates[chosenPredicateIndex];
            STORM_LOG_DEBUG("Derived new predicate (based on weakest-precondition): " << newPredicate << ", (equivalent to "
                                                                                      << (refinementPredicateIndexToCount[chosenPredicateIndex] - 1)
                                                                                      << " other refinement predicates).");
        } else {
            newPredicate = possibleRefinementPredicates.front();
            STORM_LOG_DEBUG("Derived new predicate (based on weakest-precondition): " << newPredicate << ".");
        }

        STORM_LOG_ASSERT(newPredicate.isInitialized(), "Could not derive new predicate as there is no deviation.");
    }

    return RefinementPredicates(fromGuard ? RefinementPredicates::Source::Guard : RefinementPredicates::Source::WeakestPrecondition, {newPredicate});
}

template<storm::dd::DdType Type, typename ValueType>
RefinementPredicates MenuGameRefiner<Type, ValueType>::derivePredicatesFromChoice(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
                                                                                  storm::dd::Bdd<Type> const& pivotState,
                                                                                  storm::dd::Bdd<Type> const& player1Choice, storm::dd::Bdd<Type> const& choice,
                                                                                  storm::dd::Bdd<Type> const& choiceSuccessors) const {
    // Prepare result.
    storm::expressions::Expression newPredicate;
    bool fromGuard = false;

    // Get abstraction information for easier access.
    AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();

    // Decode the index of the command chosen by player 1.
    storm::dd::Add<Type, ValueType> player1ChoiceAsAdd = player1Choice.template toAdd<ValueType>();
    auto pl1It = player1ChoiceAsAdd.begin();
    uint_fast64_t player1Index = abstractionInformation.decodePlayer1Choice((*pl1It).first, abstractionInformation.getPlayer1VariableCount());

    // Check whether there are bottom states in the game and whether the choice actually picks the bottom state
    // as the successor.
    bool buttomStateSuccessor = !((abstractionInformation.getBottomStateBdd(false, false) && choiceSuccessors)).isZero();

    std::vector<storm::expressions::Expression> possibleRefinementPredicates;

    // If the choice picks the bottom state, the new predicate is based on the guard of the appropriate  command
    // (that is the player 1 choice).
    if (buttomStateSuccessor) {
        STORM_LOG_TRACE("One of the successors is a bottom state, taking a guard as a new predicate.");
        possibleRefinementPredicates.emplace_back(abstractor.get().getGuard(player1Index));
        fromGuard = true;
        STORM_LOG_DEBUG("Derived new predicate (based on guard): " << possibleRefinementPredicates.back());
    } else {
        STORM_LOG_TRACE("No bottom state successor. Deriving a new predicate using weakest precondition.");

        // Decode the choice successors.
        std::map<uint64_t, std::pair<storm::storage::BitVector, ValueType>> choiceUpdateToSuccessorMapping =
            abstractionInformation.template decodeChoiceToUpdateSuccessorMapping<ValueType>(choiceSuccessors);
        std::vector<std::pair<uint64_t, ValueType>> sortedChoiceUpdateIndicesAndMasses;
        for (auto const& e : choiceUpdateToSuccessorMapping) {
            sortedChoiceUpdateIndicesAndMasses.emplace_back(e.first, e.second.second);
        }
        std::sort(sortedChoiceUpdateIndicesAndMasses.begin(), sortedChoiceUpdateIndicesAndMasses.end(),
                  [](std::pair<uint64_t, ValueType> const& a, std::pair<uint64_t, ValueType> const& b) { return a.second > b.second; });

        // Compute all other (not taken) choices.
        std::set<storm::expressions::Variable> variablesToAbstract = game.getRowVariables();
        variablesToAbstract.insert(game.getPlayer1Variables().begin(), game.getPlayer1Variables().end());

        storm::dd::Bdd<Type> otherChoices =
            (pivotState && !choice && player1Choice && game.getExtendedTransitionMatrix().toBdd()).existsAbstract(variablesToAbstract);
        STORM_LOG_ASSERT(!otherChoices.isZero(), "Expected other choices.");

        // Decode the other choices.
        std::vector<std::map<uint64_t, std::pair<storm::storage::BitVector, ValueType>>> otherChoicesUpdateToSuccessorMappings =
            abstractionInformation.template decodeChoicesToUpdateSuccessorMapping<ValueType>(game.getPlayer2Variables(), otherChoices);

        for (auto const& otherChoice : otherChoicesUpdateToSuccessorMappings) {
            for (uint64_t updateIndex = 0; updateIndex < sortedChoiceUpdateIndicesAndMasses.size(); ++updateIndex) {
                storm::storage::BitVector const& choiceSuccessor =
                    choiceUpdateToSuccessorMapping.at(sortedChoiceUpdateIndicesAndMasses[updateIndex].first).first;
                storm::storage::BitVector const& otherChoiceSuccessor = otherChoice.at(sortedChoiceUpdateIndicesAndMasses[updateIndex].first).first;

                bool deviates = choiceSuccessor != otherChoiceSuccessor;
                if (deviates) {
                    std::map<storm::expressions::Variable, storm::expressions::Expression> variableUpdates =
                        abstractor.get().getVariableUpdates(player1Index, sortedChoiceUpdateIndicesAndMasses[updateIndex].first);

                    for (uint64_t predicateIndex = 0; predicateIndex < choiceSuccessor.size(); ++predicateIndex) {
                        if (choiceSuccessor[predicateIndex] != otherChoiceSuccessor[predicateIndex]) {
                            possibleRefinementPredicates.push_back(
                                abstractionInformation.getPredicateByIndex(predicateIndex).substitute(variableUpdates).simplify());
                            if (!rankPredicates) {
                                break;
                            }
                        }
                    }

                    break;
                }
            }
        }

        STORM_LOG_ASSERT(!possibleRefinementPredicates.empty(), "Expected refinement predicates.");

        STORM_LOG_TRACE("Possible refinement predicates:");
        for (auto const& pred : possibleRefinementPredicates) {
            STORM_LOG_TRACE(pred);
        }
    }

    return RefinementPredicates(fromGuard ? RefinementPredicates::Source::Guard : RefinementPredicates::Source::WeakestPrecondition,
                                {possibleRefinementPredicates});
}

template<storm::dd::DdType Type, typename ValueType>
PivotStateCandidatesResult<Type> computePivotStates(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
                                                    storm::dd::Bdd<Type> const& transitionMatrixBdd, storm::dd::Bdd<Type> const& minPlayer1Strategy,
                                                    storm::dd::Bdd<Type> const& minPlayer2Strategy, storm::dd::Bdd<Type> const& maxPlayer1Strategy,
                                                    storm::dd::Bdd<Type> const& maxPlayer2Strategy) {
    PivotStateCandidatesResult<Type> result;

    // Build the fragment of transitions that is reachable by either the min or the max strategies.
    result.reachableTransitionsMin = (transitionMatrixBdd && minPlayer1Strategy && minPlayer2Strategy).existsAbstract(game.getNondeterminismVariables());
    result.reachableTransitionsMax = (transitionMatrixBdd && maxPlayer1Strategy && maxPlayer2Strategy).existsAbstract(game.getNondeterminismVariables());

    // Start with all reachable states as potential pivot states.
    result.pivotStates =
        storm::utility::dd::computeReachableStates(game.getInitialStates(), result.reachableTransitionsMin, game.getRowVariables(), game.getColumnVariables())
            .first ||
        storm::utility::dd::computeReachableStates(game.getInitialStates(), result.reachableTransitionsMax, game.getRowVariables(), game.getColumnVariables())
            .first;

    // Then constrain these states by the requirement that for either the lower or upper player 1 choice the player 2 choices must be different and
    // that the difference is not because of a missing strategy in either case.

    // Start with constructing the player 2 states that have a min and a max strategy.
    storm::dd::Bdd<Type> constraint =
        minPlayer2Strategy.existsAbstract(game.getPlayer2Variables()) && maxPlayer2Strategy.existsAbstract(game.getPlayer2Variables());

    // Now construct all player 2 choices that actually exist and differ in the min and max case.
    constraint &= minPlayer2Strategy.exclusiveOr(maxPlayer2Strategy);

    // Then restrict the pivot states by requiring existing and different player 2 choices.
    result.pivotStates &= ((minPlayer1Strategy || maxPlayer1Strategy) && constraint).existsAbstract(game.getNondeterminismVariables());

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
RefinementPredicates MenuGameRefiner<Type, ValueType>::derivePredicatesFromPivotState(
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& pivotState, storm::dd::Bdd<Type> const& minPlayer1Strategy,
    storm::dd::Bdd<Type> const& minPlayer2Strategy, storm::dd::Bdd<Type> const& maxPlayer1Strategy, storm::dd::Bdd<Type> const& maxPlayer2Strategy) const {
    // Compute the lower and the upper choice for the pivot state.
    std::set<storm::expressions::Variable> variablesToAbstract = game.getNondeterminismVariables();
    variablesToAbstract.insert(game.getRowVariables().begin(), game.getRowVariables().end());

    bool player1ChoicesDifferent = !(pivotState && minPlayer1Strategy).exclusiveOr(pivotState && maxPlayer1Strategy).isZero();

    boost::optional<RefinementPredicates> predicates;

    // Derive predicates from lower choice.
    storm::dd::Bdd<Type> lowerChoice = pivotState && game.getExtendedTransitionMatrix().toBdd() && minPlayer1Strategy;
    storm::dd::Bdd<Type> lowerChoice1 = (lowerChoice && minPlayer2Strategy).existsAbstract(variablesToAbstract);
    storm::dd::Bdd<Type> lowerChoice2 = (lowerChoice && maxPlayer2Strategy).existsAbstract(variablesToAbstract);

    bool lowerChoicesDifferent = !lowerChoice1.exclusiveOr(lowerChoice2).isZero() && !lowerChoice1.isZero() && !lowerChoice2.isZero();
    if (lowerChoicesDifferent) {
        STORM_LOG_TRACE("Deriving predicates based on lower choice.");
        if (this->addPredicatesEagerly) {
            predicates = derivePredicatesFromChoice(game, pivotState, (pivotState && minPlayer1Strategy).existsAbstract(game.getRowVariables()),
                                                    lowerChoice && minPlayer2Strategy, lowerChoice1);
        } else {
            predicates =
                derivePredicatesFromDifferingChoices((pivotState && minPlayer1Strategy).existsAbstract(game.getRowVariables()), lowerChoice1, lowerChoice2);
        }
    }

    if (predicates && !player1ChoicesDifferent) {
        return predicates.get();
    }

    boost::optional<RefinementPredicates> additionalPredicates;

    storm::dd::Bdd<Type> upperChoice = pivotState && game.getExtendedTransitionMatrix().toBdd() && maxPlayer1Strategy;
    storm::dd::Bdd<Type> upperChoice1 = (upperChoice && minPlayer2Strategy).existsAbstract(variablesToAbstract);
    storm::dd::Bdd<Type> upperChoice2 = (upperChoice && maxPlayer2Strategy).existsAbstract(variablesToAbstract);

    bool upperChoicesDifferent = !upperChoice1.exclusiveOr(upperChoice2).isZero() && !upperChoice1.isZero() && !upperChoice2.isZero();
    if (upperChoicesDifferent) {
        STORM_LOG_TRACE("Deriving predicates based on upper choice.");
        if (this->addPredicatesEagerly) {
            additionalPredicates = derivePredicatesFromChoice(game, pivotState, (pivotState && maxPlayer1Strategy).existsAbstract(game.getRowVariables()),
                                                              upperChoice && maxPlayer2Strategy, upperChoice1);
        } else {
            additionalPredicates =
                derivePredicatesFromDifferingChoices((pivotState && maxPlayer1Strategy).existsAbstract(game.getRowVariables()), upperChoice1, upperChoice2);
        }
    }

    if (additionalPredicates) {
        if (additionalPredicates.get().getSource() == RefinementPredicates::Source::Guard) {
            return additionalPredicates.get();
        } else {
            if (!predicates) {
                predicates = additionalPredicates;
            } else {
                predicates.get().addPredicates(additionalPredicates.get().getPredicates());
            }
        }
    }

    STORM_LOG_THROW(static_cast<bool>(predicates), storm::exceptions::InvalidStateException, "Could not derive predicates for refinement.");

    return predicates.get();
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<std::vector<std::vector<storm::expressions::Expression>>, std::map<storm::expressions::Variable, storm::expressions::Expression>>
MenuGameRefiner<Type, ValueType>::buildTrace(storm::expressions::ExpressionManager& expressionManager,
                                             storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& spanningTree,
                                             storm::dd::Bdd<Type> const& pivotState) const {
    std::vector<std::vector<storm::expressions::Expression>> predicates;

    // Prepare some variables.
    AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();
    std::set<storm::expressions::Variable> variablesToAbstract(game.getColumnVariables());
    variablesToAbstract.insert(game.getPlayer1Variables().begin(), game.getPlayer1Variables().end());
    variablesToAbstract.insert(game.getProbabilisticBranchingVariables().begin(), game.getProbabilisticBranchingVariables().end());

    storm::expressions::Expression initialExpression = abstractor.get().getInitialExpression();

    std::set<storm::expressions::Variable> oldVariables = initialExpression.getVariables();
    for (auto const& predicate : abstractionInformation.getPredicates()) {
        std::set<storm::expressions::Variable> usedVariables = predicate.getVariables();
        oldVariables.insert(usedVariables.begin(), usedVariables.end());
    }

    std::map<storm::expressions::Variable, storm::expressions::Variable> oldToNewVariables;
    for (auto const& variable : oldVariables) {
        oldToNewVariables[variable] = expressionManager.getVariable(variable.getName());
    }
    std::map<storm::expressions::Variable, storm::expressions::Expression> lastSubstitution;
    for (auto const& variable : oldToNewVariables) {
        lastSubstitution[variable.second] = variable.second;
    }
    std::map<storm::expressions::Variable, storm::expressions::Expression> stepVariableToCopiedVariableMap;

    // Start with the target state part of the trace.
    storm::storage::BitVector decodedTargetState = abstractionInformation.decodeState(pivotState);
    predicates.emplace_back(abstractionInformation.getPredicates(decodedTargetState));
    for (auto& predicate : predicates.back()) {
        predicate = predicate.changeManager(expressionManager);
    }
    // Add ranges of variables.
    for (auto const& pred : abstractionInformation.getConstraints()) {
        predicates.back().push_back(pred.changeManager(expressionManager));
    }

    // Perform a backward search for an initial state.
    storm::dd::Bdd<Type> currentState = pivotState;
    while ((currentState && game.getInitialStates()).isZero()) {
        storm::dd::Bdd<Type> predecessorTransition = currentState.swapVariables(game.getRowColumnMetaVariablePairs()) && spanningTree;
        std::tuple<storm::storage::BitVector, uint64_t, uint64_t> decodedPredecessor =
            abstractionInformation.decodeStatePlayer1ChoiceAndUpdate(predecessorTransition);

        // Create a new copy of each variable to use for this step.
        std::map<storm::expressions::Variable, storm::expressions::Expression> substitution;
        for (auto const& variablePair : oldToNewVariables) {
            storm::expressions::Variable variableCopy = expressionManager.declareVariableCopy(variablePair.second);
            substitution[variablePair.second] = variableCopy;
            stepVariableToCopiedVariableMap[variableCopy] = variablePair.second;
        }

        // Retrieve the variable updates that the predecessor needs to perform to get to the current state.
        auto variableUpdates = abstractor.get().getVariableUpdates(std::get<1>(decodedPredecessor), std::get<2>(decodedPredecessor));
        for (auto const& oldNewVariablePair : oldToNewVariables) {
            storm::expressions::Variable const& newVariable = oldNewVariablePair.second;

            // If the variable was set, use its update expression.
            auto updateIt = variableUpdates.find(oldNewVariablePair.first);
            if (updateIt != variableUpdates.end()) {
                auto const& update = *updateIt;

                if (update.second.hasBooleanType()) {
                    predicates.back().push_back(
                        storm::expressions::iff(lastSubstitution.at(newVariable), update.second.changeManager(expressionManager).substitute(substitution)));
                } else {
                    predicates.back().push_back(lastSubstitution.at(newVariable) == update.second.changeManager(expressionManager).substitute(substitution));
                }
            } else {
                // Otherwise, make sure that the new variable maintains the old value.
                if (newVariable.hasBooleanType()) {
                    predicates.back().push_back(storm::expressions::iff(lastSubstitution.at(newVariable), substitution.at(newVariable)));
                } else {
                    predicates.back().push_back(lastSubstitution.at(newVariable) == substitution.at(newVariable));
                }
            }
        }

        // Add the guard of the choice.
        predicates.back().push_back(abstractor.get().getGuard(std::get<1>(decodedPredecessor)).changeManager(expressionManager).substitute(substitution));

        // Retrieve the predicate valuation in the predecessor.
        predicates.emplace_back(abstractionInformation.getPredicates(std::get<0>(decodedPredecessor)));
        for (auto& predicate : predicates.back()) {
            predicate = predicate.changeManager(expressionManager).substitute(substitution);
        }
        // Add ranges of variables.
        for (auto const& pred : abstractionInformation.getConstraints()) {
            predicates.back().push_back(pred.changeManager(expressionManager).substitute(substitution));
        }
        // Move backwards one step.
        lastSubstitution = std::move(substitution);
        currentState = predecessorTransition.existsAbstract(variablesToAbstract);
    }

    predicates.back().push_back(initialExpression.changeManager(expressionManager).substitute(lastSubstitution));
    return std::make_pair(predicates, stepVariableToCopiedVariableMap);
}

template<typename ValueType>
const uint64_t ExplicitPivotStateResult<ValueType>::NO_PREDECESSOR = std::numeric_limits<uint64_t>::max();

template<storm::dd::DdType Type, typename ValueType>
std::pair<std::vector<uint64_t>, std::vector<uint64_t>> MenuGameRefiner<Type, ValueType>::buildReversedLabeledPath(
    ExplicitPivotStateResult<ValueType> const& pivotStateResult) const {
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> result;
    result.first.emplace_back(pivotStateResult.pivotState);

    uint64_t currentState = pivotStateResult.predecessors[pivotStateResult.pivotState].first;
    uint64_t currentAction = pivotStateResult.predecessors[pivotStateResult.pivotState].second;
    while (currentState != ExplicitPivotStateResult<ValueType>::NO_PREDECESSOR) {
        STORM_LOG_ASSERT(currentAction != ExplicitPivotStateResult<ValueType>::NO_PREDECESSOR, "Expected predecessor action.");
        result.first.emplace_back(currentState);
        result.second.emplace_back(currentAction);
        currentAction = pivotStateResult.predecessors[currentState].second;
        currentState = pivotStateResult.predecessors[currentState].first;
    }

    STORM_LOG_ASSERT(result.first.size() == result.second.size() + 1, "Path size mismatch.");
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<std::vector<std::vector<storm::expressions::Expression>>, std::map<storm::expressions::Variable, storm::expressions::Expression>>
MenuGameRefiner<Type, ValueType>::buildTraceFromReversedLabeledPath(storm::expressions::ExpressionManager& expressionManager,
                                                                    std::vector<uint64_t> const& reversedPath, std::vector<uint64_t> const& reversedLabels,
                                                                    storm::dd::Odd const& odd, std::vector<uint64_t> const* stateToOffset) const {
    STORM_LOG_ASSERT(reversedPath.size() == reversedLabels.size() + 1, "Path size mismatch.");

    std::vector<std::vector<storm::expressions::Expression>> predicates;

    AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();
    storm::expressions::Expression initialExpression = abstractor.get().getInitialExpression();

    std::set<storm::expressions::Variable> oldVariables = initialExpression.getVariables();
    for (auto const& predicate : abstractionInformation.getPredicates()) {
        std::set<storm::expressions::Variable> usedVariables = predicate.getVariables();
        oldVariables.insert(usedVariables.begin(), usedVariables.end());
    }

    std::map<storm::expressions::Variable, storm::expressions::Variable> oldToNewVariables;
    for (auto const& variable : oldVariables) {
        oldToNewVariables[variable] = expressionManager.getVariable(variable.getName());
    }
    std::map<storm::expressions::Variable, storm::expressions::Expression> currentSubstitution;
    for (auto const& variable : oldToNewVariables) {
        currentSubstitution[variable.second] = variable.second;
    }
    std::map<storm::expressions::Variable, storm::expressions::Expression> stepVariableToCopiedVariableMap;

    auto pathIt = reversedPath.rbegin();

    // Decode pivot state. The state valuation also includes
    // * the bottom state, so we need to reserve one more, and
    // * the location variables,
    // so we need to reserve an appropriate size.
    uint64_t predicateValuationOffset = abstractionInformation.getNumberOfDdSourceLocationVariables() + 1;
    storm::storage::BitVector extendedPredicateValuation =
        odd.getEncoding(stateToOffset ? (*stateToOffset)[*pathIt] : *pathIt, abstractionInformation.getNumberOfPredicates() + predicateValuationOffset);
    ++pathIt;

    // Add all predicates of initial block.
    predicates.emplace_back(abstractionInformation.getPredicatesExcludingBottom(extendedPredicateValuation));
    for (auto& predicate : predicates.back()) {
        predicate = predicate.changeManager(expressionManager);
    }

    // Add further constraints (like ranges).
    for (auto const& pred : abstractionInformation.getConstraints()) {
        predicates.back().push_back(pred.changeManager(expressionManager));
    }

    // Add initial expression.
    predicates.back().push_back(initialExpression.changeManager(expressionManager));

    // Traverse the path and construct necessary formula parts.
    auto actionIt = reversedLabels.rbegin();
    for (; pathIt != reversedPath.rend(); ++pathIt) {
        // Add new predicate frame.
        predicates.emplace_back();

        // Add guard of action.
        predicates.back().emplace_back(abstractor.get().getGuard(*actionIt).changeManager(expressionManager).substitute(currentSubstitution));

        // Determine which variables are affected by the updates of the player 1 choice.
        std::set<storm::expressions::Variable> const& assignedVariables = abstractor.get().getAssignedVariables(*actionIt);

        // Create new instances of the affected variables.
        std::map<storm::expressions::Variable, storm::expressions::Variable> newVariableMaps;
        for (auto const& variable : assignedVariables) {
            storm::expressions::Variable variableCopy = expressionManager.declareVariableCopy(variable);
            newVariableMaps[oldToNewVariables.at(variable)] = variableCopy;
            stepVariableToCopiedVariableMap[variableCopy] = variable;
        }

        // Retrieves the possible updates to the variables.
        auto variableUpdateVector = abstractor.get().getVariableUpdates(*actionIt);

        // Encode these updates.
        storm::expressions::Expression allVariableUpdateExpression;
        for (auto const& variableUpdates : variableUpdateVector) {
            storm::expressions::Expression variableUpdateExpression;
            for (auto const& update : variableUpdates) {
                if (update.second.hasBooleanType()) {
                    variableUpdateExpression =
                        variableUpdateExpression && storm::expressions::iff(newVariableMaps.at(update.first),
                                                                            update.second.changeManager(expressionManager).substitute(currentSubstitution));
                } else {
                    variableUpdateExpression = variableUpdateExpression && newVariableMaps.at(update.first) ==
                                                                               update.second.changeManager(expressionManager).substitute(currentSubstitution);
                }
            }

            allVariableUpdateExpression = allVariableUpdateExpression || variableUpdateExpression;
        }
        if (!allVariableUpdateExpression.isInitialized()) {
            allVariableUpdateExpression = expressionManager.boolean(true);
        }
        predicates.back().emplace_back(allVariableUpdateExpression);

        // Incorporate the new variables in the current substitution.
        for (auto const& variablePair : newVariableMaps) {
            currentSubstitution[variablePair.first] = variablePair.second;
        }

        // Decode current state.
        extendedPredicateValuation =
            odd.getEncoding(stateToOffset ? (*stateToOffset)[*pathIt] : *pathIt, abstractionInformation.getNumberOfPredicates() + predicateValuationOffset);

        // Encode the predicates whose value might have changed.
        // FIXME: could be optimized by precomputation.
        for (uint64_t predicateIndex = 0; predicateIndex < abstractionInformation.getNumberOfPredicates(); ++predicateIndex) {
            auto const& predicate = abstractionInformation.getPredicateByIndex(predicateIndex);
            std::set<storm::expressions::Variable> usedVariables = predicate.getVariables();

            bool containsAssignedVariables = false;
            for (auto usedIt = usedVariables.begin(), assignedIt = assignedVariables.begin();;) {
                if (usedIt == usedVariables.end() || assignedIt == assignedVariables.end()) {
                    break;
                }

                if (*usedIt == *assignedIt) {
                    containsAssignedVariables = true;
                    break;
                }

                if (*usedIt < *assignedIt) {
                    ++usedIt;
                } else {
                    ++assignedIt;
                }
            }

            if (containsAssignedVariables) {
                auto transformedPredicate = predicate.changeManager(expressionManager).substitute(currentSubstitution);
                predicates.back().emplace_back(extendedPredicateValuation.get(predicateIndex + predicateValuationOffset) ? transformedPredicate
                                                                                                                         : !transformedPredicate);
            }
        }

        // Enforce constraints of all assigned variables.
        for (auto const& constraint : abstractionInformation.getConstraints()) {
            std::set<storm::expressions::Variable> usedVariables = constraint.getVariables();

            bool containsAssignedVariables = false;
            for (auto usedIt = usedVariables.begin(), assignedIt = assignedVariables.begin();;) {
                if (usedIt == usedVariables.end() || assignedIt == assignedVariables.end()) {
                    break;
                }

                if (*usedIt == *assignedIt) {
                    containsAssignedVariables = true;
                    break;
                }

                if (*usedIt < *assignedIt) {
                    ++usedIt;
                } else {
                    ++assignedIt;
                }
            }

            if (containsAssignedVariables) {
                auto transformedConstraint = constraint.changeManager(expressionManager).substitute(currentSubstitution);
                predicates.back().emplace_back(transformedConstraint);
            }
        }

        ++actionIt;
    }

    return std::make_pair(predicates, stepVariableToCopiedVariableMap);
}

template<storm::dd::DdType Type, typename ValueType>
boost::optional<RefinementPredicates> MenuGameRefiner<Type, ValueType>::derivePredicatesFromInterpolationFromTrace(
    storm::expressions::ExpressionManager& interpolationManager, std::vector<std::vector<storm::expressions::Expression>> const& trace,
    std::map<storm::expressions::Variable, storm::expressions::Expression> const& variableSubstitution) const {
    AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();

    auto start = std::chrono::high_resolution_clock::now();
    boost::optional<RefinementPredicates> predicates;

    // Create solver and interpolation groups.
    auto assertionStart = std::chrono::high_resolution_clock::now();
    storm::solver::MathsatSmtSolver interpolatingSolver(interpolationManager, storm::solver::MathsatSmtSolver::Options(true, false, true));
    uint64_t stepCounter = 0;
    for (auto const& traceElement : trace) {
        // interpolatingSolver.push();

        interpolatingSolver.setInterpolationGroup(stepCounter);
        for (auto const& predicate : traceElement) {
            interpolatingSolver.add(predicate);
        }

        ++stepCounter;
    }
    auto assertionEnd = std::chrono::high_resolution_clock::now();
    STORM_LOG_TRACE("Asserting trace formula took " << std::chrono::duration_cast<std::chrono::milliseconds>(assertionEnd - assertionStart).count() << "ms.");

    // Now encode the trace as an SMT problem.
    storm::solver::SmtSolver::CheckResult result = interpolatingSolver.check();
    if (result == storm::solver::SmtSolver::CheckResult::Unsat) {
        STORM_LOG_TRACE("Trace formula is unsatisfiable. Starting interpolation.");

        std::vector<storm::expressions::Expression> interpolants;
        std::vector<uint64_t> prefix;
        for (uint64_t step = 0; step < stepCounter; ++step) {
            prefix.push_back(step);
            storm::expressions::Expression interpolant =
                interpolatingSolver.getInterpolant(prefix).substitute(variableSubstitution).changeManager(abstractionInformation.getExpressionManager());
            if (interpolant.isFalse()) {
                // If the interpolant is false, it means that the prefix has become unsatisfiable.
                STORM_LOG_TRACE("Trace formula became unsatisfiable at position " << step << " of " << stepCounter << ".");
                break;
            }
            if (!interpolant.isTrue()) {
                STORM_LOG_DEBUG("Derived new predicate (based on interpolation at step " << step << " out of " << stepCounter << "): " << interpolant);
                interpolants.push_back(interpolant);
            }
        }

        STORM_LOG_ASSERT(!interpolants.empty(), "Expected to have non-empty set of interpolants.");

        predicates = boost::make_optional(RefinementPredicates(RefinementPredicates::Source::Interpolation, interpolants));
    } else {
        STORM_LOG_TRACE("Trace formula is satisfiable, not using interpolation.");
    }
    auto end = std::chrono::high_resolution_clock::now();

    if (predicates) {
        STORM_LOG_TRACE("Deriving predicates using interpolation from witness of size "
                        << trace.size() << " took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");
    } else {
        STORM_LOG_TRACE("Tried deriving predicates using interpolation but failed in "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");
    }

    return predicates;
}

template<storm::dd::DdType Type, typename ValueType>
boost::optional<RefinementPredicates> MenuGameRefiner<Type, ValueType>::derivePredicatesFromInterpolation(
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, SymbolicPivotStateResult<Type, ValueType> const& symbolicPivotStateResult,
    storm::dd::Bdd<Type> const& minPlayer1Strategy, storm::dd::Bdd<Type> const& minPlayer2Strategy, storm::dd::Bdd<Type> const& maxPlayer1Strategy,
    storm::dd::Bdd<Type> const& maxPlayer2Strategy) const {
    // Compute the most probable path from any initial state to the pivot state.
    SymbolicMostProbablePathsResult<Type, ValueType> symbolicMostProbablePathsResult;
    if (!symbolicPivotStateResult.symbolicMostProbablePathsResult) {
        symbolicMostProbablePathsResult = getMostProbablePathSpanningTree(game, symbolicPivotStateResult.fromDirection == storm::OptimizationDirection::Minimize
                                                                                    ? minPlayer1Strategy && minPlayer2Strategy
                                                                                    : maxPlayer1Strategy && maxPlayer2Strategy);
    } else {
        symbolicMostProbablePathsResult = symbolicPivotStateResult.symbolicMostProbablePathsResult.get();
    }

    // Create a new expression manager that we can use for the interpolation.
    AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();
    std::shared_ptr<storm::expressions::ExpressionManager> interpolationManager = abstractionInformation.getExpressionManager().clone();

    // Build the trace of the most probable path in terms of which predicates hold in each step.
    auto start = std::chrono::high_resolution_clock::now();
    std::pair<std::vector<std::vector<storm::expressions::Expression>>, std::map<storm::expressions::Variable, storm::expressions::Expression>>
        traceAndVariableSubstitution =
            buildTrace(*interpolationManager, game, symbolicMostProbablePathsResult.spanningTree, symbolicPivotStateResult.pivotState);
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_DEBUG("Building the trace and variable substitution for interpolation from symbolic most-probable paths result took "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    return derivePredicatesFromInterpolationFromTrace(*interpolationManager, traceAndVariableSubstitution.first, traceAndVariableSubstitution.second);
}

template<storm::dd::DdType Type, typename ValueType>
boost::optional<RefinementPredicates> MenuGameRefiner<Type, ValueType>::derivePredicatesFromInterpolation(
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& /*game*/, ExplicitPivotStateResult<ValueType> const& pivotStateResult,
    storm::dd::Odd const& odd) const {
    // Game is not necessary if representation is explicit via odd
    // Create a new expression manager that we can use for the interpolation.
    AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();
    std::shared_ptr<storm::expressions::ExpressionManager> interpolationManager = abstractionInformation.getExpressionManager().clone();

    // Build the trace of the most probable path in terms of which predicates hold in each step.
    auto start = std::chrono::high_resolution_clock::now();
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> labeledReversedPath = buildReversedLabeledPath(pivotStateResult);

    // If the initial state is the pivot state, we can stop here.
    if (labeledReversedPath.first.size() == 1) {
        return boost::none;
    }

    std::pair<std::vector<std::vector<storm::expressions::Expression>>, std::map<storm::expressions::Variable, storm::expressions::Expression>>
        traceAndVariableSubstitution = buildTraceFromReversedLabeledPath(*interpolationManager, labeledReversedPath.first, labeledReversedPath.second, odd);
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_DEBUG("Building the trace and variable substitution for interpolation from explicit most-probable paths result took "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    return derivePredicatesFromInterpolationFromTrace(*interpolationManager, traceAndVariableSubstitution.first, traceAndVariableSubstitution.second);
}

template<storm::dd::DdType Type, typename ValueType>
bool MenuGameRefiner<Type, ValueType>::refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& transitionMatrixBdd,
                                              SymbolicQualitativeGameResultMinMax<Type> const& qualitativeResult) const {
    STORM_LOG_TRACE("Trying refinement after qualitative check.");
    // Get all relevant strategies.
    storm::dd::Bdd<Type> minPlayer1Strategy = qualitativeResult.prob0Min.getPlayer1Strategy();
    storm::dd::Bdd<Type> minPlayer2Strategy = qualitativeResult.prob0Min.getPlayer2Strategy();
    storm::dd::Bdd<Type> maxPlayer1Strategy = qualitativeResult.prob1Max.getPlayer1Strategy();
    storm::dd::Bdd<Type> maxPlayer2Strategy = qualitativeResult.prob1Max.getPlayer2Strategy();

    // Redirect all player 1 choices of the min strategy to that of the max strategy if this leads to a player 2
    // state that is also a prob 0 state.
    auto oldMinPlayer1Strategy = minPlayer1Strategy;
    minPlayer1Strategy = (maxPlayer1Strategy && qualitativeResult.prob0Min.getPlayer2States())
                             .existsAbstract(game.getPlayer1Variables())
                             .ite(maxPlayer1Strategy, minPlayer1Strategy);

    // Compute all reached pivot states.
    PivotStateCandidatesResult<Type> pivotStateCandidatesResult =
        computePivotStates(game, transitionMatrixBdd, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy, maxPlayer2Strategy);

    // We can only refine in case we have a reachable player 1 state with a player 2 successor (under either
    // player 1's min or max strategy) such that from this player 2 state, both prob0 min and prob1 max define
    // strategies and they differ. Hence, it is possible that we arrive at a point where no suitable pivot state
    // is found. In this case, we abort the qualitative refinement here.
    if (pivotStateCandidatesResult.pivotStates.isZero()) {
        STORM_LOG_TRACE("Did not find pivot state in qualitative fragment.");
        return false;
    }
    STORM_LOG_ASSERT(!pivotStateCandidatesResult.pivotStates.isZero(), "Unable to proceed without pivot state candidates.");

    // Now that we have the pivot state candidates, we need to pick one.
    SymbolicPivotStateResult<Type, ValueType> symbolicPivotStateResult =
        pickPivotState<Type, ValueType>(pivotSelectionHeuristic, game, pivotStateCandidatesResult, qualitativeResult, boost::none);

    boost::optional<RefinementPredicates> predicates;
    if (useInterpolation) {
        predicates =
            derivePredicatesFromInterpolation(game, symbolicPivotStateResult, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy, maxPlayer2Strategy);
    }
    if (predicates) {
        STORM_LOG_TRACE("Obtained predicates by interpolation.");
    } else {
        predicates = derivePredicatesFromPivotState(game, symbolicPivotStateResult.pivotState, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy,
                                                    maxPlayer2Strategy);
    }
    STORM_LOG_THROW(static_cast<bool>(predicates), storm::exceptions::InvalidStateException, "Predicates needed to continue.");

    // Derive predicate based on the selected pivot state.
    std::vector<storm::expressions::Expression> preparedPredicates = preprocessPredicates(predicates.get().getPredicates(), predicates.get().getSource());
    performRefinement(createGlobalRefinement(preparedPredicates));
    return true;
}

template<typename ValueType>
struct ExplicitDijkstraQueueElement {
    ExplicitDijkstraQueueElement(ValueType const& distance, uint64_t state, bool lower) : distance(distance), state(state), lower(lower) {
        // Intentionally left empty.
    }

    ValueType distance;
    uint64_t state;
    bool lower;
};

template<typename ValueType>
struct ExplicitDijkstraQueueElementLess {
    bool operator()(ExplicitDijkstraQueueElement<ValueType> const& a, ExplicitDijkstraQueueElement<ValueType> const& b) const {
        if (a.distance < b.distance) {
            return true;
        } else if (a.distance > b.distance) {
            return false;
        } else {
            if (a.state < b.state) {
                return true;
            } else if (a.state > b.state) {
                return false;
            } else {
                if (a.lower < b.lower) {
                    return true;
                } else {
                    return false;
                }
            }
        }
    }
};

template<typename ValueType>
void performDijkstraStep(std::set<ExplicitDijkstraQueueElement<ValueType>, ExplicitDijkstraQueueElementLess<ValueType>>& dijkstraQueue,
                         bool probabilityDistances, std::vector<ValueType>& distances, std::vector<std::pair<uint64_t, uint64_t>>& predecessors,
                         bool generatePredecessors, bool lower, uint64_t currentState, ValueType const& currentDistance, [[maybe_unused]] bool isPivotState,
                         storm::storage::ExplicitGameStrategyPair const& strategyPair,
                         [[maybe_unused]] storm::storage::ExplicitGameStrategyPair const& otherStrategyPair, std::vector<uint64_t> const& player1Labeling,
                         [[maybe_unused]] std::vector<uint64_t> const& player2Grouping, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                         [[maybe_unused]] storm::storage::BitVector const& targetStates, storm::storage::BitVector const& relevantStates) {
    if (strategyPair.getPlayer1Strategy().hasDefinedChoice(currentState)) {
        uint64_t player2Successor = strategyPair.getPlayer1Strategy().getChoice(currentState);
        uint64_t player2Choice = strategyPair.getPlayer2Strategy().getChoice(player2Successor);
        STORM_LOG_ASSERT(isPivotState || !otherStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2Successor) ||
                             strategyPair.getPlayer2Strategy().getChoice(player2Successor) == player2Choice,
                         "Did not expect deviation in player 2 strategy.");
        STORM_LOG_ASSERT(player2Grouping[player2Successor] <= player2Choice && player2Choice < player2Grouping[player2Successor + 1],
                         "Illegal choice for player 2.");

        for (auto const& entry : transitionMatrix.getRow(player2Choice)) {
            uint64_t player1Successor = entry.getColumn();
            if (!relevantStates.get(player1Successor)) {
                continue;
            }

            ValueType alternateDistance;
            if (probabilityDistances) {
                alternateDistance = currentDistance * entry.getValue();
            } else {
                alternateDistance = currentDistance + storm::utility::one<ValueType>();
            }
            if (probabilityDistances ? alternateDistance > distances[player1Successor] : alternateDistance < distances[player1Successor]) {
                distances[player1Successor] = alternateDistance;
                if (generatePredecessors) {
                    predecessors[player1Successor] = std::make_pair(currentState, player1Labeling[player2Successor]);
                }
                dijkstraQueue.emplace(alternateDistance, player1Successor, lower);
            }
        }
    } else {
        STORM_LOG_ASSERT(targetStates.get(currentState), "Expecting min strategy for non-target states.");
    }
}

template<typename ValueType>
boost::optional<ExplicitPivotStateResult<ValueType>> pickPivotState(
    bool generatePredecessors, AbstractionSettings::PivotSelectionHeuristic pivotSelectionHeuristic,
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Labeling,
    storm::storage::BitVector const& initialStates, storm::storage::BitVector const& relevantStates, storm::storage::BitVector const& targetStates,
    storage::ExplicitGameStrategyPair const& minStrategyPair, storage::ExplicitGameStrategyPair const& maxStrategyPair,
    std::vector<ValueType> const* lowerValues = nullptr, std::vector<ValueType> const* upperValues = nullptr) {
    // The method used to have an unused player1Grouping.
    STORM_LOG_ASSERT(!lowerValues || upperValues, "Expected none or both value results.");
    STORM_LOG_ASSERT(!upperValues || lowerValues, "Expected none or both value results.");

    // Perform Dijkstra search that stays within the relevant states and searches for a (pivot) state in which
    // the strategies the lower or upper player 1 action leads to a player 2 state in which the choices differ.
    // To guarantee that the pivot state is reachable by either of the strategies, we do a parallel Dijkstra
    // search in both the lower and upper strategy.

    bool probabilityDistances = pivotSelectionHeuristic == storm::settings::modules::AbstractionSettings::PivotSelectionHeuristic::MostProbablePath;
    uint64_t numberOfStates = initialStates.size();
    ValueType inftyDistance = probabilityDistances ? storm::utility::zero<ValueType>() : storm::utility::infinity<ValueType>();
    ValueType zeroDistance = probabilityDistances ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>();

    // Create storages for the lower and upper Dijkstra search.
    std::vector<ValueType> lowerDistances(numberOfStates, inftyDistance);
    std::vector<std::pair<uint64_t, uint64_t>> lowerPredecessors;
    std::vector<ValueType> upperDistances(numberOfStates, inftyDistance);
    std::vector<std::pair<uint64_t, uint64_t>> upperPredecessors;

    if (generatePredecessors) {
        lowerPredecessors.resize(numberOfStates,
                                 std::make_pair(ExplicitPivotStateResult<ValueType>::NO_PREDECESSOR, ExplicitPivotStateResult<ValueType>::NO_PREDECESSOR));
        upperPredecessors.resize(numberOfStates,
                                 std::make_pair(ExplicitPivotStateResult<ValueType>::NO_PREDECESSOR, ExplicitPivotStateResult<ValueType>::NO_PREDECESSOR));
    }

    // Use set as priority queue with unique membership.
    std::set<ExplicitDijkstraQueueElement<ValueType>, ExplicitDijkstraQueueElementLess<ValueType>> dijkstraQueue;

    for (auto initialState : initialStates) {
        if (!relevantStates.get(initialState)) {
            continue;
        }

        lowerDistances[initialState] = zeroDistance;
        upperDistances[initialState] = zeroDistance;
        dijkstraQueue.emplace(zeroDistance, initialState, true);
        dijkstraQueue.emplace(zeroDistance, initialState, false);
    }

    // For some heuristics, we need to potentially find more than just one pivot.
    bool considerDeviation = (pivotSelectionHeuristic == storm::settings::modules::AbstractionSettings::PivotSelectionHeuristic::NearestMaximalDeviation ||
                              pivotSelectionHeuristic == storm::settings::modules::AbstractionSettings::PivotSelectionHeuristic::MaxWeightedDeviation) &&
                             lowerValues && upperValues;
    bool foundPivotState = false;

    ExplicitDijkstraQueueElement<ValueType> pivotState(inftyDistance, 0, true);
    ValueType pivotStateDeviation = storm::utility::zero<ValueType>();
    auto const& player2Grouping = transitionMatrix.getRowGroupIndices();

    while (!dijkstraQueue.empty()) {
        // Take out currently best state.
        auto currentDijkstraElement = *dijkstraQueue.begin();
        ValueType currentDistance = currentDijkstraElement.distance;
        uint64_t currentState = currentDijkstraElement.state;
        bool currentLower = currentDijkstraElement.lower;
        dijkstraQueue.erase(dijkstraQueue.begin());

        if (foundPivotState && (probabilityDistances ? currentDistance < pivotState.distance : currentDistance > pivotState.distance)) {
            if (pivotSelectionHeuristic != storm::settings::modules::AbstractionSettings::PivotSelectionHeuristic::MaxWeightedDeviation) {
                // For the nearest maximal deviation and most probable path heuristics, future pivot states are
                // not important any more, because their distance will be strictly larger, so we can return the
                // current pivot state.

                return ExplicitPivotStateResult<ValueType>(pivotState.state, pivotState.distance,
                                                           pivotState.lower ? std::move(lowerPredecessors) : std::move(upperPredecessors));
            } else if (pivotStateDeviation >= currentDistance) {
                // If the heuristic is maximal weighted deviation and the weighted deviation for any future pivot
                // state is necessarily at most as high as the current one, we can abort the search.
                return ExplicitPivotStateResult<ValueType>(pivotState.state, pivotState.distance,
                                                           pivotState.lower ? std::move(lowerPredecessors) : std::move(upperPredecessors));
            }
        }

        // Determine whether the current state is a pivot state.
        bool isPivotState = false;
        if (minStrategyPair.getPlayer1Strategy().hasDefinedChoice(currentState)) {
            uint64_t player2Successor = minStrategyPair.getPlayer1Strategy().getChoice(currentState);
            if (minStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2Successor) &&
                maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2Successor) &&
                minStrategyPair.getPlayer2Strategy().getChoice(player2Successor) != maxStrategyPair.getPlayer2Strategy().getChoice(player2Successor)) {
                isPivotState = true;
            }
        }
        if (!isPivotState && maxStrategyPair.getPlayer1Strategy().hasDefinedChoice(currentState)) {
            uint64_t player2Successor = maxStrategyPair.getPlayer1Strategy().getChoice(currentState);
            if (minStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2Successor) &&
                maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2Successor) &&
                minStrategyPair.getPlayer2Strategy().getChoice(player2Successor) != maxStrategyPair.getPlayer2Strategy().getChoice(player2Successor)) {
                isPivotState = true;
            }
        }

        // If it is indeed a pivot state, we can potentially abort the search here.
        if (isPivotState) {
            if (considerDeviation && foundPivotState) {
                ValueType deviationOfCurrentState = (*upperValues)[currentState] - (*lowerValues)[currentState];

                if (deviationOfCurrentState > pivotStateDeviation) {
                    pivotState = currentDijkstraElement;
                    pivotStateDeviation = deviationOfCurrentState;
                    if (pivotSelectionHeuristic == storm::settings::modules::AbstractionSettings::PivotSelectionHeuristic::MaxWeightedDeviation) {
                        // Scale the deviation with the distance (probability) for this heuristic.
                        pivotStateDeviation *= currentDistance;
                    }
                }
            } else if (!foundPivotState) {
                pivotState = currentDijkstraElement;
                foundPivotState = true;
            }

            // If there is no need to look at other deviations, stop here.
            if (!considerDeviation) {
                return ExplicitPivotStateResult<ValueType>(pivotState.state, pivotState.distance,
                                                           pivotState.lower ? std::move(lowerPredecessors) : std::move(upperPredecessors));
            }
        }

        // We only need to search further if the state has some value deviation.
        if (!lowerValues || !upperValues || (*lowerValues)[currentState] < (*upperValues)[currentState]) {
            if (currentLower) {
                performDijkstraStep(dijkstraQueue, probabilityDistances, lowerDistances, lowerPredecessors, generatePredecessors, true, currentState,
                                    currentDistance, isPivotState, minStrategyPair, maxStrategyPair, player1Labeling, player2Grouping, transitionMatrix,
                                    targetStates, relevantStates);
            } else {
                performDijkstraStep(dijkstraQueue, probabilityDistances, upperDistances, upperPredecessors, generatePredecessors, false, currentState,
                                    currentDistance, isPivotState, maxStrategyPair, minStrategyPair, player1Labeling, player2Grouping, transitionMatrix,
                                    targetStates, relevantStates);
            }
        }
    }

    if (foundPivotState) {
        return ExplicitPivotStateResult<ValueType>(pivotState.state, pivotState.distance,
                                                   pivotState.lower ? std::move(lowerPredecessors) : std::move(upperPredecessors));
    }

    // If we arrived at this point, we have explored all relevant states, but none of them was a pivot state,
    // which can happen when trying to refine using the qualitative result only.
    STORM_LOG_TRACE("Did not find pivot state in explicit Dijkstra search.");
    return boost::none;
}

template<storm::dd::DdType Type, typename ValueType>
boost::optional<RefinementPredicates> MenuGameRefiner<Type, ValueType>::derivePredicatesFromInterpolationReversedPath(
    storm::dd::Odd const& odd, storm::expressions::ExpressionManager& interpolationManager, std::vector<uint64_t> const& reversedPath,
    std::vector<uint64_t> const& stateToOffset, std::vector<uint64_t> const& reversedLabels) const {
    // Build the trace of the most probable path in terms of which predicates hold in each step.
    auto start = std::chrono::high_resolution_clock::now();
    std::pair<std::vector<std::vector<storm::expressions::Expression>>, std::map<storm::expressions::Variable, storm::expressions::Expression>>
        traceAndVariableSubstitution = buildTraceFromReversedLabeledPath(interpolationManager, reversedPath, reversedLabels, odd, &stateToOffset);
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_DEBUG("Building the trace and variable substitution for interpolation from explicit most-probable paths result took "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    return derivePredicatesFromInterpolationFromTrace(interpolationManager, traceAndVariableSubstitution.first, traceAndVariableSubstitution.second);
}

template<storm::dd::DdType Type, typename ValueType>
bool MenuGameRefiner<Type, ValueType>::refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Odd const& odd,
                                              storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                              [[maybe_unused]] std::vector<uint64_t> const& player1Grouping, std::vector<uint64_t> const& player1Labeling,
                                              std::vector<uint64_t> const& player2Labeling, storm::storage::BitVector const& initialStates,
                                              storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                                              ExplicitQualitativeGameResultMinMax const& qualitativeResult,
                                              storage::ExplicitGameStrategyPair const& minStrategyPair,
                                              storage::ExplicitGameStrategyPair const& maxStrategyPair) const {
    STORM_LOG_ASSERT(constraintStates.full(), "Constraint states are ignored. Likely buggy.");
    (void)constraintStates;  // Disable warning, in debug mode we monitor wrong usage of this method.
    // Compute the set of states whose result we have for the min and max case.
    storm::storage::BitVector relevantStates = (qualitativeResult.getProb0Min().getStates() | qualitativeResult.getProb1Min().getStates()) &
                                               (qualitativeResult.getProb0Max().getStates() | qualitativeResult.getProb1Max().getStates());

    boost::optional<ExplicitPivotStateResult<ValueType>> optionalPivotStateResult =
        pickPivotState(useInterpolation, pivotSelectionHeuristic, transitionMatrix, player1Labeling, initialStates, relevantStates, targetStates,
                       minStrategyPair, maxStrategyPair);

    // If there was no pivot state, continue the search.
    if (!optionalPivotStateResult) {
        STORM_LOG_TRACE("Did not find pivot state in qualitative fragment.");
        return false;
    }

    // Otherwise, we can refine.
    auto const& pivotStateResult = optionalPivotStateResult.get();
    STORM_LOG_TRACE("Found pivot state " << pivotStateResult.pivotState << " with distance " << pivotStateResult.distance << ".");

    // Translate the explicit states/choices to the symbolic ones, so we can reuse the predicate derivation techniques.
    auto const& abstractionInformation = abstractor.get().getAbstractionInformation();
    uint64_t pivotState = pivotStateResult.pivotState;
    storm::dd::Bdd<Type> symbolicPivotState = storm::dd::Bdd<Type>::getEncoding(game.getManager(), pivotState, odd, game.getRowVariables());
    storm::dd::Bdd<Type> minPlayer1Strategy = game.getManager().getBddZero();
    storm::dd::Bdd<Type> maxPlayer1Strategy = game.getManager().getBddZero();
    storm::dd::Bdd<Type> minPlayer2Strategy = game.getManager().getBddZero();
    storm::dd::Bdd<Type> maxPlayer2Strategy = game.getManager().getBddZero();
    if (minStrategyPair.getPlayer1Strategy().hasDefinedChoice(pivotState)) {
        uint64_t player2State = minStrategyPair.getPlayer1Strategy().getChoice(pivotState);
        STORM_LOG_ASSERT(player1Grouping[pivotState] <= player2State && player2State < player1Grouping[pivotState + 1], "Illegal choice for player 1.");
        minPlayer1Strategy |=
            symbolicPivotState && abstractionInformation.encodePlayer1Choice(player1Labeling[player2State], abstractionInformation.getPlayer1VariableCount());

        if (minStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = minStrategyPair.getPlayer2Strategy().getChoice(player2State);
            minPlayer2Strategy |=
                minPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
        if (maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(player2State);
            maxPlayer2Strategy |=
                minPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
    }
    if (maxStrategyPair.getPlayer1Strategy().hasDefinedChoice(pivotState)) {
        uint64_t player2State = maxStrategyPair.getPlayer1Strategy().getChoice(pivotState);
        STORM_LOG_ASSERT(player1Grouping[pivotState] <= player2State && player2State < player1Grouping[pivotState + 1], "Illegal choice for player 1.");
        maxPlayer1Strategy |=
            symbolicPivotState && abstractionInformation.encodePlayer1Choice(player1Labeling[player2State], abstractionInformation.getPlayer1VariableCount());

        if (minStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = minStrategyPair.getPlayer2Strategy().getChoice(player2State);
            minPlayer2Strategy |=
                maxPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
        if (maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(player2State);
            maxPlayer2Strategy |=
                maxPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
    }
    boost::optional<RefinementPredicates> predicates;
    if (useInterpolation) {
        predicates = derivePredicatesFromInterpolation(game, pivotStateResult, odd);
    }
    if (!predicates) {
        predicates = derivePredicatesFromPivotState(game, symbolicPivotState, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy, maxPlayer2Strategy);
    }
    STORM_LOG_THROW(static_cast<bool>(predicates), storm::exceptions::InvalidStateException, "Predicates needed to continue.");

    std::vector<storm::expressions::Expression> preparedPredicates = preprocessPredicates(predicates.get().getPredicates(), predicates.get().getSource());
    performRefinement(createGlobalRefinement(preparedPredicates));
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
bool MenuGameRefiner<Type, ValueType>::refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Odd const& odd,
                                              storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                              [[maybe_unused]] std::vector<uint64_t> const& player1Grouping, std::vector<uint64_t> const& player1Labeling,
                                              std::vector<uint64_t> const& player2Labeling, storm::storage::BitVector const& initialStates,
                                              storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                                              ExplicitQuantitativeResultMinMax<ValueType> const& quantitativeResult,
                                              storage::ExplicitGameStrategyPair const& minStrategyPair,
                                              storage::ExplicitGameStrategyPair const& maxStrategyPair) const {
    STORM_LOG_ASSERT(constraintStates.full(), "Constraint states are ignored. Likely buggy.");
    (void)constraintStates;  // Disable warning, in debug mode we monitor wrong usage of this method.

    // Compute the set of states whose result we have for the min and max case.
    storm::storage::BitVector relevantStates(odd.getTotalOffset(), true);

    boost::optional<ExplicitPivotStateResult<ValueType>> optionalPivotStateResult =
        pickPivotState(useInterpolation, pivotSelectionHeuristic, transitionMatrix, player1Labeling, initialStates, relevantStates, targetStates,
                       minStrategyPair, maxStrategyPair, &quantitativeResult.getMin().getValues(), &quantitativeResult.getMax().getValues());

    STORM_LOG_THROW(optionalPivotStateResult, storm::exceptions::InvalidStateException, "Did not find pivot state to proceed.");

    // Otherwise, we can refine.
    auto const& pivotStateResult = optionalPivotStateResult.get();
    STORM_LOG_TRACE("Found pivot state " << pivotStateResult.pivotState << " with distance " << pivotStateResult.distance << ".");

    // Translate the explicit states/choices to the symbolic ones, so we can reuse the predicate derivation techniques.
    auto const& abstractionInformation = abstractor.get().getAbstractionInformation();
    uint64_t pivotState = pivotStateResult.pivotState;
    storm::dd::Bdd<Type> symbolicPivotState = storm::dd::Bdd<Type>::getEncoding(game.getManager(), pivotState, odd, game.getRowVariables());
    storm::dd::Bdd<Type> minPlayer1Strategy = game.getManager().getBddZero();
    storm::dd::Bdd<Type> maxPlayer1Strategy = game.getManager().getBddZero();
    storm::dd::Bdd<Type> minPlayer2Strategy = game.getManager().getBddZero();
    storm::dd::Bdd<Type> maxPlayer2Strategy = game.getManager().getBddZero();
    if (minStrategyPair.getPlayer1Strategy().hasDefinedChoice(pivotState)) {
        uint64_t player2State = minStrategyPair.getPlayer1Strategy().getChoice(pivotState);
        STORM_LOG_ASSERT(player1Grouping[pivotState] <= player2State && player2State < player1Grouping[pivotState + 1], "Illegal choice for player 1.");
        minPlayer1Strategy |=
            symbolicPivotState && abstractionInformation.encodePlayer1Choice(player1Labeling[player2State], abstractionInformation.getPlayer1VariableCount());

        if (minStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = minStrategyPair.getPlayer2Strategy().getChoice(player2State);
            minPlayer2Strategy |=
                minPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
        if (maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(player2State);
            maxPlayer2Strategy |=
                minPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
    }
    if (maxStrategyPair.getPlayer1Strategy().hasDefinedChoice(pivotState)) {
        uint64_t player2State = maxStrategyPair.getPlayer1Strategy().getChoice(pivotState);
        STORM_LOG_ASSERT(player1Grouping[pivotState] <= player2State && player2State < player1Grouping[pivotState + 1], "Illegal choice for player 1.");
        maxPlayer1Strategy |=
            symbolicPivotState && abstractionInformation.encodePlayer1Choice(player1Labeling[player2State], abstractionInformation.getPlayer1VariableCount());

        if (minStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = minStrategyPair.getPlayer2Strategy().getChoice(player2State);
            minPlayer2Strategy |=
                maxPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
        if (maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(player2State)) {
            uint64_t player2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(player2State);
            maxPlayer2Strategy |=
                maxPlayer1Strategy && abstractionInformation.encodePlayer2Choice(player2Labeling[player2Choice], 0, game.getPlayer2Variables().size());
        }
    }

    boost::optional<RefinementPredicates> predicates;
    if (useInterpolation) {
        predicates = derivePredicatesFromInterpolation(game, pivotStateResult, odd);
    }
    if (!predicates) {
        predicates = derivePredicatesFromPivotState(game, symbolicPivotState, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy, maxPlayer2Strategy);
    }
    STORM_LOG_THROW(static_cast<bool>(predicates), storm::exceptions::InvalidStateException, "Predicates needed to continue.");

    std::vector<storm::expressions::Expression> preparedPredicates = preprocessPredicates(predicates.get().getPredicates(), predicates.get().getSource());
    performRefinement(createGlobalRefinement(preparedPredicates));
    return true;
}

template<storm::dd::DdType Type, typename ValueType>
bool MenuGameRefiner<Type, ValueType>::refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& transitionMatrixBdd,
                                              SymbolicQuantitativeGameResultMinMax<Type, ValueType> const& quantitativeResult) const {
    STORM_LOG_TRACE("Refining after quantitative check.");
    // Get all relevant strategies.
    storm::dd::Bdd<Type> minPlayer1Strategy = quantitativeResult.min.getPlayer1Strategy();
    storm::dd::Bdd<Type> minPlayer2Strategy = quantitativeResult.min.getPlayer2Strategy();
    storm::dd::Bdd<Type> maxPlayer1Strategy = quantitativeResult.max.getPlayer1Strategy();
    storm::dd::Bdd<Type> maxPlayer2Strategy = quantitativeResult.max.getPlayer2Strategy();

    // Compute all reached pivot states.
    PivotStateCandidatesResult<Type> pivotStateCandidatesResult =
        computePivotStates(game, transitionMatrixBdd, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy, maxPlayer2Strategy);

    STORM_LOG_ASSERT(!pivotStateCandidatesResult.pivotStates.isZero(), "Unable to refine without pivot state candidates.");

    // Now that we have the pivot state candidates, we need to pick one.
    SymbolicPivotStateResult<Type, ValueType> symbolicPivotStateResult =
        pickPivotState<Type, ValueType>(pivotSelectionHeuristic, game, pivotStateCandidatesResult, boost::none, quantitativeResult);

    boost::optional<RefinementPredicates> predicates;
    if (useInterpolation) {
        predicates =
            derivePredicatesFromInterpolation(game, symbolicPivotStateResult, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy, maxPlayer2Strategy);
    }
    if (predicates) {
        STORM_LOG_TRACE("Obtained predicates by interpolation.");
    } else {
        predicates = derivePredicatesFromPivotState(game, symbolicPivotStateResult.pivotState, minPlayer1Strategy, minPlayer2Strategy, maxPlayer1Strategy,
                                                    maxPlayer2Strategy);
    }
    STORM_LOG_THROW(static_cast<bool>(predicates), storm::exceptions::InvalidStateException, "Predicates needed to continue.");

    std::vector<storm::expressions::Expression> preparedPredicates = preprocessPredicates(predicates.get().getPredicates(), predicates.get().getSource());
    performRefinement(createGlobalRefinement(preparedPredicates));
    return true;
}

struct VariableSetHash {
    std::size_t operator()(std::set<storm::expressions::Variable> const& set) const {
        return set.size();
    }
};

template<storm::dd::DdType Type, typename ValueType>
std::vector<storm::expressions::Expression> MenuGameRefiner<Type, ValueType>::preprocessPredicates(
    std::vector<storm::expressions::Expression> const& predicates, RefinementPredicates::Source const& source) const {
    bool split = source == RefinementPredicates::Source::WeakestPrecondition && splitPredicates;
    split |= source == RefinementPredicates::Source::Interpolation && splitPredicates;
    split |= splitAll;

    if (split) {
        auto start = std::chrono::high_resolution_clock::now();
        AbstractionInformation<Type> const& abstractionInformation = abstractor.get().getAbstractionInformation();
        std::vector<storm::expressions::Expression> cleanedAtoms;

        std::unordered_map<std::set<storm::expressions::Variable>, std::vector<storm::expressions::Expression>, VariableSetHash> predicateClasses;

        for (auto const& predicate : predicates) {
            // Split the predicates.
            std::vector<storm::expressions::Expression> atoms = splitter.split(predicate);

            // Put the atoms into the right class.
            for (auto const& atom : atoms) {
                std::set<storm::expressions::Variable> vars = atom.getVariables();
                predicateClasses[vars].push_back(atom);
            }
        }

        // Now clean the classes in the sense that redundant predicates are cleaned.
        uint64_t checkCounter = 0;
        for (auto& predicateClass : predicateClasses) {
            std::vector<storm::expressions::Expression> cleanedAtomsOfClass;

            for (auto const& predicate : predicateClass.second) {
                bool addPredicate = true;
                for (auto const& atom : cleanedAtomsOfClass) {
                    if (predicate.areSame(atom)) {
                        addPredicate = false;
                        break;
                    }

                    ++checkCounter;
                    if (equivalenceChecker.areEquivalentModuloNegation(predicate, atom)) {
                        addPredicate = false;
                        break;
                    }
                }
                if (addPredicate) {
                    cleanedAtomsOfClass.push_back(predicate);
                }
            }

            predicateClass.second = std::move(cleanedAtomsOfClass);
        }

        std::unordered_map<std::set<storm::expressions::Variable>, std::vector<storm::expressions::Expression>, VariableSetHash> oldPredicateClasses;
        for (auto const& oldPredicate : abstractionInformation.getPredicates()) {
            std::set<storm::expressions::Variable> vars = oldPredicate.getVariables();

            oldPredicateClasses[vars].push_back(oldPredicate);
        }

        for (auto const& predicateClass : predicateClasses) {
            auto oldPredicateClassIt = oldPredicateClasses.find(predicateClass.first);
            if (oldPredicateClassIt != oldPredicateClasses.end()) {
                for (auto const& newAtom : predicateClass.second) {
                    bool addAtom = true;
                    for (auto const& oldPredicate : oldPredicateClassIt->second) {
                        if (newAtom.areSame(oldPredicate)) {
                            addAtom = false;
                            break;
                        }
                        ++checkCounter;
                        if (equivalenceChecker.areEquivalentModuloNegation(newAtom, oldPredicate)) {
                            addAtom = false;
                            break;
                        }
                    }
                    if (addAtom) {
                        cleanedAtoms.push_back(newAtom);
                    }
                }
            } else {
                cleanedAtoms.insert(cleanedAtoms.end(), predicateClass.second.begin(), predicateClass.second.end());
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        STORM_LOG_TRACE("Preprocessing predicates took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms (" << checkCounter
                                                         << " checks).");

        return cleanedAtoms;
    } else {
        // If no splitting of the predicates is required, just forward the refinement request to the abstractor.
    }

    return predicates;
}

template<storm::dd::DdType Type, typename ValueType>
std::vector<RefinementCommand> MenuGameRefiner<Type, ValueType>::createGlobalRefinement(std::vector<storm::expressions::Expression> const& predicates) const {
    std::vector<RefinementCommand> commands;
    commands.emplace_back(predicates);
    return commands;
}

template<storm::dd::DdType Type, typename ValueType>
void MenuGameRefiner<Type, ValueType>::performRefinement(std::vector<RefinementCommand> const& refinementCommands, bool allowInjection) const {
    if (!refinementPredicatesToInject.empty() && allowInjection) {
        STORM_LOG_INFO("Refining with (injected) predicates.");

        for (auto const& predicate : refinementPredicatesToInject.back()) {
            STORM_LOG_INFO(predicate);
        }

        abstractor.get().refine(RefinementCommand(refinementPredicatesToInject.back()));
        refinementPredicatesToInject.pop_back();
    } else {
        for (auto const& command : refinementCommands) {
            STORM_LOG_INFO("Refining with " << command.getPredicates().size() << " predicates.");
            for (auto const& predicate : command.getPredicates()) {
                STORM_LOG_INFO(predicate);
            }
            if (!command.getPredicates().empty()) {
                abstractor.get().refine(command);
            }
        }
    }

    STORM_LOG_TRACE("Current set of predicates:");
    for (auto const& predicate : abstractor.get().getAbstractionInformation().getPredicates()) {
        STORM_LOG_TRACE(predicate);
    }
}

template<storm::dd::DdType Type, typename ValueType>
bool MenuGameRefiner<Type, ValueType>::addedAllGuards() const {
    return addedAllGuardsFlag;
}

template class MenuGameRefiner<storm::dd::DdType::CUDD, double>;
template class MenuGameRefiner<storm::dd::DdType::Sylvan, double>;
template class MenuGameRefiner<storm::dd::DdType::Sylvan, storm::RationalNumber>;

}  // namespace abstraction
}  // namespace storm::gbar
