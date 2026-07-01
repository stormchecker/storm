#include "storm/modelchecker/helper/infinitehorizon/SparseNondeterministicInfiniteHorizonHelper.h"

#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/modelchecker/helper/infinitehorizon/internal/ComponentUtility.h"
#include "storm/modelchecker/helper/infinitehorizon/internal/LraViHelper.h"
#include "storm/solver/LpSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/SchedulerChoice.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/graph.h"
#include "storm/utility/solver.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType>
SparseNondeterministicInfiniteHorizonHelper<ValueType>::SparseNondeterministicInfiniteHorizonHelper(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix)
    : SparseInfiniteHorizonHelper<ValueType, true>(transitionMatrix) {
    // Intentionally left empty.
}

template<typename ValueType>
SparseNondeterministicInfiniteHorizonHelper<ValueType>::SparseNondeterministicInfiniteHorizonHelper(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& markovianStates, std::vector<ValueType> const& exitRates)
    : SparseInfiniteHorizonHelper<ValueType, true>(transitionMatrix, markovianStates, exitRates) {
    // Intentionally left empty.
}

template<typename ValueType>
std::vector<uint64_t> const& SparseNondeterministicInfiniteHorizonHelper<ValueType>::getProducedOptimalChoices() const {
    STORM_LOG_ASSERT(this->isProduceSchedulerSet(), "Trying to get the produced optimal choices although no scheduler was requested.");
    STORM_LOG_ASSERT(this->_producedOptimalChoices.is_initialized(),
                     "Trying to get the produced optimal choices but none were available. Was there a computation call before?");
    return this->_producedOptimalChoices.get();
}

template<typename ValueType>
std::vector<uint64_t>& SparseNondeterministicInfiniteHorizonHelper<ValueType>::getProducedOptimalChoices() {
    STORM_LOG_ASSERT(this->isProduceSchedulerSet(), "Trying to get the produced optimal choices although no scheduler was requested.");
    STORM_LOG_ASSERT(this->_producedOptimalChoices.is_initialized(),
                     "Trying to get the produced optimal choices but none were available. Was there a computation call before?");
    return this->_producedOptimalChoices.get();
}

template<typename ValueType>
storm::storage::Scheduler<ValueType> SparseNondeterministicInfiniteHorizonHelper<ValueType>::extractScheduler() const {
    auto const& optimalChoices = getProducedOptimalChoices();
    storm::storage::Scheduler<ValueType> scheduler(optimalChoices.size());
    for (uint64_t state = 0; state < optimalChoices.size(); ++state) {
        scheduler.setChoice(optimalChoices[state], state);
    }
    return scheduler;
}

template<typename ValueType>
void SparseNondeterministicInfiniteHorizonHelper<ValueType>::createDecomposition() {
    if (this->_longRunComponentDecomposition == nullptr) {
        // The decomposition has not been provided or computed, yet.
        this->createBackwardTransitions();
        this->_computedLongRunComponentDecomposition =
            std::make_unique<storm::storage::MaximalEndComponentDecomposition<ValueType>>(this->_transitionMatrix, *this->_backwardTransitions);
        this->_longRunComponentDecomposition = this->_computedLongRunComponentDecomposition.get();
    }
}

