#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "OptimizationDirection.h"

namespace storm {
namespace expressions {
class ExpressionManager;
class Variable;
class Expression;
enum class RelationType;
}  // namespace expressions

namespace solver {

template<typename ValueType>
struct RawLpConstraint {
    using VariableIndexType = uint64_t;

    /*!
     * Creates a RawLpConstraint which represents a linear (in)equality of the form a_1*x_1 + ... + a_n*x_n ~ b
     * @param relationType The relation ~ in the (in)equality
     * @param rhs The right-hand side b of the inequality
     * @param reservedSize An estimate of the number of terms n in the left hand side. Doesn't need to be accurate but helps reserving appropriate memory
     */
    RawLpConstraint(storm::expressions::RelationType relationType, ValueType const& rhs = {}, uint64_t reservedSize = 0);

    /*!
     * Adds the summand 'coefficient * variable' to the left hand side
     * @param variable
     * @param coefficient
     */
    void addToLhs(VariableIndexType const& variable, ValueType const& coefficient);

    std::vector<VariableIndexType> lhsVariableIndices;
    std::vector<ValueType> lhsCoefficients;
    storm::expressions::RelationType relationType;
    ValueType rhs;
};

/*!
 * An interface that captures the functionality of an LP solver.
 * @tparam RawMode: If set, variables are given as indices and constraints as RawConstraints. This avoids overhead introduced by storm::expression's
 */
template<typename ValueType, bool RawMode = false>
class LpSolver {
   public:
    using Variable = std::conditional_t<RawMode, typename RawLpConstraint<ValueType>::VariableIndexType, storm::expressions::Variable>;
    using Constant = std::conditional_t<RawMode, ValueType, storm::expressions::Expression>;
    using Constraint = std::conditional_t<RawMode, RawLpConstraint<ValueType>, storm::expressions::Expression>;

    /*!
     * Enumerates the different types of variables
     */
    enum class VariableType { Continuous, Integer, Binary };

    /*!
     * Creates an empty LP solver. By default the objective function is assumed to be minimized.
     */
    LpSolver();
    /*!
     * Creates an empty LP solver with the given model sense.
     *
     * @param optDir A value indicating whether the objective function of this model is to be minimized or
     * maximized.
     */
    LpSolver(OptimizationDirection const& optDir);

    virtual ~LpSolver() = default;

    /*!
     * Registers an upper- and lower-bounded continuous variable, i.e. a variable that may take all real values
     * within its bounds.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param lowerBound The lower bound of the variable.
     * @param upperBound The upper bound of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addBoundedContinuousVariable(std::string const& name, ValueType lowerBound, ValueType upperBound, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers a lower-bounded continuous variable, i.e. a variable that may take all real values up to its
     * lower bound.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param lowerBound The lower bound of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addLowerBoundedContinuousVariable(std::string const& name, ValueType lowerBound, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers an upper-bounded continuous variable, i.e. a variable that may take all real values up to its
     * upper bound.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param upperBound The upper bound of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addUpperBoundedContinuousVariable(std::string const& name, ValueType upperBound, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers a unbounded continuous variable, i.e. a variable that may take all real values.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addUnboundedContinuousVariable(std::string const& name, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers a continuous variable, i.e. a variable that may take all real values within its bounds (if given).
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param lowerBound The lower bound of the variable (unbounded if not given).
     * @param upperBound The upper bound of the variable (unbounded if not given).
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addContinuousVariable(std::string const& name, std::optional<ValueType> const& lowerBound = std::nullopt,
                                   std::optional<ValueType> const& upperBound = std::nullopt, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers an upper- and lower-bounded integer variable, i.e. a variable that may take all integer values
     * within its bounds.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param lowerBound The lower bound of the variable.
     * @param upperBound The upper bound of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addBoundedIntegerVariable(std::string const& name, ValueType lowerBound, ValueType upperBound, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers a lower-bounded integer variable, i.e. a variable that may take all integer values up to its
     * lower bound.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param lowerBound The lower bound of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addLowerBoundedIntegerVariable(std::string const& name, ValueType lowerBound, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers an upper-bounded integer variable, i.e. a variable that may take all integer values up to its
     * lower bound.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param upperBound The upper bound of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addUpperBoundedIntegerVariable(std::string const& name, ValueType upperBound, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers an unbounded integer variable, i.e. a variable that may take all integer values.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addUnboundedIntegerVariable(std::string const& name, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers an integer variable, i.e. a variable that may take all integer values within its bounds (if given).
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param lowerBound The lower bound of the variable.
     * @param upperBound The upper bound of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addIntegerVariable(std::string const& name, std::optional<ValueType> const& lowerBound = std::nullopt,
                                std::optional<ValueType> const& upperBound = std::nullopt, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers a boolean variable, i.e. a variable that may be either 0 or 1.
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    Variable addBinaryVariable(std::string const& name, ValueType objectiveFunctionCoefficient = 0);

    /*!
     * Registers a variable of the given type
     *
     * @note In RawMode this returns a variable index. Variable indices are assigned consecutively starting at 0, i.e. the n'th variable has index (n-1).
     *
     * @param name The name of the variable.
     * @param type The type of the variable
     * @param lowerBound The optional lower bound
     * @param upperBound The optional upper bound
     * @param objectiveFunctionCoefficient The coefficient with which the variable appears in the objective
     * function. If omitted (or set to zero), the variable is irrelevant for the objective value.
     */
    virtual Variable addVariable(std::string const& name, VariableType const& type, std::optional<ValueType> const& lowerBound = std::nullopt,
                                 std::optional<ValueType> const& upperBound = std::nullopt, ValueType objectiveFunctionCoefficient = 0) = 0;

    /*!
     * Retrieves an expression that characterizes the given constant value.
     * In RawMode, this just returns the given value
     *
     * @param value The value of the constant.
     * @return The resulting expression.
     */
    Constant getConstant(ValueType value) const;

    /*!
     * Updates the model to make the variables that have been declared since the last call to <code>update</code>
     * usable.
     */
    virtual void update() const = 0;

    /*!
     * Adds a the given constraint to the LP problem.
     *
     * @param name The name of the constraint. If empty, the constraint has no name.
     * @param constraint An expression that represents the constraint. The given constraint must be a linear
     * (in)equality over the registered variables.
     */
    virtual void addConstraint(std::string const& name, Constraint const& constraint) = 0;

    /*!
     * Adds the given indicator constraint to the LP problem:
     * "If indicatorVariable == indicatorValue, then constraint"
     *
     * @param name The name of the constraint. If empty, the constraint has no name.
     * @param indicatorVariable must be a registered binary
     * @param indicatorValue
     * @param constraint An expression that represents the constraint. The given constraint must be a linear
     * (in)equality over the registered variables.
     */
    virtual void addIndicatorConstraint(std::string const& name, Variable indicatorVariable, bool indicatorValue, Constraint const& constraint) = 0;

    /*!
     * Optimizes the LP problem previously constructed. Afterwards, the methods isInfeasible, isUnbounded and
     * isOptimal can be used to query the optimality status.
     */
    virtual void optimize() const = 0;

    /*!
     * Retrieves whether the model was found to be infeasible. This can only be called after the model has been
     * optimized and not modified afterwards. Otherwise, an exception is thrown.
     *
     * @return True if the model was optimized and found to be infeasible.
     */
    virtual bool isInfeasible() const = 0;

    /*!
     * Retrieves whether the model was found to be infeasible. This can only be called after the model has been
     * optimized and not modified afterwards. Otherwise, an exception is thrown.
     *
     * @return True if the model was optimized and found to be unbounded.
     */
    virtual bool isUnbounded() const = 0;

    /*!
     * Retrieves whether the model was found to be optimal, i.e. neither infeasible nor unbounded. This can only
     * be called after the model has been optimized and not modified afterwards. Otherwise, an exception is
     * thrown.
     *
     * @return True if the model was optimized and found to be neither infeasible nor unbounded.
     */
    virtual bool isOptimal() const = 0;

    /*!
     * Retrieves the value of the integer variable with the given name. Note that this may only be called, if
     * the model was found to be optimal, i.e. iff isOptimal() returns true.
     *
     * @param variable The variable whose integer value (in the optimal solution) to retrieve.
     * @return The value of the integer variable in the optimal solution.
     */
    virtual int_fast64_t getIntegerValue(Variable const& variable) const = 0;

    /*!
     * Retrieves the value of the binary variable with the given name. Note that this may only be called, if
     * the model was found to be optimal, i.e. iff isOptimal() returns true.
     *
     * @param variable The variable whose integer value (in the optimal solution) to retrieve.
     * @return The value of the binary variable in the optimal solution.
     */
    virtual bool getBinaryValue(Variable const& variable) const = 0;

    /*!
     * Retrieves the value of the continuous variable with the given name. Note that this may only be called,
     * if the model was found to be optimal, i.e. iff isOptimal() returns true.
     *
     * @param variable The variable whose integer value (in the optimal solution) to retrieve.
     * @return The value of the continuous variable in the optimal solution.
     */
    virtual ValueType getContinuousValue(Variable const& variable) const = 0;

    /*!
     * Retrieves the value of the objective function. Note that this may only be called, if the model was found
     * to be optimal,  i.e. iff isOptimal() returns true.
     *
     * @return The value of the objective function in the optimal solution.
     */
    virtual ValueType getObjectiveValue() const = 0;

    /*!
     * Writes the current LP problem to the given file.
     *
     * @param filename The file to which to write the string representation of the LP.
     */
    virtual void writeModelToFile(std::string const& filename) const = 0;

    /*!
     * Sets whether the objective function of this model is to be minimized or maximized.
     *
     * @param modelSense The model sense to use.
     */
    void setOptimizationDirection(OptimizationDirection const& optimizationDirection);

    /*!
     * Retrieves whether the objective function of this model is to be minimized or maximized.
     *
     * @return A value indicating whether the objective function of this model is to be minimized or maximized.
     */
    OptimizationDirection getOptimizationDirection() const;

    /*!
     * Retrieves the manager for the variables created for this solver.
     *
     * @return The manager for the variables created for this solver.
     */
    storm::expressions::ExpressionManager const& getManager() const;

    /*!
     * Pushes a backtracking point on the solver's stack. A following call to pop() deletes exactly those
     * assertions from the solver's stack that were added after this call.
     */
    virtual void push() = 0;

    /*!
     * Pops a backtracking point from the solver's stack. This deletes all assertions from the solver's stack
     * that were added after the last call to push().
     */
    virtual void pop() = 0;

    /*!
     * Specifies the maximum difference between lower- and upper objective bounds that triggers termination.
     * That means a solution is considered optimal if
     * upperBound - lowerBound < gap * (relative ? |upperBound| : 1).
     * Only relevant for programs with integer/boolean variables.
     */
    virtual void setMaximalMILPGap(ValueType const& gap, bool relative) = 0;

    /*!
     * Returns the obtained gap after a call to optimize()
     */
    virtual ValueType getMILPGap(bool relative) const = 0;

   protected:
    storm::expressions::Variable declareOrGetExpressionVariable(std::string const& name, VariableType const& type);

    // The manager responsible for the variables.
    std::shared_ptr<storm::expressions::ExpressionManager> manager;

    // A flag indicating whether the current model has been optimized and not changed afterwards.
    mutable bool currentModelHasBeenOptimized;

   private:
    // A flag that indicates the model sense.
    OptimizationDirection optimizationDirection;
};
}  // namespace solver
}  // namespace storm
