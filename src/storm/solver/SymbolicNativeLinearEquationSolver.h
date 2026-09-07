#pragma once

#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/SymbolicLinearEquationSolver.h"
#include "storm/utility/NumberTraits.h"

namespace storm {
namespace solver {

/*!
 * An interface that represents an abstract symbolic linear equation solver. In addition to solving a system of
 * linear equations, the functionality to repeatedly multiply a matrix with a given vector is provided.
 */
template<storm::dd::DdType DdType, typename ValueType = double>
class SymbolicNativeLinearEquationSolver : public SymbolicLinearEquationSolver<DdType, ValueType> {
   public:
    /*!
     * Constructs a symbolic linear equation solver.
     *
     * @param settings The settings to use.
     */
    SymbolicNativeLinearEquationSolver();

    /*!
     * Constructs a symbolic linear equation solver with the given meta variable sets and pairs.
     *
     * @param A The matrix defining the coefficients of the linear equation system.
     * @param diagonal An ADD characterizing the elements on the diagonal of the matrix.
     * @param allRows A BDD characterizing all rows of the equation system.
     * @param rowMetaVariables The meta variables used to encode the rows of the matrix.
     * @param columnMetaVariables The meta variables used to encode the columns of the matrix.
     * @param rowColumnMetaVariablePairs The pairs of row meta variables and the corresponding column meta
     * variables.
     * @param settings The settings to use.
     */
    SymbolicNativeLinearEquationSolver(storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows,
                                       std::set<storm::expressions::Variable> const& rowMetaVariables,
                                       std::set<storm::expressions::Variable> const& columnMetaVariables,
                                       std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs);

    /*!
     * Constructs a symbolic linear equation solver with the given meta variable sets and pairs.
     *
     * @param diagonal An ADD characterizing the elements on the diagonal of the matrix.
     * @param allRows A BDD characterizing all rows of the equation system.
     * @param rowMetaVariables The meta variables used to encode the rows of the matrix.
     * @param columnMetaVariables The meta variables used to encode the columns of the matrix.
     * @param rowColumnMetaVariablePairs The pairs of row meta variables and the corresponding column meta
     * variables.
     * @param settings The settings to use.
     */
    SymbolicNativeLinearEquationSolver(storm::dd::Bdd<DdType> const& allRows, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                       std::set<storm::expressions::Variable> const& columnMetaVariables,
                                       std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs);

    /*!
     * Solves the equation system A*x = b. The matrix A is required to be square and have a unique solution.
     * The solution of the set of linear equations will be written to the vector x. Note that the matrix A has
     * to be given upon construction time of the solver object.
     *
     * @param x The initial guess for the solution vector. Its length must be equal to the number of rows of A.
     * @param b The right-hand side of the equation system. Its length must be equal to the number of rows of A.
     * @return The solution of the equation system.
     */
    virtual storm::dd::Add<DdType, ValueType> solveEquations(Environment const& env, storm::dd::Add<DdType, ValueType> const& x,
                                                             storm::dd::Add<DdType, ValueType> const& b) const override;

    virtual LinearEquationSolverProblemFormat getEquationProblemFormat(Environment const& env) const override;
    virtual LinearEquationSolverRequirements getRequirements(Environment const& env) const override;

   private:
    NativeLinearEquationSolverMethod getMethod(Environment const& env, bool isExactMode) const;

    storm::dd::Add<DdType, ValueType> solveEquationsJacobi(Environment const& env, storm::dd::Add<DdType, ValueType> const& x,
                                                           storm::dd::Add<DdType, ValueType> const& b) const;
    storm::dd::Add<DdType, ValueType> solveEquationsPower(Environment const& env, storm::dd::Add<DdType, ValueType> const& x,
                                                          storm::dd::Add<DdType, ValueType> const& b) const;
    storm::dd::Add<DdType, ValueType> solveEquationsRationalSearch(Environment const& env, storm::dd::Add<DdType, ValueType> const& x,
                                                                   storm::dd::Add<DdType, ValueType> const& b) const;

    /*!
     * Determines whether the given vector x satisfies x = Ax + b.
     */
    bool isSolutionFixedPoint(storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const;

    template<typename RationalType, typename ImpreciseType>
    static storm::dd::Add<DdType, RationalType> sharpen(uint64_t precision, SymbolicNativeLinearEquationSolver<DdType, RationalType> const& rationalSolver,
                                                        storm::dd::Add<DdType, ImpreciseType> const& x, storm::dd::Add<DdType, RationalType> const& rationalB,
                                                        bool& isSolution);

    template<typename RationalType, typename ImpreciseType>
    storm::dd::Add<DdType, RationalType> solveEquationsRationalSearchHelper(Environment const& env,
                                                                            SymbolicNativeLinearEquationSolver<DdType, RationalType> const& rationalSolver,
                                                                            SymbolicNativeLinearEquationSolver<DdType, ImpreciseType> const& impreciseSolver,
                                                                            storm::dd::Add<DdType, RationalType> const& rationalB,
                                                                            storm::dd::Add<DdType, ImpreciseType> const& x,
                                                                            storm::dd::Add<DdType, ImpreciseType> const& b) const;
    template<typename ImpreciseType>
    typename std::enable_if<std::is_same<ValueType, ImpreciseType>::value && storm::NumberTraits<ValueType>::IsExact, storm::dd::Add<DdType, ValueType>>::type
    solveEquationsRationalSearchHelper(Environment const& env, storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const;
    template<typename ImpreciseType>
    typename std::enable_if<std::is_same<ValueType, ImpreciseType>::value && !storm::NumberTraits<ValueType>::IsExact, storm::dd::Add<DdType, ValueType>>::type
    solveEquationsRationalSearchHelper(Environment const& env, storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const;
    template<typename ImpreciseType>
    typename std::enable_if<!std::is_same<ValueType, ImpreciseType>::value, storm::dd::Add<DdType, ValueType>>::type solveEquationsRationalSearchHelper(
        Environment const& env, storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const;

    template<storm::dd::DdType DdTypePrime, typename ValueTypePrime>
    friend class SymbolicNativeLinearEquationSolver;

    struct PowerIterationResult {
        PowerIterationResult(SolverStatus status, uint64_t iterations, storm::dd::Add<DdType, ValueType> const& values)
            : status(status), iterations(iterations), values(values) {
            // Intentionally left empty.
        }

        SolverStatus status;
        uint64_t iterations;
        storm::dd::Add<DdType, ValueType> values;
    };

    PowerIterationResult performPowerIteration(storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b,
                                               ValueType const& precision, bool relativeTerminationCriterion, uint64_t maximalIterations) const;
};

template<storm::dd::DdType DdType, typename ValueType>
class SymbolicNativeLinearEquationSolverFactory : SymbolicLinearEquationSolverFactory<DdType, ValueType> {
   public:
    using SymbolicLinearEquationSolverFactory<DdType, ValueType>::create;

    virtual std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> create(Environment const& env) const override;
};

}  // namespace solver
}  // namespace storm