template<typename ValueType>
ValueType SparseNondeterministicInfiniteHorizonHelper<ValueType>::computeLraForComponent(Environment const& env, ValueGetter const& stateRewardsGetter,
                                                                                         ValueGetter const& actionRewardsGetter,
                                                                                         storm::storage::MaximalEndComponent const& component) {
    // For models with potential nondeterminisim, we compute the LRA for a maximal end component (MEC)

    // Allocate memory for the nondeterministic choices.
    if (this->isProduceSchedulerSet()) {
        if (!this->_producedOptimalChoices.is_initialized()) {
            this->_producedOptimalChoices.emplace();
        }
        this->_producedOptimalChoices->resize(this->_transitionMatrix.getRowGroupCount());
    }

    // Catch cases where the component is a single state
    if (auto const trivialResult = this->computeLraForTrivialMec(env, stateRewardsGetter, actionRewardsGetter, component); trivialResult.has_value()) {
        return trivialResult.value();
    }
    // Catch cases where the component contains another component as a subcomponent with zero reward
    if (auto const zeroResult = this->computeLraForZeroMec(env, stateRewardsGetter, actionRewardsGetter, component); zeroResult.has_value()) {
        return zeroResult.value();
    }

    // Solve nontrivial MEC with the method specified in the settings
    storm::solver::LraMethod method = env.solver().lra().getNondetLraMethod();
    if ((storm::NumberTraits<ValueType>::IsExact || env.solver().isForceExact()) && env.solver().lra().isNondetLraMethodSetFromDefault() &&
        method != storm::solver::LraMethod::LinearProgramming) {
        STORM_LOG_INFO(
            "Selecting 'LP' as the solution technique for long-run properties to guarantee exact results. If you want to override this, please explicitly "
            "specify a different LRA method.");
        method = storm::solver::LraMethod::LinearProgramming;
    } else if (env.solver().isForceSoundness() && env.solver().lra().isNondetLraMethodSetFromDefault() && method != storm::solver::LraMethod::ValueIteration) {
        STORM_LOG_INFO(
            "Selecting 'VI' as the solution technique for long-run properties to guarantee sound results. If you want to override this, please explicitly "
            "specify a different LRA method.");
        method = storm::solver::LraMethod::ValueIteration;
    }
    STORM_LOG_ERROR_COND(!this->isProduceSchedulerSet() || method == storm::solver::LraMethod::ValueIteration,
                         "Scheduler generation not supported for the chosen LRA method. Try value-iteration.");
    if (method == storm::solver::LraMethod::LinearProgramming) {
        return computeLraForMecLp(env, stateRewardsGetter, actionRewardsGetter, component);
    } else if (method == storm::solver::LraMethod::ValueIteration) {
        return computeLraForMecVi(env, stateRewardsGetter, actionRewardsGetter, component);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException, "Unsupported technique.");
    }
}

template<typename ValueType>
std::optional<ValueType> SparseNondeterministicInfiniteHorizonHelper<ValueType>::computeLraForTrivialMec(Environment const& env,
                                                                                                         ValueGetter const& stateRewardsGetter,
                                                                                                         ValueGetter const& actionRewardsGetter,
                                                                                                         storm::storage::MaximalEndComponent const& component) {
    if (component.size() != 1) {
        return std::nullopt;  // MEC is not trivial
    }
    // If the component only consists of a single state, we compute the LRA value directly
    auto const& element = *component.begin();
    uint64_t const state = internal::getComponentElementState(element);
    auto choiceIt = internal::getComponentElementChoicesBegin(element);
    if (!this->isContinuousTime()) {
        // This is an MDP.
        // Find the choice with the highest/lowest reward
        ValueType bestValue = actionRewardsGetter(*choiceIt);
        uint64_t bestChoice = *choiceIt;
        for (++choiceIt; choiceIt != internal::getComponentElementChoicesEnd(element); ++choiceIt) {
            ValueType currentValue = actionRewardsGetter(*choiceIt);
            if ((this->minimize() && currentValue < bestValue) || (this->maximize() && currentValue > bestValue)) {
                bestValue = std::move(currentValue);
                bestChoice = *choiceIt;
            }
        }
        if (this->isProduceSchedulerSet()) {
            this->_producedOptimalChoices.get()[state] = bestChoice - this->_transitionMatrix.getRowGroupIndices()[state];
        }
        bestValue += stateRewardsGetter(state);
        return bestValue;
    } else {
        // In a Markov Automaton, singleton components have to consist of a Markovian state because of the non-Zenoness assumption. Then, there is just one
        // possible choice.
        STORM_LOG_ASSERT(this->_markovianStates != nullptr,
                         "Nondeterministic continuous time model without Markovian states... Is this a not a Markov Automaton?");
        STORM_LOG_THROW(this->_markovianStates->get(state), storm::exceptions::InvalidOperationException,
                        "Markov Automaton has Zeno behavior. Computation of Long Run Average values not supported.");
        STORM_LOG_ASSERT(internal::getComponentElementChoiceCount(element) == 1, "Markovian state has Nondeterministic behavior.");
        if (this->isProduceSchedulerSet()) {
            this->_producedOptimalChoices.get()[state] = 0;
        }
        ValueType result = stateRewardsGetter(state) +
                           (this->isContinuousTime() ? (*this->_exitRates)[state] * actionRewardsGetter(*choiceIt) : actionRewardsGetter(*choiceIt));
        return result;
    }
}

