#include "storm/modelchecker/helper/indefinitehorizon/visitingtimes/SparseDeterministicVisitingTimesHelper.h"

#include <algorithm>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace helper {
template<typename ValueType>
SparseDeterministicVisitingTimesHelper<ValueType>::SparseDeterministicVisitingTimesHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix)
    : transitionMatrix(transitionMatrix),
      exitRates(storm::NullRef),
      backwardTransitions(storm::NullRef),
      sccDecomposition(storm::NullRef),
      nonBsccStates(transitionMatrix.getRowCount(), false) {
    // Intentionally left empty
}

template<typename ValueType>
SparseDeterministicVisitingTimesHelper<ValueType>::SparseDeterministicVisitingTimesHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                          std::vector<ValueType> const& exitRates)
    : transitionMatrix(transitionMatrix),
      exitRates(exitRates),
      backwardTransitions(storm::NullRef),
      sccDecomposition(storm::NullRef),
      nonBsccStates(transitionMatrix.getRowCount(), false) {
    // Note that we are not checking assumptions regarding well-formedness of the input, even in debug mode.
}

template<typename ValueType>
void SparseDeterministicVisitingTimesHelper<ValueType>::provideBackwardTransitions(storm::storage::SparseMatrix<ValueType> const& providedBackwardTransitions) {
    STORM_LOG_WARN_COND(!backwardTransitions, "Backward transition matrix was provided but it was already computed or provided before.");
    backwardTransitions.reset(providedBackwardTransitions);
}

template<typename ValueType>
void SparseDeterministicVisitingTimesHelper<ValueType>::provideSCCDecomposition(
    storm::storage::StronglyConnectedComponentDecomposition<ValueType> const& decomposition) {
    STORM_LOG_WARN_COND(!sccDecomposition, "SCC Decomposition was provided but it was already computed or provided before.");
    sccDecomposition.reset(decomposition);
}

template<typename ValueType>
std::vector<ValueType> SparseDeterministicVisitingTimesHelper<ValueType>::computeExpectedVisitingTimes(Environment const& env,
                                                                                                       storm::storage::BitVector const& initialStates) {
    STORM_LOG_ASSERT(!initialStates.empty(), "Provided an empty set of initial states.");
    STORM_LOG_ASSERT(initialStates.size() == transitionMatrix.getRowCount(), "Dimension mismatch.");
    ValueType const p = storm::utility::one<ValueType>() / storm::utility::convertNumber<ValueType, uint64_t>(initialStates.getNumberOfSetBits());
    std::vector<ValueType> result(transitionMatrix.getRowCount(), storm::utility::zero<ValueType>());
    storm::utility::vector::setVectorValues(result, initialStates, p);
    computeExpectedVisitingTimes(env, result);
    return result;
}

template<typename ValueType>
std::vector<ValueType> SparseDeterministicVisitingTimesHelper<ValueType>::computeExpectedVisitingTimes(Environment const& env, uint64_t initialState) {
    STORM_LOG_ASSERT(initialState < transitionMatrix.getRowCount(), "Invalid initial state index.");
    std::vector<ValueType> result(transitionMatrix.getRowCount(), storm::utility::zero<ValueType>());
    result[initialState] = storm::utility::one<ValueType>();
    computeExpectedVisitingTimes(env, result);
    return result;
}

template<typename ValueType>
std::vector<ValueType> SparseDeterministicVisitingTimesHelper<ValueType>::computeExpectedVisitingTimes(Environment const& env,
                                                                                                       ValueGetter const& initialStateValueGetter) {
    std::vector<ValueType> result;
    result.reserve(transitionMatrix.getRowCount());
    for (uint64_t s = 0; s != transitionMatrix.getRowCount(); ++s) {
        result.push_back(initialStateValueGetter(s));
    }
    computeExpectedVisitingTimes(env, result);
    return result;
}

