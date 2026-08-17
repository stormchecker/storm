#include "storm/solver/SymbolicMinMaxLinearEquationSolver.h"

#include "storm/storage/dd/DdManager.h"

#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"

#include "storm/utility/constants.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"

#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/PrecisionExceededException.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/dd.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<storm::dd::DdType DdType, typename ValueType>
SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::SymbolicMinMaxLinearEquationSolver()
    : SymbolicEquationSolver<DdType, ValueType>(), uniqueSolution(false), requirementsChecked(false) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::SymbolicMinMaxLinearEquationSolver(
    storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows, storm::dd::Bdd<DdType> const& illegalMask,
    std::set<storm::expressions::Variable> const& rowMetaVariables, std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::set<storm::expressions::Variable> const& choiceVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::unique_ptr<SymbolicLinearEquationSolverFactory<DdType, ValueType>>&& linearEquationSolverFactory)
    : SymbolicEquationSolver<DdType, ValueType>(allRows),
      A(A),
      illegalMask(illegalMask),
      illegalMaskAdd(illegalMask.ite(A.getDdManager().getConstant(storm::utility::infinity<ValueType>()), A.getDdManager().template getAddZero<ValueType>())),
      rowMetaVariables(rowMetaVariables),
      columnMetaVariables(columnMetaVariables),
      choiceVariables(choiceVariables),
      rowColumnMetaVariablePairs(rowColumnMetaVariablePairs),
      linearEquationSolverFactory(std::move(linearEquationSolverFactory)),
      uniqueSolution(false),
      requirementsChecked(false) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
MinMaxMethod SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::getMethod(Environment const& env, bool isExactMode) const {
    // Adjust the method if none was specified and we are using rational numbers.
    auto method = env.solver().minMax().getMethod();

    if (isExactMode && method != MinMaxMethod::RationalSearch) {
        if (env.solver().minMax().isMethodSetFromDefault()) {
            STORM_LOG_INFO(
                "Selecting 'rational search' as the solution technique to guarantee exact results. If you want to override this, please explicitly specify a "
                "different method.");
            method = MinMaxMethod::RationalSearch;
        } else {
            STORM_LOG_WARN("The selected solution method does not guarantee exact results.");
        }
    }
    if (method != MinMaxMethod::ValueIteration && method != MinMaxMethod::PolicyIteration && method != MinMaxMethod::RationalSearch) {
        STORM_LOG_WARN("Selected method is not supported for this solver, switching to value iteration.");
        method = MinMaxMethod::ValueIteration;
    }

    return method;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquations(Environment const& env,
                                                                                                        storm::solver::OptimizationDirection const& dir,
                                                                                                        storm::dd::Add<DdType, ValueType> const& x,
                                                                                                        storm::dd::Add<DdType, ValueType> const& b) const {
    STORM_LOG_WARN_COND_DEBUG(this->isRequirementsCheckedSet(),
                              "The requirements of the solver have not been marked as checked. Please provide the appropriate check or mark the requirements "
                              "as checked (if applicable).");

    switch (getMethod(env, std::is_same<ValueType, storm::RationalNumber>::value)) {
        case MinMaxMethod::ValueIteration:
            return solveEquationsValueIteration(env, dir, x, b);
            break;
        case MinMaxMethod::PolicyIteration:
            return solveEquationsPolicyIteration(env, dir, x, b);
            break;
        case MinMaxMethod::RationalSearch:
            return solveEquationsRationalSearch(env, dir, x, b);
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "The selected min max technique is not supported by this solver.");
    }
    return storm::dd::Add<DdType, ValueType>();
}

template<storm::dd::DdType DdType, typename ValueType>
typename SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::ValueIterationResult
SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::performValueIteration(storm::solver::OptimizationDirection const& dir,
                                                                             storm::dd::Add<DdType, ValueType> const& x,
                                                                             storm::dd::Add<DdType, ValueType> const& b, ValueType const& precision,
                                                                             bool relativeTerminationCriterion, uint64_t maximalIterations) const {
    // Set up local variables.
    storm::dd::Add<DdType, ValueType> localX = x;
    uint64_t iterations = 0;

    // Value iteration loop.
    SolverStatus status = SolverStatus::InProgress;
    while (status == SolverStatus::InProgress && iterations < maximalIterations) {
        // Compute tmp = A * x + b
        storm::dd::Add<DdType, ValueType> localXAsColumn = localX.swapVariables(this->rowColumnMetaVariablePairs);
        storm::dd::Add<DdType, ValueType> tmp = this->A.multiplyMatrix(localXAsColumn, this->columnMetaVariables);
        tmp += b;

        if (dir == storm::solver::OptimizationDirection::Minimize) {
            tmp += illegalMaskAdd;
            tmp = tmp.minAbstract(this->choiceVariables);
        } else {
            tmp = tmp.maxAbstract(this->choiceVariables);
        }

        // Now check if the process already converged within our precision.
        if (localX.equalModuloPrecision(tmp, precision, relativeTerminationCriterion)) {
            status = SolverStatus::Converged;
        }

        // Set up next iteration.
        localX = tmp;
        ++iterations;
        if (storm::utility::resources::isTerminate()) {
            status = SolverStatus::Aborted;
        }
    }

    if (status == SolverStatus::InProgress && iterations < maximalIterations) {
        status = SolverStatus::MaximalIterationsExceeded;
    }

    return SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::ValueIterationResult(status, iterations, localX);
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::isSolution(OptimizationDirection dir, storm::dd::Add<DdType, ValueType> const& x,
                                                                       storm::dd::Add<DdType, ValueType> const& b) const {
    storm::dd::Add<DdType, ValueType> xAsColumn = x.swapVariables(this->rowColumnMetaVariablePairs);
    storm::dd::Add<DdType, ValueType> tmp = this->A.multiplyMatrix(xAsColumn, this->columnMetaVariables);
    tmp += b;

    if (dir == storm::solver::OptimizationDirection::Minimize) {
        tmp += illegalMaskAdd;
        tmp = tmp.minAbstract(this->choiceVariables);
    } else {
        tmp = tmp.maxAbstract(this->choiceVariables);
    }

    return x == tmp;
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename RationalType, typename ImpreciseType>
storm::dd::Add<DdType, RationalType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::sharpen(
    OptimizationDirection dir, uint64_t precision, SymbolicMinMaxLinearEquationSolver<DdType, RationalType> const& rationalSolver,
    storm::dd::Add<DdType, ImpreciseType> const& x, storm::dd::Add<DdType, RationalType> const& rationalB, bool& isSolution) {
    storm::dd::Add<DdType, RationalType> sharpenedX;

    for (uint64_t p = 1; p < precision; ++p) {
        sharpenedX = x.sharpenKwekMehlhorn(p);
        isSolution = rationalSolver.isSolution(dir, sharpenedX, rationalB);

        if (isSolution) {
            break;
        }
    }

    return sharpenedX;
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename RationalType, typename ImpreciseType>
storm::dd::Add<DdType, RationalType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(
    Environment const& env, OptimizationDirection dir, SymbolicMinMaxLinearEquationSolver<DdType, RationalType> const& rationalSolver,
    SymbolicMinMaxLinearEquationSolver<DdType, ImpreciseType> const& impreciseSolver, storm::dd::Add<DdType, RationalType> const& rationalB,
    storm::dd::Add<DdType, ImpreciseType> const& x, storm::dd::Add<DdType, ImpreciseType> const& b) const {
    // Storage for the rational sharpened vector and the power iteration intermediate vector.
    storm::dd::Add<DdType, RationalType> sharpenedX;
    storm::dd::Add<DdType, ImpreciseType> currentX = x;

    // The actual rational search.
    uint64_t overallIterations = 0;
    uint64_t valueIterationInvocations = 0;
    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision());
    uint64_t maxIter = env.solver().minMax().getMaximalNumberOfIterations();
    bool relative = env.solver().minMax().getRelativeTerminationCriterion();
    SolverStatus status = SolverStatus::InProgress;
    while (status == SolverStatus::InProgress && overallIterations < maxIter) {
        typename SymbolicMinMaxLinearEquationSolver<DdType, ImpreciseType>::ValueIterationResult viResult =
            impreciseSolver.performValueIteration(dir, currentX, b, storm::utility::convertNumber<ImpreciseType, ValueType>(precision), relative, maxIter);

        ++valueIterationInvocations;
        STORM_LOG_TRACE("Completed " << valueIterationInvocations << " value iteration invocations, the last one with precision " << precision
                                     << " completed in " << viResult.iterations << " iterations.");

        // Count the iterations.
        overallIterations += viResult.iterations;

        // Compute maximal precision until which to sharpen.
        uint64_t p =
            storm::utility::convertNumber<uint64_t>(storm::utility::ceil(storm::utility::log10<ValueType>(storm::utility::one<ValueType>() / precision)));

        bool isSolution = false;
        sharpenedX = sharpen<RationalType, ImpreciseType>(dir, p, rationalSolver, viResult.values, rationalB, isSolution);

        if (isSolution) {
            status = SolverStatus::Converged;
        } else {
            currentX = viResult.values;
            precision /= storm::utility::convertNumber<ValueType, uint64_t>(10);
        }
        if (storm::utility::resources::isTerminate()) {
            status = SolverStatus::Aborted;
        }
    }

    if (status == SolverStatus::InProgress && overallIterations < maxIter) {
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
SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(Environment const& env,
                                                                                          storm::solver::OptimizationDirection const& dir,
                                                                                          storm::dd::Add<DdType, ValueType> const& x,
                                                                                          storm::dd::Add<DdType, ValueType> const& b) const {
    return solveEquationsRationalSearchHelper<ValueType, ValueType>(env, dir, *this, *this, b, this->getLowerBoundsVector(), b);
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename ImpreciseType>
typename std::enable_if<std::is_same<ValueType, ImpreciseType>::value && !storm::NumberTraits<ValueType>::IsExact, storm::dd::Add<DdType, ValueType>>::type
SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(Environment const& env,
                                                                                          storm::solver::OptimizationDirection const& dir,
                                                                                          storm::dd::Add<DdType, ValueType> const& x,
                                                                                          storm::dd::Add<DdType, ValueType> const& b) const {
    storm::dd::Add<DdType, storm::RationalNumber> rationalB = b.template toValueType<storm::RationalNumber>();
    SymbolicMinMaxLinearEquationSolver<DdType, storm::RationalNumber> rationalSolver(
        this->A.template toValueType<storm::RationalNumber>(), this->allRows, this->illegalMask, this->rowMetaVariables, this->columnMetaVariables,
        this->choiceVariables, this->rowColumnMetaVariablePairs, std::make_unique<GeneralSymbolicLinearEquationSolverFactory<DdType, storm::RationalNumber>>());

    storm::dd::Add<DdType, storm::RationalNumber> rationalResult =
        solveEquationsRationalSearchHelper<storm::RationalNumber, ImpreciseType>(env, dir, rationalSolver, *this, rationalB, this->getLowerBoundsVector(), b);
    return rationalResult.template toValueType<ValueType>();
}

template<storm::dd::DdType DdType, typename ValueType>
template<typename ImpreciseType>
typename std::enable_if<!std::is_same<ValueType, ImpreciseType>::value, storm::dd::Add<DdType, ValueType>>::type
SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearchHelper(Environment const& env,
                                                                                          storm::solver::OptimizationDirection const& dir,
                                                                                          storm::dd::Add<DdType, ValueType> const& x,
                                                                                          storm::dd::Add<DdType, ValueType> const& b) const {
    // First try to find a solution using the imprecise value type.
    storm::dd::Add<DdType, ValueType> rationalResult;
    storm::dd::Add<DdType, ImpreciseType> impreciseX;
    try {
        impreciseX = this->getLowerBoundsVector().template toValueType<ImpreciseType>();
        storm::dd::Add<DdType, ImpreciseType> impreciseB = b.template toValueType<ImpreciseType>();
        SymbolicMinMaxLinearEquationSolver<DdType, ImpreciseType> impreciseSolver(
            this->A.template toValueType<ImpreciseType>(), this->allRows, this->illegalMask, this->rowMetaVariables, this->columnMetaVariables,
            this->choiceVariables, this->rowColumnMetaVariablePairs, std::make_unique<GeneralSymbolicLinearEquationSolverFactory<DdType, ImpreciseType>>());

        rationalResult = solveEquationsRationalSearchHelper<ValueType, ImpreciseType>(env, dir, *this, impreciseSolver, b, impreciseX, impreciseB);
    } catch (storm::exceptions::PrecisionExceededException const&) {
        STORM_LOG_WARN("Precision of value type was exceeded, trying to recover by switching to rational arithmetic.");

        // Fall back to precise value type if the precision of the imprecise value type was exceeded.
        rationalResult = solveEquationsRationalSearchHelper<ValueType, ValueType>(env, dir, *this, *this, b, impreciseX.template toValueType<ValueType>(), b);
    }
    return rationalResult.template toValueType<ValueType>();
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsRationalSearch(
    Environment const& env, storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
    storm::dd::Add<DdType, ValueType> const& b) const {
    return solveEquationsRationalSearchHelper<double>(env, dir, x, b);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsValueIteration(
    Environment const& env, storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
    storm::dd::Add<DdType, ValueType> const& b) const {
    // Set up the environment.
    storm::dd::Add<DdType, ValueType> localX;

    if (this->hasUniqueSolution()) {
        localX = x;
    } else {
        // If we were given an initial scheduler, we take its solution as the starting point.
        if (this->hasInitialScheduler()) {
            // The linear equation solver should be at least as precise as this solver
            std::unique_ptr<storm::Environment> environmentOfSolverStorage;
            auto precOfSolver = env.solver().getPrecisionOfLinearEquationSolver(env.solver().getLinearEquationSolverType());
            if (!storm::NumberTraits<ValueType>::IsExact) {
                bool changePrecision = precOfSolver.first && precOfSolver.first.get() > env.solver().minMax().getPrecision();
                bool changeRelative = precOfSolver.second && !precOfSolver.second.get() && env.solver().minMax().getRelativeTerminationCriterion();
                if (changePrecision || changeRelative) {
                    environmentOfSolverStorage = std::make_unique<storm::Environment>(env);
                    boost::optional<storm::RationalNumber> newPrecision;
                    boost::optional<bool> newRelative;
                    if (changePrecision) {
                        newPrecision = env.solver().minMax().getPrecision();
                    }
                    if (changeRelative) {
                        newRelative = true;
                    }
                    environmentOfSolverStorage->solver().setLinearEquationSolverPrecision(newPrecision, newRelative);
                }
            }
            storm::Environment const& environmentOfSolver = environmentOfSolverStorage ? *environmentOfSolverStorage : env;
            localX = solveEquationsWithScheduler(environmentOfSolver, this->getInitialScheduler(), x, b);
        } else {
            localX = this->getLowerBoundsVector();
        }
    }

    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision());
    ValueIterationResult viResult = performValueIteration(dir, localX, b, precision, env.solver().minMax().getRelativeTerminationCriterion(),
                                                          env.solver().minMax().getMaximalNumberOfIterations());

    if (viResult.status == SolverStatus::Converged) {
        STORM_LOG_INFO("Iterative solver (value iteration) converged in " << viResult.iterations << " iterations.");
    } else {
        STORM_LOG_WARN("Iterative solver (value iteration) did not converge in " << viResult.iterations << " iterations.");
    }

    return viResult.values;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsWithScheduler(
    Environment const& env, storm::dd::Bdd<DdType> const& scheduler, storm::dd::Add<DdType, ValueType> const& x,
    storm::dd::Add<DdType, ValueType> const& b) const {
    std::unique_ptr<SymbolicLinearEquationSolver<DdType, ValueType>> solver =
        linearEquationSolverFactory->create(env, this->allRows, this->rowMetaVariables, this->columnMetaVariables, this->rowColumnMetaVariablePairs);
    this->forwardBounds(*solver);

    storm::dd::Add<DdType, ValueType> diagonal =
        (storm::utility::dd::getRowColumnDiagonal<DdType>(x.getDdManager(), this->rowColumnMetaVariablePairs) && this->allRows).template toAdd<ValueType>();
    return solveEquationsWithScheduler(env, *solver, scheduler, x, b, diagonal);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsWithScheduler(
    Environment const& env, SymbolicLinearEquationSolver<DdType, ValueType>& solver, storm::dd::Bdd<DdType> const& scheduler,
    storm::dd::Add<DdType, ValueType> const& x, storm::dd::Add<DdType, ValueType> const& b, storm::dd::Add<DdType, ValueType> const& diagonal) const {
    // Apply scheduler to the matrix and vector.
    storm::dd::Add<DdType, ValueType> schedulerA =
        scheduler.ite(this->A, scheduler.getDdManager().template getAddZero<ValueType>()).sumAbstract(this->choiceVariables);
    if (solver.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem) {
        schedulerA = diagonal - schedulerA;
    }

    storm::dd::Add<DdType, ValueType> schedulerB =
        scheduler.ite(b, scheduler.getDdManager().template getAddZero<ValueType>()).sumAbstract(this->choiceVariables);

    // Set the matrix for the solver.
    solver.setMatrix(schedulerA);

    // Solve for the value of the scheduler.
    storm::dd::Add<DdType, ValueType> schedulerX = solver.solveEquations(env, x, schedulerB);

    return schedulerX;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::solveEquationsPolicyIteration(
    Environment const& env, storm::solver::OptimizationDirection const& dir, storm::dd::Add<DdType, ValueType> const& x,
    storm::dd::Add<DdType, ValueType> const& b) const {
    // Set up the environment.
    storm::dd::Add<DdType, ValueType> currentSolution = x;
    storm::dd::Add<DdType, ValueType> diagonal =
        (storm::utility::dd::getRowColumnDiagonal<DdType>(x.getDdManager(), this->rowColumnMetaVariablePairs) && this->allRows).template toAdd<ValueType>();
    uint_fast64_t iterations = 0;
    bool converged = false;

    // Choose initial scheduler.
    storm::dd::Bdd<DdType> scheduler =
        this->hasInitialScheduler()
            ? this->getInitialScheduler()
            : (this->A.sumAbstract(this->columnMetaVariables).notZero() || b.notZero()).existsAbstractRepresentative(this->choiceVariables);

    // Initialize linear equation solver.
    // It should be at least as precise as this solver.
    std::unique_ptr<storm::Environment> environmentOfSolverStorage;
    auto precOfSolver = env.solver().getPrecisionOfLinearEquationSolver(env.solver().getLinearEquationSolverType());
    if (!storm::NumberTraits<ValueType>::IsExact) {
        bool changePrecision = precOfSolver.first && precOfSolver.first.get() > env.solver().minMax().getPrecision();
        bool changeRelative = precOfSolver.second && !precOfSolver.second.get() && env.solver().minMax().getRelativeTerminationCriterion();
        if (changePrecision || changeRelative) {
            environmentOfSolverStorage = std::make_unique<storm::Environment>(env);
            boost::optional<storm::RationalNumber> newPrecision;
            boost::optional<bool> newRelative;
            if (changePrecision) {
                newPrecision = env.solver().minMax().getPrecision();
            }
            if (changeRelative) {
                newRelative = true;
            }
            environmentOfSolverStorage->solver().setLinearEquationSolverPrecision(newPrecision, newRelative);
        }
    }
    storm::Environment const& environmentOfSolver = environmentOfSolverStorage ? *environmentOfSolverStorage : env;

    std::unique_ptr<SymbolicLinearEquationSolver<DdType, ValueType>> linearEquationSolver = linearEquationSolverFactory->create(
        environmentOfSolver, this->allRows, this->rowMetaVariables, this->columnMetaVariables, this->rowColumnMetaVariablePairs);
    this->forwardBounds(*linearEquationSolver);

    // Iteratively solve and improve the scheduler.
    uint64_t maxIter = env.solver().minMax().getMaximalNumberOfIterations();
    while (!converged && iterations < maxIter) {
        storm::dd::Add<DdType, ValueType> schedulerX =
            solveEquationsWithScheduler(environmentOfSolver, *linearEquationSolver, scheduler, currentSolution, b, diagonal);

        // Policy improvement step.
        storm::dd::Add<DdType, ValueType> choiceValues =
            this->A.multiplyMatrix(schedulerX.swapVariables(this->rowColumnMetaVariablePairs), this->columnMetaVariables) + b;

        storm::dd::Bdd<DdType> nextScheduler;
        if (dir == storm::solver::OptimizationDirection::Minimize) {
            choiceValues += illegalMaskAdd;

            storm::dd::Add<DdType, ValueType> newStateValues = choiceValues.minAbstract(this->choiceVariables);
            storm::dd::Bdd<DdType> improvedStates = newStateValues.less(schedulerX);

            nextScheduler = improvedStates.ite(choiceValues.minAbstractRepresentative(this->choiceVariables), scheduler);
        } else {
            storm::dd::Add<DdType, ValueType> newStateValues = choiceValues.maxAbstract(this->choiceVariables);
            storm::dd::Bdd<DdType> improvedStates = newStateValues.greater(schedulerX);

            nextScheduler = improvedStates.ite(choiceValues.maxAbstractRepresentative(this->choiceVariables), scheduler);
        }

        // Check for convergence.
        converged = nextScheduler == scheduler;

        // Set up next iteration.
        if (!converged) {
            scheduler = nextScheduler;
        }

        currentSolution = schedulerX;
        ++iterations;
    }

    if (converged) {
        STORM_LOG_INFO("Iterative solver (policy iteration) converged in " << iterations << " iterations.");
    } else {
        STORM_LOG_WARN("Iterative solver (policy iteration) did not converge in " << iterations << " iterations.");
    }

    return currentSolution;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::multiply(storm::solver::OptimizationDirection const& dir,
                                                                                                  storm::dd::Add<DdType, ValueType> const& x,
                                                                                                  storm::dd::Add<DdType, ValueType> const* b,
                                                                                                  uint_fast64_t n) const {
    storm::dd::Add<DdType, ValueType> xCopy = x;

    // Perform matrix-vector multiplication while the bound is met.
    for (uint_fast64_t i = 0; i < n; ++i) {
        xCopy = xCopy.swapVariables(this->rowColumnMetaVariablePairs);
        xCopy = this->A.multiplyMatrix(xCopy, this->columnMetaVariables);
        if (b != nullptr) {
            xCopy += *b;
        }

        if (dir == storm::solver::OptimizationDirection::Minimize) {
            // This is a hack and only here because of the lack of a suitable minAbstract/maxAbstract function
            // that can properly deal with a restriction of the choices.
            xCopy += illegalMaskAdd;
            xCopy = xCopy.minAbstract(this->choiceVariables);
        } else {
            xCopy = xCopy.maxAbstract(this->choiceVariables);
        }
    }

    return xCopy;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::setInitialScheduler(storm::dd::Bdd<DdType> const& scheduler) {
    this->initialScheduler = scheduler;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> const& SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::getInitialScheduler() const {
    return initialScheduler.get();
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::hasInitialScheduler() const {
    return static_cast<bool>(initialScheduler);
}

template<storm::dd::DdType DdType, typename ValueType>
MinMaxLinearEquationSolverRequirements SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::getRequirements(
    Environment const& env, boost::optional<storm::solver::OptimizationDirection> const& direction) const {
    MinMaxLinearEquationSolverRequirements requirements;

    auto method = getMethod(env, std::is_same<ValueType, storm::RationalNumber>::value);
    if (method == MinMaxMethod::PolicyIteration) {
        if (!this->hasUniqueSolution()) {
            requirements.requireValidInitialScheduler();
        }
    } else if (method == MinMaxMethod::ValueIteration) {
        if (!this->hasUniqueSolution()) {
            if (!direction || direction.get() == storm::solver::OptimizationDirection::Maximize) {
                requirements.requireLowerBounds();
            }
            if (!direction || direction.get() == storm::solver::OptimizationDirection::Minimize) {
                requirements.requireValidInitialScheduler();
            }
        }
    } else if (method == MinMaxMethod::RationalSearch) {
        requirements.requireLowerBounds();
        if (!this->hasUniqueSolution() && (!direction || direction.get() == storm::solver::OptimizationDirection::Minimize)) {
            requirements.requireUniqueSolution();
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "The selected min max technique is not supported by this solver.");
    }

    return requirements;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::setHasUniqueSolution(bool value) {
    this->uniqueSolution = value;
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::hasUniqueSolution() const {
    return this->uniqueSolution;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::setRequirementsChecked(bool value) {
    this->requirementsChecked = value;
}

template<storm::dd::DdType DdType, typename ValueType>
bool SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::isRequirementsCheckedSet() const {
    return this->requirementsChecked;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicMinMaxLinearEquationSolver<DdType, ValueType>::forwardBounds(storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>& solver) const {
    if (this->hasLowerBound()) {
        solver.setLowerBound(this->getLowerBound());
    }
    if (this->hasLowerBounds()) {
        solver.setLowerBounds(this->getLowerBounds());
    }
    if (this->hasUpperBound()) {
        solver.setUpperBound(this->getUpperBound());
    }
    if (this->hasUpperBounds()) {
        solver.setUpperBounds(this->getUpperBounds());
    }
}

template<storm::dd::DdType DdType, typename ValueType>
MinMaxLinearEquationSolverRequirements SymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType>::getRequirements(
    Environment const& env, bool hasUniqueSolution, boost::optional<storm::solver::OptimizationDirection> const& direction) const {
    std::unique_ptr<storm::solver::SymbolicMinMaxLinearEquationSolver<DdType, ValueType>> solver = this->create();
    solver->setHasUniqueSolution(hasUniqueSolution);
    return solver->getRequirements(env, direction);
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::solver::SymbolicMinMaxLinearEquationSolver<DdType, ValueType>>
GeneralSymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType>::create(
    storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows, storm::dd::Bdd<DdType> const& illegalMask,
    std::set<storm::expressions::Variable> const& rowMetaVariables, std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::set<storm::expressions::Variable> const& choiceVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs) const {
    return std::make_unique<SymbolicMinMaxLinearEquationSolver<DdType, ValueType>>(
        A, allRows, illegalMask, rowMetaVariables, columnMetaVariables, choiceVariables, rowColumnMetaVariablePairs,
        std::make_unique<GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType>>());
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::solver::SymbolicMinMaxLinearEquationSolver<DdType, ValueType>>
GeneralSymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType>::create() const {
    return std::make_unique<SymbolicMinMaxLinearEquationSolver<DdType, ValueType>>();
}

template class SymbolicMinMaxLinearEquationSolver<storm::dd::DdType::CUDD, double>;
template class SymbolicMinMaxLinearEquationSolver<storm::dd::DdType::Sylvan, double>;
template class SymbolicMinMaxLinearEquationSolver<storm::dd::DdType::Sylvan, storm::RationalNumber>;

template class GeneralSymbolicMinMaxLinearEquationSolverFactory<storm::dd::DdType::CUDD, double>;
template class GeneralSymbolicMinMaxLinearEquationSolverFactory<storm::dd::DdType::Sylvan, double>;
template class GeneralSymbolicMinMaxLinearEquationSolverFactory<storm::dd::DdType::Sylvan, storm::RationalNumber>;

}  // namespace solver
}  // namespace storm