template<typename ValueType>
std::optional<ValueType> SparseNondeterministicInfiniteHorizonHelper<ValueType>::computeLraForZeroMec(Environment const& env,
                                                                                                      ValueGetter const& stateRewardsGetter,
                                                                                                      ValueGetter const& actionRewardsGetter,
                                                                                                      storm::storage::MaximalEndComponent const& component) {
    // We first get those component states and choices that do not immediately collect non-zero reward
    bool hasNonZeroReward = false;
    std::map<uint64_t, std::set<uint64_t>> zeroLraStatesChoices;
    for (auto const& [state, choices] : component) {
        auto const stateReward = stateRewardsGetter(state);
        if (!storm::utility::isZero(stateReward)) {
            // If we minimize and see a negative reward, we cannot infer zero long-run average reward for the entire MEC, even if a 0 reward sub-EC exists.
            // Maximize and seeing a positive reward is similar.
            if ((stateReward < storm::utility::zero<ValueType>()) == this->minimize()) {
                // Component has non-zero reward, but in the wrong direction
                return std::nullopt;
            }
            hasNonZeroReward = true;
            continue;
        }
        std::set<uint64_t> zeroLraChoices;
        for (auto const choice : choices) {
            auto const actionReward = actionRewardsGetter(choice);
            if (!storm::utility::isZero(actionReward)) {
                // Catch non-zero reward in the wrong direction, similar to state rewards above.
                if ((actionReward < storm::utility::zero<ValueType>()) == this->minimize()) {
                    return std::nullopt;
                }
                hasNonZeroReward = true;
            } else {
                zeroLraChoices.insert(choice);
            }
        }
        if (!zeroLraChoices.empty()) {
            zeroLraStatesChoices.emplace(state, std::move(zeroLraChoices));
        }
    }
    // If there are non-zero rewards in this MEC, we have to check whether there is a sub-EC with zero rewards.
    if (hasNonZeroReward) {
        this->createBackwardTransitions();
        std::set<uint64_t> candidates;
        for (auto const& [state, _] : zeroLraStatesChoices) {
            candidates.insert(state);
        }
        while (!candidates.empty()) {
            uint64_t const state = *candidates.begin();
            candidates.erase(candidates.begin());
            auto& choices = zeroLraStatesChoices.at(state);
            // We erase some choices while iterating over them, but this is fine for std::set as long as we do it like that
            for (auto choiceIt = choices.begin(); choiceIt != choices.end();) {
                auto const row = this->_transitionMatrix.getRow(*choiceIt);
                if (std::all_of(row.begin(), row.end(),
                                [&zeroLraStatesChoices](auto const& transition) { return zeroLraStatesChoices.contains(transition.getColumn()); })) {
                    ++choiceIt;  // keep the choice
                } else {
                    // This choice has a transition to a non-zero LRA state, so we erase it
                    choiceIt = choices.erase(choiceIt);  // Note: erase returns the iterator to the next element
                }
            }
            if (choices.empty()) {
                // No zero LRA choice for this state, so we erase it and add its predecessors to the stack
                zeroLraStatesChoices.erase(state);
                for (auto const& backwardTransition : this->_backwardTransitions->getRow(state)) {
                    auto const predecessor = backwardTransition.getColumn();
                    if (zeroLraStatesChoices.contains(predecessor)) {
                        candidates.insert(predecessor);
                    }
                }
            }
        }
    }
    if (zeroLraStatesChoices.empty()) {
        // No zero LRA sub-EC, so we cannot infer zero LRA for the entire MEC
        return std::nullopt;
    }
    // We have found a zero LRA sub-EC. We can infer zero LRA
    if (this->isProduceSchedulerSet()) {
        // Ensure that all other states get some choice that leads to the zero LRA sub-EC
        if (zeroLraStatesChoices.size() < component.size()) {
            this->createBackwardTransitions();
            std::set<uint64_t> candidates;  // Always contains states with one transition to zeroLraStatesChoices
            // helper function to add predecessors of a state to the candidate set
            auto insertPredecessors = [&](uint64_t const state) {
                for (auto const& backwardTransition : this->_backwardTransitions->getRow(state)) {
                    auto const predecessor = backwardTransition.getColumn();
                    if (!zeroLraStatesChoices.contains(predecessor) && component.containsState(predecessor)) {
                        candidates.insert(predecessor);
                    }
                }
            };
            for (auto const& [state, _] : zeroLraStatesChoices) {
                insertPredecessors(state);
            }
            while (!candidates.empty()) {
                uint64_t const state = *candidates.begin();
                candidates.erase(candidates.begin());
                for (auto const choice : this->_transitionMatrix.getRowGroupIndices(state)) {
                    auto const row = this->_transitionMatrix.getRow(choice);
                    if (std::any_of(row.begin(), row.end(),
                                    [&zeroLraStatesChoices](auto const& transition) { return zeroLraStatesChoices.contains(transition.getColumn()); })) {
                        zeroLraStatesChoices.emplace(state, std::set<uint64_t>{choice});
                    }
                }
                STORM_LOG_ASSERT(zeroLraStatesChoices.contains(state), "No suitable choice found for state " << state);
                insertPredecessors(state);
            }
        }
        // Now all states should have a choice
        STORM_LOG_ASSERT(component.size() == zeroLraStatesChoices.size(),
                         "Did not find a choice for all states: " << zeroLraStatesChoices.size() << " / " << component.size() << ".");
        // Finally, set some choice as optimal
        for (auto const& [state, choices] : zeroLraStatesChoices) {
            this->_producedOptimalChoices.get()[state] = *choices.begin() - this->_transitionMatrix.getRowGroupIndices()[state];
        }
    }

    return storm::utility::zero<ValueType>();
}

template<typename ValueType>
ValueType SparseNondeterministicInfiniteHorizonHelper<ValueType>::computeLraForMecVi(Environment const& env, ValueGetter const& stateRewardsGetter,
                                                                                     ValueGetter const& actionRewardsGetter,
                                                                                     storm::storage::MaximalEndComponent const& mec) {
    // Collect some parameters of the computation
    ValueType aperiodicFactor = storm::utility::convertNumber<ValueType>(env.solver().lra().getAperiodicFactor());
    std::vector<uint64_t>* optimalChoices = nullptr;
    if (this->isProduceSchedulerSet()) {
        optimalChoices = &this->_producedOptimalChoices.get();
    }

    // Now create a helper and perform the algorithm
    if (this->isContinuousTime()) {
        // We assume a Markov Automaton (with deterministic timed states and nondeterministic instant states)
        storm::modelchecker::helper::internal::LraViHelper<ValueType, storm::storage::MaximalEndComponent,
                                                           storm::modelchecker::helper::internal::LraViTransitionsType::DetTsNondetIs>
            viHelper(mec, this->_transitionMatrix, aperiodicFactor, this->_markovianStates, this->_exitRates);
        return viHelper.performValueIteration(env, stateRewardsGetter, actionRewardsGetter, this->_exitRates, &this->getOptimizationDirection(),
                                              optimalChoices);
    } else {
        // We assume an MDP (with nondeterministic timed states and no instant states)
        storm::modelchecker::helper::internal::LraViHelper<ValueType, storm::storage::MaximalEndComponent,
                                                           storm::modelchecker::helper::internal::LraViTransitionsType::NondetTsNoIs>
            viHelper(mec, this->_transitionMatrix, aperiodicFactor);
        return viHelper.performValueIteration(env, stateRewardsGetter, actionRewardsGetter, nullptr, &this->getOptimizationDirection(), optimalChoices);
    }
}

