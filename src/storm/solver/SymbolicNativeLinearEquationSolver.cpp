#include "storm/solver/SymbolicNativeLinearEquationSolver.h"

#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/PrecisionExceededException.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/KwekMehlhorn.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/dd.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<storm::dd::DdType DdType, typename ValueType>
SymbolicNativeLinearEquationSolver<DdType, ValueType>::SymbolicNativeLinearEquationSolver() : SymbolicLinearEquationSolver<DdType, ValueType>() {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
SymbolicNativeLinearEquationSolver<DdType, ValueType>::SymbolicNativeLinearEquationSolver(
    storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs)
    : SymbolicNativeLinearEquationSolver(allRows, rowMetaVariables, columnMetaVariables, rowColumnMetaVariablePairs) {
    this->setMatrix(A);
}

template<storm::dd::DdType DdType, typename ValueType>
SymbolicNativeLinearEquationSolver<DdType, ValueType>::SymbolicNativeLinearEquationSolver(
    storm::dd::Bdd<DdType> const& allRows, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs)
    : SymbolicLinearEquationSolver<DdType, ValueType>(allRows, rowMetaVariables, columnMetaVariables, rowColumnMetaVariablePairs) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
NativeLinearEquationSolverMethod SymbolicNativeLinearEquationSolver<DdType, ValueType>::getMethod(Environment const& env, bool isExactMode) const {
    // Adjust the method if it is not supported by this solver.
    // none was specified and we want exact computations
    auto method = env.solver().native().getMethod();

    if (isExactMode) {
        if (method != NativeLinearEquationSolverMethod::RationalSearch) {
            if (env.solver().native().isMethodSetFromDefault()) {
                method = NativeLinearEquationSolverMethod::RationalSearch;
                STORM_LOG_INFO(
                    "Selecting '" + toString(method) +
                    "' as the solution technique to guarantee exact results. If you want to override this, please explicitly specify a different method.");
            } else {
                STORM_LOG_WARN("The selected solution method does not guarantee exact results.");
            }
        }
    } else {
        if (method != NativeLinearEquationSolverMethod::Power && method != NativeLinearEquationSolverMethod::RationalSearch &&
            method != NativeLinearEquationSolverMethod::Jacobi) {
            method = NativeLinearEquationSolverMethod::Jacobi;
            STORM_LOG_INFO("The selected solution method is not supported in the dd engine. Falling back to '" + toString(method) + "'.");
        }
        STORM_LOG_WARN_COND_DEBUG(!env.solver().isForceSoundness(), "Sound computations are not supported in the Dd engine.");
    }
    return method;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquations(Environment const& env,
                                                                                                        storm::dd::Add<DdType, ValueType> const& x,
                                                                                                        storm::dd::Add<DdType, ValueType> const& b) const {
    switch (getMethod(env, std::is_same<ValueType, storm::RationalNumber>::value)) {
        case NativeLinearEquationSolverMethod::Jacobi:
            return solveEquationsJacobi(env, x, b);
        case NativeLinearEquationSolverMethod::Power:
            return solveEquationsPower(env, x, b);
        case NativeLinearEquationSolverMethod::RationalSearch:
            return solveEquationsRationalSearch(env, x, b);
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "The selected solution technique is not supported.");
            return storm::dd::Add<DdType, ValueType>();
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquationsJacobi(
    Environment const& env, storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const {
    storm::dd::DdManager<DdType>& manager = this->getDdManager();

    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    uint64_t maxIter = env.solver().native().getMaximalNumberOfIterations();
    bool relative = env.solver().native().getRelativeTerminationCriterion();

    STORM_LOG_INFO("Solving symbolic linear equation system with NativeLinearEquationSolver (jacobi)");

    // Start by computing the Jacobi decomposition of the matrix A.
    storm::dd::Bdd<DdType> diagonal = storm::utility::dd::getRowColumnDiagonal(x.getDdManager(), this->rowColumnMetaVariablePairs);
    diagonal &= this->allRows;

    storm::dd::Add<DdType, ValueType> lu = diagonal.ite(manager.template getAddZero<ValueType>(), this->A);
    storm::dd::Add<DdType, ValueType> diagonalAdd = diagonal.template toAdd<ValueType>();
    storm::dd::Add<DdType, ValueType> diag = diagonalAdd.multiplyMatrix(this->A, this->columnMetaVariables);

    storm::dd::Add<DdType, ValueType> scaledLu = lu / diag;
    storm::dd::Add<DdType, ValueType> scaledB = b / diag;

    // Set up additional environment variables.
    storm::dd::Add<DdType, ValueType> xCopy = x;
    uint_fast64_t iterationCount = 0;
    bool converged = false;

    while (!converged && iterationCount < maxIter) {
        storm::dd::Add<DdType, ValueType> xCopyAsColumn = xCopy.swapVariables(this->rowColumnMetaVariablePairs);
        storm::dd::Add<DdType, ValueType> tmp = scaledB - scaledLu.multiplyMatrix(xCopyAsColumn, this->columnMetaVariables);

        // Now check if the process already converged within our precision.
        converged = tmp.equalModuloPrecision(xCopy, precision, relative);

        xCopy = tmp;

        // Increase iteration count so we can abort if convergence is too slow.
        ++iterationCount;
    }

    if (converged) {
        STORM_LOG_INFO("Iterative solver (jacobi) converged in " << iterationCount << " iterations.");
    } else {
        STORM_LOG_WARN("Iterative solver (jacobi) did not converge in " << iterationCount << " iterations.");
    }

    return xCopy;
}

template<storm::dd::DdType DdType, typename ValueType>
typename SymbolicNativeLinearEquationSolver<DdType, ValueType>::PowerIterationResult
SymbolicNativeLinearEquationSolver<DdType, ValueType>::performPowerIteration(storm::dd::Add<DdType, ValueType> const& x,
                                                                             storm::dd::Add<DdType, ValueType> const& b, ValueType const& precision,
                                                                             bool relativeTerminationCriterion, uint64_t maximalIterations) const {
    // Set up additional environment variables.
    storm::dd::Add<DdType, ValueType> currentX = x;
    uint_fast64_t iterations = 0;
    SolverStatus status = SolverStatus::InProgress;

    while (status == SolverStatus::InProgress && iterations < maximalIterations) {
        storm::dd::Add<DdType, ValueType> currentXAsColumn = currentX.swapVariables(this->rowColumnMetaVariablePairs);
        storm::dd::Add<DdType, ValueType> tmp = this->A.multiplyMatrix(currentXAsColumn, this->columnMetaVariables) + b;

        // Now check if the process already converged within our precision.
        if (tmp.equalModuloPrecision(currentX, precision, relativeTerminationCriterion)) {
            status = SolverStatus::Converged;
        }

        // Set up next iteration.
        ++iterations;
        currentX = tmp;
    }

    return PowerIterationResult(status, iterations, currentX);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquationsPower(Environment const& env,
                                                                                                             storm::dd::Add<DdType, ValueType> const& x,
                                                                                                             storm::dd::Add<DdType, ValueType> const& b) const {
    STORM_LOG_INFO("Solving symbolic linear equation system with NativeLinearEquationSolver (power)");
    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    PowerIterationResult result =
        performPowerIteration(x, b, precision, env.solver().native().getRelativeTerminationCriterion(), env.solver().native().getMaximalNumberOfIterations());

    if (result.status == SolverStatus::Converged) {
        STORM_LOG_INFO("Iterative solver (power iteration) converged in " << result.iterations << " iterations.");
    } else {
        STORM_LOG_WARN("Iterative solver (power iteration) did not converge in " << result.iterations << " iterations.");
    }

    return result.values;
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicNativeLinearEquationSolver<DdType, ValueType>::isSolutionFixedPoint(storm::dd::Add<DdType, ValueType> const& x,
                                                                                 storm::dd::Add<DdType, ValueType> const& b) const {
    storm::dd::Add<DdType, ValueType> xAsColumn = x.swapVariables(this->rowColumnMetaVariablePairs);
    storm::dd::Add<DdType, ValueType> tmp = this->A.multiplyMatrix(xAsColumn, this->columnMetaVariables);
    tmp += b;

    return x == tmp;
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename RationalType, typename ImpreciseType>
storm::dd::Add<DdType, RationalType> SymbolicNativeLinearEquationSolver<DdType, ValueType>::sharpen(
    uint64_t precision, SymbolicNativeLinearEquationSolver<DdType, RationalType> const& rationalSolver, storm::dd::Add<DdType, ImpreciseType> const& x,
    storm::dd::Add<DdType, RationalType> const& rationalB, bool& isSolution) {
    storm::dd::Add<DdType, RationalType> sharpenedX;

    for (uint64_t p = 1; p <= precision; ++p) {
        sharpenedX = x.sharpenKwekMehlhorn(p);

        isSolution = rationalSolver.isSolutionFixedPoint(sharpenedX, rationalB);
        if (isSolution) {
            break;
        }
    }

    return sharpenedX;
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename RationalType, typename ImpreciseType>
storm::dd::Add<DdType, RationalType> SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(
    Environment const& env, SymbolicNativeLinearEquationSolver<DdType, RationalType> const& rationalSolver,
    SymbolicNativeLinearEquationSolver<DdType, ImpreciseType> const& impreciseSolver, storm::dd::Add<DdType, RationalType> const& rationalB,
    storm::dd::Add<DdType, ImpreciseType> const& x, storm::dd::Add<DdType, ImpreciseType> const& b) const {
    // Storage for the rational sharpened vector and the power iteration intermediate vector.
    storm::dd::Add<DdType, RationalType> sharpenedX;
    storm::dd::Add<DdType, ImpreciseType> currentX = x;

    // The actual rational search.
    uint64_t overallIterations = 0;
    uint64_t powerIterationInvocations = 0;
    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().native().getPrecision());
    uint64_t maxIter = env.solver().native().getMaximalNumberOfIterations();
    bool relative = env.solver().native().getRelativeTerminationCriterion();
    SolverStatus status = SolverStatus::InProgress;
    while (status == SolverStatus::InProgress && overallIterations < maxIter) {
        typename SymbolicNativeLinearEquationSolver<DdType, ImpreciseType>::PowerIterationResult result = impreciseSolver.performPowerIteration(
            currentX, b, storm::utility::convertNumber<ImpreciseType, ValueType>(precision), relative, maxIter - overallIterations);

        ++powerIterationInvocations;
        STORM_LOG_TRACE("Completed " << powerIterationInvocations << " power iteration invocations, the last one with precision " << precision
                                     << " completed in " << result.iterations << " iterations.");

        // Count the iterations.
        overallIterations += result.iterations;

        // Compute maximal precision until which to sharpen.
        uint64_t p =
            storm::utility::convertNumber<uint64_t>(storm::utility::ceil(storm::utility::log10<ValueType>(storm::utility::one<ValueType>() / precision)));

        bool isSolution = false;
        sharpenedX = sharpen<RationalType, ImpreciseType>(p, rationalSolver, result.values, rationalB, isSolution);

        if (isSolution) {
            status = SolverStatus::Converged;
        } else {
            currentX = result.values;
            precision /= storm::utility::convertNumber<ValueType, uint64_t>(10);
        }
        if (storm::utility::resources::isTerminate()) {
            status = SolverStatus::Aborted;
        }
    }

    if (status == SolverStatus::InProgress && overallIterations == maxIter) {
        status = SolverStatus::MaximalIterationsExceeded;
    }

    if (status == SolverStatus::Converged) {
        STORM_LOG_INFO("Iterative solver (rational search) converged in " << overallIterations << " iterations.");
    } else {
        STORM_LOG_WARN("Iterative solver (rational search) did not converge in " << overallIterations << " iterations.");
    }

    return sharpenedX;
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename ImpreciseType>
typename std::enable_if<std::is_same<ValueType, ImpreciseType>::value && storm::NumberTraits<ValueType>::IsExact, storm::dd::Add<DdType, ValueType>>::type
SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(Environment const& env, storm::dd::Add<DdType, ValueType> const& x,
                                                                                          storm::dd::Add<DdType, ValueType> const& b) const {
    return solveEquationsRationalSearchHelper<ValueType, ValueType>(env, *this, *this, b, this->getLowerBoundsVector(), b);
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename ImpreciseType>
typename std::enable_if<std::is_same<ValueType, ImpreciseType>::value && !storm::NumberTraits<ValueType>::IsExact, storm::dd::Add<DdType, ValueType>>::type
SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(Environment const& env, storm::dd::Add<DdType, ValueType> const& x,
                                                                                          storm::dd::Add<DdType, ValueType> const& b) const {
    storm::dd::Add<DdType, storm::RationalNumber> rationalB = b.template toValueType<storm::RationalNumber>();
    SymbolicNativeLinearEquationSolver<DdType, storm::RationalNumber> rationalSolver(this->A.template toValueType<storm::RationalNumber>(), this->allRows,
                                                                                     this->rowMetaVariables, this->columnMetaVariables,
                                                                                     this->rowColumnMetaVariablePairs);

    storm::dd::Add<DdType, storm::RationalNumber> rationalResult =
        solveEquationsRationalSearchHelper<storm::RationalNumber, ImpreciseType>(env, rationalSolver, *this, rationalB, this->getLowerBoundsVector(), b);
    return rationalResult.template toValueType<ValueType>();
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename ImpreciseType>
typename std::enable_if<!std::is_same<ValueType, ImpreciseType>::value, storm::dd::Add<DdType, ValueType>>::type
SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(Environment const& env, storm::dd::Add<DdType, ValueType> const& x,
                                                                                          storm::dd::Add<DdType, ValueType> const& b) const {
    // First try to find a solution using the imprecise value type.
    storm::dd::Add<DdType, ValueType> rationalResult;
    storm::dd::Add<DdType, ImpreciseType> impreciseX;
    try {
        impreciseX = this->getLowerBoundsVector().template toValueType<ImpreciseType>();
        storm::dd::Add<DdType, ImpreciseType> impreciseB = b.template toValueType<ImpreciseType>();
        SymbolicNativeLinearEquationSolver<DdType, ImpreciseType> impreciseSolver(
            this->A.template toValueType<ImpreciseType>(), this->allRows, this->rowMetaVariables, this->columnMetaVariables, this->rowColumnMetaVariablePairs);

        rationalResult = solveEquationsRationalSearchHelper<ValueType, ImpreciseType>(env, *this, impreciseSolver, b, impreciseX, impreciseB);
    } catch (storm::exceptions::PrecisionExceededException const&) {
        STORM_LOG_WARN("Precision of value type was exceeded, trying to recover by switching to rational arithmetic.");

        // Fall back to precise value type if the precision of the imprecise value type was exceeded.
        rationalResult = solveEquationsRationalSearchHelper<ValueType, ValueType>(env, *this, *this, b, impreciseX.template toValueType<ValueType>(), b);
    }
    return rationalResult.template toValueType<ValueType>();
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicNativeLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearch(
    Environment const& env, storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b) const {
    STORM_LOG_INFO("Solving symbolic linear equation system with NativeLinearEquationSolver (rational search)");
    return solveEquationsRationalSearchHelper<double>(env, x, b);
}

template<storm::dd::DdType DdType, typename ValueType>
LinearEquationSolverProblemFormat SymbolicNativeLinearEquationSolver<DdType, ValueType>::getEquationProblemFormat(Environment const& env) const {
    auto method = getMethod(env, std::is_same<ValueType, storm::RationalNumber>::value);
    if (method == NativeLinearEquationSolverMethod::Jacobi) {
        return LinearEquationSolverProblemFormat::EquationSystem;
    }
    return LinearEquationSolverProblemFormat::FixedPointSystem;
}

template<storm::dd::DdType DdType, typename ValueType>
LinearEquationSolverRequirements SymbolicNativeLinearEquationSolver<DdType, ValueType>::getRequirements(Environment const& env) const {
    LinearEquationSolverRequirements requirements;
    auto method = getMethod(env, std::is_same<ValueType, storm::RationalNumber>::value);
    if (method == NativeLinearEquationSolverMethod::RationalSearch) {
        requirements.requireLowerBounds();
    }
    return requirements;
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> SymbolicNativeLinearEquationSolverFactory<DdType, ValueType>::create(
    Environment const& env) const {
    return std::make_unique<SymbolicNativeLinearEquationSolver<DdType, ValueType>>();
}

template class SymbolicNativeLinearEquationSolver<storm::dd::DdType::CUDD, double>;
template class SymbolicNativeLinearEquationSolver<storm::dd::DdType::CUDD, storm::RationalNumber>;
template class SymbolicNativeLinearEquationSolver<storm::dd::DdType::Sylvan, double>;
template class SymbolicNativeLinearEquationSolver<storm::dd::DdType::Sylvan, storm::RationalNumber>;

template class SymbolicNativeLinearEquationSolverFactory<storm::dd::DdType::CUDD, double>;
template class SymbolicNativeLinearEquationSolverFactory<storm::dd::DdType::CUDD, storm::RationalNumber>;
template class SymbolicNativeLinearEquationSolverFactory<storm::dd::DdType::Sylvan, double>;
template class SymbolicNativeLinearEquationSolverFactory<storm::dd::DdType::Sylvan, storm::RationalNumber>;
}  // namespace solver
}  // namespace storm
