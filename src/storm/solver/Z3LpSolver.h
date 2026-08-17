#pragma once

#include <map>

#include "storm-config.h"  // To detect whether the usage of Z3 is possible, this include is neccessary.
#include "storm/adapters/Z3ExpressionAdapter.h"
#include "storm/solver/LpSolver.h"
#include "storm/storage/expressions/Expressions.h"

#ifdef STORM_HAVE_Z3
#include <z3++.h>
#include <z3.h>
#endif

namespace storm {
namespace solver {

/*!
 * A class that implements the LpSolver interface using Z3.
 * @note There is no benefit of using this solver in raw mode as it deals with storm::expressions anyway
 */
template<typename ValueType, bool RawMode = false>
class Z3LpSolver : public LpSolver<ValueType, RawMode> {
   public:
    using VariableType = typename LpSolver<ValueType, RawMode>::VariableType;
    using Variable = typename LpSolver<ValueType, RawMode>::Variable;
    using Constant = typename LpSolver<ValueType, RawMode>::Constant;
    using Constraint = typename LpSolver<ValueType, RawMode>::Constraint;
    /*!
     * Constructs a solver with the given name and optimization direction
     *
     * @param name The name of the LP problem.
     * @param optDir A value indicating whether the value of the objective function is to be minimized or
     * maximized.
     */
    Z3LpSolver(std::string const& name, OptimizationDirection const& optDir);

    /*!
     * Constructs a solver with the given name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setOptimizationDirection.
     *
     * @param name The name of the LP problem.
     */
    Z3LpSolver(std::string const& name);

    /*!
     * Constructs a solver without a name and the given optimization direction.
     *
     * @param optDir A value indicating whether the value of the objective function is to be minimized or
     * maximized.
     */
    Z3LpSolver(OptimizationDirection const& optDir);

    /*!
     * Constructs a solver without a name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setModelSense.
     */
    Z3LpSolver();

    /*!
     * Destructs a solver by freeing the pointers to Z3's structures.
     */
    virtual ~Z3LpSolver();

    // Methods to add variables.
    virtual Variable addVariable(std::string const& name, VariableType const& type, std::optional<ValueType> const& lowerBound = std::nullopt,
                                 std::optional<ValueType> const& upperBound = std::nullopt, ValueType objectiveFunctionCoefficient = 0) override;

    // Methods to incorporate recent changes.
    virtual void update() const override;

    // Methods to add constraints
    virtual void addConstraint(std::string const& name, Constraint const& constraint) override;
    virtual void addIndicatorConstraint(std::string const& name, Variable indicatorVariable, bool indicatorValue, Constraint const& constraint) override;

    // Methods to optimize and retrieve optimality status.
    virtual void optimize() const override;
    virtual bool isInfeasible() const override;
    virtual bool isUnbounded() const override;
    virtual bool isOptimal() const override;

    // Methods to retrieve values of variables and the objective function in the optimal solutions.
    virtual ValueType getContinuousValue(Variable const& variable) const override;
    virtual int_fast64_t getIntegerValue(Variable const& variable) const override;
    virtual bool getBinaryValue(Variable const& variable) const override;
    virtual ValueType getObjectiveValue() const override;

    // Methods to print the LP problem to a file.
    virtual void writeModelToFile(std::string const& filename) const override;

    virtual void push() override;
    virtual void pop() override;

    virtual void setMaximalMILPGap(ValueType const& gap, bool relative) override;
    virtual ValueType getMILPGap(bool relative) const override;

   private:
    virtual storm::expressions::Expression getValue(Variable const& variable) const;

#ifdef STORM_HAVE_Z3

    // The context used by the solver.
    std::unique_ptr<z3::context> context;

    // The actual solver object.
    std::unique_ptr<z3::optimize> solver;

    // The results of the most recent check
    mutable bool lastCheckInfeasible;
    mutable bool lastCheckUnbounded;
    mutable std::unique_ptr<z3::expr> lastCheckObjectiveValue;
    mutable std::unique_ptr<z3::model> lastCheckModel;

    // An expression adapter that is used for translating the expression into Z3's format.
    std::unique_ptr<storm::adapters::Z3ExpressionAdapter> expressionAdapter;

    // The function that is to be optimized (we interpret this as a sum)
    std::vector<storm::expressions::Expression> optimizationSummands;

    // Stores whether this solver is used in an incremental way (with push() and pop())
    bool isIncremental;

    // Stores the number of optimization summands at each call of push().
    std::vector<uint64_t> incrementaOptimizationSummandIndicators;

    // allows to map from raw indices to expression variables. Only used in RawMode
    std::vector<storm::expressions::Variable> rawIndexToVariableMap;

#endif
};

}  // namespace solver
}  // namespace storm