template<typename ValueType>
ValueType SparseNondeterministicInfiniteHorizonHelper<ValueType>::computeLraForMecLp(Environment const& env, ValueGetter const& stateRewardsGetter,
                                                                                     ValueGetter const& actionRewardsGetter,
                                                                                     storm::storage::MaximalEndComponent const& mec) {
    // Create an LP solver
    auto solver = storm::utility::solver::getLpSolver<ValueType>("LRA for MEC");

    // Now build the LP formulation as described in:
    // Guck et al.: Modelling and Analysis of Markov Reward Automata (ATVA'14), https://doi.org/10.1007/978-3-319-11936-6_13
    solver->setOptimizationDirection(invert(this->getOptimizationDirection()));

    // Create variables
    // TODO: Investigate whether we can easily make the variables bounded
    std::map<uint_fast64_t, storm::expressions::Variable> stateToVariableMap;
    for (auto const& stateChoicesPair : mec) {
        std::string variableName = "x" + std::to_string(stateChoicesPair.first);
        stateToVariableMap[stateChoicesPair.first] = solver->addUnboundedContinuousVariable(variableName);
    }
    storm::expressions::Variable k = solver->addUnboundedContinuousVariable("k", storm::utility::one<ValueType>());
    solver->update();

    // Add constraints.
    for (auto const& stateChoicesPair : mec) {
        uint_fast64_t state = stateChoicesPair.first;
        bool stateIsMarkovian = this->_markovianStates && this->_markovianStates->get(state);

        // Now create a suitable constraint for each choice
        // x_s  {≤, ≥}  -k/rate(s) + sum_s' P(s,act,s') * x_s' + (value(s)/rate(s) + value(s,act))
        for (auto choice : stateChoicesPair.second) {
            std::vector<storm::expressions::Expression> summands;
            auto matrixRow = this->_transitionMatrix.getRow(choice);
            summands.reserve(matrixRow.getNumberOfEntries() + 2);
            // add -k/rate(s) (only if s is either a Markovian state or we have an MDP)
            if (stateIsMarkovian) {
                summands.push_back(-(k / solver->getManager().rational((*this->_exitRates)[state])));
            } else if (!this->isContinuousTime()) {
                summands.push_back(-k);
            }
            // add sum_s' P(s,act,s') * x_s'
            for (auto const& element : matrixRow) {
                summands.push_back(stateToVariableMap.at(element.getColumn()) * solver->getConstant(element.getValue()));
            }
            // add value for state and choice
            ValueType value;
            if (stateIsMarkovian) {
                // divide state reward with exit rate
                value = stateRewardsGetter(state) / (*this->_exitRates)[state] + actionRewardsGetter(choice);
            } else if (!this->isContinuousTime()) {
                // in discrete time models no scaling is needed
                value = stateRewardsGetter(state) + actionRewardsGetter(choice);
            } else {
                // state is a probabilistic state of a Markov automaton. The state reward is not collected
                value = actionRewardsGetter(choice);
            }
            summands.push_back(solver->getConstant(value));
            storm::expressions::Expression constraint;
            if (this->minimize()) {
                constraint = stateToVariableMap.at(state) <= storm::expressions::sum(summands);
            } else {
                constraint = stateToVariableMap.at(state) >= storm::expressions::sum(summands);
            }
            solver->addConstraint("s" + std::to_string(state) + "," + std::to_string(choice), constraint);
        }
    }

    solver->optimize();
    STORM_LOG_THROW(!this->isProduceSchedulerSet(), storm::exceptions::NotImplementedException,
                    "Scheduler extraction is not yet implemented for LP based LRA method.");
    return solver->getContinuousValue(k);
}

/*!
 * Auxiliary function that adds the entries of the Ssp Matrix for a single choice (i.e., row)
 * Transitions that lead to a Component state will be redirected to a new auxiliary state (there is one aux. state for each component).
 * Transitions that don't lead to a Component state are copied (taking a state index mapping into account).
 */
template<typename ValueType>
void addSspMatrixChoice(uint64_t const& inputMatrixChoice, storm::storage::SparseMatrix<ValueType> const& inputTransitionMatrix,
                        std::vector<uint64_t> const& inputToSspStateMap, uint64_t const& numberOfNonComponentStates, uint64_t const& currentSspChoice,
                        storm::storage::SparseMatrixBuilder<ValueType>& sspMatrixBuilder) {
    // As there could be multiple transitions to the same MEC, we accumulate them in this map before adding them to the matrix builder.
    std::map<uint64_t, ValueType> auxiliaryStateToProbabilityMap;

    for (auto const& transition : inputTransitionMatrix.getRow(inputMatrixChoice)) {
        if (!storm::utility::isZero(transition.getValue())) {
            auto const& sspTransitionTarget = inputToSspStateMap[transition.getColumn()];
            // Since the auxiliary Component states are appended at the end of the matrix, we can use this check to
            // decide whether the transition leads to a component state or not
            if (sspTransitionTarget < numberOfNonComponentStates) {
                // If the target state is not contained in a component, we can copy over the entry.
                sspMatrixBuilder.addNextValue(currentSspChoice, sspTransitionTarget, transition.getValue());
            } else {
                // If the target state is contained in component i, we need to add the probability to the corresponding field in the vector
                // so that we are able to write the cumulative probability to the component into the matrix later.
                auto insertionRes = auxiliaryStateToProbabilityMap.emplace(sspTransitionTarget, transition.getValue());
                if (!insertionRes.second) {
                    // sspTransitionTarget already existed in the map, i.e., there already was a transition to that component.
                    // Hence, we add up the probabilities.
                    insertionRes.first->second += transition.getValue();
                }
            }
        }
    }

    // Now insert all (cumulative) probability values that target a component.
    for (auto const& componentToProbEntry : auxiliaryStateToProbabilityMap) {
        sspMatrixBuilder.addNextValue(currentSspChoice, componentToProbEntry.first, componentToProbEntry.second);
    }
}

