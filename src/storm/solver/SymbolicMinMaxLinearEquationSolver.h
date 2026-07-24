#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <set>
#include <vector>

#include "storm/solver/MinMaxLinearEquationSolverRequirements.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/SymbolicEquationSolver.h"
#include "storm/solver/SymbolicLinearEquationSolver.h"

#include "storm/utility/NumberTraits.h"

#include "SolverSelectionOptions.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {

class Environment;

namespace dd {
template<storm::dd::DdType Type, typename ValueType>
class Add;

template<storm::dd::DdType T>
class Bdd;
}  // namespace dd

namespace solver {
/*!
 * An interface that represents an abstract symbolic linear equation solver. In addition to solving a system of
 * linear equations, the functionality to repeatedly multiply a matrix with a given vector is provided.
 */
template<storm::dd::DdType DdType, typename ValueType>
class SymbolicMinMaxLinearEquationSolver : public SymbolicEquationSolver<DdType, ValueType> {
   public:
    SymbolicMinMaxLinearEquationSolver();
    virtual ~SymbolicMinMaxLinearEquationSolver() = default;

    /*!
     * Constructs a symbolic min/max linear equation solver with the given meta variable sets and pairs.
     *
     * @param A The matrix defining the coefficients of the linear equation system.
     * @param diagonal An ADD characterizing the elements on the diagonal of the matrix.
     * @param allRows A BDD characterizing all rows of the equation system.
     * @param illegalMask A mask that characterizes all illegal choices (that are therefore not to be taken).
     * @param rowMetaVariables The meta variables used to encode the rows of the matrix.
     * @param columnMetaVariables The meta variables used to encode the columns of the matrix.
     * @param choiceVariables The variables encoding the choices of each row group.
     * @param rowColumnMetaVariablePairs The pairs of row meta variables and the corresponding column meta
     * variables.
     * @param settings The settings to use.
     */
    SymbolicMinMaxLinearEquationSolver(storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows,
                                       storm::dd::Bdd<DdType> const& illegalMask, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                       std::set<storm::expressions::Variable> const& columnMetaVariables,
                                       std::set<storm::expressions::Variable> const& choiceVariables,
                                       std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                                       std::unique_ptr<SymbolicLinearEquationSolverFactory<DdType, ValueType>>&& linearEquationSolverFactory);

    /*!
     * Solves the equation system A*x = b. The matrix A is required to be square and have a unique solution.
     * The solution of the set of linear equations will be written to the vector x. Note that the matrix A has
     * to be given upon construction time of the solver object.
     *
     * @param dir Determines the direction of the optimization.
     * @param x The initual guess for the solution vector. Its length must be equal to the number of row
     * groups of A.
     * @param b The right-hand side of the equation system. Its length must be equal to the number of row groups
     * of A.
     * @return The solution of the equation system.
     */
    virtual storm::dd::Add<DdType, ValueType> solveEquations(Environment const& env, storm::solver::OptimizationDirection const& dir,
                                                             storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const;

    /*!
     * Performs repeated matrix-vector multiplication, using x[0] = x and x[i + 1] = A*x[i] + b. After
     * performing the necessary multiplications, the result is written to the input vector x. Note that the
     * matrix A has to be given upon construction time of the solver object.
     *
     * @param dir Determines the direction of the optimization.
     * @param x The initial vector with which to perform matrix-vector multiplication. Its length must be equal
     * to the number of row groups of A.
     * @param b If non-null, this vector is added after each multiplication. If given, its length must be equal
     * to the number of row groups of A.
     * @return The solution of the equation system.
     */
    virtual storm::dd::Add<DdType, ValueType> multiply(storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
                                                       storm::dd::Add<DdType, ValueType> const* b = nullptr, uint_fast64_t n = 1) const;

    /*!
     * Sets an initial scheduler that is required by some solvers (see requirements).
     */
    void setInitialScheduler(storm::dd::Bdd<DdType> const& scheduler);

    /*!
     * Retrieves the initial scheduler (if there is any).
     */
    storm::dd::Bdd<DdType> const& getInitialScheduler() const;

    /*!
     * Retrieves whether an initial scheduler was set.
     */
    bool hasInitialScheduler() const;

    /*!
     * Retrieves the requirements of the solver.
     */
    virtual MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env,
                                                                   boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none) const;

    /*!
     * Sets whether the solution to the min max equation system is known to be unique.
     */
    void setHasUniqueSolution(bool value = true);

    /*!
     * Retrieves whether the solution to the min max equation system is assumed to be unique
     */
    bool hasUniqueSolution() const;

    /*!
     * Notifies the solver that the requirements for solving equations have been checked. If this has not been
     * done before solving equations, the solver might issue a warning, perform the checks itself or even fail.
     */
    void setRequirementsChecked(bool value = true);

    /*!
     * Retrieves whether the solver is aware that the requirements were checked.
     */
    bool isRequirementsCheckedSet() const;

    /*!
     * Determines whether the given vector x satisfies x = min/max Ax + b.
     */
    bool isSolution(OptimizationDirection dir, storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const;

   private:
    MinMaxMethod getMethod(Environment const& env, bool isExactMode) const;

    storm::dd::Add<DdType, ValueType> solveEquationsWithScheduler(Environment const& env, storm::dd::Bdd<DdType> const& scheduler,
                                                                  storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const;
    storm::dd::Add<DdType, ValueType> solveEquationsWithScheduler(Environment const& env, SymbolicLinearEquationSolver<DdType, ValueType>& solver,
                                                                  storm::dd::Bdd<DdType> const& scheduler, storm::dd::Add<DdType, ValueType> const& x,
                                                                  storm::dd::Add<DdType, ValueType> const& b,
                                                                  storm::dd::Add<DdType, ValueType> const& diagonal) const;

    storm::dd::Add<DdType, ValueType> solveEquationsValueIteration(Environment const& env, storm::solver::OptimizationDirection const& dir,
                                                                   storm::dd::Add<DdType, ValueType> const& x,
                                                                   storm::dd::Add<DdType, ValueType> const& b) const;
    storm::dd::Add<DdType, ValueType> solveEquationsPolicyIteration(Environment const& env, storm::solver::OptimizationDirection const& dir,
                                                                    storm::dd::Add<DdType, ValueType> const& x,
                                                                    storm::dd::Add<DdType, ValueType> const& b) const;
    storm::dd::Add<DdType, ValueType> solveEquationsRationalSearch(Environment const& env, storm::solver::OptimizationDirection const& dir,
                                                                   storm::dd::Add<DdType, ValueType> const& x,
                                                                   storm::dd::Add<DdType, ValueType> const& b) const;

    template<typename RationalType, typename ImpreciseType>
    static storm::dd::Add<DdType, RationalType> sharpen(OptimizationDirection dir, uint64_t precision,
                                                        SymbolicMinMaxLinearEquationSolver<DdType, RationalType> const& rationalSolver,
                                                        storm::dd::Add<DdType, ImpreciseType> const& x, storm::dd::Add<DdType, RationalType> const& rationalB,
                                                        bool& isSolution);

    template<typename RationalType, typename ImpreciseType>
    storm::dd::Add<DdType, RationalType> solveEquationsRationalSearchHelper(Environment const& env, OptimizationDirection dir,
                                                                            SymbolicMinMaxLinearEquationSolver<DdType, RationalType> const& rationalSolver,
                                                                            SymbolicMinMaxLinearEquationSolver<DdType, ImpreciseType> const& impreciseSolver,
                                                                            storm::dd::Add<DdType, RationalType> const& rationalB,
                                                                            storm::dd::Add<DdType, ImpreciseType> const& x,
                                                                            storm::dd::Add<DdType, ImpreciseType> const& b) const;
    template<typename ImpreciseType>
    typename std::enable_if<std::is_same<ValueType, ImpreciseType>::value && storm::NumberTraits<ValueType>::IsExact, storm::dd::Add<DdType, ValueType>>::type
    solveEquationsRationalSearchHelper(Environment const& env, storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
                                       storm::dd::Add<DdType, ValueType> const& b) const;
    template<typename ImpreciseType>
    typename std::enable_if<std::is_same<ValueType, ImpreciseType>::value && !storm::NumberTraits<ValueType>::IsExact, storm::dd::Add<DdType, ValueType>>::type
    solveEquationsRationalSearchHelper(Environment const& env, storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
                                       storm::dd::Add<DdType, ValueType> const& b) const;
    template<typename ImpreciseType>
    typename std::enable_if<!std::is_same<ValueType, ImpreciseType>::value, storm::dd::Add<DdType, ValueType>>::type solveEquationsRationalSearchHelper(
        Environment const& env, storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
        storm::dd::Add<DdType, ValueType> const& b) const;

    template<storm::dd::DdType DdTypePrime, typename ValueTypePrime>
    friend class SymbolicMinMaxLinearEquationSolver;

    struct ValueIterationResult {
        ValueIterationResult(SolverStatus status, uint64_t iterations, storm::dd::Add<DdType, ValueType> const& values)
            : status(status), iterations(iterations), values(values) {
            // Intentionally left empty.
        }

        SolverStatus status;
        uint64_t iterations;
        storm::dd::Add<DdType, ValueType> values;
    };

    ValueIterationResult performValueIteration(storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
                                               storm::dd::Add<DdType, ValueType> const& b, ValueType const& precision, bool relativeTerminationCriterion,
                                               uint64_t maximalIterations) const;

   protected:
    // The matrix defining the coefficients of the linear equation system.
    storm::dd::Add<DdType, ValueType> A;

    // A BDD characterizing the illegal choices.
    storm::dd::Bdd<DdType> illegalMask;

    // An ADD characterizing the illegal choices.
    storm::dd::Add<DdType, ValueType> illegalMaskAdd;

    // The row variables.
    std::set<storm::expressions::Variable> rowMetaVariables;

    // The column variables.
    std::set<storm::expressions::Variable> columnMetaVariables;

    // The choice variables.
    std::set<storm::expressions::Variable> choiceVariables;

    // The pairs of meta variables used for renaming.
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> rowColumnMetaVariablePairs;

    // A factory for creating linear equation solvers when needed.
    std::unique_ptr<SymbolicLinearEquationSolverFactory<DdType, ValueType>> linearEquationSolverFactory;

    // Whether the solver can assume that the min-max equation system has a unique solution
    bool uniqueSolution;

    // A flag indicating whether the requirements were checked.
    bool requirementsChecked;

    // A scheduler that specifies with which schedulers to start.
    boost::optional<storm::dd::Bdd<DdType>> initialScheduler;

   private:
    /*!
     * Forwards the known bounds of this solver to the given linear equation solver.
     */
    void forwardBounds(storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>& solver) const;
};

template<storm::dd::DdType DdType, typename ValueType>
class SymbolicMinMaxLinearEquationSolverFactory {
   public:
    virtual ~SymbolicMinMaxLinearEquationSolverFactory() = default;

    virtual std::unique_ptr<storm::solver::SymbolicMinMaxLinearEquationSolver<DdType, ValueType>> create(
        storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows, storm::dd::Bdd<DdType> const& illegalMask,
        std::set<storm::expressions::Variable> const& rowMetaVariables, std::set<storm::expressions::Variable> const& columnMetaVariables,
        std::set<storm::expressions::Variable> const& choiceVariables,
        std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs) const = 0;

    /*!
     * Retrieves the requirements of the solver that would be created when calling create() right now. The
     * requirements are guaranteed to be ordered according to their appearance in the SolverRequirement type.
     */
    MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env, bool hasUniqueSolution = false,
                                                           boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none) const;

   private:
    virtual std::unique_ptr<storm::solver::SymbolicMinMaxLinearEquationSolver<DdType, ValueType>> create() const = 0;
};

template<storm::dd::DdType DdType, typename ValueType>
class GeneralSymbolicMinMaxLinearEquationSolverFactory : public SymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType> {
   public:
    virtual std::unique_ptr<storm::solver::SymbolicMinMaxLinearEquationSolver<DdType, ValueType>> create(
        storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows, storm::dd::Bdd<DdType> const& illegalMask,
        std::set<storm::expressions::Variable> const& rowMetaVariables, std::set<storm::expressions::Variable> const& columnMetaVariables,
        std::set<storm::expressions::Variable> const& choiceVariables,
        std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs) const override;

   private:
    virtual std::unique_ptr<storm::solver::SymbolicMinMaxLinearEquationSolver<DdType, ValueType>> create() const override;
};

}  // namespace solver
}  // namespace storm
