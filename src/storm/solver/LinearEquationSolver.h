#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "storm/solver/AbstractEquationSolver.h"
#include "storm/solver/LinearEquationSolverProblemFormat.h"
#include "storm/solver/LinearEquationSolverRequirements.h"
#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {

class Environment;

namespace solver {

/*!
 * An interface that represents an abstract linear equation solver.
 */
template<class ValueType>
class LinearEquationSolver : public AbstractEquationSolver<ValueType> {
   public:
    LinearEquationSolver();

    virtual ~LinearEquationSolver() {
        // Intentionally left empty.
    }

    virtual void setMatrix(storm::storage::SparseMatrix<ValueType> const& A) = 0;
    virtual void setMatrix(storm::storage::SparseMatrix<ValueType>&& A) = 0;

    /*!
     * If the solver expects the equation system format, it solves Ax = b. If it it expects a fixed point
     * format, it solves Ax + b = x. In both versions, the matrix A is required to be square and the problem
     * is required to have a unique solution. The solution will be written to the vector x. Note that the matrix
     * A has to be given upon construction time of the solver object.
     *
     * @param x The solution vector that has to be computed. Its length must be equal to the number of rows of A.
     * @param b The vector b. Its length must be equal to the number of rows of A.
     *
     * @return true
     */
    bool solveEquations(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const;

    /*!
     * Retrieves the format in which this solver expects to solve equations. If the solver expects the equation
     * system format, it solves Ax = b. If it it expects a fixed point format, it solves Ax + b = x.
     */
    virtual LinearEquationSolverProblemFormat getEquationProblemFormat(Environment const& env) const = 0;

    /*!
     * Retrieves the requirements of the solver under the current settings. Note that these requirements only
     * apply to solving linear equations and not to the matrix vector multiplications.
     */
    virtual LinearEquationSolverRequirements getRequirements(Environment const& env) const;

    /*!
     * Sets whether some of the generated data during solver calls should be cached.
     * This possibly increases the runtime of subsequent calls but also increases memory consumption.
     */
    void setCachingEnabled(bool value) const;

    /*!
     * Retrieves whether some of the generated data during solver calls should be cached.
     */
    bool isCachingEnabled() const;

    /*
     * Clears the currently cached data that has been stored during previous calls of the solver.
     */
    virtual void clearCache() const;

   protected:
    virtual bool internalSolveEquations(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const = 0;

    // auxiliary storage. If set, this vector has getMatrixRowCount() entries.
    mutable std::unique_ptr<std::vector<ValueType>> cachedRowVector;

   private:
    /*!
     * Retrieves the row count of the matrix associated with this solver.
     */
    virtual uint64_t getMatrixRowCount() const = 0;

    /*!
     * Retrieves the column count of the matrix associated with this solver.
     */
    virtual uint64_t getMatrixColumnCount() const = 0;

    /// Whether some of the generated data during solver calls should be cached.
    mutable bool cachingEnabled;
};

enum class EquationSolverType;

template<typename ValueType>
class LinearEquationSolverFactory {
   public:
    virtual ~LinearEquationSolverFactory() = default;

    /*!
     * Creates a new linear equation solver instance with the given matrix.
     *
     * @param matrix The matrix that defines the equation system.
     * @return A pointer to the newly created solver.
     */
    std::unique_ptr<LinearEquationSolver<ValueType>> create(Environment const& env, storm::storage::SparseMatrix<ValueType> const& matrix) const;

    /*!
     * Creates a new linear equation solver instance with the given matrix. The caller gives up posession of the
     * matrix by calling this function.
     *
     * @param matrix The matrix that defines the equation system.
     * @return A pointer to the newly created solver.
     */
    std::unique_ptr<LinearEquationSolver<ValueType>> create(Environment const& env, storm::storage::SparseMatrix<ValueType>&& matrix) const;

    /*!
     * Creates an equation solver with the current settings, but without a matrix.
     */
    virtual std::unique_ptr<LinearEquationSolver<ValueType>> create(Environment const& env) const = 0;

    /*!
     * Creates a copy of this factory.
     */
    virtual std::unique_ptr<LinearEquationSolverFactory<ValueType>> clone() const = 0;

    /*!
     * Retrieves the problem format that the solver expects if it was created with the current settings.
     */
    virtual LinearEquationSolverProblemFormat getEquationProblemFormat(Environment const& env) const;

    /*!
     * Retrieves the requirements of the solver if it was created with the current settings. Note that these
     * requirements only apply to solving linear equations and not to the matrix vector multiplications.
     */
    LinearEquationSolverRequirements getRequirements(Environment const& env) const;
};

template<typename ValueType>
class GeneralLinearEquationSolverFactory : public LinearEquationSolverFactory<ValueType> {
   public:
    GeneralLinearEquationSolverFactory();

    using LinearEquationSolverFactory<ValueType>::create;

    virtual std::unique_ptr<LinearEquationSolver<ValueType>> create(Environment const& env) const override;

    virtual std::unique_ptr<LinearEquationSolverFactory<ValueType>> clone() const override;
};

}  // namespace solver
}  // namespace storm
