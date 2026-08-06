#pragma once

#include <map>
#include "storm/exceptions/NotImplementedException.h"
#include "storm/solver/LpSolver.h"

#include "storm-config.h"

#ifdef STORM_HAVE_GLPK
#include <glpk.h>
#endif

namespace storm {

class GlpkSolverEnvironment;

namespace solver {
/*!
 * A class that implements the LpSolver interface using glpk as the background solver.
 */
template<typename ValueType, bool RawMode = false>
class GlpkLpSolver : public LpSolver<ValueType, RawMode> {
   public:
    using VariableType = typename LpSolver<ValueType, RawMode>::VariableType;
    using Variable = typename LpSolver<ValueType, RawMode>::Variable;
    using Constant = typename LpSolver<ValueType, RawMode>::Constant;
    using Constraint = typename LpSolver<ValueType, RawMode>::Constraint;

    /*!
     * Constructs a solver with the given name and model sense.
     *
     * @param name The name of the LP problem.
     * @param optDir A value indicating whether the value of the objective function is to be minimized or
     * maximized.
     */
    GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug, std::string const& name, OptimizationDirection const& optDir);

    /*!
     * Constructs a solver with the given name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setOptimizationDirection.
     *
     * @param name The name of the LP problem.
     */
    GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug, std::string const& name);

    /*!
     * Constructs a solver without a name and the given model sense.
     *
     * @param optDir A value indicating whether the value of the objective function is to be minimized or
     * maximized.
     */
    GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug, OptimizationDirection const& optDir);

    /*!
     * Constructs a solver without a name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setOptimizationDirection.
     */
    GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug);

    /*!
     * Destructs a solver by freeing the pointers to glpk's structures.
     */
    virtual ~GlpkLpSolver();

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
    virtual ValueType getContinuousValue(Variable const& name) const override;
    virtual int_fast64_t getIntegerValue(Variable const& name) const override;
    virtual bool getBinaryValue(Variable const& name) const override;
    virtual ValueType getObjectiveValue() const override;

    // Methods to print the LP problem to a file.
    virtual void writeModelToFile(std::string const& filename) const override;

    virtual void push() override;
    virtual void pop() override;

    virtual void setMaximalMILPGap(ValueType const& gap, bool relative) override;
    virtual ValueType getMILPGap(bool relative) const override;

   private:
#ifdef STORM_HAVE_GLPK
    // The glpk LP problem.
    glp_prob* lp;
#endif

    // A mapping from variables to their indices.
    std::conditional_t<RawMode, std::vector<int>, std::map<storm::expressions::Variable, int>> variableToIndexMap;

    // A flag storing whether the model is an LP or an MILP.
    bool modelContainsIntegerVariables;

    // The tolerance used when checking whether integer/binary variables have (near-)integral values.
    double integerTolerance;

    // Whether glpk's built-in MILP presolver should be used.
    bool milpPresolverEnabled;

    // Flags that store whether the MILP was found to be infeasible or unbounded.
    mutable bool isInfeasibleFlag;
    mutable bool isUnboundedFlag;

    mutable double maxMILPGap;
    mutable bool maxMILPGapRelative;
    mutable double actualRelativeMILPGap;

    struct IncrementalLevel {
        std::vector<Variable> variables;
        int firstConstraintIndex;
    };
    std::vector<IncrementalLevel> incrementalData;
};

}  // namespace solver
}  // namespace storm