template<typename ValueType>
void SparseDeterministicVisitingTimesHelper<ValueType>::computeExpectedVisitingTimes(Environment const& env, std::vector<ValueType>& stateValues) {
    STORM_LOG_ASSERT(stateValues.size() == transitionMatrix.getRowCount(), "Dimension missmatch.");
    createBackwardTransitions();
    createDecomposition(env);
    createNonBsccStateVector();

    // Create auxiliary data and lambdas
    storm::storage::BitVector sccAsBitVector(stateValues.size(), false);
    auto isLeavingTransition = [&sccAsBitVector](auto const& e) { return !sccAsBitVector.get(e.getColumn()); };
    auto isLeavingTransitionWithNonZeroValue = [&isLeavingTransition, &stateValues](auto const& e) {
        return isLeavingTransition(e) && !storm::utility::isZero(stateValues[e.getColumn()]);
    };
    auto isReachableInState = [this, &isLeavingTransitionWithNonZeroValue, &stateValues](uint64_t state) {
        if (!storm::utility::isZero(stateValues[state])) {
            return true;
        }
        auto row = this->backwardTransitions->getRow(state);
        return std::any_of(row.begin(), row.end(), isLeavingTransitionWithNonZeroValue);
    };

    if (env.solver().getLinearEquationSolverType() == storm::solver::EquationSolverType::Topological) {
        // Compute EVTs SCC wise in topological order
        // We need to adapt precision if we solve each SCC separately (in topological order) and/or consider CTMCs
        auto sccEnv = getEnvironmentForSolver(env, true);

        // We solve each SCC individually in *forward* topological order
        storm::utility::ProgressMeasurement progress("sccs");
        progress.setMaxCount(sccDecomposition->size());
        progress.startNewMeasurement(0);
        uint64_t sccIndex = 0;
        auto sccItEnd = std::make_reverse_iterator(sccDecomposition->begin());
        for (auto sccIt = std::make_reverse_iterator(sccDecomposition->end()); sccIt != sccItEnd; ++sccIt) {
            auto const& scc = *sccIt;
            if (scc.size() == 1) {
                processSingletonScc(*scc.begin(), stateValues);
            } else {
                sccAsBitVector.set(scc.begin(), scc.end(), true);
                if (sccAsBitVector.isSubsetOf(nonBsccStates)) {
                    // This is not a BSCC
                    auto sccResult = computeValueForStateSet(sccEnv, sccAsBitVector, stateValues);
                    storm::utility::vector::setVectorValues(stateValues, sccAsBitVector, sccResult);
                } else {
                    // This is a BSCC
                    if (std::any_of(sccAsBitVector.begin(), sccAsBitVector.end(), isReachableInState)) {
                        storm::utility::vector::setVectorValues(stateValues, sccAsBitVector, storm::utility::infinity<ValueType>());
                    } else {
                        storm::utility::vector::setVectorValues(stateValues, sccAsBitVector, storm::utility::zero<ValueType>());
                    }
                }
                sccAsBitVector.clear();
            }
            ++sccIndex;
            progress.updateProgress(sccIndex);
            if (storm::utility::resources::isTerminate()) {
                STORM_LOG_WARN("Visiting times computation aborted after analyzing " << sccIndex << "/" << this->computedSccDecomposition->size() << " SCCs.");
                break;
            }
        }
    } else {
        // We solve the equation system for all non-BSCC in one step (not each SCC individually - adaption of precision is not necessary).
        if (!nonBsccStates.empty()) {
            // We need to adapt precision if we consider CTMCs.
            Environment adjustedEnv = getEnvironmentForSolver(env, false);
            auto result = computeValueForStateSet(adjustedEnv, nonBsccStates, stateValues);
            storm::utility::vector::setVectorValues(stateValues, nonBsccStates, result);
        }

        // After computing the state values for the  non-BSCCs, we can set the values of the BSCC states.
        auto sccItEnd = std::make_reverse_iterator(sccDecomposition->begin());
        for (auto sccIt = std::make_reverse_iterator(sccDecomposition->end()); sccIt != sccItEnd; ++sccIt) {
            auto const& scc = *sccIt;
            sccAsBitVector.set(scc.begin(), scc.end(), true);
            if (sccAsBitVector.isSubsetOf(~nonBsccStates)) {
                // This is a BSCC, we set the values of the states to infinity or 0.
                if (std::any_of(sccAsBitVector.begin(), sccAsBitVector.end(), isReachableInState)) {
                    // The BSCC is reachable: The EVT is infinity
                    storm::utility::vector::setVectorValues(stateValues, sccAsBitVector, storm::utility::infinity<ValueType>());
                } else {
                    // The BSCC is not reachable: The EVT is zero
                    storm::utility::vector::setVectorValues(stateValues, sccAsBitVector, storm::utility::zero<ValueType>());
                }
            }
            sccAsBitVector.clear();
        }
    }

    if (isContinuousTime()) {
        // Divide with the exit rates
        // Since storm::utility::infinity<storm::RationalNumber>() is just set to some big number, we have to treat the infinity-case explicitly.
        storm::utility::vector::applyPointwise(stateValues, *exitRates, stateValues, [](ValueType const& xi, ValueType const& yi) -> ValueType {
            return storm::utility::isInfinity(xi) ? xi : xi / yi;
        });
    }
}

template<typename ValueType>
bool SparseDeterministicVisitingTimesHelper<ValueType>::isContinuousTime() const {
    return exitRates;
}

template<typename ValueType>
void SparseDeterministicVisitingTimesHelper<ValueType>::createBackwardTransitions() {
    if (!this->backwardTransitions) {
        this->computedBackwardTransitions =
            std::make_unique<storm::storage::SparseMatrix<ValueType>>(transitionMatrix.transpose(true, false));  // will drop zeroes
        this->backwardTransitions.reset(*this->computedBackwardTransitions);
    }
}

template<typename ValueType>
void SparseDeterministicVisitingTimesHelper<ValueType>::createDecomposition(Environment const& env) {
    if (this->sccDecomposition && !this->sccDecomposition->hasSccDepth() && env.solver().isForceSoundness()) {
        // We are missing SCCDepths in the given decomposition.
        STORM_LOG_WARN("Recomputing SCC Decomposition because the currently available decomposition is computed without SCCDepths.");
        this->computedSccDecomposition.reset();
        this->sccDecomposition.reset();
    }

    if (!this->sccDecomposition) {
        // The decomposition has not been provided or computed, yet.
        auto options =
            storm::storage::StronglyConnectedComponentDecompositionOptions().forceTopologicalSort().computeSccDepths(env.solver().isForceSoundness());
        this->computedSccDecomposition = std::make_unique<storm::storage::StronglyConnectedComponentDecomposition<ValueType>>(this->transitionMatrix, options);
        this->sccDecomposition.reset(*this->computedSccDecomposition);
    }
}

template<typename ValueType>
void SparseDeterministicVisitingTimesHelper<ValueType>::createNonBsccStateVector() {
    // Create auxiliary data and lambdas
    storm::storage::BitVector sccAsBitVector(transitionMatrix.getRowCount(), false);
    auto isLeavingTransition = [&sccAsBitVector](auto const& e) { return !sccAsBitVector.get(e.getColumn()); };
    auto isExitState = [this, &isLeavingTransition](uint64_t state) {
        auto row = this->transitionMatrix.getRow(state);
        return std::any_of(row.begin(), row.end(), isLeavingTransition);
    };

    auto sccItEnd = std::make_reverse_iterator(sccDecomposition->begin());
    for (auto sccIt = std::make_reverse_iterator(sccDecomposition->end()); sccIt != sccItEnd; ++sccIt) {
        auto const& scc = *sccIt;
        sccAsBitVector.set(scc.begin(), scc.end(), true);
        if (std::any_of(sccAsBitVector.begin(), sccAsBitVector.end(), isExitState)) {
            // This is not a BSCC, mark the states correspondingly.
            nonBsccStates = nonBsccStates | sccAsBitVector;
        }
        sccAsBitVector.clear();
    }
}

template<>
std::vector<storm::RationalFunction> SparseDeterministicVisitingTimesHelper<storm::RationalFunction>::computeUpperBounds(
    storm::storage::BitVector const& /*stateSetAsBitvector*/) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                    "Computing upper bounds for expected visiting times over rational functions is not supported.");
}

template<typename ValueType>
std::vector<ValueType> SparseDeterministicVisitingTimesHelper<ValueType>::computeUpperBounds(storm::storage::BitVector const& stateSetAsBitvector) const {
    // Compute the one-step probabilities that lead to states outside stateSetAsBitvector
    std::vector<ValueType> leavingTransitions = transitionMatrix.getConstrainedRowGroupSumVector(stateSetAsBitvector, ~stateSetAsBitvector);

    // Build the submatrix that only has the transitions between states in the state set.
    storm::storage::SparseMatrix<ValueType> subTransitionMatrix = transitionMatrix.getSubmatrix(false, stateSetAsBitvector, stateSetAsBitvector);

    // Compute the upper bounds on EVTs for non-BSCC states (using the same state-to-scc mapping).
    std::vector<ValueType> upperBounds = storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType>::computeUpperBoundOnExpectedVisitingTimes(
        subTransitionMatrix, leavingTransitions);
    return upperBounds;
}

template<typename ValueType>
storm::Environment SparseDeterministicVisitingTimesHelper<ValueType>::getEnvironmentForSolver(storm::Environment const& env, bool topological) const {
    storm::Environment newEnv(env);

    if (topological) {
        // Overwrite the environment with the environment for the underlying solver.
        newEnv = getEnvironmentForTopologicalSolver(env);
    }

    // Adjust precision for CTMCs, because the EVTs are divided by the exit rates at the end.
    if (isContinuousTime()) {
        auto prec = newEnv.solver().getPrecisionOfLinearEquationSolver(newEnv.solver().getLinearEquationSolverType());

        bool needAdaptPrecision =
            env.solver().isForceSoundness() && prec.first.is_initialized() && prec.second.is_initialized() &&
            !newEnv.solver().getPrecisionOfLinearEquationSolver(newEnv.solver().getLinearEquationSolverType()).second.get();  // only for the absolute criterion

        // Assert that we only adapt the precision for native solvers
        STORM_LOG_ASSERT(
            !needAdaptPrecision || env.solver().getLinearEquationSolverType() == storm::solver::EquationSolverType::Native,
            "The precision for the current solver type is not adjusted for this solving method. Ensure that this is correct for topological computation.");

        if (needAdaptPrecision) {
            // the precision is relevant (e.g. not the case for elimination, sparselu etc.)
            ValueType min = *std::min_element(exitRates->begin(), exitRates->end());
            STORM_LOG_THROW(!storm::utility::isZero(min), storm::exceptions::InvalidOperationException,
                            "An error occurred during the adjustment of the precision. Min. rate = " << min << ".");
            newEnv.solver().setLinearEquationSolverPrecision(
                static_cast<storm::RationalNumber>(prec.first.get() * storm::utility::convertNumber<storm::RationalNumber>(min)));
        }
    }

    auto prec = newEnv.solver().getPrecisionOfLinearEquationSolver(newEnv.solver().getLinearEquationSolverType());
    if (prec.first.is_initialized()) {
        STORM_LOG_INFO("Precision for EVTs computation: " << storm::utility::convertNumber<double>(prec.first.get()) << " (exact: " << prec.first.get() << ")"
                                                          << '\n');
    }

    return newEnv;
}

template<typename ValueType>
storm::Environment SparseDeterministicVisitingTimesHelper<ValueType>::getEnvironmentForTopologicalSolver(storm::Environment const& env) const {
    storm::Environment subEnv(env);
    subEnv.solver().setLinearEquationSolverType(env.solver().topological().getUnderlyingEquationSolverType(),
                                                env.solver().topological().isUnderlyingEquationSolverTypeSetFromDefault());

    // To guarantee soundness for OVI, II, SVI we need to increase the precision in each SCC.
    auto subEnvPrec = subEnv.solver().getPrecisionOfLinearEquationSolver(subEnv.solver().getLinearEquationSolverType());

    bool singletonSCCs = true;  // true if each SCC is a singleton (self-loops are allowed)
    storm::storage::BitVector sccAsBitVector(transitionMatrix.getRowCount(), false);
    auto sccItEnd = std::make_reverse_iterator(sccDecomposition->begin());
    for (auto sccIt = std::make_reverse_iterator(sccDecomposition->end()); sccIt != sccItEnd; ++sccIt) {
        auto const& scc = *sccIt;
        sccAsBitVector.set(scc.begin(), scc.end(), true);
        if (sccAsBitVector.isSubsetOf(nonBsccStates)) {
            // This is not a BSCC, mark the states correspondingly.
            if (sccAsBitVector.getNumberOfSetBits() > 1) {
                singletonSCCs = false;
                break;
            }
        }
        sccAsBitVector.clear();
    }

    bool needAdaptPrecision = env.solver().isForceSoundness() && subEnvPrec.first.is_initialized() && subEnvPrec.second.is_initialized() &&
                              !singletonSCCs;  // singleton sccs are solved directly

    // Assert that we only adapt the precision for native solvers
    STORM_LOG_ASSERT(
        !needAdaptPrecision || subEnv.solver().getLinearEquationSolverType() == storm::solver::EquationSolverType::Native,
        "The precision for the current solver type is not adjusted for this solving method. Ensure that this is correct for topological computation.");

    if (needAdaptPrecision && subEnvPrec.second.get()) {
        STORM_LOG_ASSERT(sccDecomposition->hasSccDepth(), "Did not compute the longest SCC chain size although it is needed.");
        // Sound computations wrt. relative precision:
        // We need to increase the solver's relative precision that is used in an SCC depending on the maximal SCC chain length.
        auto subEnvPrec = subEnv.solver().getPrecisionOfLinearEquationSolver(subEnv.solver().getLinearEquationSolverType());

        double scaledPrecision1 = 1 - std::pow(1 - storm::utility::convertNumber<double>(subEnvPrec.first.get()), 1.0 / sccDecomposition->getMaxSccDepth());

        // set new precision
        subEnv.solver().setLinearEquationSolverPrecision(storm::utility::convertNumber<storm::RationalNumber>(scaledPrecision1));

    } else if (needAdaptPrecision && !subEnv.solver().getPrecisionOfLinearEquationSolver(subEnv.solver().getLinearEquationSolverType()).second.get()) {
        // Sound computations wrt. absolute precision:
        // STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Sound computations of EVTs wrt. absolute precision is not supported.");

        // TODO: there is probably a better way to implement this
        if (sccDecomposition->getMaxSccDepth() > 1) {
            // The chain length exceeds one, we need to adjust the precision
            // The adjustment of the precision used in each SCC depends on the maximal SCC chain length,
            // the maximal number of incoming transitions, and the maximal probability <1 to reach another states in an SCC.

            storm::storage::BitVector sccAsBitVector(transitionMatrix.getRowCount(), false);

            // maxNumInc: the maximal number of incoming transitions to an SCC.
            uint_fast64_t maxNumInc = 0;

            // maxEVT: the maximal value of 1/(1-p), where p is the recurrence probability within an SCC (this value stays 0 if there are no cycles)
            ValueType boundEVT = storm::utility::zero<ValueType>();

            auto sccItEnd = std::make_reverse_iterator(sccDecomposition->begin());
            for (auto sccIt = std::make_reverse_iterator(sccDecomposition->end()); sccIt != sccItEnd; ++sccIt) {
                auto const& scc = *sccIt;
                sccAsBitVector.set(scc.begin(), scc.end(), true);
                if (sccAsBitVector.isSubsetOf(nonBsccStates)) {
                    // This is NOT a BSCC.

                    // Get number of incoming transitions to this scc (from different sccs)
                    auto toSccMatrix = transitionMatrix.getSubmatrix(false, ~sccAsBitVector, sccAsBitVector);
                    uint_fast64_t localNumInc =
                        std::count_if(toSccMatrix.begin(), toSccMatrix.end(), [](auto const& e) { return !storm::utility::isZero(e.getValue()); });
                    maxNumInc = std::max(maxNumInc, localNumInc);

                    // Compute the upper bounds on 1/(1-p) within the SCC
                    // p is the maximal recurrence probability, i.e., 1/(1-p) is an upperBound on the EVTs of the DTMC restricted to the SCC (Baier)
                    std::vector<ValueType> upperBounds = computeUpperBounds(sccAsBitVector);
                    boundEVT = std::max(boundEVT, (*std::max_element(upperBounds.begin(), upperBounds.end())));

                    sccAsBitVector.clear();
                }
            }

            storm::RationalNumber one = storm::RationalNumber(1);
            storm::RationalNumber scale = one;

            if (maxNumInc != 0) {
                // As the maximal number of incoming transitions is greater than one, adjustment is necessary.
                // For this, we need the number of SCCs in the longest SCC chain -1, i.e., sccDecomposition->getMaxSccDepth() -1
                for (uint64_t i = 1; i < sccDecomposition->getMaxSccDepth(); i++) {
                    scale = scale + storm::utility::pow(storm::utility::convertNumber<storm::RationalNumber>(maxNumInc), i) *
                                        storm::utility::convertNumber<storm::RationalNumber>(boundEVT);
                }
            }
            subEnv.solver().setLinearEquationSolverPrecision(static_cast<storm::RationalNumber>(subEnvPrec.first.get() / scale));
        }
    }

    return subEnv;
}

template<typename ValueType>
void SparseDeterministicVisitingTimesHelper<ValueType>::processSingletonScc(uint64_t sccState, std::vector<ValueType>& stateValues) const {
    auto& stateVal = stateValues[sccState];
    auto forwardRow = transitionMatrix.getRow(sccState);
    auto backwardRow = backwardTransitions->getRow(sccState);
    if (forwardRow.getNumberOfEntries() == 1 && forwardRow.begin()->getColumn() == sccState) {
        // This is a BSCC. We only have to check if there is some non-zero "input"
        if (!storm::utility::isZero(stateVal) || std::any_of(backwardRow.begin(), backwardRow.end(),
                                                             [&stateValues](auto const& e) { return !storm::utility::isZero(stateValues[e.getColumn()]); })) {
            stateVal = storm::utility::infinity<ValueType>();
        }  // else stateVal = 0 (already implied by !(if-condition))
    } else {
        // This is not a BSCC. Compute the state value
        ValueType divisor = storm::utility::one<ValueType>();
        for (auto const& entry : backwardRow) {
            if (entry.getColumn() == sccState) {
                STORM_LOG_ASSERT(!storm::utility::isOne(entry.getValue()), "Found a self-loop state. This is not expected.");
                divisor -= entry.getValue();
            } else {
                stateVal += entry.getValue() * stateValues[entry.getColumn()];
            }
        }
        stateVal /= divisor;
    }
}

template<typename ValueType>
std::vector<ValueType> SparseDeterministicVisitingTimesHelper<ValueType>::computeValueForStateSet(storm::Environment const& env,
                                                                                                  storm::storage::BitVector const& stateSetAsBitvector,
                                                                                                  std::vector<ValueType> const& stateValues) const {
    // Get the vector for the equation system
    auto sccVector = storm::utility::vector::filterVector(stateValues, stateSetAsBitvector);
    auto valIt = sccVector.begin();
    for (auto sccState : stateSetAsBitvector) {
        for (auto const& entry : backwardTransitions->getRow(sccState)) {
            if (!stateSetAsBitvector.get(entry.getColumn())) {
                (*valIt) += entry.getValue() * stateValues[entry.getColumn()];
            }
        }
        ++valIt;
    }
    return computeExpectedVisitingTimes(env, stateSetAsBitvector, sccVector);
}

template<typename ValueType>
std::vector<ValueType> SparseDeterministicVisitingTimesHelper<ValueType>::computeExpectedVisitingTimes(Environment const& env,
                                                                                                       storm::storage::BitVector const& subsystem,
                                                                                                       std::vector<ValueType> const& initialValues) const {
    STORM_LOG_ASSERT(subsystem.getNumberOfSetBits() == initialValues.size(), "Inconsistent size of subsystem.");

    if (initialValues.empty()) {  // Catch the case where the subsystem is empty.
        return {};
    }

    // Here we assume that the subsystem does not contain a BSCC
    // Let P be the subsystem matrix. We solve the equation system
    //       x * P + b = x
    // <=> P^T * x + b = x   <- fixpoint system
    // <=> (1-P^T) * x = b   <- equation system

    // TODO We need to check if SVI works on this kind of equation system (OVI and II are correct)
    storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
    bool isFixpointFormat = linearEquationSolverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::FixedPointSystem;

    // Get the matrix for the equation system
    auto sccMatrix = backwardTransitions->getSubmatrix(false, subsystem, subsystem, !isFixpointFormat);
    if (!isFixpointFormat) {
        sccMatrix.convertToEquationSystem();
    }

    // Get the solver object and satisfy requirements
    // The solver consumes the matrix, so the acyclic check must be performed before moving it.
    bool const hasCycles = storm::utility::graph::hasCycle(sccMatrix);
    auto solver = linearEquationSolverFactory.create(env, std::move(sccMatrix));
    solver->setLowerBound(storm::utility::zero<ValueType>());
    auto req = solver->getRequirements(env);
    req.clearLowerBounds();
    if (req.upperBounds().isCritical()) {
        // Compute upper bounds on EVTs using techniques from Baier et al. [CAV'17] (https://doi.org/10.1007/978-3-319-63387-9_8)
        std::vector<ValueType> upperBounds = computeUpperBounds(subsystem);
        solver->setUpperBounds(upperBounds);
        req.clearUpperBounds();
    }

    if (req.acyclic().isCritical()) {
        STORM_LOG_THROW(!hasCycles, storm::exceptions::UnmetRequirementException, "The solver requires an acyclic model, but the model is not acyclic.");
        req.clearAcyclic();
    }

    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UnmetRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    std::vector<ValueType> eqSysValues(initialValues.size());
    solver->solveEquations(env, eqSysValues, initialValues);
    return eqSysValues;
}

template class SparseDeterministicVisitingTimesHelper<double>;
template class SparseDeterministicVisitingTimesHelper<storm::RationalNumber>;
template class SparseDeterministicVisitingTimesHelper<storm::RationalFunction>;
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm