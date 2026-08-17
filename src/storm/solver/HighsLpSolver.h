#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "storm/solver/LpSolver.h"
// To detect whether the usage of HiGHS is possible, this include is necessary.
#include "storm-config.h"

#ifdef STORM_HAVE_HIGHS
#include "Highs.h"
#endif

namespace storm {
namespace solver {

/*!
 * A class that implements the LpSolver interface using HiGHS.
 *
 * Note that HiGHS only supports variables and values of type double. In particular, no support for exact arithmetic (storm::RationalNumber) is provided.
 * For exact computations, choose a different solver, e.g., Z3.
 */
template<typename ValueType, bool RawMode = false>
class HighsLpSolver : public LpSolver<ValueType, RawMode> {
   public:
    using VariableType = typename LpSolver<ValueType, RawMode>::VariableType;
    using Variable = typename LpSolver<ValueType, RawMode>::Variable;
    using Constant = typename LpSolver<ValueType, RawMode>::Constant;
    using Constraint = typename LpSolver<ValueType, RawMode>::Constraint;

    /*!
     * Constructs a solver with the given name and model sense.
     *
     * @param name The name of the LP problem.
     * @param optDir A value indicating whether the value of the objective function is to be minimized or maximized.
     */
    HighsLpSolver(std::string const& name, OptimizationDirection const& optDir);

    /*!
     * Constructs a solver with the given name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setOptimizationDirection.
     *
     * @param name The name of the LP problem.
     */
    HighsLpSolver(std::string const& name);

    /*!
     * Constructs a solver without a name and the given model sense.
     *
     * @param optDir A value indicating whether the value of the objective function is to be minimized or maximized.
     */
    HighsLpSolver(OptimizationDirection const& optDir);

    /*!
     * Constructs a solver without a name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setOptimizationDirection.
     */
    HighsLpSolver();

    virtual ~HighsLpSolver();

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
#ifdef STORM_HAVE_HIGHS
    // Translates an internally used (possibly infinite) bound value into the representation HiGHS expects for infinity.
    // Internally, unboundedness is tracked using actual infinity (rather than HiGHS' finite infinity sentinel) so that
    // std::isfinite can be used to detect genuinely unbounded variables, e.g. when computing big-M coefficients.
    double toHighsBound(double value) const;

    // The HiGHS LP/MILP solver instance. HiGHS offers no incremental solving interface, hence we always (re)solve the full model.
    mutable Highs highs;

    // The index of the next variable. In RawMode, this is also the index of the variable in the HiGHS model.
    uint64_t nextVariableIndex;

    // A mapping from variables to their indices. Only used in non-RawMode.
    std::map<storm::expressions::Variable, uint64_t> variableToIndexMap;

    // The bounds (lower, upper) of all variables that have been added so far, indexed by their column index.
    // This is used to compute big-M coefficients for indicator constraints.
    std::vector<std::pair<double, double>> variableBounds;

    // The model status after the last call to optimize().
    mutable HighsModelStatus modelStatus;
#endif
};

}  // namespace solver
}  // namespace storm