template<typename ValueType>
std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> SparseNondeterministicInfiniteHorizonHelper<ValueType>::buildSspMatrixVector(
    std::vector<ValueType> const& mecLraValues, std::vector<uint64_t> const& inputToSspStateMap, storm::storage::BitVector const& statesNotInComponent,
    uint64_t numberOfNonComponentStates, std::vector<std::pair<uint64_t, uint64_t>>* sspComponentExitChoicesToOriginalMap) {
    auto const& choiceIndices = this->_transitionMatrix.getRowGroupIndices();

    std::vector<ValueType> rhs;
    uint64_t numberOfSspStates = numberOfNonComponentStates + this->_longRunComponentDecomposition->size();
    storm::storage::SparseMatrixBuilder<ValueType> sspMatrixBuilder(0, numberOfSspStates, 0, true, true, numberOfSspStates);
    // If the source state of a transition is not contained in any component, we copy its choices (and perform the necessary modifications).
    uint64_t currentSspChoice = 0;
    for (auto nonComponentState : statesNotInComponent) {
        sspMatrixBuilder.newRowGroup(currentSspChoice);
        for (uint64_t choice = choiceIndices[nonComponentState]; choice < choiceIndices[nonComponentState + 1]; ++choice, ++currentSspChoice) {
            rhs.push_back(storm::utility::zero<ValueType>());
            addSspMatrixChoice(choice, this->_transitionMatrix, inputToSspStateMap, numberOfNonComponentStates, currentSspChoice, sspMatrixBuilder);
        }
    }
    // Now we construct the choices for the auxiliary states which reflect former Component states.
    for (uint64_t componentIndex = 0; componentIndex < this->_longRunComponentDecomposition->size(); ++componentIndex) {
        auto const& component = (*this->_longRunComponentDecomposition)[componentIndex];
        sspMatrixBuilder.newRowGroup(currentSspChoice);
        // For nondeterministic models it might still be that we leave the component again. This needs to be reflected in the SSP
        // by adding the "exiting" choices of the MEC to the axiliary states
        for (auto const& element : component) {
            uint64_t componentState = internal::getComponentElementState(element);
            for (uint64_t choice = choiceIndices[componentState]; choice < choiceIndices[componentState + 1]; ++choice) {
                // If the choice is not contained in the component itself, we have to add a similar distribution to the auxiliary state.
                if (!internal::componentElementChoicesContains(element, choice)) {
                    rhs.push_back(storm::utility::zero<ValueType>());
                    addSspMatrixChoice(choice, this->_transitionMatrix, inputToSspStateMap, numberOfNonComponentStates, currentSspChoice, sspMatrixBuilder);
                    if (sspComponentExitChoicesToOriginalMap) {
                        // Later we need to be able to map this choice back to the original input model
                        sspComponentExitChoicesToOriginalMap->emplace_back(componentState, choice - choiceIndices[componentState]);
                    }
                    ++currentSspChoice;
                }
            }
        }
        // For each auxiliary state, there is the option to achieve the reward value of the LRA associated with the component.
        rhs.push_back(mecLraValues[componentIndex]);
        if (sspComponentExitChoicesToOriginalMap) {
            // Insert some invalid values so we can later detect that this choice is not an exit choice
            sspComponentExitChoicesToOriginalMap->emplace_back(std::numeric_limits<uint_fast64_t>::max(), std::numeric_limits<uint_fast64_t>::max());
        }
        ++currentSspChoice;
    }
    return std::make_pair(sspMatrixBuilder.build(currentSspChoice, numberOfSspStates, numberOfSspStates), std::move(rhs));
}

template<typename ValueType>
void SparseNondeterministicInfiniteHorizonHelper<ValueType>::constructOptimalChoices(
    std::vector<uint64_t> const& sspChoices, storm::storage::SparseMatrix<ValueType> const& sspMatrix, std::vector<uint64_t> const& inputToSspStateMap,
    storm::storage::BitVector const& statesNotInComponent, uint64_t numberOfNonComponentStates,
    std::vector<std::pair<uint64_t, uint64_t>> const& sspComponentExitChoicesToOriginalMap) {
    // We first take care of non-mec states
    storm::utility::vector::setVectorValues(this->_producedOptimalChoices.get(), statesNotInComponent, sspChoices);
    // Secondly, we consider MEC states. There are 3 cases for each MEC state:
    // 1. The SSP choices encode that we want to stay in the MEC
    // 2. The SSP choices encode that we want to leave the MEC and
    //      a) we take an exit (non-MEC) choice at the given state
    //      b) we have to take a MEC choice at the given state in a way that eventually an exit state of the MEC is reached
    uint64_t exitChoiceOffset = sspMatrix.getRowGroupIndices()[numberOfNonComponentStates];
    for (auto const& mec : *this->_longRunComponentDecomposition) {
        // Get the sspState of this MEC (using one representative mec state)
        auto const& sspState = inputToSspStateMap[mec.begin()->first];
        uint64_t sspChoiceIndex = sspMatrix.getRowGroupIndices()[sspState] + sspChoices[sspState];
        // Obtain the state and choice of the original model to which the selected choice corresponds.
        auto const& originalStateChoice = sspComponentExitChoicesToOriginalMap[sspChoiceIndex - exitChoiceOffset];
        // Check if we are in Case 1 or 2
        if (originalStateChoice.first == std::numeric_limits<uint_fast64_t>::max()) {
            // The optimal choice is to stay in this mec (Case 1)
            // In this case, no further operations are necessary. The scheduler has already been set to the optimal choices during the call of
            // computeLraForMec.
            STORM_LOG_ASSERT(sspMatrix.getRow(sspState, sspChoices[sspState]).getNumberOfEntries() == 0, "Expected empty row at choice that stays in MEC.");
        } else {
            // The best choice is to leave this MEC via the selected state and choice. (Case 2)
            // Set the exit choice (Case 2.a)
            this->_producedOptimalChoices.get()[originalStateChoice.first] = originalStateChoice.second;
            // The remaining states in this MEC need to reach the state with the exit choice with probability 1. (Case 2.b)
            // Perform a backwards search from the exit state, only using MEC choices
            // We start by setting an invalid choice to all remaining mec states (so that we can easily detect them as unprocessed)
            for (auto const& stateActions : mec) {
                if (stateActions.first != originalStateChoice.first) {
                    this->_producedOptimalChoices.get()[stateActions.first] = std::numeric_limits<uint64_t>::max();
                }
            }
            // Ensure that backwards transitions are available
            this->createBackwardTransitions();
            // Now start a backwards DFS
            std::vector<uint64_t> stack = {originalStateChoice.first};
            while (!stack.empty()) {
                uint64_t currentState = stack.back();
                stack.pop_back();
                for (auto const& backwardsTransition : this->_backwardTransitions->getRowGroup(currentState)) {
                    uint64_t predecessorState = backwardsTransition.getColumn();
                    if (mec.containsState(predecessorState)) {
                        auto& selectedPredChoice = this->_producedOptimalChoices.get()[predecessorState];
                        if (selectedPredChoice == std::numeric_limits<uint64_t>::max()) {
                            // We don't already have a choice for this predecessor.
                            // We now need to check whether there is a *MEC* choice leading to currentState
                            for (auto const& predChoice : mec.getChoicesForState(predecessorState)) {
                                for (auto const& forwardTransition : this->_transitionMatrix.getRow(predChoice)) {
                                    if (forwardTransition.getColumn() == currentState && !storm::utility::isZero(forwardTransition.getValue())) {
                                        // Playing this choice (infinitely often) will lead to current state (infinitely often)!
                                        selectedPredChoice = predChoice - this->_transitionMatrix.getRowGroupIndices()[predecessorState];
                                        stack.push_back(predecessorState);
                                        break;
                                    }
                                }
                                if (selectedPredChoice != std::numeric_limits<uint64_t>::max()) {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

template<typename ValueType>
std::vector<ValueType> SparseNondeterministicInfiniteHorizonHelper<ValueType>::buildAndSolveSsp(Environment const& env,
                                                                                                std::vector<ValueType> const& componentLraValues) {
    STORM_LOG_ASSERT(this->_longRunComponentDecomposition != nullptr, "Decomposition not computed, yet.");

    // For fast transition rewriting, we build a mapping from the input state indices to the state indices of a new transition matrix
    // which redirects all transitions leading to a former component state to a new auxiliary state.
    // There will be one auxiliary state for each component. These states will be appended to the end of the matrix.

    // First gather the states that are part of a component
    // and create a mapping from states that lie in a component to the corresponding component index.
    storm::storage::BitVector statesInComponents(this->_transitionMatrix.getRowGroupCount());
    std::vector<uint64_t> inputToSspStateMap(this->_transitionMatrix.getRowGroupCount(), std::numeric_limits<uint64_t>::max());
    for (uint64_t currentComponentIndex = 0; currentComponentIndex < this->_longRunComponentDecomposition->size(); ++currentComponentIndex) {
        for (auto const& element : (*this->_longRunComponentDecomposition)[currentComponentIndex]) {
            uint64_t state = internal::getComponentElementState(element);
            statesInComponents.set(state);
            inputToSspStateMap[state] = currentComponentIndex;
        }
    }
    // Now take care of the non-component states. Note that the order of these states will be preserved.
    uint64_t numberOfNonComponentStates = 0;
    storm::storage::BitVector statesNotInComponent = ~statesInComponents;
    for (auto nonComponentState : statesNotInComponent) {
        inputToSspStateMap[nonComponentState] = numberOfNonComponentStates;
        ++numberOfNonComponentStates;
    }
    // Finalize the mapping for the component states which now still assigns component states to to their component index.
    // To make sure that they point to the auxiliary states (located at the end of the SspMatrix), we need to shift them by the
    // number of states that are not in a component.
    for (auto mecState : statesInComponents) {
        inputToSspStateMap[mecState] += numberOfNonComponentStates;
    }

    // For scheduler extraction, we will need to create a mapping between choices at the auxiliary states and the
    // corresponding choices in the original model.
    std::vector<std::pair<uint_fast64_t, uint_fast64_t>> sspComponentExitChoicesToOriginalMap;

    // The next step is to create the SSP matrix and the right-hand side of the SSP.
    auto sspMatrixVector = buildSspMatrixVector(componentLraValues, inputToSspStateMap, statesNotInComponent, numberOfNonComponentStates,
                                                this->isProduceSchedulerSet() ? &sspComponentExitChoicesToOriginalMap : nullptr);

    // Set-up a solver
    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> minMaxLinearEquationSolverFactory;
    storm::solver::MinMaxLinearEquationSolverRequirements requirements =
        minMaxLinearEquationSolverFactory.getRequirements(env, true, true, this->getOptimizationDirection(), false, this->isProduceSchedulerSet());
    requirements.clearBounds();
    STORM_LOG_THROW(!requirements.hasEnabledCriticalRequirement(), storm::exceptions::UnmetRequirementException,
                    "Solver requirements " + requirements.getEnabledRequirementsAsString() + " not checked.");
    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = minMaxLinearEquationSolverFactory.create(env, sspMatrixVector.first);
    solver->setHasUniqueSolution();
    solver->setHasNoEndComponents();
    solver->setTrackScheduler(this->isProduceSchedulerSet());
    auto lowerUpperBounds = std::minmax_element(componentLraValues.begin(), componentLraValues.end());
    solver->setLowerBound(*lowerUpperBounds.first);
    solver->setUpperBound(*lowerUpperBounds.second);
    solver->setRequirementsChecked();

    // Solve the equation system
    std::vector<ValueType> x(sspMatrixVector.first.getRowGroupCount());
    solver->solveEquations(env, this->getOptimizationDirection(), x, sspMatrixVector.second);

    // Prepare scheduler (if requested)
    if (this->isProduceSchedulerSet() && solver->hasScheduler()) {
        // Translate result for ssp matrix to original model
        constructOptimalChoices(solver->getSchedulerChoices(), sspMatrixVector.first, inputToSspStateMap, statesNotInComponent, numberOfNonComponentStates,
                                sspComponentExitChoicesToOriginalMap);
    } else {
        STORM_LOG_ERROR_COND(!this->isProduceSchedulerSet(), "Requested to produce a scheduler, but no scheduler was generated.");
    }

    // Prepare result vector.
    // For efficiency reasons, we re-use the memory of our rhs for this!
    std::vector<ValueType> result = std::move(sspMatrixVector.second);
    result.resize(this->_transitionMatrix.getRowGroupCount());
    result.shrink_to_fit();
    storm::utility::vector::selectVectorValues(result, inputToSspStateMap, x);
    return result;
}

template class SparseNondeterministicInfiniteHorizonHelper<double>;
template class SparseNondeterministicInfiniteHorizonHelper<storm::RationalNumber>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm