#include "LraViHelper.h"

#include "storm/modelchecker/helper/infinitehorizon/internal/ComponentUtility.h"

#include "storm/storage/MaximalEndComponent.h"
#include "storm/storage/StronglyConnectedComponent.h"

#include "storm/utility/SignalHandler.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"

#include "storm/exceptions/UnmetRequirementException.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace internal {

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
LraViHelper<ValueType, ComponentType, TransitionsType>::LraViHelper(ComponentType const& component,
                                                                    storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                    ValueType const& aperiodicFactor, storm::storage::BitVector const* timedStates,
                                                                    std::vector<ValueType> const* exitRates)
    : _transitionMatrix(transitionMatrix),
      _timedStates(timedStates),
      _hasInstantStates(TransitionsType == LraViTransitionsType::DetTsNondetIs || TransitionsType == LraViTransitionsType::DetTsDetIs),
      _Tsx1IsCurrent(false) {
    setComponent(component);

    // Run through the component and collect some data:
    // We create two submodels, one consisting of the timed states of the component and one consisting of the instant states of the component.
    // For this, we create a state index map that point from state indices of the input model to indices of the corresponding submodel of that state.
    std::map<uint64_t, uint64_t> toSubModelStateMapping;
    // We also obtain state and choices counts of the two submodels
    uint64_t numTsSubModelStates(0), numTsSubModelChoices(0);
    uint64_t numIsSubModelStates(0), numIsSubModelChoices(0);
    // We will need to uniformize the timed MEC states by introducing a selfloop.
    // For this, we need to find a uniformization rate which will be a little higher (given by aperiodicFactor) than the maximum rate occurring in the
    // component.
    _uniformizationRate = exitRates == nullptr ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>();
    // Now run over the MEC and collect the required data.
    for (auto const& element : _component) {
        uint64_t componentState = element.first;
        if (isTimedState(componentState)) {
            toSubModelStateMapping.emplace(componentState, numTsSubModelStates);
            ++numTsSubModelStates;
            numTsSubModelChoices += element.second.size();
            STORM_LOG_ASSERT(nondetTs() || element.second.size() == 1, "Timed state has multiple choices but only a single choice was expected.");
            if (exitRates) {
                _uniformizationRate = std::max(_uniformizationRate, (*exitRates)[componentState]);
            }
        } else {
            toSubModelStateMapping.emplace(componentState, numIsSubModelStates);
            ++numIsSubModelStates;
            numIsSubModelChoices += element.second.size();
            STORM_LOG_ASSERT(nondetIs() || element.second.size() == 1, "Instant state has multiple choices but only a single choice was expected.");
        }
    }
    assert(numIsSubModelStates + numTsSubModelStates == _component.size());
    assert(_hasInstantStates || numIsSubModelStates == 0);
    STORM_LOG_ASSERT(nondetTs() || numTsSubModelStates == numTsSubModelChoices, "Unexpected choice count of deterministic timed submodel.");
    STORM_LOG_ASSERT(nondetIs() || numIsSubModelStates == numIsSubModelChoices, "Unexpected choice count of deterministic instant submodel.");
    _hasInstantStates = _hasInstantStates && numIsSubModelStates > 0;
    STORM_LOG_THROW(
        numTsSubModelStates > 0, storm::exceptions::InvalidOperationException,
        "Bottom Component has no timed states. Computation of Long Run Average values not supported. Is this a Markov Automaton with Zeno behavior?");

    // We make sure that every timed state gets a selfloop to make the model aperiodic
    _uniformizationRate *= storm::utility::one<ValueType>() + aperiodicFactor;

    // Now build the timed and the instant submodels.
    // In addition, we also need the transitions between the two.
    storm::storage::SparseMatrixBuilder<ValueType> tsTransitionsBuilder(numTsSubModelChoices, numTsSubModelStates, 0, true, nondetTs(),
                                                                        nondetTs() ? numTsSubModelStates : 0);
    storm::storage::SparseMatrixBuilder<ValueType> tsToIsTransitionsBuilder, isTransitionsBuilder, isToTsTransitionsBuilder;
    if (_hasInstantStates) {
        tsToIsTransitionsBuilder = storm::storage::SparseMatrixBuilder<ValueType>(numTsSubModelChoices, numIsSubModelStates, 0, true, nondetTs(),
                                                                                  nondetTs() ? numTsSubModelStates : 0);
        isTransitionsBuilder = storm::storage::SparseMatrixBuilder<ValueType>(numIsSubModelChoices, numIsSubModelStates, 0, true, nondetIs(),
                                                                              nondetIs() ? numIsSubModelStates : 0);
        isToTsTransitionsBuilder = storm::storage::SparseMatrixBuilder<ValueType>(numIsSubModelChoices, numTsSubModelStates, 0, true, nondetIs(),
                                                                                  nondetIs() ? numIsSubModelStates : 0);
        _IsChoiceValues.reserve(numIsSubModelChoices);
    }
    ValueType uniformizationFactor = storm::utility::one<ValueType>() / _uniformizationRate;
    uint64_t currTsRow = 0;
    uint64_t currIsRow = 0;
    for (auto const& element : _component) {
        uint64_t componentState = element.first;
        if (isTimedState(componentState)) {
            // The currently processed state is timed.
            if (nondetTs()) {
                tsTransitionsBuilder.newRowGroup(currTsRow);
                if (_hasInstantStates) {
                    tsToIsTransitionsBuilder.newRowGroup(currTsRow);
                }
            }
            // If there are exit rates, the uniformization factor needs to be updated.
            if (exitRates) {
                uniformizationFactor = (*exitRates)[componentState] / _uniformizationRate;
            }
            // We need to uniformize which means that a diagonal entry for the selfloop will be inserted.
            ValueType selfLoopProb = storm::utility::one<ValueType>() - uniformizationFactor;
            for (auto const& componentChoice : element.second) {
                tsTransitionsBuilder.addDiagonalEntry(currTsRow, selfLoopProb);
                for (auto const& entry : this->_transitionMatrix.getRow(componentChoice)) {
                    uint64_t subModelColumn = toSubModelStateMapping[entry.getColumn()];
                    if (isTimedState(entry.getColumn())) {
                        // We have a transition from a timed state to a timed state
                        STORM_LOG_ASSERT(subModelColumn < numTsSubModelStates, "Invalid state for timed submodel");
                        tsTransitionsBuilder.addNextValue(currTsRow, subModelColumn, uniformizationFactor * entry.getValue());
                    } else {
                        // We have a transition from a timed to a instant state
                        STORM_LOG_ASSERT(subModelColumn < numIsSubModelStates, "Invalid state for instant submodel");
                        tsToIsTransitionsBuilder.addNextValue(currTsRow, subModelColumn, uniformizationFactor * entry.getValue());
                    }
                }
                ++currTsRow;
            }
        } else {
            // The currently processed state is instant
            if (nondetIs()) {
                isTransitionsBuilder.newRowGroup(currIsRow);
                isToTsTransitionsBuilder.newRowGroup(currIsRow);
            }
            for (auto const& componentChoice : element.second) {
                for (auto const& entry : this->_transitionMatrix.getRow(componentChoice)) {
                    uint64_t subModelColumn = toSubModelStateMapping[entry.getColumn()];
                    if (isTimedState(entry.getColumn())) {
                        // We have a transition from an instant state to a timed state
                        STORM_LOG_ASSERT(subModelColumn < numTsSubModelStates, "Invalid state for timed submodel");
                        isToTsTransitionsBuilder.addNextValue(currIsRow, subModelColumn, entry.getValue());
                    } else {
                        // We have a transition from an instant to an instant state
                        STORM_LOG_ASSERT(subModelColumn < numIsSubModelStates, "Invalid state for instant submodel");
                        isTransitionsBuilder.addNextValue(currIsRow, subModelColumn, entry.getValue());
                    }
                }
                ++currIsRow;
            }
        }
    }
    _TsTransitions = tsTransitionsBuilder.build();
    if (_hasInstantStates) {
        _TsToIsTransitions = tsToIsTransitionsBuilder.build();
        _IsTransitions = isTransitionsBuilder.build();
        _IsToTsTransitions = isToTsTransitionsBuilder.build();
    }
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
void LraViHelper<ValueType, ComponentType, TransitionsType>::setComponent(ComponentType component) {
    _component.clear();
    for (auto const& element : component) {
        uint64_t componentState = getComponentElementState(element);
        std::set<uint64_t> componentChoices;
        for (auto componentChoiceIt = getComponentElementChoicesBegin(element); componentChoiceIt != getComponentElementChoicesEnd(element);
             ++componentChoiceIt) {
            componentChoices.insert(*componentChoiceIt);
        }
        _component.emplace(std::move(componentState), std::move(componentChoices));
    }
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
ValueType LraViHelper<ValueType, ComponentType, TransitionsType>::performValueIteration(Environment const& env, ValueGetter const& stateValueGetter,
                                                                                        ValueGetter const& actionValueGetter,
                                                                                        std::vector<ValueType> const* exitRates,
                                                                                        storm::solver::OptimizationDirection const* dir,
                                                                                        std::vector<uint64_t>* choices) {
    initializeNewValues(stateValueGetter, actionValueGetter, exitRates);
    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().lra().getPrecision());
    bool relative = env.solver().lra().getRelativeTerminationCriterion();
    boost::optional<uint64_t> maxIter;
    if (env.solver().lra().isMaximalIterationCountSet()) {
        maxIter = env.solver().lra().getMaximalIterationCount();
    }

    // start the iterations
    ValueType result = storm::utility::zero<ValueType>();
    uint64_t iter = 0;
    while (!maxIter.is_initialized() || iter < maxIter.get()) {
        ++iter;
        performIterationStep(env, dir);

        // Check if we are done
        auto convergenceCheckResult = checkConvergence(relative, precision);
        result = convergenceCheckResult.currentValue;
        if (convergenceCheckResult.isPrecisionAchieved) {
            break;
        }
        if (storm::utility::resources::isTerminate()) {
            break;
        }
        // If there will be a next iteration, we have to prepare it.
        prepareNextIteration(env);
    }
    if (maxIter.is_initialized() && iter == maxIter.get()) {
        STORM_LOG_WARN("LRA computation did not converge within " << iter << " iterations.");
    } else if (storm::utility::resources::isTerminate()) {
        STORM_LOG_WARN("LRA computation aborted after " << iter << " iterations.");
    } else {
        STORM_LOG_TRACE("LRA computation converged after " << iter << " iterations.");
    }

    if (choices) {
        // We will be doing one more iteration step and track scheduler choices this time.
        prepareNextIteration(env);
        performIterationStep(env, dir, choices);
    }
    return result;
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
void LraViHelper<ValueType, ComponentType, TransitionsType>::initializeNewValues(ValueGetter const& stateValueGetter, ValueGetter const& actionValueGetter,
                                                                                 std::vector<ValueType> const* exitRates) {
    // clear potential old values and reserve enough space for new values
    _TsChoiceValues.clear();
    _TsChoiceValues.reserve(_TsTransitions.getRowCount());
    if (_hasInstantStates) {
        _IsChoiceValues.clear();
        _IsChoiceValues.reserve(_IsTransitions.getRowCount());
    }

    // Set the new choice-based values
    ValueType actionRewardScalingFactor = storm::utility::one<ValueType>() / _uniformizationRate;
    for (auto const& element : _component) {
        uint64_t componentState = element.first;
        if (isTimedState(componentState)) {
            if (exitRates) {
                actionRewardScalingFactor = (*exitRates)[componentState] / _uniformizationRate;
            }
            for (auto const& componentChoice : element.second) {
                // Compute the values obtained for this choice.
                _TsChoiceValues.push_back(stateValueGetter(componentState) / _uniformizationRate +
                                          actionValueGetter(componentChoice) * actionRewardScalingFactor);
            }
        } else {
            for (auto const& componentChoice : element.second) {
                // Compute the values obtained for this choice.
                // State values do not count here since no time passes in instant states.
                _IsChoiceValues.push_back(actionValueGetter(componentChoice));
            }
        }
    }

    // Set-up new iteration vectors for timed states
    _Tsx1.assign(_TsTransitions.getRowGroupCount(), storm::utility::zero<ValueType>());
    _Tsx2 = _Tsx1;

    if (_hasInstantStates) {
        // Set-up vectors for storing intermediate results for instant states.
        _Isx.resize(_IsTransitions.getRowGroupCount(), storm::utility::zero<ValueType>());
        _Isb = _IsChoiceValues;
    }
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
void LraViHelper<ValueType, ComponentType, TransitionsType>::prepareSolversAndMultipliers(const Environment& env,
                                                                                          storm::solver::OptimizationDirection const* dir) {
    _TsMultiplier = storm::solver::MultiplierFactory<ValueType>().create(env, _TsTransitions);
    if (_hasInstantStates) {
        if (_IsTransitions.getNonzeroEntryCount() > 0) {
            // Set-up a solver for transitions within instant states
            _IsSolverEnv = std::make_unique<storm::Environment>(env);
            if (env.solver().isForceSoundness()) {
                // To get correct results, the inner equation systems are solved exactly.
                // TODO investigate how an error would propagate
                _IsSolverEnv->solver().setForceExact(true);
            }
            bool isAcyclic = !storm::utility::graph::hasCycle(_IsTransitions);
            if (isAcyclic) {
                STORM_LOG_INFO("Instant transitions are acyclic.");
                _IsSolverEnv->solver().minMax().setMethod(storm::solver::MinMaxMethod::Acyclic);
                _IsSolverEnv->solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Acyclic);
            }
            if (nondetIs()) {
                storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> factory;
                _NondetIsSolver = factory.create(*_IsSolverEnv, _IsTransitions);
                _NondetIsSolver->setHasUniqueSolution(true);   // Assume non-zeno MA
                _NondetIsSolver->setHasNoEndComponents(true);  // assume non-zeno MA
                _NondetIsSolver->setCachingEnabled(true);
                auto req = _NondetIsSolver->getRequirements(*_IsSolverEnv, *dir);
                req.clearUniqueSolution();
                if (isAcyclic) {
                    req.clearAcyclic();
                }
                // Computing a priori lower/upper bounds is not particularly easy, as there might be selfloops with high probabilities
                // Which accumulate a lot of reward. Moreover, the right-hand-side of the equation system changes dynamically.
                STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UnmetRequirementException,
                                "The solver requirement " << req.getEnabledRequirementsAsString() << " has not been cleared.");
                _NondetIsSolver->setRequirementsChecked(true);
            } else {
                storm::solver::GeneralLinearEquationSolverFactory<ValueType> factory;
                if (factory.getEquationProblemFormat(*_IsSolverEnv) != storm::solver::LinearEquationSolverProblemFormat::FixedPointSystem) {
                    // We need to convert the transition matrix connecting instant states
                    // TODO: This could have been done already during construction of the matrix.
                    // Insert diagonal entries.
                    storm::storage::SparseMatrix<ValueType> converted(_IsTransitions, true);
                    // Compute A' = 1-A
                    converted.convertToEquationSystem();
                    STORM_LOG_WARN("The selected equation solver requires to create a temporary " << converted.getDimensionsAsString());
                    // Note that the solver has ownership of the converted matrix.
                    _DetIsSolver = factory.create(*_IsSolverEnv, std::move(converted));
                } else {
                    _DetIsSolver = factory.create(*_IsSolverEnv, _IsTransitions);
                }
                _DetIsSolver->setCachingEnabled(true);
                auto req = _DetIsSolver->getRequirements(*_IsSolverEnv);
                if (isAcyclic) {
                    req.clearAcyclic();
                }
                // A priori lower/upper bounds are hard (see MinMax version of this above)
                STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UnmetRequirementException,
                                "The solver requirement " << req.getEnabledRequirementsAsString() << " has not been cleared.");
            }
        }

        // Set up multipliers for transitions connecting timed and instant states
        _TsToIsMultiplier = storm::solver::MultiplierFactory<ValueType>().create(env, _TsToIsTransitions);
        _IsToTsMultiplier = storm::solver::MultiplierFactory<ValueType>().create(env, _IsToTsTransitions);
    }
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
void LraViHelper<ValueType, ComponentType, TransitionsType>::setInputModelChoices(std::vector<uint64_t>& choices, std::vector<uint64_t> const& localMecChoices,
                                                                                  bool setChoiceZeroToTimedStates, bool setChoiceZeroToInstantStates) const {
    // Transform the local choices (within this mec) to choice indices for the input model
    uint64_t localState = 0;
    for (auto const& element : _component) {
        uint64_t elementState = element.first;
        if ((setChoiceZeroToTimedStates && isTimedState(elementState)) || (setChoiceZeroToInstantStates && !isTimedState(elementState))) {
            choices[elementState] = 0;
        } else {
            uint64_t choice = localMecChoices[localState];
            STORM_LOG_ASSERT(choice < element.second.size(), "The selected choice does not seem to exist.");
            auto globalChoiceIndexIt = element.second.begin();
            for (uint64_t i = 0; i < choice; ++i) {
                ++globalChoiceIndexIt;
            }
            uint64_t globalChoiceIndex = *(globalChoiceIndexIt);
            choices[elementState] = globalChoiceIndex - _transitionMatrix.getRowGroupIndices()[elementState];
            ++localState;
        }
    }
    STORM_LOG_ASSERT(localState == localMecChoices.size(), "Did not traverse all component states.");
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
void LraViHelper<ValueType, ComponentType, TransitionsType>::performIterationStep(Environment const& env, storm::solver::OptimizationDirection const* dir,
                                                                                  std::vector<uint64_t>* choices) {
    STORM_LOG_ASSERT(!((nondetTs() || nondetIs()) && dir == nullptr), "No optimization direction provided for model with nondeterminism");
    // Initialize value vectors, multiplers, and solver if this has not been done, yet
    if (!_TsMultiplier) {
        prepareSolversAndMultipliers(env, dir);
    }

    // Compute new x values for the timed states
    // Flip what is new and what is old
    _Tsx1IsCurrent = !_Tsx1IsCurrent;
    // At this point, xOld() points to what has been computed in the most recent call of performIterationStep (initially, this is the 0-vector).
    // The result of this ongoing computation will be stored in xNew()

    // Compute the values obtained by a single uniformization step between timed states only
    if (nondetTs()) {
        if (choices == nullptr) {
            _TsMultiplier->multiplyAndReduce(env, *dir, xOld(), &_TsChoiceValues, xNew());
        } else {
            // Also keep track of the choices made.
            std::vector<uint64_t> tsChoices(_TsTransitions.getRowGroupCount());
            _TsMultiplier->multiplyAndReduce(env, *dir, xOld(), &_TsChoiceValues, xNew(), UncertaintyResolutionMode::Unset, &tsChoices);
            // Note that nondeterminism within the timed states means that there can not be instant states (We either have MDPs or MAs)
            // Hence, in this branch we don't have to care for choices at instant states.
            STORM_LOG_ASSERT(!_hasInstantStates, "Nondeterministic timed states are only supported if there are no instant states.");
            setInputModelChoices(*choices, tsChoices);
        }
    } else {
        _TsMultiplier->multiply(env, xOld(), &_TsChoiceValues, xNew());
    }
    if (_hasInstantStates) {
        // Add the values obtained by taking a single uniformization step that leads to an instant state followed by arbitrarily many instant steps.
        // First compute the total values when taking arbitrarily many instant transitions (in no time)
        if (_NondetIsSolver) {
            // We might need to track the optimal choices.
            if (choices == nullptr) {
                _NondetIsSolver->solveEquations(*_IsSolverEnv, *dir, _Isx, _Isb);
            } else {
                _NondetIsSolver->setTrackScheduler();
                _NondetIsSolver->solveEquations(*_IsSolverEnv, *dir, _Isx, _Isb);
                setInputModelChoices(*choices, _NondetIsSolver->getSchedulerChoices(), true);
            }
        } else if (_DetIsSolver) {
            _DetIsSolver->solveEquations(*_IsSolverEnv, _Isx, _Isb);
        } else {
            STORM_LOG_ASSERT(_IsTransitions.getNonzeroEntryCount() == 0, "If no solver was initialized, an empty matrix would have been expected.");
            if (nondetIs()) {
                if (choices == nullptr) {
                    storm::utility::vector::reduceVectorMinOrMax(*dir, _Isb, _Isx, _IsTransitions.getRowGroupIndices());
                } else {
                    std::vector<uint64_t> psChoices(_IsTransitions.getRowGroupCount());
                    storm::utility::vector::reduceVectorMinOrMax(*dir, _Isb, _Isx, _IsTransitions.getRowGroupIndices(), &psChoices);
                    setInputModelChoices(*choices, psChoices, true);
                }
            } else {
                // For deterministic instant states, there is nothing to reduce, i.e., we could just set _Isx = _Isb.
                // For efficiency reasons, we do a swap instead:
                _Isx.swap(_Isb);
                // Note that at this point we have changed the contents of _Isb, but they will be overwritten anyway.
                if (choices) {
                    // Set choice 0 to all states.
                    setInputModelChoices(*choices, {}, true, true);
                }
            }
        }
        // Now add the (weighted) values of the instant states to the values of the timed states.
        _TsToIsMultiplier->multiply(env, _Isx, &xNew(), xNew());
    }
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
typename LraViHelper<ValueType, ComponentType, TransitionsType>::ConvergenceCheckResult
LraViHelper<ValueType, ComponentType, TransitionsType>::checkConvergence(bool relative, ValueType precision) const {
    STORM_LOG_ASSERT(_TsMultiplier, "tried to check for convergence without doing an iteration first.");
    // All values are scaled according to the uniformizationRate.
    // We need to 'revert' this scaling when computing the absolute precision.
    // However, for relative precision, the scaling cancels out.
    ValueType threshold = relative ? precision : ValueType(precision / _uniformizationRate);

    ConvergenceCheckResult res = {true, storm::utility::one<ValueType>()};
    // Now check whether the currently produced results are precise enough
    STORM_LOG_ASSERT(threshold > storm::utility::zero<ValueType>(), "Did not expect a non-positive threshold.");
    auto x1It = xOld().begin();
    auto x1Ite = xOld().end();
    auto x2It = xNew().begin();
    ValueType maxDiff = (*x2It - *x1It);
    ValueType minDiff = maxDiff;
    // The difference between maxDiff and minDiff is zero at this point. Thus, it doesn't make sense to check the threshold now.
    for (++x1It, ++x2It; x1It != x1Ite; ++x1It, ++x2It) {
        ValueType diff = (*x2It - *x1It);
        // Potentially update maxDiff or minDiff
        bool skipCheck = false;
        if (maxDiff < diff) {
            maxDiff = diff;
        } else if (minDiff > diff) {
            minDiff = diff;
        } else {
            skipCheck = true;
        }
        // Check convergence
        if (!skipCheck && (maxDiff - minDiff) > (relative ? (threshold * minDiff) : threshold)) {
            res.isPrecisionAchieved = false;
            break;
        }
    }

    // Compute the average of the maximal and the minimal difference.
    ValueType avgDiff = (maxDiff + minDiff) / (storm::utility::convertNumber<ValueType>(2.0));

    // "Undo" the scaling of the values
    res.currentValue = avgDiff * _uniformizationRate;
    return res;
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
void LraViHelper<ValueType, ComponentType, TransitionsType>::prepareNextIteration(Environment const& env) {
    // To avoid large (and numerically unstable) x-values, we substract a reference value.
    ValueType referenceValue = xNew().front();
    storm::utility::vector::applyPointwise<ValueType, ValueType>(xNew(), xNew(),
                                                                 [&referenceValue](ValueType const& x_i) -> ValueType { return x_i - referenceValue; });
    if (_hasInstantStates) {
        // Update the RHS of the equation system for the instant states by taking the new values of timed states into account.
        STORM_LOG_ASSERT(!nondetTs(), "Nondeterministic timed states not expected when there are also instant states.");
        _IsToTsMultiplier->multiply(env, xNew(), &_IsChoiceValues, _Isb);
    }
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
bool LraViHelper<ValueType, ComponentType, TransitionsType>::isTimedState(uint64_t const& inputModelStateIndex) const {
    STORM_LOG_ASSERT(!_hasInstantStates || _timedStates != nullptr, "Model has instant states but no partition into timed and instant states is given.");
    STORM_LOG_ASSERT(!_hasInstantStates || inputModelStateIndex < _timedStates->size(),
                     "Unable to determine whether state " << inputModelStateIndex << " is timed.");
    return !_hasInstantStates || _timedStates->get(inputModelStateIndex);
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
std::vector<ValueType>& LraViHelper<ValueType, ComponentType, TransitionsType>::xNew() {
    return _Tsx1IsCurrent ? _Tsx1 : _Tsx2;
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
std::vector<ValueType> const& LraViHelper<ValueType, ComponentType, TransitionsType>::xNew() const {
    return _Tsx1IsCurrent ? _Tsx1 : _Tsx2;
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
std::vector<ValueType>& LraViHelper<ValueType, ComponentType, TransitionsType>::xOld() {
    return _Tsx1IsCurrent ? _Tsx2 : _Tsx1;
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
std::vector<ValueType> const& LraViHelper<ValueType, ComponentType, TransitionsType>::xOld() const {
    return _Tsx1IsCurrent ? _Tsx2 : _Tsx1;
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
bool LraViHelper<ValueType, ComponentType, TransitionsType>::nondetTs() const {
    return TransitionsType == LraViTransitionsType::NondetTsNoIs;
}

template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
bool LraViHelper<ValueType, ComponentType, TransitionsType>::nondetIs() const {
    return TransitionsType == LraViTransitionsType::DetTsNondetIs;
}

template class LraViHelper<double, storm::storage::MaximalEndComponent, LraViTransitionsType::NondetTsNoIs>;
template class LraViHelper<storm::RationalNumber, storm::storage::MaximalEndComponent, LraViTransitionsType::NondetTsNoIs>;
template class LraViHelper<double, storm::storage::MaximalEndComponent, LraViTransitionsType::DetTsNondetIs>;
template class LraViHelper<storm::RationalNumber, storm::storage::MaximalEndComponent, LraViTransitionsType::DetTsNondetIs>;

template class LraViHelper<double, storm::storage::StronglyConnectedComponent, LraViTransitionsType::DetTsNoIs>;
template class LraViHelper<storm::RationalNumber, storm::storage::StronglyConnectedComponent, LraViTransitionsType::DetTsNoIs>;

}  // namespace internal
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm