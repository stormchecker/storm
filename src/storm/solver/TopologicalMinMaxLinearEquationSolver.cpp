#include "storm/solver/TopologicalMinMaxLinearEquationSolver.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"

#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

template<typename ValueType, typename SolutionType>
TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::TopologicalMinMaxLinearEquationSolver() {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::TopologicalMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A)
    : StandardMinMaxLinearEquationSolver<ValueType, SolutionType>(A) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::TopologicalMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A)
    : StandardMinMaxLinearEquationSolver<ValueType, SolutionType>(std::move(A)) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
storm::Environment TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::getEnvironmentForUnderlyingSolver(storm::Environment const& env,
                                                                                                                     bool adaptPrecision) const {
    storm::Environment subEnv(env);
    subEnv.solver().minMax().setMethod(env.solver().topological().getUnderlyingMinMaxMethod(),
                                       env.solver().topological().isUnderlyingMinMaxMethodSetFromDefault());
    if (adaptPrecision) {
        STORM_LOG_ASSERT(this->longestSccChainSize, "Did not compute the longest SCC chain size although it is needed.");
        storm::RationalNumber subEnvPrec =
            subEnv.solver().minMax().getPrecision() / storm::utility::convertNumber<storm::RationalNumber>(this->longestSccChainSize.get());
        subEnv.solver().minMax().setPrecision(subEnvPrec);
    }
    return subEnv;
}

template<typename ValueType, typename SolutionType>
bool TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::internalSolveEquations(Environment const& env, OptimizationDirection dir,
                                                                                            std::vector<SolutionType>& x,
                                                                                            std::vector<ValueType> const& b) const {
    STORM_LOG_ASSERT(x.size() == this->A->getRowGroupCount(), "Provided x-vector has invalid size.");
    STORM_LOG_ASSERT(b.size() == this->A->getRowCount(), "Provided b-vector has invalid size.");

    // For sound computations we need to increase the precision in each SCC
    bool needAdaptPrecision = env.solver().isForceSoundness();

    if (!this->sortedSccDecomposition || (needAdaptPrecision && !this->longestSccChainSize)) {
        STORM_LOG_TRACE("Creating SCC decomposition.");
        storm::utility::Stopwatch sccSw(true);
        createSortedSccDecomposition(needAdaptPrecision);
        sccSw.stop();
        STORM_LOG_INFO("SCC decomposition computed in "
                       << sccSw << ". Found " << this->sortedSccDecomposition->size() << " SCC(s) containing a total of " << x.size()
                       << " states. Average SCC size is "
                       << static_cast<double>(this->A->getRowGroupCount()) / static_cast<double>(this->sortedSccDecomposition->size()) << ".");
    }

    // We do not need to adapt the precision if all SCCs are trivial (i.e., the system is acyclic)
    needAdaptPrecision = needAdaptPrecision && (this->sortedSccDecomposition->size() != this->A->getRowGroupCount());

    storm::Environment sccSolverEnvironment = getEnvironmentForUnderlyingSolver(env, needAdaptPrecision);

    if (this->longestSccChainSize) {
        STORM_LOG_INFO("Longest SCC chain size is " << this->longestSccChainSize.get());
    }

    bool returnValue = true;
    if (this->sortedSccDecomposition->size() == 1 && (!this->choiceFixedForRowGroup || this->choiceFixedForRowGroup.get().empty())) {
        // Handle the case where there is just one large SCC, as there are no fixed choices for states, we solve it like this
        if (auto const& scc = *this->sortedSccDecomposition->begin(); scc.size() == 1) {
            // Catch the trivial case where the whole system is just a single state.
            if (this->isTrackSchedulerSet()) {
                this->schedulerChoices = std::vector<uint64_t>(1);
            }
            returnValue = solveTrivialScc(*scc.begin(), dir, x, b);
        } else {
            returnValue = solveFullyConnectedEquationSystem(sccSolverEnvironment, dir, x, b);
        }
    } else {
        // Solve each SCC individually
        if (this->isTrackSchedulerSet()) {
            if (this->schedulerChoices) {
                this->schedulerChoices.get().resize(x.size());
            } else {
                this->schedulerChoices = std::vector<uint64_t>(x.size());
            }
        }
        std::optional<storm::storage::BitVector> newRelevantValues;
        if (env.solver().topological().isExtendRelevantValues() && this->hasRelevantValues() &&
            this->sortedSccDecomposition->size() < this->A->getRowGroupCount()) {
            newRelevantValues = this->getRelevantValues();
            // Extend the relevant values towards those that have an incoming transition from another SCC
            std::vector<uint64_t> rowGroupToScc = this->sortedSccDecomposition->computeStateToSccIndexMap(this->A->getRowGroupCount());
            for (uint64_t rowGroup = 0; rowGroup < this->A->getRowGroupCount(); ++rowGroup) {
                auto currScc = rowGroupToScc[rowGroup];
                for (auto const& successor : this->A->getRowGroup(rowGroup)) {
                    if (rowGroupToScc[successor.getColumn()] != currScc) {
                        newRelevantValues->set(successor.getColumn(), true);
                    }
                }
            }
        }
        storm::storage::BitVector sccRowGroupsAsBitVector(x.size(), false);
        storm::storage::BitVector sccRowsAsBitVector(b.size(), false);
        uint64_t sccIndex = 0;
        storm::utility::ProgressMeasurement progress("states");
        progress.setMaxCount(x.size());
        progress.startNewMeasurement(0);
        for (auto const& scc : *this->sortedSccDecomposition) {
            if (scc.size() == 1) {
                returnValue = solveTrivialScc(*scc.begin(), dir, x, b) && returnValue;
            } else {
                STORM_LOG_TRACE("Solving SCC of size " << scc.size() << ".");
                sccRowGroupsAsBitVector.clear();
                sccRowsAsBitVector.clear();
                for (auto const& group : scc) {  // Group refers to state
                    sccRowGroupsAsBitVector.set(group, true);

                    if (!this->choiceFixedForRowGroup || !this->choiceFixedForRowGroup.get()[group]) {
                        for (uint64_t row = this->A->getRowGroupIndices()[group]; row < this->A->getRowGroupIndices()[group + 1]; ++row) {
                            sccRowsAsBitVector.set(row, true);
                        }
                    } else {
                        auto row = this->A->getRowGroupIndices()[group] + this->getInitialScheduler()[group];
                        sccRowsAsBitVector.set(row, true);
                        STORM_LOG_TRACE("Fixing state " << group << " to choice " << this->getInitialScheduler()[group] << ".");
                    }
                }
                returnValue = solveScc(sccSolverEnvironment, dir, sccRowGroupsAsBitVector, sccRowsAsBitVector, x, b, newRelevantValues) && returnValue;
            }
            ++sccIndex;
            progress.updateProgress(sccIndex);
            if (storm::utility::resources::isTerminate()) {
                STORM_LOG_WARN("Topological solver aborted after analyzing " << sccIndex << "/" << this->sortedSccDecomposition->size() << " SCCs.");
                break;
            }
        }
    }

    if (!this->isCachingEnabled()) {
        clearCache();
    }

    return returnValue;
}

template<typename ValueType, typename SolutionType>
void TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::createSortedSccDecomposition(bool needLongestChainSize) const {
    // Obtain the scc decomposition
    this->sortedSccDecomposition = std::make_unique<storm::storage::StronglyConnectedComponentDecomposition<ValueType>>(
        *this->A, storm::storage::StronglyConnectedComponentDecompositionOptions().forceTopologicalSort().computeSccDepths(needLongestChainSize));
    if (needLongestChainSize) {
        this->longestSccChainSize = this->sortedSccDecomposition->getMaxSccDepth() + 1;
    }
}

template<typename ValueType, typename SolutionType>
bool TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::solveTrivialScc(uint64_t const& sccState, OptimizationDirection dir,
                                                                                     std::vector<SolutionType>& globalX,
                                                                                     std::vector<ValueType> const& globalB) const {
    SolutionType& xi = globalX[sccState];
    if (this->choiceFixedForRowGroup && this->choiceFixedForRowGroup.get()[sccState]) {
        // if the choice in the scheduler is fixed we only update for the fixed choice
        uint_fast64_t row = this->A->getRowGroupIndices()[sccState] + this->getInitialScheduler()[sccState];
        ValueType rowValue = globalB[row];
        bool hasDiagonalEntry = false;
        ValueType denominator;
        for (auto const& entry : this->A->getRow(row)) {
            if (entry.getColumn() == sccState) {
                hasDiagonalEntry = true;
                denominator = storm::utility::one<ValueType>() - entry.getValue();
            } else {
                rowValue += entry.getValue() * globalX[entry.getColumn()];
            }
        }
        if (hasDiagonalEntry) {
            STORM_LOG_WARN_COND_DEBUG(
                storm::NumberTraits<ValueType>::IsExact || !storm::utility::isAlmostZero(denominator) || storm::utility::isZero(denominator),
                "State " << sccState << " has a selfloop with probability '1-(" << denominator << ")'. This could be an indication for numerical issues.");
            if (storm::utility::isZero(denominator)) {
                // In this case we have a selfloop on this state. This can never an optimal choice:
                // When minimizing, we are looking for the largest fixpoint (which will never be attained by this action)
                // When maximizing, this choice reflects probability zero (non-optimal) or reward infinity (should already be handled during preprocessing).
            } else {
                rowValue /= denominator;
            }
        }
        xi = std::move(rowValue);
    } else {
        bool firstRow = true;
        uint64_t bestRow;
        for (uint64_t row = this->A->getRowGroupIndices()[sccState]; row < this->A->getRowGroupIndices()[sccState + 1]; ++row) {
            ValueType rowValue = globalB[row];
            bool hasDiagonalEntry = false;
            ValueType denominator;
            for (auto const& entry : this->A->getRow(row)) {
                if (entry.getColumn() == sccState) {
                    hasDiagonalEntry = true;
                    denominator = storm::utility::one<ValueType>() - entry.getValue();
                } else {
                    rowValue += entry.getValue() * globalX[entry.getColumn()];
                }
            }
            if (hasDiagonalEntry) {
                STORM_LOG_WARN_COND_DEBUG(
                    storm::NumberTraits<ValueType>::IsExact || !storm::utility::isAlmostZero(denominator) || storm::utility::isZero(denominator),
                    "State " << sccState << " has a selfloop with probability '1-(" << denominator << ")'. This could be an indication for numerical issues.");
                if (storm::utility::isZero(denominator)) {
                    // In this case we have a selfloop on this state. This can never be an optimal choice:
                    // When minimizing, we are looking for the largest fixpoint (which will never be attained by this action)
                    // When maximizing, this choice reflects probability zero (non-optimal) or reward infinity (should already be handled during preprocessing).
                    continue;
                } else {
                    rowValue /= denominator;
                }
            }
            if (firstRow) {
                xi = std::move(rowValue);
                bestRow = row;
                firstRow = false;
            } else {
                if (minimize(dir)) {
                    if (rowValue < xi) {
                        xi = std::move(rowValue);
                        bestRow = row;
                    }
                } else {
                    if (rowValue > xi) {
                        xi = std::move(rowValue);
                        bestRow = row;
                    }
                }
            }
        }
        if (this->isTrackSchedulerSet()) {
            this->schedulerChoices.get()[sccState] = bestRow - this->A->getRowGroupIndices()[sccState];
        }
        STORM_LOG_THROW(!firstRow, storm::exceptions::UnexpectedException, "Empty row group in MinMax equation system.");
    }
    return true;
}

template<typename ValueType, typename SolutionType>
bool TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::solveFullyConnectedEquationSystem(storm::Environment const& sccSolverEnvironment,
                                                                                                       OptimizationDirection dir, std::vector<SolutionType>& x,
                                                                                                       std::vector<ValueType> const& b) const {
    STORM_LOG_ASSERT(!this->choiceFixedForRowGroup || this->choiceFixedForRowGroup.get().empty(),
                     "Expecting no fixed choices for states when solving the fully connected equation system");
    if (!this->sccSolver) {
        this->sccSolver = GeneralMinMaxLinearEquationSolverFactory<ValueType>().create(sccSolverEnvironment);
        this->sccSolver->setCachingEnabled(true);
    }
    this->sccSolver->setMatrix(*this->A);
    this->sccSolver->setHasUniqueSolution(this->hasUniqueSolution());
    this->sccSolver->setHasNoEndComponents(this->hasNoEndComponents());
    this->sccSolver->setTrackScheduler(this->isTrackSchedulerSet());
    if (this->hasInitialScheduler()) {
        auto choices = this->getInitialScheduler();
        this->sccSolver->setInitialScheduler(std::move(choices));
    }
    if (this->hasRelevantValues()) {
        this->sccSolver->setRelevantValues(this->getRelevantValues());
    }
    auto req = this->sccSolver->getRequirements(sccSolverEnvironment, dir);
    this->sccSolver->setBoundsFromOtherSolver(*this);
    if (req.upperBounds() && this->hasUpperBound()) {
        req.clearUpperBounds();
    }
    if (req.lowerBounds() && this->hasLowerBound()) {
        req.clearLowerBounds();
    }
    if (req.validInitialScheduler() && this->hasInitialScheduler()) {
        req.clearValidInitialScheduler();
    }
    if (req.uniqueSolution() && this->hasUniqueSolution()) {
        req.clearUniqueSolution();
    }
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    this->sccSolver->setRequirementsChecked(true);

    bool res = this->sccSolver->solveEquations(sccSolverEnvironment, dir, x, b);
    if (this->isTrackSchedulerSet()) {
        this->schedulerChoices = this->sccSolver->getSchedulerChoices();
    }
    return res;
}

template<typename ValueType, typename SolutionType>
bool TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::solveScc(storm::Environment const& sccSolverEnvironment, OptimizationDirection dir,
                                                                              storm::storage::BitVector const& sccRowGroups,
                                                                              storm::storage::BitVector const& sccRows, std::vector<SolutionType>& globalX,
                                                                              std::vector<ValueType> const& globalB,
                                                                              std::optional<storm::storage::BitVector> const& globalRelevantValues) const {
    // Set up the SCC solver
    if (!this->sccSolver) {
        this->sccSolver = GeneralMinMaxLinearEquationSolverFactory<ValueType>().create(sccSolverEnvironment);
        this->sccSolver->setCachingEnabled(true);
    }
    this->sccSolver->setHasUniqueSolution(this->hasUniqueSolution());
    this->sccSolver->setHasNoEndComponents(this->hasNoEndComponents());
    this->sccSolver->setTrackScheduler(this->isTrackSchedulerSet());
    if (globalRelevantValues) {
        this->sccSolver->setRelevantValues((*globalRelevantValues) % sccRowGroups);
    }

    storm::storage::SparseMatrix<ValueType> sccA;
    if (this->choiceFixedForRowGroup) {
        // Obtain choiceFixedForState bitvector containing only the states of the scc.
        storm::storage::BitVector choiceFixedForStateSCC = this->choiceFixedForRowGroup.get() % sccRowGroups;
        sccA = this->A->getSubmatrix(false, sccRows, sccRowGroups);

        // initial scheduler
        if (this->hasInitialScheduler()) {
            std::vector<uint_fast64_t> sccInitChoices = storm::utility::vector::filterVector(this->getInitialScheduler(), sccRowGroups);
            // As we removed the entries where the choice was fixed, we need to change the scheduler.
            // We set the scheduler to 0 for those states.
            storm::utility::vector::setVectorValues<uint_fast64_t>(sccInitChoices, choiceFixedForStateSCC, 0);
            this->sccSolver->setInitialScheduler(std::move(sccInitChoices));
        }

    } else {
        sccA = this->A->getSubmatrix(true, sccRowGroups, sccRowGroups);

        // initial scheduler
        if (this->hasInitialScheduler()) {
            auto sccInitChoices = storm::utility::vector::filterVector(this->getInitialScheduler(), sccRowGroups);
            this->sccSolver->setInitialScheduler(std::move(sccInitChoices));
        }
    }

    this->sccSolver->setMatrix(std::move(sccA));

    // x Vector
    auto sccX = storm::utility::vector::filterVector(globalX, sccRowGroups);

    // b Vector
    std::vector<ValueType> sccB;
    sccB.reserve(sccRows.getNumberOfSetBits());
    for (auto row : sccRows) {
        ValueType bi = globalB[row];
        for (auto const& entry : this->A->getRow(row)) {
            if (!sccRowGroups.get(entry.getColumn())) {
                bi += entry.getValue() * globalX[entry.getColumn()];
            }
        }
        sccB.push_back(std::move(bi));
    }

    auto req = this->sccSolver->getRequirements(sccSolverEnvironment, dir);
    this->sccSolver->clearBounds();
    // lower/upper bounds
    if (this->hasLowerBound(storm::solver::AbstractEquationSolver<ValueType>::BoundType::Global)) {
        this->sccSolver->setLowerBound(this->getLowerBound());
        req.clearLowerBounds();
    } else if (this->hasLowerBound(storm::solver::AbstractEquationSolver<ValueType>::BoundType::Local)) {
        this->sccSolver->setLowerBounds(storm::utility::vector::filterVector(this->getLowerBounds(), sccRowGroups));
        req.clearLowerBounds();
    }
    if (this->hasUpperBound(storm::solver::AbstractEquationSolver<ValueType>::BoundType::Global)) {
        this->sccSolver->setUpperBound(this->getUpperBound());
        req.clearUpperBounds();
    } else if (this->hasUpperBound(storm::solver::AbstractEquationSolver<ValueType>::BoundType::Local)) {
        this->sccSolver->setUpperBounds(storm::utility::vector::filterVector(this->getUpperBounds(), sccRowGroups));
        req.clearUpperBounds();
    }

    // Requirements
    if (req.validInitialScheduler() && (this->hasInitialScheduler() || this->hasNoEndComponents())) {
        req.clearValidInitialScheduler();
    }
    if (req.uniqueSolution() && this->hasUniqueSolution()) {
        req.clearUniqueSolution();
    }
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    this->sccSolver->setRequirementsChecked(true);

    // Invoke scc solver
    bool res = this->sccSolver->solveEquations(sccSolverEnvironment, dir, sccX, sccB);

    // Set Scheduler choices
    if (this->isTrackSchedulerSet()) {
        storm::utility::vector::setVectorValues(this->schedulerChoices.get(), sccRowGroups, this->sccSolver->getSchedulerChoices());
    }

    // Set solution
    storm::utility::vector::setVectorValues(globalX, sccRowGroups, sccX);

    return res;
}

template<typename ValueType, typename SolutionType>
MinMaxLinearEquationSolverRequirements TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::getRequirements(
    Environment const& env, boost::optional<storm::solver::OptimizationDirection> const& direction, bool const& hasInitialScheduler) const {
    // Return the requirements of the underlying solver
    return GeneralMinMaxLinearEquationSolverFactory<ValueType>().getRequirements(getEnvironmentForUnderlyingSolver(env), this->hasUniqueSolution(),
                                                                                 this->hasNoEndComponents(), direction, hasInitialScheduler,
                                                                                 this->isTrackSchedulerSet());
}

template<typename ValueType, typename SolutionType>
void TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>::clearCache() const {
    sortedSccDecomposition.reset();
    longestSccChainSize = boost::none;
    sccSolver.reset();
    auxiliaryRowGroupVector.reset();
    StandardMinMaxLinearEquationSolver<ValueType, SolutionType>::clearCache();
}

// Explicitly instantiate the min max linear equation solver.
template class TopologicalMinMaxLinearEquationSolver<double>;
template class TopologicalMinMaxLinearEquationSolver<storm::RationalNumber>;
// TODO implement topological mode for intervals
// template class TopologicalMinMaxLinearEquationSolver<storm::Interval, double>;

}  // namespace solver
}  // namespace storm
