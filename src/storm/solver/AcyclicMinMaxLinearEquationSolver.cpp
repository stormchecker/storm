#include "storm/solver/AcyclicMinMaxLinearEquationSolver.h"

#include "storm/solver/helper/AcyclicSolverHelper.h"

#include "storm/utility/vector.h"

namespace storm {
namespace solver {

template<typename ValueType>
AcyclicMinMaxLinearEquationSolver<ValueType>::AcyclicMinMaxLinearEquationSolver() {
    // Intentionally left empty.
}

template<typename ValueType>
AcyclicMinMaxLinearEquationSolver<ValueType>::AcyclicMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A)
    : StandardMinMaxLinearEquationSolver<ValueType>(A) {
    // Intentionally left empty.
}

template<typename ValueType>
AcyclicMinMaxLinearEquationSolver<ValueType>::AcyclicMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A)
    : StandardMinMaxLinearEquationSolver<ValueType>(std::move(A)) {
    // Intentionally left empty.
}

template<typename ValueType>
bool AcyclicMinMaxLinearEquationSolver<ValueType>::internalSolveEquations(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x,
                                                                          std::vector<ValueType> const& b) const {
    STORM_LOG_ASSERT(x.size() == this->A->getRowGroupCount(), "Provided x-vector has invalid size.");
    STORM_LOG_ASSERT(b.size() == this->A->getRowCount(), "Provided b-vector has invalid size.");

    if (!multiplier) {
        // We have not allocated cache memory, yet
        rowGroupOrdering = helper::computeTopologicalGroupOrdering(*this->A);
        if (!rowGroupOrdering) {
            // It is not required to reorder the elements.
            this->multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, *this->A);
        } else {
            bFactors.clear();
            orderedMatrix = helper::createReorderedMatrix(*this->A, *rowGroupOrdering, bFactors);
            this->multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, *orderedMatrix);
        }
        auxiliaryRowVector = std::vector<ValueType>(this->A->getRowCount());
        auxiliaryRowGroupVector = std::vector<ValueType>(this->A->getRowGroupCount());
    }

    std::vector<ValueType>* xPtr = &x;
    std::vector<ValueType> const* bPtr = &b;
    if (rowGroupOrdering) {
        STORM_LOG_ASSERT(rowGroupOrdering->size() == x.size(), "X-vector has unexpected size.");
        STORM_LOG_ASSERT(auxiliaryRowGroupVector->size() == x.size(), "X-vector has unexpected size.");
        STORM_LOG_ASSERT(auxiliaryRowVector->size() == b.size(), "B-vector has unexpected size.");
        for (uint64_t newGroupIndex = 0; newGroupIndex < x.size(); ++newGroupIndex) {
            uint64_t newRow = orderedMatrix->getRowGroupIndices()[newGroupIndex];
            uint64_t newRowGroupEnd = orderedMatrix->getRowGroupIndices()[newGroupIndex + 1];
            uint64_t oldRow = this->A->getRowGroupIndices()[(*rowGroupOrdering)[newGroupIndex]];
            for (; newRow < newRowGroupEnd; ++newRow, ++oldRow) {
                (*auxiliaryRowVector)[newRow] = b[oldRow];
            }
        }
        for (auto const& bFactor : bFactors) {
            (*auxiliaryRowVector)[bFactor.first] *= bFactor.second;
        }
        xPtr = &auxiliaryRowGroupVector.get();
        bPtr = &auxiliaryRowVector.get();
    }

    // Allocate memory for the scheduler (if required)
    std::vector<uint64_t>* choicesPtr = nullptr;
    if (this->isTrackSchedulerSet()) {
        if (this->schedulerChoices) {
            this->schedulerChoices->resize(this->A->getRowGroupCount());
        } else {
            this->schedulerChoices = std::vector<uint64_t>(this->A->getRowGroupCount());
        }
        if (rowGroupOrdering) {
            if (auxiliaryRowGroupIndexVector) {
                auxiliaryRowGroupIndexVector->resize(this->A->getRowGroupCount());
            } else {
                auxiliaryRowGroupIndexVector = std::vector<uint64_t>(this->A->getRowGroupCount());
            }
            choicesPtr = &(auxiliaryRowGroupIndexVector.get());
        } else {
            choicesPtr = &(this->schedulerChoices.get());
        }
    }

    // Since a topological ordering is guaranteed, we can solve the equations with a single matrix-vector Multiplication step.
    this->multiplier->multiplyAndReduceGaussSeidel(env, dir, *xPtr, bPtr, choicesPtr, true);

    if (rowGroupOrdering) {
        // Restore the correct input-order for the output vector
        for (uint64_t newGroupIndex = 0; newGroupIndex < x.size(); ++newGroupIndex) {
            x[(*rowGroupOrdering)[newGroupIndex]] = (*xPtr)[newGroupIndex];
        }
        if (this->isTrackSchedulerSet()) {
            // Do the same for the scheduler choices
            for (uint64_t newGroupIndex = 0; newGroupIndex < x.size(); ++newGroupIndex) {
                this->schedulerChoices.get()[(*rowGroupOrdering)[newGroupIndex]] = (*choicesPtr)[newGroupIndex];
            }
        }
    }

    if (!this->isCachingEnabled()) {
        this->clearCache();
    }
    return true;
}

template<typename ValueType>
MinMaxLinearEquationSolverRequirements AcyclicMinMaxLinearEquationSolver<ValueType>::getRequirements(
    Environment const&, boost::optional<storm::solver::OptimizationDirection> const& direction, bool const& hasInitialScheduler) const {
    // Return the requirements of the underlying solver
    MinMaxLinearEquationSolverRequirements requirements;
    requirements.requireAcyclic();
    return requirements;
}

template<typename ValueType>
void AcyclicMinMaxLinearEquationSolver<ValueType>::clearCache() const {
    multiplier.reset();
    orderedMatrix = boost::none;
    rowGroupOrdering = boost::none;
    auxiliaryRowVector = boost::none;
    auxiliaryRowGroupVector = boost::none;
    auxiliaryRowGroupIndexVector = boost::none;
    bFactors.clear();
}

// Explicitly instantiate the min max linear equation solver.
template class AcyclicMinMaxLinearEquationSolver<double>;
template class AcyclicMinMaxLinearEquationSolver<storm::RationalNumber>;
}  // namespace solver
}  // namespace storm
