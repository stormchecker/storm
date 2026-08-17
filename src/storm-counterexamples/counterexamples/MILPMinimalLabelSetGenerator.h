#pragma once

#include <chrono>

#include "storm-counterexamples/counterexamples/GuaranteedLabelSet.h"
#include "storm-counterexamples/counterexamples/HighLevelCounterexample.h"
#include "storm/settings/modules/CounterexampleGeneratorSettings.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/solver/LpSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/storage/BoostTypes.h"
#include "storm/storage/prism/Program.h"
#include "storm/storage/sparse/JaniChoiceOrigins.h"
#include "storm/storage/sparse/PrismChoiceOrigins.h"
#include "storm/utility/graph.h"
#include "storm/utility/solver.h"

namespace storm {

class Environment;

namespace counterexamples {

/*!
 * This class provides functionality to generate a minimal counterexample to a probabilistic reachability
 * property in terms of used labels.
 */
template<class T>
class MILPMinimalLabelSetGenerator {
   private:
    /*!
     * A helper class that provides the functionality to compute a hash value for pairs of state indices.
     */
    class PairHash {
       public:
        std::size_t operator()(std::pair<uint_fast64_t, uint_fast64_t> const& pair) const {
            size_t seed = 0;
            boost::hash_combine(seed, pair.first);
            boost::hash_combine(seed, pair.second);
            return seed;
        }
    };

    /*!
     * A helper struct storing which states are relevant or problematic.
     */
    struct StateInformation {
        storm::storage::BitVector relevantStates;
        storm::storage::BitVector problematicStates;
    };

    /*!
     * A helper struct capturing information about relevant and problematic choices of states and which labels
     * are relevant.
     */
    struct ChoiceInformation {
        std::unordered_map<uint_fast64_t, std::list<uint_fast64_t>> relevantChoicesForRelevantStates;
        std::unordered_map<uint_fast64_t, std::list<uint_fast64_t>> problematicChoicesForProblematicStates;
        storm::storage::FlatSet<uint_fast64_t> allRelevantLabels;
        storm::storage::FlatSet<uint_fast64_t> knownLabels;
    };

    /*!
     * A helper struct capturing information about the variables of the MILP formulation.
     */
    struct VariableInformation {
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> labelToVariableMap;
        std::unordered_map<uint_fast64_t, std::list<storm::expressions::Variable>> stateToChoiceVariablesMap;
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> initialStateToChoiceVariableMap;
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> stateToProbabilityVariableMap;
        storm::expressions::Variable virtualInitialStateVariable;
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> problematicStateToVariableMap;
        std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable, PairHash> problematicTransitionToVariableMap;
        uint_fast64_t numberOfVariables;

        VariableInformation() : numberOfVariables(0) {}
    };

    /*!
     * Determines the relevant and the problematic states of the given MDP with respect to the given phi and psi
     * state sets. The relevant states are those for which there exists at least one scheduler that attains a
     * non-zero probability of satisfying phi until psi. Problematic states are relevant states that have at
     * least one scheduler such that the probability of satisfying phi until psi is zero.
     *
     * @param mdp The MDP whose states to search.
     * @param phiStates A bit vector characterizing all states satisfying phi.
     * @param psiStates A bit vector characterizing all states satisfying psi.
     * @return A structure that stores the relevant and problematic states.
     */
    static StateInformation determineRelevantAndProblematicStates(storm::models::sparse::Mdp<T> const& mdp, storm::storage::BitVector const& phiStates,
                                                                  storm::storage::BitVector const& psiStates) {
        StateInformation result;
        result.relevantStates = storm::utility::graph::performProbGreater0E(mdp.getBackwardTransitions(), phiStates, psiStates);
        result.relevantStates &= ~psiStates;
        result.problematicStates = storm::utility::graph::performProb0E(mdp.getTransitionMatrix(), mdp.getNondeterministicChoiceIndices(),
                                                                        mdp.getBackwardTransitions(), phiStates, psiStates);
        result.problematicStates &= result.relevantStates;
        STORM_LOG_DEBUG("Found " << phiStates.getNumberOfSetBits() << " filter states.");
        STORM_LOG_DEBUG("Found " << psiStates.getNumberOfSetBits() << " target states.");
        STORM_LOG_DEBUG("Found " << result.relevantStates.getNumberOfSetBits() << " relevant states.");
        STORM_LOG_DEBUG("Found " << result.problematicStates.getNumberOfSetBits() << " problematic states.");
        return result;
    }

    /*!
     * Determines the relevant and problematic choices of the given MDP with respect to the given parameters.
     *
     * @param mdp The MDP whose choices to search.
     * @param stateInformation The relevant and problematic states of the model.
     * @param psiStates A bit vector characterizing the psi states in the model.
     * @return A structure that stores the relevant and problematic choices in the model as well as the set
     * of relevant labels.
     */
    static ChoiceInformation determineRelevantAndProblematicChoices(storm::models::sparse::Mdp<T> const& mdp,
                                                                    std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                                                    StateInformation const& stateInformation, storm::storage::BitVector const& psiStates) {
        // Create result and shortcuts to needed data for convenience.
        ChoiceInformation result;
        storm::storage::SparseMatrix<T> const& transitionMatrix = mdp.getTransitionMatrix();
        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = mdp.getNondeterministicChoiceIndices();

        // Now traverse all choices of all relevant states and check whether there is a relevant target state.
        // If so, the associated labels become relevant. Also, if a choice of relevant state has at least one
        // relevant successor, the choice is considered to be relevant.
        for (auto state : stateInformation.relevantStates) {
            result.relevantChoicesForRelevantStates.emplace(state, std::list<uint_fast64_t>());
            if (stateInformation.problematicStates.get(state)) {
                result.problematicChoicesForProblematicStates.emplace(state, std::list<uint_fast64_t>());
            }
            for (uint_fast64_t row = nondeterministicChoiceIndices[state]; row < nondeterministicChoiceIndices[state + 1]; ++row) {
                bool currentChoiceRelevant = false;
                bool allSuccessorsProblematic = true;
                for (auto const& successorEntry : transitionMatrix.getRow(row)) {
                    // If there is a relevant successor, we need to add the labels of the current choice.
                    if (stateInformation.relevantStates.get(successorEntry.getColumn()) || psiStates.get(successorEntry.getColumn())) {
                        for (auto const& label : labelSets[row]) {
                            result.allRelevantLabels.insert(label);
                        }
                        if (!currentChoiceRelevant) {
                            currentChoiceRelevant = true;
                            result.relevantChoicesForRelevantStates[state].push_back(row);
                        }
                    }
                    if (!stateInformation.problematicStates.get(successorEntry.getColumn())) {
                        allSuccessorsProblematic = false;
                    }
                }

                // If all successors of a problematic state are problematic themselves, we record this choice
                // as being problematic.
                if (stateInformation.problematicStates.get(state) && allSuccessorsProblematic) {
                    result.problematicChoicesForProblematicStates[state].push_back(row);
                }
            }
        }

        // Finally, determine the set of labels that are known to be taken.
        result.knownLabels = storm::counterexamples::getGuaranteedLabelSet(mdp, labelSets, psiStates, result.allRelevantLabels);
        STORM_LOG_DEBUG("Found " << result.allRelevantLabels.size() << " relevant labels and " << result.knownLabels.size() << " known labels.");

        return result;
    }

    /*!
     * Creates the variables for the labels of the model.
     *
     * @param solver The MILP solver.
     * @param relevantLabels The set of relevant labels of the model.
     * @return A mapping from labels to variable indices.
     */
    static std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> createLabelVariables(
        storm::solver::LpSolver<double>& solver, storm::storage::FlatSet<uint_fast64_t> const& relevantLabels) {
        std::stringstream variableNameBuffer;
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> resultingMap;
        for (auto const& label : relevantLabels) {
            variableNameBuffer.str("");
            variableNameBuffer.clear();
            variableNameBuffer << "label" << label;
            resultingMap[label] = solver.addBinaryVariable(variableNameBuffer.str(), 1);
        }
        return std::make_pair(resultingMap, relevantLabels.size());
    }

    /*!
     * Creates the variables for the relevant choices in the model.
     *
     * @param solver The MILP solver.
     * @param stateInformation The information about the states of the model.
     * @param choiceInformation The information about the choices of the model.
     * @return A mapping from states to a list of choice variable indices.
     */
    static std::pair<std::unordered_map<uint_fast64_t, std::list<storm::expressions::Variable>>, uint_fast64_t> createSchedulerVariables(
        storm::solver::LpSolver<double>& solver, StateInformation const& stateInformation, ChoiceInformation const& choiceInformation) {
        std::stringstream variableNameBuffer;
        uint_fast64_t numberOfVariablesCreated = 0;
        std::unordered_map<uint_fast64_t, std::list<storm::expressions::Variable>> resultingMap;

        for (auto state : stateInformation.relevantStates) {
            resultingMap.emplace(state, std::list<storm::expressions::Variable>());
            std::list<uint_fast64_t> const& relevantChoicesForState = choiceInformation.relevantChoicesForRelevantStates.at(state);
            for (uint_fast64_t row : relevantChoicesForState) {
                variableNameBuffer.str("");
                variableNameBuffer.clear();
                variableNameBuffer << "choice" << row << "in" << state;
                resultingMap[state].push_back(solver.addBinaryVariable(variableNameBuffer.str()));
                ++numberOfVariablesCreated;
            }
        }
        return std::make_pair(resultingMap, numberOfVariablesCreated);
    }

    /*!
     * Creates the variables needed for encoding the nondeterministic selection of one of the initial states
     * in the model.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param stateInformation The information about the states of the model.
     * @return A mapping from initial states to choice variable indices.
     */
    static std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> createInitialChoiceVariables(
        storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp, StateInformation const& stateInformation) {
        std::stringstream variableNameBuffer;
        uint_fast64_t numberOfVariablesCreated = 0;
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> resultingMap;

        for (auto initialState : mdp.getStates("init")) {
            // Only consider this initial state if it is relevant.
            if (stateInformation.relevantStates.get(initialState)) {
                variableNameBuffer.str("");
                variableNameBuffer.clear();
                variableNameBuffer << "init" << initialState;
                resultingMap[initialState] = solver.addBinaryVariable(variableNameBuffer.str());
                ++numberOfVariablesCreated;
            }
        }
        return std::make_pair(resultingMap, numberOfVariablesCreated);
    }

    /*!
     * Creates the variables for the probabilities in the model.
     *
     * @param solver The MILP solver.
     * @param stateInformation The information about the states in the model.
     * @return A mapping from states to the index of the corresponding probability variables.
     */
    static std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> createProbabilityVariables(
        storm::solver::LpSolver<double>& solver, StateInformation const& stateInformation) {
        std::stringstream variableNameBuffer;
        uint_fast64_t numberOfVariablesCreated = 0;
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> resultingMap;

        for (auto state : stateInformation.relevantStates) {
            variableNameBuffer.str("");
            variableNameBuffer.clear();
            variableNameBuffer << "p" << state;
            resultingMap[state] = solver.addBoundedContinuousVariable(variableNameBuffer.str(), 0, 1);
            ++numberOfVariablesCreated;
        }
        return std::make_pair(resultingMap, numberOfVariablesCreated);
    }

    /*!
     * Creates the variable for the probability of the virtual initial state.
     *
     * @param solver The MILP solver.
     * @return The index of the variable for the probability of the virtual initial state.
     */
    static std::pair<storm::expressions::Variable, uint_fast64_t> createVirtualInitialStateVariable(storm::solver::LpSolver<double>& solver) {
        std::stringstream variableNameBuffer;
        variableNameBuffer << "pinit";
        storm::expressions::Variable variable = solver.addBoundedContinuousVariable(variableNameBuffer.str(), 0, 1);
        return std::make_pair(variable, 1);
    }

    /*!
     * Creates the variables for the problematic states in the model.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param stateInformation The information about the states in the model.
     * @return A mapping from problematic states to the index of the corresponding variables.
     */
    static std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> createProblematicStateVariables(
        storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp, StateInformation const& stateInformation,
        ChoiceInformation const& choiceInformation) {
        std::stringstream variableNameBuffer;
        uint_fast64_t numberOfVariablesCreated = 0;
        std::unordered_map<uint_fast64_t, storm::expressions::Variable> resultingMap;

        for (auto state : stateInformation.problematicStates) {
            // First check whether there is not already a variable for this state and advance to the next state
            // in this case.
            if (resultingMap.find(state) == resultingMap.end()) {
                variableNameBuffer.str("");
                variableNameBuffer.clear();
                variableNameBuffer << "r" << state;
                resultingMap[state] = solver.addBoundedContinuousVariable(variableNameBuffer.str(), 0, 1);
                ++numberOfVariablesCreated;
            }

            std::list<uint_fast64_t> const& relevantChoicesForState = choiceInformation.relevantChoicesForRelevantStates.at(state);
            for (uint_fast64_t row : relevantChoicesForState) {
                for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(row)) {
                    if (stateInformation.relevantStates.get(successorEntry.getColumn())) {
                        if (resultingMap.find(successorEntry.getColumn()) == resultingMap.end()) {
                            variableNameBuffer.str("");
                            variableNameBuffer.clear();
                            variableNameBuffer << "r" << successorEntry.getColumn();
                            resultingMap[successorEntry.getColumn()] = solver.addBoundedContinuousVariable(variableNameBuffer.str(), 0, 1);
                            ++numberOfVariablesCreated;
                        }
                    }
                }
            }
        }
        return std::make_pair(resultingMap, numberOfVariablesCreated);
    }

    /*!
     * Creates the variables for the problematic choices in the model.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param stateInformation The information about the states in the model.
     * @param choiceInformation The information about the choices in the model.
     * @return A mapping from problematic choices to the index of the corresponding variables.
     */
    static std::pair<std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable, PairHash>, uint_fast64_t>
    createProblematicChoiceVariables(storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp,
                                     StateInformation const& stateInformation, ChoiceInformation const& choiceInformation) {
        std::stringstream variableNameBuffer;
        uint_fast64_t numberOfVariablesCreated = 0;
        std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable, PairHash> resultingMap;

        for (auto state : stateInformation.problematicStates) {
            std::list<uint_fast64_t> const& relevantChoicesForState = choiceInformation.relevantChoicesForRelevantStates.at(state);
            for (uint_fast64_t row : relevantChoicesForState) {
                for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(row)) {
                    if (stateInformation.relevantStates.get(successorEntry.getColumn()) &&
                        resultingMap.find(std::make_pair(state, successorEntry.getColumn())) == resultingMap.end()) {
                        variableNameBuffer.str("");
                        variableNameBuffer.clear();
                        variableNameBuffer << "t" << state << "to" << successorEntry.getColumn();
                        resultingMap[std::make_pair(state, successorEntry.getColumn())] = solver.addBinaryVariable(variableNameBuffer.str());
                        ++numberOfVariablesCreated;
                    }
                }
            }
        }
        return std::make_pair(resultingMap, numberOfVariablesCreated);
    }

    /*!
     * Creates all variables needed to encode the problem as an MILP problem and returns a struct containing
     * information about the variables that were created. This implicitly establishes the objective function
     * passed to the solver.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param stateInformation The information about the states in the model.
     * @param choiceInformation The information about the choices in the model.
     */
    static VariableInformation createVariables(storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp,
                                               StateInformation const& stateInformation, ChoiceInformation const& choiceInformation) {
        // Create a struct that stores all information about variables.
        VariableInformation result;

        // Create variables for involved labels.
        std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> labelVariableResult =
            createLabelVariables(solver, choiceInformation.allRelevantLabels);
        result.labelToVariableMap = std::move(labelVariableResult.first);
        result.numberOfVariables += labelVariableResult.second;
        STORM_LOG_DEBUG("Created variables for labels.");

        // Create scheduler variables for relevant states and their actions.
        std::pair<std::unordered_map<uint_fast64_t, std::list<storm::expressions::Variable>>, uint_fast64_t> schedulerVariableResult =
            createSchedulerVariables(solver, stateInformation, choiceInformation);
        result.stateToChoiceVariablesMap = std::move(schedulerVariableResult.first);
        result.numberOfVariables += schedulerVariableResult.second;
        STORM_LOG_DEBUG("Created variables for nondeterministic choices.");

        // Create scheduler variables for nondeterministically choosing an initial state.
        std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> initialChoiceVariableResult =
            createInitialChoiceVariables(solver, mdp, stateInformation);
        result.initialStateToChoiceVariableMap = std::move(initialChoiceVariableResult.first);
        result.numberOfVariables += initialChoiceVariableResult.second;
        STORM_LOG_DEBUG("Created variables for the nondeterministic choice of the initial state.");

        // Create variables for probabilities for all relevant states.
        std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> probabilityVariableResult =
            createProbabilityVariables(solver, stateInformation);
        result.stateToProbabilityVariableMap = std::move(probabilityVariableResult.first);
        result.numberOfVariables += probabilityVariableResult.second;
        STORM_LOG_DEBUG("Created variables for the reachability probabilities.");

        // Create a probability variable for a virtual initial state that nondeterministically chooses one of the system's real initial states as its target
        // state.
        std::pair<storm::expressions::Variable, uint_fast64_t> virtualInitialStateVariableResult = createVirtualInitialStateVariable(solver);
        result.virtualInitialStateVariable = virtualInitialStateVariableResult.first;
        result.numberOfVariables += virtualInitialStateVariableResult.second;
        STORM_LOG_DEBUG("Created variables for the virtual initial state.");

        // Create variables for problematic states.
        std::pair<std::unordered_map<uint_fast64_t, storm::expressions::Variable>, uint_fast64_t> problematicStateVariableResult =
            createProblematicStateVariables(solver, mdp, stateInformation, choiceInformation);
        result.problematicStateToVariableMap = std::move(problematicStateVariableResult.first);
        result.numberOfVariables += problematicStateVariableResult.second;
        STORM_LOG_DEBUG("Created variables for the problematic states.");

        // Create variables for problematic choices.
        std::pair<std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable, PairHash>, uint_fast64_t>
            problematicTransitionVariableResult = createProblematicChoiceVariables(solver, mdp, stateInformation, choiceInformation);
        result.problematicTransitionToVariableMap = problematicTransitionVariableResult.first;
        result.numberOfVariables += problematicTransitionVariableResult.second;
        STORM_LOG_DEBUG("Created variables for the problematic choices.");

        // Finally, we need to update the model to make the new variables usable.
        solver.update();
        STORM_LOG_INFO("Successfully created " << result.numberOfVariables << " MILP variables.");

        // Finally, return variable information struct.
        return result;
    }

    /*!
     * Asserts a constraint in the MILP problem that makes sure the reachability probability in the subsystem
     * exceeds the given threshold.
     *
     * @param solver The MILP solver.
     * @param variableInformation A struct with information about the variables of the model.
     * @param probabilityThreshold The probability that the subsystem must exceed.
     * @param strictBound A flag indicating whether the threshold must be exceeded or only matched.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertProbabilityGreaterThanThreshold(storm::solver::LpSolver<double>& solver, VariableInformation const& variableInformation,
                                                               double probabilityThreshold, bool strictBound) {
        storm::expressions::Expression constraint;
        if (strictBound) {
            constraint = variableInformation.virtualInitialStateVariable > solver.getConstant(probabilityThreshold);
        } else {
            constraint = variableInformation.virtualInitialStateVariable >= solver.getConstant(probabilityThreshold);
        }
        solver.addConstraint("ProbGreaterThreshold", constraint);
        return 1;
    }

    /*!
     * Asserts constraints that make sure the selected policy is valid, i.e. chooses at most one action in each state.
     *
     * @param solver The MILP solver.
     * @param stateInformation The information about the states in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertValidPolicy(storm::solver::LpSolver<double>& solver, StateInformation const& stateInformation,
                                           VariableInformation const& variableInformation) {
        // Assert that the policy chooses at most one action in each state of the system.
        uint_fast64_t numberOfConstraintsCreated = 0;
        for (auto state : stateInformation.relevantStates) {
            std::list<storm::expressions::Variable> const& choiceVariableIndices = variableInformation.stateToChoiceVariablesMap.at(state);
            storm::expressions::Expression constraint = solver.getConstant(0);

            for (auto const& choiceVariable : choiceVariableIndices) {
                constraint = constraint + choiceVariable;
            }

            constraint = constraint <= solver.getConstant(1);

            solver.addConstraint("ValidPolicy" + std::to_string(numberOfConstraintsCreated), constraint);
            ++numberOfConstraintsCreated;
        }

        // Now assert that the virtual initial state picks exactly one initial state from the system as its
        // successor state.
        storm::expressions::Expression constraint = solver.getConstant(0);
        for (auto const& initialStateVariablePair : variableInformation.initialStateToChoiceVariableMap) {
            constraint = constraint + initialStateVariablePair.second;
        }
        constraint = constraint == solver.getConstant(1);

        solver.addConstraint("VirtualInitialStateChoosesOneInitialState", constraint);
        ++numberOfConstraintsCreated;

        return numberOfConstraintsCreated;
    }

    /*!
     * Asserts constraints that make sure the labels are included in the solution set if the policy selects a
     * choice that originates from the label in question.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param stateInformation The information about the states in the model.
     * @param choiceInformation The information about the choices in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertChoicesImplyLabels(storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const&,
                                                  std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                                  StateInformation const& stateInformation, ChoiceInformation const& choiceInformation,
                                                  VariableInformation const& variableInformation) {
        // mdp argument is unused by retained for consistency over different assertConstraints
        uint_fast64_t numberOfConstraintsCreated = 0;

        for (auto state : stateInformation.relevantStates) {
            std::list<storm::expressions::Variable>::const_iterator choiceVariableIterator = variableInformation.stateToChoiceVariablesMap.at(state).begin();
            for (auto choice : choiceInformation.relevantChoicesForRelevantStates.at(state)) {
                for (auto label : labelSets[choice]) {
                    storm::expressions::Expression constraint =
                        variableInformation.labelToVariableMap.at(label) - *choiceVariableIterator >= solver.getConstant(0);
                    solver.addConstraint("ChoicesImplyLabels" + std::to_string(numberOfConstraintsCreated), constraint);
                    ++numberOfConstraintsCreated;
                }
                ++choiceVariableIterator;
            }
        }
        return numberOfConstraintsCreated;
    }

    /*!
     * Asserts constraints that make sure the reachability probability is zero for states in which the policy
     * does not pick any outgoing action.
     *
     * @param solver The MILP solver.
     * @param stateInformation The information about the states in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertZeroProbabilityWithoutChoice(storm::solver::LpSolver<double>& solver, StateInformation const& stateInformation,
                                                            VariableInformation const& variableInformation) {
        uint_fast64_t numberOfConstraintsCreated = 0;
        for (auto state : stateInformation.relevantStates) {
            storm::expressions::Expression constraint = variableInformation.stateToProbabilityVariableMap.at(state);
            for (auto const& choiceVariable : variableInformation.stateToChoiceVariablesMap.at(state)) {
                constraint = constraint - choiceVariable;
            }
            constraint = constraint <= solver.getConstant(0);
            solver.addConstraint("ProbabilityIsZeroIfNoAction" + std::to_string(numberOfConstraintsCreated), constraint);
            ++numberOfConstraintsCreated;
        }
        return numberOfConstraintsCreated;
    }

    /*!
     * Asserts constraints that encode the correct reachability probabilities for all states.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param psiStates A bit vector characterizing the psi states in the model.
     * @param stateInformation The information about the states in the model.
     * @param choiceInformation The information about the choices in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertReachabilityProbabilities(storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp,
                                                         storm::storage::BitVector const& psiStates, StateInformation const& stateInformation,
                                                         ChoiceInformation const& choiceInformation, VariableInformation const& variableInformation) {
        uint_fast64_t numberOfConstraintsCreated = 0;
        for (auto state : stateInformation.relevantStates) {
            std::list<storm::expressions::Variable>::const_iterator choiceVariableIterator = variableInformation.stateToChoiceVariablesMap.at(state).begin();
            for (auto choice : choiceInformation.relevantChoicesForRelevantStates.at(state)) {
                storm::expressions::Expression constraint = variableInformation.stateToProbabilityVariableMap.at(state);

                double rightHandSide = 1;
                for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(choice)) {
                    if (stateInformation.relevantStates.get(successorEntry.getColumn())) {
                        constraint = constraint - solver.getConstant(successorEntry.getValue()) *
                                                      variableInformation.stateToProbabilityVariableMap.at(successorEntry.getColumn());
                    } else if (psiStates.get(successorEntry.getColumn())) {
                        rightHandSide += successorEntry.getValue();
                    }
                }

                constraint = constraint + *choiceVariableIterator <= solver.getConstant(rightHandSide);
                solver.addConstraint("ReachabilityProbabilities" + std::to_string(numberOfConstraintsCreated), constraint);

                ++numberOfConstraintsCreated;
                ++choiceVariableIterator;
            }
        }

        // Make sure that the virtual initial state is being assigned the probability from the initial state
        // that it selected as a successor state.
        for (auto const& initialStateVariablePair : variableInformation.initialStateToChoiceVariableMap) {
            storm::expressions::Expression constraint = variableInformation.virtualInitialStateVariable -
                                                            variableInformation.stateToProbabilityVariableMap.at(initialStateVariablePair.first) +
                                                            initialStateVariablePair.second <=
                                                        solver.getConstant(1);
            solver.addConstraint("VirtualInitialStateHasCorrectProbability" + std::to_string(numberOfConstraintsCreated), constraint);
            ++numberOfConstraintsCreated;
        }

        return numberOfConstraintsCreated;
    }

    /*!
     * Asserts constraints that make sure an unproblematic state is reachable from each problematic state.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param stateInformation The information about the states in the model.
     * @param choiceInformation The information about the choices in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertUnproblematicStateReachable(storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp,
                                                           StateInformation const& stateInformation, ChoiceInformation const& choiceInformation,
                                                           VariableInformation const& variableInformation) {
        uint_fast64_t numberOfConstraintsCreated = 0;

        for (auto stateListPair : choiceInformation.problematicChoicesForProblematicStates) {
            for (auto problematicChoice : stateListPair.second) {
                std::list<storm::expressions::Variable>::const_iterator choiceVariableIterator =
                    variableInformation.stateToChoiceVariablesMap.at(stateListPair.first).begin();
                for (auto relevantChoice : choiceInformation.relevantChoicesForRelevantStates.at(stateListPair.first)) {
                    if (relevantChoice == problematicChoice) {
                        break;
                    }
                    ++choiceVariableIterator;
                }

                storm::expressions::Expression constraint = *choiceVariableIterator;
                for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(problematicChoice)) {
                    constraint =
                        constraint - variableInformation.problematicTransitionToVariableMap.at(std::make_pair(stateListPair.first, successorEntry.getColumn()));
                }
                constraint = constraint <= solver.getConstant(0);

                solver.addConstraint("UnproblematicStateReachable" + std::to_string(numberOfConstraintsCreated), constraint);
                ++numberOfConstraintsCreated;
            }
        }

        for (auto state : stateInformation.problematicStates) {
            for (auto problematicChoice : choiceInformation.problematicChoicesForProblematicStates.at(state)) {
                for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(problematicChoice)) {
                    storm::expressions::Expression constraint = variableInformation.problematicStateToVariableMap.at(state);
                    constraint = constraint - variableInformation.problematicStateToVariableMap.at(successorEntry.getColumn());
                    constraint = constraint + variableInformation.problematicTransitionToVariableMap.at(std::make_pair(state, successorEntry.getColumn()));
                    constraint = constraint < solver.getConstant(1);

                    solver.addConstraint("UnproblematicStateReachable" + std::to_string(numberOfConstraintsCreated), constraint);
                    ++numberOfConstraintsCreated;
                }
            }
        }
        return numberOfConstraintsCreated;
    }

    /*
     * Asserts that labels that are on all paths from initial to target states are definitely taken.
     *
     * @param solver The MILP solver.
     * @param choiceInformation The information about the choices in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertKnownLabels(storm::solver::LpSolver<double>& solver, ChoiceInformation const& choiceInformation,
                                           VariableInformation const& variableInformation) {
        uint_fast64_t numberOfConstraintsCreated = 0;

        for (auto label : choiceInformation.knownLabels) {
            storm::expressions::Expression constraint = variableInformation.labelToVariableMap.at(label) == solver.getConstant(1);
            solver.addConstraint("KnownLabels" + std::to_string(numberOfConstraintsCreated), constraint);
            ++numberOfConstraintsCreated;
        }

        return numberOfConstraintsCreated;
    }

    /*!
     * Asserts constraints that rule out many suboptimal policies.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param psiStates A bit vector characterizing the psi states in the model.
     * @param stateInformation The information about the states in the model.
     * @param choiceInformation The information about the choices in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @return The total number of constraints that were created.
     */
    static uint_fast64_t assertSchedulerCuts(storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp,
                                             storm::storage::BitVector const& psiStates, StateInformation const& stateInformation,
                                             ChoiceInformation const& choiceInformation, VariableInformation const& variableInformation) {
        storm::storage::SparseMatrix<T> backwardTransitions = mdp.getBackwardTransitions();
        uint_fast64_t numberOfConstraintsCreated = 0;

        for (auto state : stateInformation.relevantStates) {
            // Assert that all states, that select an action, this action either has a non-zero probability to
            // go to a psi state directly, or in the successor states, at least one action is selected as well.
            std::list<storm::expressions::Variable>::const_iterator choiceVariableIterator = variableInformation.stateToChoiceVariablesMap.at(state).begin();
            for (auto choice : choiceInformation.relevantChoicesForRelevantStates.at(state)) {
                bool psiStateReachableInOneStep = false;
                for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(choice)) {
                    if (psiStates.get(successorEntry.getColumn())) {
                        psiStateReachableInOneStep = true;
                    }
                }

                if (!psiStateReachableInOneStep) {
                    storm::expressions::Expression constraint = *choiceVariableIterator;
                    for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(choice)) {
                        if (state != successorEntry.getColumn() && stateInformation.relevantStates.get(successorEntry.getColumn())) {
                            std::list<storm::expressions::Variable> const& successorChoiceVariableIndices =
                                variableInformation.stateToChoiceVariablesMap.at(successorEntry.getColumn());

                            for (auto const& choiceVariable : successorChoiceVariableIndices) {
                                constraint = constraint - choiceVariable;
                            }
                        }
                    }
                    constraint = constraint <= solver.getConstant(1);

                    solver.addConstraint("SchedulerCuts" + std::to_string(numberOfConstraintsCreated), constraint);
                    ++numberOfConstraintsCreated;
                }

                ++choiceVariableIterator;
            }

            // For all states assert that there is either a selected incoming transition in the subsystem or the
            // state is the chosen initial state if there is one selected action in the current state.
            storm::expressions::Expression constraint = solver.getConstant(0);

            for (auto const& choiceVariable : variableInformation.stateToChoiceVariablesMap.at(state)) {
                constraint = constraint + choiceVariable;
            }

            // Compute the set of predecessors.
            std::unordered_set<uint_fast64_t> predecessors;
            for (auto const& predecessorEntry : backwardTransitions.getRow(state)) {
                if (state != predecessorEntry.getColumn()) {
                    predecessors.insert(predecessorEntry.getColumn());
                }
            }

            for (auto predecessor : predecessors) {
                // If the predecessor is not a relevant state, we need to skip it.
                if (!stateInformation.relevantStates.get(predecessor)) {
                    continue;
                }

                std::list<storm::expressions::Variable>::const_iterator choiceVariableIterator =
                    variableInformation.stateToChoiceVariablesMap.at(predecessor).begin();
                for (auto relevantChoice : choiceInformation.relevantChoicesForRelevantStates.at(predecessor)) {
                    bool choiceTargetsCurrentState = false;

                    // Check if the current choice targets the current state.
                    for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(relevantChoice)) {
                        if (state == successorEntry.getColumn()) {
                            choiceTargetsCurrentState = true;
                            break;
                        }
                    }

                    // If it does, we can add the choice to the sum.
                    if (choiceTargetsCurrentState) {
                        constraint = constraint - *choiceVariableIterator;
                    }
                    ++choiceVariableIterator;
                }
            }

            // If the current state is an initial state and is selected as a successor state by the virtual
            // initial state, then this also justifies making a choice in the current state.
            if (mdp.getStates("init").get(state)) {
                constraint = constraint - variableInformation.initialStateToChoiceVariableMap.at(state);
            }
            constraint = constraint <= solver.getConstant(0);

            solver.addConstraint("SchedulerCuts" + std::to_string(numberOfConstraintsCreated), constraint);
            ++numberOfConstraintsCreated;
        }

        // Assert that at least one initial state selects at least one action.
        storm::expressions::Expression constraint = solver.getConstant(0);
        for (auto initialState : mdp.getStates("init")) {
            for (auto const& choiceVariable : variableInformation.stateToChoiceVariablesMap.at(initialState)) {
                constraint = constraint + choiceVariable;
            }
        }
        constraint = constraint >= solver.getConstant(1);
        solver.addConstraint("SchedulerCuts" + std::to_string(numberOfConstraintsCreated), constraint);
        ++numberOfConstraintsCreated;

        // Add constraints that ensure at least one choice is selected that targets a psi state.
        constraint = solver.getConstant(0);
        std::unordered_set<uint_fast64_t> predecessors;
        for (auto psiState : psiStates) {
            // Compute the set of predecessors.
            for (auto const& predecessorEntry : backwardTransitions.getRow(psiState)) {
                if (psiState != predecessorEntry.getColumn()) {
                    predecessors.insert(predecessorEntry.getColumn());
                }
            }
        }

        for (auto predecessor : predecessors) {
            // If the predecessor is not a relevant state, we need to skip it.
            if (!stateInformation.relevantStates.get(predecessor)) {
                continue;
            }

            std::list<storm::expressions::Variable>::const_iterator choiceVariableIterator =
                variableInformation.stateToChoiceVariablesMap.at(predecessor).begin();
            for (auto relevantChoice : choiceInformation.relevantChoicesForRelevantStates.at(predecessor)) {
                bool choiceTargetsPsiState = false;

                // Check if the current choice targets the current state.
                for (auto const& successorEntry : mdp.getTransitionMatrix().getRow(relevantChoice)) {
                    if (psiStates.get(successorEntry.getColumn())) {
                        choiceTargetsPsiState = true;
                        break;
                    }
                }

                // If it does, we can add the choice to the sum.
                if (choiceTargetsPsiState) {
                    constraint = constraint + *choiceVariableIterator;
                }
                ++choiceVariableIterator;
            }
        }
        constraint = constraint >= solver.getConstant(1);

        solver.addConstraint("SchedulerCuts" + std::to_string(numberOfConstraintsCreated), constraint);
        ++numberOfConstraintsCreated;

        return numberOfConstraintsCreated;
    }

    /*!
     * Builds a system of constraints that express that the reachability probability in the subsystem exceeeds
     * the given threshold.
     *
     * @param solver The MILP solver.
     * @param mdp The MDP.
     * @param psiStates A bit vector characterizing all psi states in the model.
     * @param stateInformation The information about the states in the model.
     * @param choiceInformation The information about the choices in the model.
     * @param variableInformation A struct with information about the variables of the model.
     * @param probabilityThreshold The probability threshold the subsystem is required to exceed.
     * @param strictBound A flag indicating whether the threshold must be exceeded or only matched.
     * @param includeSchedulerCuts If set to true, additional constraints are asserted that reduce the set of
     * possible choices.
     */
    static void buildConstraintSystem(storm::solver::LpSolver<double>& solver, storm::models::sparse::Mdp<T> const& mdp,
                                      std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets, storm::storage::BitVector const& psiStates,
                                      StateInformation const& stateInformation, ChoiceInformation const& choiceInformation,
                                      VariableInformation const& variableInformation, double probabilityThreshold, bool strictBound,
                                      bool includeSchedulerCuts = false) {
        // Assert that the reachability probability in the subsystem exceeds the given threshold.
        uint_fast64_t numberOfConstraints = assertProbabilityGreaterThanThreshold(solver, variableInformation, probabilityThreshold, strictBound);
        STORM_LOG_DEBUG("Asserted that reachability probability exceeds threshold.");

        // Add constraints that assert the policy takes at most one action in each state.
        numberOfConstraints += assertValidPolicy(solver, stateInformation, variableInformation);
        STORM_LOG_DEBUG("Asserted that policy is valid.");

        // Add constraints that assert the labels that belong to some taken choices are taken as well.
        numberOfConstraints += assertChoicesImplyLabels(solver, mdp, labelSets, stateInformation, choiceInformation, variableInformation);
        STORM_LOG_DEBUG("Asserted that labels implied by choices are taken.");

        // Add constraints that encode that the reachability probability from states which do not pick any action
        // is zero.
        numberOfConstraints += assertZeroProbabilityWithoutChoice(solver, stateInformation, variableInformation);
        STORM_LOG_DEBUG("Asserted that reachability probability is zero if no choice is taken.");

        // Add constraints that encode the reachability probabilities for states.
        numberOfConstraints += assertReachabilityProbabilities(solver, mdp, psiStates, stateInformation, choiceInformation, variableInformation);
        STORM_LOG_DEBUG("Asserted constraints for reachability probabilities.");

        // Add constraints that ensure the reachability of an unproblematic state from each problematic state.
        numberOfConstraints += assertUnproblematicStateReachable(solver, mdp, stateInformation, choiceInformation, variableInformation);
        STORM_LOG_DEBUG("Asserted that unproblematic state reachable from problematic states.");

        // Add constraints that express that certain labels are already known to be taken.
        numberOfConstraints += assertKnownLabels(solver, choiceInformation, variableInformation);
        STORM_LOG_DEBUG("Asserted known labels are taken.");

        // If required, assert additional constraints that reduce the number of possible policies.
        if (includeSchedulerCuts) {
            numberOfConstraints += assertSchedulerCuts(solver, mdp, psiStates, stateInformation, choiceInformation, variableInformation);
            STORM_LOG_DEBUG("Asserted scheduler cuts.");
        }

        // Finally, we can tell the solver to incorporate the latest changes.
        solver.update();

        STORM_LOG_INFO("Successfully created " << numberOfConstraints << " MILP constraints.");
    }

    /*!
     * Computes the set of labels that was used in the given optimized model.
     *
     * @param solver The MILP solver.
     * @param variableInformation A struct with information about the variables of the model.
     */
    static storm::storage::FlatSet<uint_fast64_t> getUsedLabelsInSolution(storm::solver::LpSolver<double> const& solver,
                                                                          VariableInformation const& variableInformation) {
        storm::storage::FlatSet<uint_fast64_t> result;

        for (auto const& labelVariablePair : variableInformation.labelToVariableMap) {
            bool labelTaken = solver.getBinaryValue(labelVariablePair.second);

            if (labelTaken) {
                result.insert(labelVariablePair.first);
            }
        }
        return result;
    }

   public:
    static storm::storage::FlatSet<uint_fast64_t> getMinimalLabelSet(Environment const& env, storm::models::sparse::Mdp<T> const& mdp,
                                                                     std::vector<storm::storage::FlatSet<uint_fast64_t>> const& labelSets,
                                                                     storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                                     double probabilityThreshold, bool strictBound, bool checkThresholdFeasible = false,
                                                                     bool includeSchedulerCuts = false) {
        // (0) Check whether the label sets are valid
        STORM_LOG_THROW(mdp.getNumberOfChoices() == labelSets.size(), storm::exceptions::InvalidArgumentException,
                        "The given number of labels does not match the number of choices.");

        // (1) Check whether its possible to exceed the threshold if checkThresholdFeasible is set.
        double maximalReachabilityProbability = 0;
        if (checkThresholdFeasible) {
            storm::modelchecker::helper::SparseMdpPrctlHelper<T> modelcheckerHelper;
            std::vector<T> result = std::move(
                modelcheckerHelper
                    .computeUntilProbabilities(env, false, mdp.getTransitionMatrix(), mdp.getBackwardTransitions(), phiStates, psiStates, false, false)
                    .values);
            for (auto state : mdp.getInitialStates()) {
                maximalReachabilityProbability = std::max(maximalReachabilityProbability, result[state]);
            }
            STORM_LOG_THROW((strictBound && maximalReachabilityProbability >= probabilityThreshold) ||
                                (!strictBound && maximalReachabilityProbability > probabilityThreshold),
                            storm::exceptions::InvalidArgumentException,
                            "Given probability threshold " << probabilityThreshold << " can not be " << (strictBound ? "achieved" : "exceeded")
                                                           << " in model with maximal reachability probability of " << maximalReachabilityProbability << ".");
            std::cout << "\nMaximal reachability in model is " << maximalReachabilityProbability << ".\n\n";
        }

        // (2) Identify relevant and problematic states.
        StateInformation stateInformation = determineRelevantAndProblematicStates(mdp, phiStates, psiStates);

        // (3) Determine sets of relevant labels and problematic choices.
        ChoiceInformation choiceInformation = determineRelevantAndProblematicChoices(mdp, labelSets, stateInformation, psiStates);

        // (4) Encode resulting system as MILP problem.
        std::shared_ptr<storm::solver::LpSolver<double>> solver = storm::utility::solver::getLpSolver<double>(env, "MinimalLabelSetCounterexample");

        //  (4.1) Create variables.
        VariableInformation variableInformation = createVariables(*solver, mdp, stateInformation, choiceInformation);

        //  (4.2) Construct constraint system.
        buildConstraintSystem(*solver, mdp, labelSets, psiStates, stateInformation, choiceInformation, variableInformation, probabilityThreshold, strictBound,
                              includeSchedulerCuts);

        // (4.3) Optimize the model.
        solver->optimize();

        // (4.4) Read off result from variables.
        storm::storage::FlatSet<uint_fast64_t> usedLabelSet = getUsedLabelsInSolution(*solver, variableInformation);
        usedLabelSet.insert(choiceInformation.knownLabels.begin(), choiceInformation.knownLabels.end());

        // (5) Return result.
        return usedLabelSet;
    }

    /*!
     * Computes a (minimal) counterexample with respect to the number of prism commands for the given model and (safety) formula. If the model satisfies the
     * property, an exception is thrown.
     *
     * @param mdp An MDP that is the model in which to generate the counterexample.
     * @param formulaPtr A pointer to a safety formula. The outermost operator must be a probabilistic bound operator with a strict upper bound. The nested
     * formula can be either an unbounded until formula or an eventually formula.
     */
    static std::shared_ptr<HighLevelCounterexample> computeCounterexample(Environment const& env, storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                          storm::models::sparse::Mdp<T> const& mdp,
                                                                          std::shared_ptr<storm::logic::Formula const> const& formula) {
        std::cout << "\nGenerating minimal label counterexample for formula " << *formula << '\n';

        // Check whether there are choice origins available
        STORM_LOG_THROW(mdp.hasChoiceOrigins(), storm::exceptions::InvalidArgumentException,
                        "Restriction to minimal command set is impossible for model without choice origns.");
        STORM_LOG_THROW(mdp.getChoiceOrigins()->isPrismChoiceOrigins() || mdp.getChoiceOrigins()->isJaniChoiceOrigins(),
                        storm::exceptions::InvalidArgumentException,
                        "Restriction to command set is impossible for model without PRISM or JANI choice origins.");

        STORM_LOG_THROW(formula->isProbabilityOperatorFormula(), storm::exceptions::InvalidPropertyException,
                        "Counterexample generation does not support this kind of formula. Expecting a probability operator as the outermost formula element.");
        storm::logic::ProbabilityOperatorFormula const& probabilityOperator = formula->asProbabilityOperatorFormula();
        STORM_LOG_THROW(probabilityOperator.hasBound(), storm::exceptions::InvalidPropertyException,
                        "Counterexample generation only supports bounded formulas.");
        storm::logic::ComparisonType comparisonType = probabilityOperator.getComparisonType();
        STORM_LOG_THROW(comparisonType == storm::logic::ComparisonType::Less || comparisonType == storm::logic::ComparisonType::LessEqual,
                        storm::exceptions::InvalidPropertyException, "Counterexample generation only supports formulas with an upper probability bound.");
        STORM_LOG_THROW(probabilityOperator.getSubformula().isUntilFormula() || probabilityOperator.getSubformula().isEventuallyFormula(),
                        storm::exceptions::InvalidPropertyException, "Path formula is required to be of the form 'phi U psi' for counterexample generation.");

        bool strictBound = comparisonType == storm::logic::ComparisonType::Less;
        double threshold = probabilityOperator.getThresholdAs<double>();

        storm::storage::BitVector phiStates;
        storm::storage::BitVector psiStates;
        storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Mdp<T>> modelchecker(mdp);

        if (probabilityOperator.getSubformula().isUntilFormula()) {
            storm::logic::UntilFormula const& untilFormula = probabilityOperator.getSubformula().asUntilFormula();

            std::unique_ptr<storm::modelchecker::CheckResult> leftResult = modelchecker.check(env, untilFormula.getLeftSubformula());
            std::unique_ptr<storm::modelchecker::CheckResult> rightResult = modelchecker.check(env, untilFormula.getRightSubformula());

            storm::modelchecker::ExplicitQualitativeCheckResult<T> const& leftQualitativeResult = leftResult->template asExplicitQualitativeCheckResult<T>();
            storm::modelchecker::ExplicitQualitativeCheckResult<T> const& rightQualitativeResult = rightResult->template asExplicitQualitativeCheckResult<T>();

            phiStates = leftQualitativeResult.getTruthValuesVector();
            psiStates = rightQualitativeResult.getTruthValuesVector();
        } else if (probabilityOperator.getSubformula().isEventuallyFormula()) {
            storm::logic::EventuallyFormula const& eventuallyFormula = probabilityOperator.getSubformula().asEventuallyFormula();

            std::unique_ptr<storm::modelchecker::CheckResult> subResult = modelchecker.check(env, eventuallyFormula.getSubformula());

            storm::modelchecker::ExplicitQualitativeCheckResult<T> const& subQualitativeResult = subResult->template asExplicitQualitativeCheckResult<T>();

            phiStates = storm::storage::BitVector(mdp.getNumberOfStates(), true);
            psiStates = subQualitativeResult.getTruthValuesVector();
        }

        // Obtain the label sets for each choice.
        std::vector<storm::storage::FlatSet<uint_fast64_t>> labelSets(mdp.getNumberOfChoices());
        if (mdp.getChoiceOrigins()->isPrismChoiceOrigins()) {
            storm::storage::sparse::PrismChoiceOrigins const& choiceOrigins = mdp.getChoiceOrigins()->asPrismChoiceOrigins();
            for (uint_fast64_t choice = 0; choice < mdp.getNumberOfChoices(); ++choice) {
                labelSets[choice] = choiceOrigins.getCommandSet(choice);
            }
        } else {
            storm::storage::sparse::JaniChoiceOrigins const& choiceOrigins = mdp.getChoiceOrigins()->asJaniChoiceOrigins();

            // The choice origins are known to be JANI ones at this point.
            for (uint_fast64_t choice = 0; choice < mdp.getNumberOfChoices(); ++choice) {
                labelSets[choice] = choiceOrigins.getEdgeIndexSet(choice);
            }
        }

        // Delegate the actual computation work to the function of equal name.
        auto startTime = std::chrono::high_resolution_clock::now();
        storm::storage::FlatSet<uint_fast64_t> usedLabelSet =
            getMinimalLabelSet(env, mdp, labelSets, phiStates, psiStates, threshold, strictBound, true,
                               storm::settings::getModule<storm::settings::modules::CounterexampleGeneratorSettings>().isUseSchedulerCutsSet());
        auto endTime = std::chrono::high_resolution_clock::now();
        std::cout << "\nComputed minimal command set of size " << usedLabelSet.size() << " in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms.\n";

        if (symbolicModel.isPrismProgram()) {
            return std::make_shared<HighLevelCounterexample>(symbolicModel.asPrismProgram().restrictCommands(usedLabelSet));
        } else {
            STORM_LOG_ASSERT(symbolicModel.isJaniModel(), "Unknown symbolic model description type.");
            return std::make_shared<HighLevelCounterexample>(symbolicModel.asJaniModel().restrictEdges(usedLabelSet));
        }
    }
};

}  // namespace counterexamples
}  // namespace storm
