#pragma once

#include <map>
#include <optional>
#include "storm/solver/LpSolver.h"
// To detect whether the usage of Gurobi is possible, this include is neccessary.
#include "storm-config.h"

#ifdef STORM_HAVE_GUROBI
extern "C" {
#include <gurobi_c.h>

int __stdcall GRBislp(GRBenv**, const char*, const char*, const char*, const char*);
}
#endif

namespace storm {

class GurobiSolverEnvironment;

namespace solver {

class GurobiEnvironment {
   public:
    GurobiEnvironment() = default;
    GurobiEnvironment(GurobiEnvironment const&) = delete;
    GurobiEnvironment& operator=(GurobiEnvironment const&) = delete;
    virtual ~GurobiEnvironment();
    /*!
     * Sets some properties of the Gurobi environment according to parameters given by the options.
     */
    void initialize(storm::GurobiSolverEnvironment const& gurobiSettings, bool debug);
    void setOutput(bool set = false);
    double getIntegerTolerance() const;
#ifdef STORM_HAVE_GUROBI
    GRBenv* operator*();
#endif
   private:
    bool initialized = false;
    double integerTolerance = 0.0;
#ifdef STORM_HAVE_GUROBI
    GRBenv* env = nullptr;
#endif
};

/*!
 * A class that implements the LpSolver interface using Gurobi.
 */
template<typename ValueType, bool RawMode = false>
class GurobiLpSolver : public LpSolver<ValueType, RawMode> {
   public:
    using VariableType = typename LpSolver<ValueType, RawMode>::VariableType;
    using Variable = typename LpSolver<ValueType, RawMode>::Variable;
    using Constant = typename LpSolver<ValueType, RawMode>::Constant;
    using Constraint = typename LpSolver<ValueType, RawMode>::Constraint;

    /*!
     * Constructs a solver with the given name and model sense.
     *
     * @param name The name of the LP problem.
     * @param modelSense A value indicating whether the value of the objective function is to be minimized or
     * maximized.
     */
    GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment, std::string const& name, OptimizationDirection const& optDir);

    /*!
     * Constructs a solver with the given name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setModelSense.
     *
     * @param name The name of the LP problem.
     */
    GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment, std::string const& name);

    /*!
     * Constructs a solver without a name and the given model sense.
     *
     * @param modelSense A value indicating whether the value of the objective function is to be minimized or
     * maximized.
     */
    GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment, OptimizationDirection const& optDir);

    /*!
     * Constructs a solver without a name. By default the objective function is assumed to be minimized,
     * but this may be altered later using a call to setModelSense.
     */
    GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment);

    /*!
     * Destructs a solver by freeing the pointers to Gurobi's structures.
     */
    virtual ~GurobiLpSolver();

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

    // Methods to retrieve values of sub-optimal solutions found along the way.
    void setMaximalSolutionCount(uint64_t value);  // How many solutions will be stored (at max)
    uint64_t getSolutionCount() const;             // How many solutions have been found
    ValueType getContinuousValue(Variable const& name, uint64_t const& solutionIndex) const;
    int_fast64_t getIntegerValue(Variable const& name, uint64_t const& solutionIndex) const;
    bool getBinaryValue(Variable const& name, uint64_t const& solutionIndex) const;
    ValueType getObjectiveValue(uint64_t solutionIndex) const;

    // Method for specifying a time limit
    void setTimeLimit(uint64_t seconds);
    // Retrieve the time limit in seconds. Requires that a time limit has been set before.
    uint64_t getTimeLimit();
    // Method for checking whether the model has a time limit
    bool hasTimeLimit();
    // Method for checking whether the optimization has timed out
    bool hasTimedOut();

   private:
#ifdef STORM_HAVE_GUROBI
    // The Gurobi model.
    GRBmodel* model;
#endif

    std::shared_ptr<GurobiEnvironment> environment;

    // The index of the next variable.
    int nextVariableIndex;

    // A mapping from variables to their indices.
    std::map<storm::expressions::Variable, int> variableToIndexMap;

    struct IncrementalLevel {
        std::vector<storm::expressions::Variable> variables;
        // Gurobi considers a different set of indices for linear constraints and general constraints.
        // The term "general constraints" is used by Gurobi to refer to "non-standard" constraints, e.g., indicator constraint.
        int firstConstraintIndex;
        int firstGenConstraintIndex;
    };
    std::vector<IncrementalLevel> incrementalData;

    std::optional<uint64_t> timeLimit;
};

enum class GurobiSolverMethod { AUTOMATIC = -1, PRIMALSIMPLEX = 0, DUALSIMPLEX = 1, BARRIER = 2, CONCURRENT = 3, DETCONCURRENT = 4, DETCONCURRENTSIMPLEX = 5 };

/**
 * Yields a string representation of the GurobiSolverMethod
 * @param method
 * @return
 */
std::string toString(GurobiSolverMethod const& method);
std::optional<GurobiSolverMethod> gurobiSolverMethodFromString(std::string const&);
std::vector<GurobiSolverMethod> getGurobiSolverMethods();

}  // namespace solver
}  // namespace storm
