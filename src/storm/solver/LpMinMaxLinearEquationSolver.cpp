#include "storm/solver/LpMinMaxLinearEquationSolver.h"

#include "storm/environment/solver/MinMaxLpSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/solver/helper/ValueIterationHelper.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/storage/expressions/BinaryRelationType.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

template<typename ValueType>
LpMinMaxLinearEquationSolver<ValueType>::LpMinMaxLinearEquationSolver(std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>>&& lpSolverFactory)
    : lpSolverFactory(std::move(lpSolverFactory)) {
    // Intentionally left empty.
}

template<typename ValueType>
LpMinMaxLinearEquationSolver<ValueType>::LpMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A,
                                                                      std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>>&& lpSolverFactory)
    : StandardMinMaxLinearEquationSolver<ValueType>(A), lpSolverFactory(std::move(lpSolverFactory)) {
    // Intentionally left empty.
}

template<typename ValueType>
LpMinMaxLinearEquationSolver<ValueType>::LpMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A,
                                                                      std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>>&& lpSolverFactory)
    : StandardMinMaxLinearEquationSolver<ValueType>(std::move(A)), lpSolverFactory(std::move(lpSolverFactory)) {
    // Intentionally left empty.
}

template<typename ValueType>
bool LpMinMaxLinearEquationSolver<ValueType>::internalSolveEquations(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x,
                                                                     std::vector<ValueType> const& b) const {
    if (env.solver().minMax().getMethod() == MinMaxMethod::LinearProgramming) {
        return solveEquationsLp(env, dir, x, b);
    } else {
        STORM_LOG_THROW(env.solver().minMax().getMethod() == MinMaxMethod::ViToLp, storm::exceptions::InvalidEnvironmentException,
                        "This min max solver does not support the selected technique.");
        return solveEquationsViToLp(env, dir, x, b);
    }
}

template<typename ValueType>
bool LpMinMaxLinearEquationSolver<ValueType>::solveEquationsViToLp(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x,
                                                                   std::vector<ValueType> const& b) const {
    // First create an (inprecise) vi solver to get a good initial bound.
    STORM_LOG_THROW(!this->choiceFixedForRowGroup, storm::exceptions::NotImplementedException, "Fixed choices not implemented for this solution method.");
    {
        auto viOperator = std::make_shared<helper::ValueIterationOperator<double, false>>();
        if constexpr (std::is_same_v<ValueType, double>) {
            viOperator->setMatrixBackwards(*this->A);
        } else {
            viOperator->setMatrixBackwards(this->A->template toValueType<double>(), &this->A->getRowGroupIndices());
        }
        storm::solver::helper::ValueIterationHelper<double, false> viHelper(viOperator);
        uint64_t numIterations{0};
        auto viCallback = [&](SolverStatus const& current) {
            this->showProgressIterative(numIterations);
            return this->updateStatus(current, false, numIterations, env.solver().minMax().getMaximalNumberOfIterations());
        };
        if (minimize(dir)) {
            this->createUpperBoundsVector(x);
        } else {
            this->createLowerBoundsVector(x);
        }
        this->startMeasureProgress();
        if constexpr (std::is_same_v<ValueType, double>) {
            viHelper.VI(x, b, numIterations, env.solver().minMax().getRelativeTerminationCriterion(),
                        storm::utility::convertNumber<double>(env.solver().minMax().getPrecision()), dir, viCallback);
        } else {
            // convert from/to double
            auto xVi = storm::utility::vector::convertNumericVector<double>(x);
            auto bVi = storm::utility::vector::convertNumericVector<double>(b);
            double const precision = storm::utility::convertNumber<double>(env.solver().minMax().getPrecision());
            bool const relative = env.solver().minMax().getRelativeTerminationCriterion();
            viHelper.VI(xVi, bVi, numIterations, relative, precision, dir, viCallback);
            auto xIt = xVi.cbegin();
            for (auto& xi : x) {
                xi = storm::utility::convertNumber<ValueType>(*xIt);
                ++xIt;
            }
        }
    }
    STORM_LOG_DEBUG("Found initial values using Value Iteration. Starting LP solving now.");
    bool res = false;
    if (minimize(dir)) {
        res = solveEquationsLp(env, dir, x, b, nullptr, &x);  // upper bounds
    } else {
        res = solveEquationsLp(env, dir, x, b, &x, nullptr);  // lower bounds
    }

    if (!res) {
        return false;
    }

    if constexpr (storm::NumberTraits<ValueType>::IsExact) {
        // The above-computed bounds might be incorrect. To obtain a correct procedure, we catch those cases here!
        for (uint64_t rowGroup = 0; rowGroup < this->A->getRowGroupCount(); ++rowGroup) {
            uint64_t row = this->A->getRowGroupIndices()[rowGroup];
            ValueType optimalGroupValue = this->A->multiplyRowWithVector(row, x) + b[row];
            for (++row; row < this->A->getRowGroupIndices()[rowGroup + 1]; ++row) {
                ValueType rowValue = this->A->multiplyRowWithVector(row, x) + b[row];
                if ((minimize(dir) && rowValue < optimalGroupValue) || (maximize(dir) && rowValue > optimalGroupValue)) {
                    optimalGroupValue = rowValue;
                }
            }
            if (x[rowGroup] != optimalGroupValue) {
                STORM_LOG_WARN("LP with provided bounds is incorrect. Restarting without bounds.");
                return solveEquationsLp(env, dir, x, b);  // no bounds
            }
        }
    }

    return true;
}

template<typename ValueType>
bool LpMinMaxLinearEquationSolver<ValueType>::solveEquationsLp(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x,
                                                               std::vector<ValueType> const& b, std::vector<ValueType> const* lowerBounds,
                                                               std::vector<ValueType> const* upperBounds) const {
    // Determine the variant of the encoding
    // Enforcing a global bound is only enabled if there is a single initial state.
    // Otherwise, cases where one state satisfies the bound but another does not will be difficult.
    bool const optimizeOnlyRelevant = this->hasRelevantValues() && env.solver().minMax().lp().getOptimizeOnlyForInitialState();
    STORM_LOG_DEBUG("Optimize only for relevant state requested:" << env.solver().minMax().lp().getOptimizeOnlyForInitialState());
    if (optimizeOnlyRelevant) {
        STORM_LOG_TRACE("Relevant values " << this->getRelevantValues());
    } else if (!this->hasRelevantValues()) {
        STORM_LOG_DEBUG("No relevant values set! Optimizing over all states.");
    }

    // Set-up lower/upper bounds
    std::function<ValueType(uint64_t const&)> lower, upper;
    if (this->hasLowerBound() && lowerBounds == nullptr) {
        lower = [this](uint64_t const& i) { return this->getLowerBound(i); };
    } else if (!this->hasLowerBound() && lowerBounds != nullptr) {
        STORM_LOG_ASSERT(lowerBounds->size() == x.size(), "lower bounds vector has invalid size.");
        lower = [&lowerBounds](uint64_t const& i) { return (*lowerBounds)[i]; };
    } else if (this->hasLowerBound() && lowerBounds != nullptr) {
        STORM_LOG_ASSERT(lowerBounds->size() == x.size(), "lower bounds vector has invalid size.");
        lower = [&lowerBounds, this](uint64_t const& i) { return std::max(this->getLowerBound(i), (*lowerBounds)[i]); };
    }
    if (this->hasUpperBound() && upperBounds == nullptr) {
        upper = [this](uint64_t const& i) { return this->getUpperBound(i); };
    } else if (!this->hasUpperBound() && upperBounds != nullptr) {
        STORM_LOG_ASSERT(upperBounds->size() == x.size(), "upper bounds vector has invalid size.");
        upper = [&upperBounds](uint64_t const& i) { return (*upperBounds)[i]; };
    } else if (this->hasUpperBound() && upperBounds != nullptr) {
        STORM_LOG_ASSERT(upperBounds->size() == x.size(), "upper bounds vector has invalid size.");
        upper = [&upperBounds, this](uint64_t const& i) { return std::min(this->getUpperBound(i), (*upperBounds)[i]); };
    }
    bool const useBounds = lower || upper;

    // Set up the LP solver
    auto solver = lpSolverFactory->createRaw(env, "");
    solver->setOptimizationDirection(invert(dir));
    using VariableIndex = typename LpSolver<ValueType, true>::Variable;
    std::map<VariableIndex, ValueType> constantRowGroups;  // Keep track of the rows that are known to be constants

    // Create a variable for each row group
    for (uint64_t rowGroup = 0; rowGroup < this->A->getRowGroupCount(); ++rowGroup) {
        ValueType const objValue =
            (optimizeOnlyRelevant && !this->getRelevantValues().get(rowGroup)) ? storm::utility::zero<ValueType>() : storm::utility::one<ValueType>();
        std::optional<ValueType> lowerBound, upperBound;
        if (useBounds) {
            if (lower) {
                lowerBound = lower(rowGroup);
            }
            if (upper) {
                upperBound = upper(rowGroup);
                if (lowerBound) {
                    STORM_LOG_ASSERT(*lowerBound <= *upperBound, "Lower Bound at row group " << rowGroup << " is " << *lowerBound
                                                                                             << " which exceeds the upper bound " << *upperBound << ".");
                    if (*lowerBound == *upperBound) {
                        // Some solvers (like glpk) don't support variables with bounds [x,x]. We therefore just use a constant instead. This should be more
                        // efficient anyway.
                        constantRowGroups.emplace(rowGroup, *lowerBound);
                        // Still, a dummy variable is added so that variable-indices coincide with state indices
                        solver->addContinuousVariable("dummy" + std::to_string(rowGroup));
                        continue;  // with next rowGroup
                    }
                }
            }
        }
        solver->addContinuousVariable("x" + std::to_string(rowGroup), lowerBound, upperBound, objValue);
    }
    solver->update();
    STORM_LOG_DEBUG("Use eq if there is a single action: " << env.solver().minMax().lp().getUseEqualityForSingleActions());
    bool const useEqualityForSingleAction = env.solver().minMax().lp().getUseEqualityForSingleActions();

    // Add a set of constraints for each row group
    auto const defaultRelationType = minimize(dir) ? storm::expressions::RelationType::GreaterOrEqual : storm::expressions::RelationType::LessOrEqual;
    for (uint64_t rowGroup = 0; rowGroup < this->A->getRowGroupCount(); ++rowGroup) {
        // The rowgroup refers to the state number
        uint64_t rowIndex, rowGroupEnd;
        if (this->choiceFixedForRowGroup && this->choiceFixedForRowGroup.get()[rowGroup]) {
            rowIndex = this->A->getRowGroupIndices()[rowGroup] + this->getInitialScheduler()[rowGroup];
            rowGroupEnd = rowIndex + 1;
        } else {
            rowIndex = this->A->getRowGroupIndices()[rowGroup];
            rowGroupEnd = this->A->getRowGroupIndices()[rowGroup + 1];
        }
        bool const singleAction = (rowIndex + 1 == rowGroupEnd);
        auto const relationType = (useEqualityForSingleAction && singleAction) ? storm::expressions::RelationType::Equal : defaultRelationType;
        // Add a constraint for each row in the current row group
        for (; rowIndex < rowGroupEnd; ++rowIndex) {
            auto row = this->A->getRow(rowIndex);
            RawLpConstraint<ValueType> constraint(relationType, -b[rowIndex], row.getNumberOfEntries());
            auto addToConstraint = [&constraint, &constantRowGroups](VariableIndex const& var, ValueType const& val) {
                if (auto findRes = constantRowGroups.find(var); findRes != constantRowGroups.end()) {
                    constraint.rhs -= findRes->second * val;
                } else {
                    constraint.addToLhs(var, val);
                }
            };
            auto entryIt = row.begin();
            auto const entryItEnd = row.end();
            for (; entryIt != entryItEnd && entryIt->getColumn() < rowGroup; ++entryIt) {
                addToConstraint(entryIt->getColumn(), entryIt->getValue());
            }
            ValueType diagVal = -storm::utility::one<ValueType>();
            if (entryIt != entryItEnd && entryIt->getColumn() == rowGroup) {
                diagVal += entryIt->getValue();
                ++entryIt;
            }
            addToConstraint(rowGroup, diagVal);
            for (; entryIt != entryItEnd; ++entryIt) {
                addToConstraint(entryIt->getColumn(), entryIt->getValue());
            }
            solver->addConstraint("", constraint);
        }
    }

    // Invoke optimization
    STORM_LOG_TRACE("Run solver...");
    solver->optimize();
    STORM_LOG_TRACE("...done.");

    bool const infeasible = solver->isInfeasible();
    if (infeasible && (lowerBounds || upperBounds)) {
        // The provied bounds (not the once set for this solver object) are meant as a hint and might thus be wrong. We restart without bounds.
        STORM_LOG_WARN("LP with provided bounds is infeasible. Restarting without bounds.");
        return solveEquationsLp(env, dir, x, b);
    }
    STORM_LOG_THROW(!infeasible, storm::exceptions::UnexpectedException, "The MinMax equation system is infeasible.");
    STORM_LOG_THROW(!solver->isUnbounded(), storm::exceptions::UnexpectedException, "The MinMax equation system is unbounded.");
    STORM_LOG_THROW(solver->isOptimal(), storm::exceptions::UnexpectedException, "Unable to find optimal solution for MinMax equation system.");

    // write the solution into the solution vector
    auto xIt = x.begin();
    VariableIndex i = 0;
    for (; xIt != x.end(); ++xIt, ++i) {
        if (auto findRes = constantRowGroups.find(i); findRes != constantRowGroups.end()) {
            *xIt = findRes->second;
        } else {
            *xIt = solver->getContinuousValue(i);
        }
    }

    // If requested, we store the scheduler for retrieval.
    if (this->isTrackSchedulerSet()) {
        this->schedulerChoices = std::vector<uint_fast64_t>(this->A->getRowGroupCount());
        for (uint64_t rowGroup = 0; rowGroup < this->A->getRowGroupCount(); ++rowGroup) {
            if (!this->choiceFixedForRowGroup || !this->choiceFixedForRowGroup.get()[rowGroup]) {
                // Only update scheduler choice for the states that don't have a fixed choice
                uint64_t row = this->A->getRowGroupIndices()[rowGroup];
                uint64_t optimalChoiceIndex = 0;
                uint64_t currChoice = 0;
                ValueType optimalGroupValue = this->A->multiplyRowWithVector(row, x) + b[row];
                for (++row, ++currChoice; row < this->A->getRowGroupIndices()[rowGroup + 1]; ++row, ++currChoice) {
                    ValueType rowValue = this->A->multiplyRowWithVector(row, x) + b[row];
                    if ((minimize(dir) && rowValue < optimalGroupValue) || (maximize(dir) && rowValue > optimalGroupValue)) {
                        optimalGroupValue = rowValue;
                        optimalChoiceIndex = currChoice;
                    }
                }
                this->schedulerChoices.get()[rowGroup] = optimalChoiceIndex;
            }
        }
    }
    // Reaching this point means that the solution was found.
    return true;
}

template<typename ValueType>
void LpMinMaxLinearEquationSolver<ValueType>::clearCache() const {
    StandardMinMaxLinearEquationSolver<ValueType>::clearCache();
}

template<typename ValueType>
MinMaxLinearEquationSolverRequirements LpMinMaxLinearEquationSolver<ValueType>::getRequirements(
    Environment const& env, boost::optional<storm::solver::OptimizationDirection> const& direction, bool const& hasInitialScheduler) const {
    MinMaxLinearEquationSolverRequirements requirements;

    if (!this->hasUniqueSolution() && (env.solver().minMax().isForceRequireUnique() || this->isTrackSchedulerSet())) {
        requirements.requireUniqueSolution();
    }

    if (env.solver().minMax().lp().getUseNonTrivialBounds()) {
        requirements.requireBounds(false);  // not critical
    }

    if (env.solver().minMax().getMethod() == MinMaxMethod::ViToLp) {
        if (direction) {
            if (minimize(*direction)) {
                requirements.requireUpperBounds(true);
            } else {
                requirements.requireLowerBounds(true);
            }
        } else {
            requirements.requireBounds(true);
        }
    }

    return requirements;
}

template class LpMinMaxLinearEquationSolver<double>;
template class LpMinMaxLinearEquationSolver<storm::RationalNumber>;
}  // namespace solver
}  // namespace storm
