#include "storm/solver/GurobiLpSolver.h"

#include <numeric>

#include "storm/environment/solver/GurobiSolverEnvironment.h"
#include "storm/exceptions/GurobiLicenseException.h"
#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/LinearCoefficientVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

GurobiEnvironment::~GurobiEnvironment() {
#ifdef STORM_HAVE_GUROBI
    if (initialized) {
        GRBfreeenv(env);
    }
#endif
}

#ifdef STORM_HAVE_GUROBI
GRBenv* GurobiEnvironment::operator*() {
    STORM_LOG_ASSERT(initialized, "Gurobi Environment has not been initialized.");
    return env;
}
#endif

void GurobiEnvironment::initialize(storm::GurobiSolverEnvironment const& gurobiSettings, bool debug) {
    integerTolerance = gurobiSettings.getIntegerTolerance();
#ifdef STORM_HAVE_GUROBI
    // Create the environment.
    int error = GRBloadenv(&env, "");
    if (error || env == nullptr) {
        STORM_LOG_THROW(error != 10009, storm::exceptions::GurobiLicenseException,
                        "Gurobi License Issue. Could not initialize Gurobi environment (" << GRBgeterrormsg(env) << ", error code " << error << ").");
        STORM_LOG_THROW(false, storm::exceptions::InvalidStateException,
                        "Could not initialize Gurobi environment (" << GRBgeterrormsg(env) << ", error code " << error << ").");
    }
    setOutput(debug || gurobiSettings.isOutputSet());

    error = GRBsetintparam(env, "Method", static_cast<int>(gurobiSettings.getMethod()));
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi Parameter Method (" << GRBgeterrormsg(env) << ", error code " << error << ").");

    // Enable the following line to restrict Gurobi to one thread only.
    error = GRBsetintparam(env, "Threads", gurobiSettings.getNumberOfThreads());
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi Parameter Threads (" << GRBgeterrormsg(env) << ", error code " << error << ").");

    error = GRBsetintparam(env, "MIPFocus", gurobiSettings.getMIPFocus());
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi Parameter MIPFocus (" << GRBgeterrormsg(env) << ", error code " << error << ").");

    error = GRBsetintparam(env, "ConcurrentMIP", gurobiSettings.getNumberOfConcurrentMipThreads());
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi Parameter ConcurrentMIP (" << GRBgeterrormsg(env) << ", error code " << error << ").");

    // Enable the following line to force Gurobi to be as precise about the binary variables as required by the given precision option.
    error = GRBsetdblparam(env, "IntFeasTol", gurobiSettings.getIntegerTolerance());
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi Parameter IntFeasTol (" << GRBgeterrormsg(env) << ", error code " << error << ").");
#endif
    initialized = true;
}

double GurobiEnvironment::getIntegerTolerance() const {
    return integerTolerance;
}

void GurobiEnvironment::setOutput(bool set) {
#ifdef STORM_HAVE_GUROBI
    int error = GRBsetintparam(env, "OutputFlag", set);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi Parameter OutputFlag (" << GRBgeterrormsg(env) << ", error code " << error << ").");
#else
    (void)set;
#endif
}

#ifdef STORM_HAVE_GUROBI

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment, std::string const& name,
                                                   OptimizationDirection const& optDir)
    : LpSolver<ValueType, RawMode>(optDir), model(nullptr), environment(environment), nextVariableIndex(0) {
    // Create the model.
    int error = 0;
    error = GRBnewmodel(**environment, &model, name.c_str(), 0, nullptr, nullptr, nullptr, nullptr, nullptr);
    STORM_LOG_THROW(!error, storm::exceptions::InvalidStateException,
                    "Could not initialize Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment, std::string const& name)
    : GurobiLpSolver(environment, name, OptimizationDirection::Minimize) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment, OptimizationDirection const& optDir)
    : GurobiLpSolver(environment, "", optDir) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const& environment)
    : GurobiLpSolver(environment, "", OptimizationDirection::Minimize) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::~GurobiLpSolver() {
    // Dispose of the objects allocated inside Gurobi.
    GRBfreemodel(model);
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::update() const {
    int error = GRBupdatemodel(model);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to update Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    // Since the model changed, we erase the optimality flag.
    this->currentModelHasBeenOptimized = false;
}

template<typename ValueType, bool RawMode>
char getGurobiType(typename GurobiLpSolver<ValueType, RawMode>::VariableType const& type) {
    switch (type) {
        case GurobiLpSolver<ValueType, RawMode>::VariableType::Continuous:
            return GRB_CONTINUOUS;
        case GurobiLpSolver<ValueType, RawMode>::VariableType::Integer:
            return GRB_INTEGER;
        case GurobiLpSolver<ValueType, RawMode>::VariableType::Binary:
            return GRB_BINARY;
    }
    STORM_LOG_ASSERT(false, "Unexpected variable type.");
    return -1;
}

template<typename ValueType, bool RawMode>
typename GurobiLpSolver<ValueType, RawMode>::Variable GurobiLpSolver<ValueType, RawMode>::addVariable(std::string const& name, VariableType const& type,
                                                                                                      std::optional<ValueType> const& lowerBound,
                                                                                                      std::optional<ValueType> const& upperBound,
                                                                                                      ValueType objectiveFunctionCoefficient) {
    Variable resultVar;
    if constexpr (RawMode) {
        resultVar = nextVariableIndex;
    } else {
        resultVar = this->declareOrGetExpressionVariable(name, type);
        //  Assert whether the variable does not exist yet.
        //  Due to incremental usage (push(), pop()), a variable might be declared in the manager but not in the lp model.
        STORM_LOG_ASSERT(variableToIndexMap.count(resultVar) == 0, "Variable " << resultVar.getName() << " exists already in the model.");
        this->variableToIndexMap.emplace(resultVar, nextVariableIndex);
        if (!incrementalData.empty()) {
            incrementalData.back().variables.push_back(resultVar);
        }
    }
    ++nextVariableIndex;

    // Create the actual variable.
    int error = 0;
    error = GRBaddvar(model, 0, nullptr, nullptr, storm::utility::convertNumber<double>(objectiveFunctionCoefficient),
                      lowerBound.has_value() ? storm::utility::convertNumber<double>(*lowerBound) : -GRB_INFINITY,
                      upperBound.has_value() ? storm::utility::convertNumber<double>(*upperBound) : GRB_INFINITY, getGurobiType<ValueType, RawMode>(type),
                      name.c_str());
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Could not create binary Gurobi variable (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    return resultVar;
}

struct GurobiConstraint {
    std::vector<int> variableIndices;
    std::vector<double> coefficients;
    char sense;
    double rhs;
};

template<typename ValueType, bool RawMode>
GurobiConstraint createConstraint(typename GurobiLpSolver<ValueType, RawMode>::Constraint const& constraint,
                                  std::map<storm::expressions::Variable, int> const& variableToIndexMap, double integerTolerance) {
    GurobiConstraint gurobiConstraint;
    storm::expressions::RelationType relationType;
    if constexpr (RawMode) {
        gurobiConstraint.rhs = storm::utility::convertNumber<double>(constraint.rhs);
        relationType = constraint.relationType;
        gurobiConstraint.variableIndices.insert(gurobiConstraint.variableIndices.end(), constraint.lhsVariableIndices.begin(),
                                                constraint.lhsVariableIndices.end());
        gurobiConstraint.coefficients.reserve(constraint.lhsCoefficients.size());
        for (auto const& coef : constraint.lhsCoefficients) {
            gurobiConstraint.coefficients.push_back(storm::utility::convertNumber<double>(coef));
        }
    } else {
        STORM_LOG_THROW(constraint.isRelationalExpression(), storm::exceptions::InvalidArgumentException, "Illegal constraint is not a relational expression.");
        storm::expressions::LinearCoefficientVisitor::VariableCoefficients leftCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(0));
        storm::expressions::LinearCoefficientVisitor::VariableCoefficients rightCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(1));
        leftCoefficients.separateVariablesFromConstantPart(rightCoefficients);
        gurobiConstraint.rhs = rightCoefficients.getConstantPart();
        relationType = constraint.getBaseExpression().asBinaryRelationExpression().getRelationType();
        int len = std::distance(leftCoefficients.begin(), leftCoefficients.end());
        gurobiConstraint.variableIndices.reserve(len);
        gurobiConstraint.coefficients.reserve(len);
        for (auto const& variableCoefficientPair : leftCoefficients) {
            auto variableIndexPair = variableToIndexMap.find(variableCoefficientPair.first);
            gurobiConstraint.variableIndices.push_back(variableIndexPair->second);
            gurobiConstraint.coefficients.push_back(variableCoefficientPair.second);
        }
    }
    // Determine the type of the constraint and add it properly.
    switch (relationType) {
        case storm::expressions::RelationType::Less:
            gurobiConstraint.sense = GRB_LESS_EQUAL;
            gurobiConstraint.rhs -= integerTolerance;
            break;
        case storm::expressions::RelationType::LessOrEqual:
            gurobiConstraint.sense = GRB_LESS_EQUAL;
            break;
        case storm::expressions::RelationType::Greater:
            gurobiConstraint.sense = GRB_GREATER_EQUAL;
            gurobiConstraint.rhs += integerTolerance;
            break;
        case storm::expressions::RelationType::GreaterOrEqual:
            gurobiConstraint.sense = GRB_GREATER_EQUAL;
            break;
        case storm::expressions::RelationType::Equal:
            gurobiConstraint.sense = GRB_EQUAL;
            break;
        default:
            STORM_LOG_ASSERT(false, "Illegal operator in LP solver constraint.");
    }
    return gurobiConstraint;
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::addConstraint(std::string const& name, Constraint const& constraint) {
    if constexpr (!RawMode) {
        STORM_LOG_TRACE("Adding constraint " << name << " to GurobiLpSolver:\n"
                                             << "\t" << constraint);
        STORM_LOG_ASSERT(constraint.getManager() == this->getManager(), "Constraint was not built over the proper variables.");
    }

    // Extract constraint data
    auto grbConstr = createConstraint<ValueType, RawMode>(constraint, this->variableToIndexMap, environment->getIntegerTolerance());
    int error = GRBaddconstr(model, grbConstr.variableIndices.size(), grbConstr.variableIndices.data(), grbConstr.coefficients.data(), grbConstr.sense,
                             grbConstr.rhs, name == "" ? nullptr : name.c_str());
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Could not assert constraint (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const& name, Variable indicatorVariable, bool indicatorValue,
                                                                Constraint const& constraint) {
    int indVar;
    // Extract constraint data
    if constexpr (RawMode) {
        indVar = indicatorVariable;
    } else {
        STORM_LOG_ASSERT(this->variableToIndexMap.count(indicatorVariable) > 0, "Indicator Variable " << indicatorVariable.getName() << " unknown to solver.");
        STORM_LOG_ASSERT(indicatorVariable.hasIntegerType(), "Indicator Variable " << indicatorVariable.getName() << " has unexpected type.");
        STORM_LOG_ASSERT(constraint.getManager() == this->getManager(), "Constraint was not built over the proper variables.");
        STORM_LOG_TRACE("Adding Indicator constraint " << name << " to GurobiLpSolver:\n"
                                                       << "\t(" << indicatorVariable.getName() << "==" << indicatorValue << ") implies " << constraint);
        indVar = this->variableToIndexMap.at(indicatorVariable);
    }
    int indVal = indicatorValue ? 1 : 0;
    auto grbConstr = createConstraint<ValueType, RawMode>(constraint, this->variableToIndexMap, environment->getIntegerTolerance());
    // Gurobi considers indicator constraints as a certain kind of what they call "general constraints".
    int error = GRBaddgenconstrIndicator(model, name == "" ? nullptr : name.c_str(), indVar, indVal, grbConstr.variableIndices.size(),
                                         grbConstr.variableIndices.data(), grbConstr.coefficients.data(), grbConstr.sense, grbConstr.rhs);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Could not assert constraint (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::optimize() const {
    // First incorporate all recent changes.
    this->update();

    // Set the most recently set model sense.
    int error = GRBsetintattr(model, "ModelSense", this->getOptimizationDirection() == OptimizationDirection::Minimize ? 1 : -1);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi model sense (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    // Then we actually optimize the model.
    error = GRBoptimize(model);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to optimize Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    this->currentModelHasBeenOptimized = true;
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to GurobiLpSolver<ValueType, RawMode>::isInfeasible: model has not been optimized.");

    int optimalityStatus = 0;

    int error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    // By default, Gurobi may tell us only that the model is either infeasible or unbounded. To decide which one
    // it is, we need to perform an extra step.
    if (optimalityStatus == GRB_INF_OR_UNBD) {
        error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 0);
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to set Gurobi parameter (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

        this->optimize();

        error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

        error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 1);
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to set Gurobi parameter (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    }

    return optimalityStatus == GRB_INFEASIBLE;
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to GurobiLpSolver<ValueType, RawMode>::isUnbounded: model has not been optimized.");

    int optimalityStatus = 0;

    int error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    // By default, Gurobi may tell us only that the model is either infeasible or unbounded. To decide which one
    // it is, we need to perform an extra step.
    if (optimalityStatus == GRB_INF_OR_UNBD) {
        error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 0);
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to set Gurobi parameter (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

        this->optimize();

        error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

        error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_DUALREDUCTIONS, 1);
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to set Gurobi parameter (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    }

    return optimalityStatus == GRB_UNBOUNDED;
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::isOptimal() const {
    if (!this->currentModelHasBeenOptimized) {
        return false;
    }
    int optimalityStatus = 0;

    int error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimalityStatus);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    return optimalityStatus == GRB_OPTIMAL;
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getContinuousValue(Variable const& variable) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }

    int varIndex;
    if constexpr (RawMode) {
        varIndex = variable;
    } else {
        STORM_LOG_ASSERT(variableToIndexMap.count(variable) != 0, "Accessing value of unknown variable '" << variable.getName() << "'.");
        varIndex = variableToIndexMap.at(variable);
    }

    double value = 0;
    int error = GRBgetdblattrelement(model, GRB_DBL_ATTR_X, varIndex, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    return storm::utility::convertNumber<ValueType>(value);
}

template<typename ValueType, bool RawMode>
int_fast64_t GurobiLpSolver<ValueType, RawMode>::getIntegerValue(Variable const& variable) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }

    int varIndex;
    if constexpr (RawMode) {
        varIndex = variable;
    } else {
        STORM_LOG_ASSERT(variableToIndexMap.count(variable) != 0, "Accessing value of unknown variable '" << variable.getName() << "'.");
        varIndex = variableToIndexMap.at(variable);
    }

    double value = 0;
    int error = GRBgetdblattrelement(model, GRB_DBL_ATTR_X, varIndex, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    double roundedValue = std::round(value);
    double diff = std::abs(roundedValue - value);
    STORM_LOG_ERROR_COND(diff <= environment->getIntegerTolerance(),
                         "Illegal value for integer variable in Gurobi solution (" << value << "). Difference to nearest int is " << diff);
    return static_cast<int_fast64_t>(roundedValue);
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::getBinaryValue(Variable const& variable) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }

    int varIndex;
    if constexpr (RawMode) {
        varIndex = variable;
    } else {
        STORM_LOG_ASSERT(variableToIndexMap.count(variable) != 0, "Accessing value of unknown variable '" << variable.getName() << "'.");
        varIndex = variableToIndexMap.at(variable);
    }

    double value = 0;
    int error = GRBgetdblattrelement(model, GRB_DBL_ATTR_X, varIndex, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    if (value > 0.5) {
        STORM_LOG_ERROR_COND(std::abs(value - 1.0) <= environment->getIntegerTolerance(),
                             "Illegal value for binary variable in Gurobi solution (" << value << ").");
        return true;
    } else {
        STORM_LOG_ERROR_COND(std::abs(value) <= environment->getIntegerTolerance(), "Illegal value for binary variable in Gurobi solution (" << value << ").");
        return false;
    }
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getObjectiveValue() const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from infeasible model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unbounded model (" << GRBgeterrormsg(**environment) << ").");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }

    double value = 0;
    int error = GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    return storm::utility::convertNumber<ValueType>(value);
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::writeModelToFile(std::string const& filename) const {
    int error = GRBwrite(model, filename.c_str());
    STORM_LOG_THROW(!error, storm::exceptions::InvalidStateException,
                    "Unable to write Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ") to file.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::push() {
    IncrementalLevel lvl;
    int num;
    GRBgetintattr(model, GRB_INT_ATTR_NUMCONSTRS, &num);
    lvl.firstConstraintIndex = num;
    GRBgetintattr(model, GRB_INT_ATTR_NUMGENCONSTRS, &num);
    lvl.firstGenConstraintIndex = num;
    incrementalData.push_back(lvl);
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::pop() {
    if (incrementalData.empty()) {
        STORM_LOG_ERROR("Tried to pop from a solver without pushing before.");
    } else {
        IncrementalLevel const& lvl = incrementalData.back();
        int num;
        GRBgetintattr(model, GRB_INT_ATTR_NUMCONSTRS, &num);
        std::vector<int> indicesToBeRemoved = storm::utility::vector::buildVectorForRange(lvl.firstConstraintIndex, num);
        int error = GRBdelconstrs(model, indicesToBeRemoved.size(), indicesToBeRemoved.data());
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to delete constraints (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
        GRBgetintattr(model, GRB_INT_ATTR_NUMGENCONSTRS, &num);
        indicesToBeRemoved = storm::utility::vector::buildVectorForRange(lvl.firstGenConstraintIndex, num);
        STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                        "Unable to delete general constraints (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
        indicesToBeRemoved.clear();

        if (!lvl.variables.empty()) {
            int firstIndex = -1;
            bool first = true;
            for (auto const& var : lvl.variables) {
                if (first) {
                    auto it = variableToIndexMap.find(var);
                    firstIndex = it->second;
                    variableToIndexMap.erase(it);
                    first = false;
                } else {
                    variableToIndexMap.erase(var);
                }
            }
            std::vector<int> indicesToBeRemoved = storm::utility::vector::buildVectorForRange(firstIndex, nextVariableIndex);
            error = GRBdelvars(model, indicesToBeRemoved.size(), indicesToBeRemoved.data());
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                            "Unable to delete variables (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
            nextVariableIndex = firstIndex;
        }
        update();
        // Assert that indices are as expected
        GRBgetintattr(model, GRB_INT_ATTR_NUMCONSTRS, &num);
        STORM_LOG_THROW(lvl.firstConstraintIndex == num, storm::exceptions::InvalidStateException, "Unexpected number of constraints after deletion.");
        GRBgetintattr(model, GRB_INT_ATTR_NUMGENCONSTRS, &num);
        STORM_LOG_THROW(lvl.firstGenConstraintIndex == num, storm::exceptions::InvalidStateException,
                        "Unexpected number of general constraints after deletion.");
        GRBgetintattr(model, GRB_INT_ATTR_NUMVARS, &num);
        STORM_LOG_THROW(nextVariableIndex == num, storm::exceptions::InvalidStateException, "Unexpected number ofvariables after deletion.");

        incrementalData.pop_back();
    }
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::setMaximalSolutionCount(uint64_t value) {
    int error = GRBsetintparam(GRBgetenv(model), "PoolSolutions", value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi Parameter PoolSolutions (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
}

template<typename ValueType, bool RawMode>
uint64_t GurobiLpSolver<ValueType, RawMode>::getSolutionCount() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to GurobiLpSolver<ValueType, RawMode>::getSolutionCount: model has not been optimized.");
    int result = -1;
    int error = GRBgetintattr(model, "SolCount", &result);
    STORM_LOG_THROW(error == 0 && result >= 0, storm::exceptions::InvalidStateException, "Unable to get solution count or invalid number of solutions.");
    return result;
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getContinuousValue(Variable const& variable, uint64_t const& solutionIndex) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }
    STORM_LOG_ASSERT(solutionIndex < getSolutionCount(), "Invalid solution index.");

    int varIndex;
    if constexpr (RawMode) {
        varIndex = variable;
    } else {
        STORM_LOG_THROW(variableToIndexMap.count(variable) != 0, storm::exceptions::InvalidAccessException,
                        "Accessing value of unknown variable '" << variable.getName() << "'.");
        varIndex = variableToIndexMap.at(variable);
    }

    double value = 0;
    int error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_SOLUTIONNUMBER, solutionIndex);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi solution index (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    error = GRBgetdblattrelement(model, GRB_DBL_ATTR_Xn, varIndex, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    return storm::utility::convertNumber<ValueType>(value);
}

template<typename ValueType, bool RawMode>
int_fast64_t GurobiLpSolver<ValueType, RawMode>::getIntegerValue(Variable const& variable, uint64_t const& solutionIndex) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }
    STORM_LOG_ASSERT(solutionIndex < getSolutionCount(), "Invalid solution index.");

    int varIndex;
    if constexpr (RawMode) {
        varIndex = variable;
    } else {
        STORM_LOG_THROW(variableToIndexMap.count(variable) != 0, storm::exceptions::InvalidAccessException,
                        "Accessing value of unknown variable '" << variable.getName() << "'.");
        varIndex = variableToIndexMap.at(variable);
    }

    double value = 0;
    int error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_SOLUTIONNUMBER, solutionIndex);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi solution index (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    error = GRBgetdblattrelement(model, GRB_DBL_ATTR_Xn, varIndex, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    double roundedValue = std::round(value);
    double diff = std::abs(roundedValue - value);
    STORM_LOG_ERROR_COND(diff <= environment->getIntegerTolerance(),
                         "Illegal value for integer variable in Gurobi solution (" << value << "). Difference to nearest int is " << diff);
    return static_cast<int_fast64_t>(roundedValue);
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::getBinaryValue(Variable const& variable, uint64_t const& solutionIndex) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }
    STORM_LOG_ASSERT(solutionIndex < getSolutionCount(), "Invalid solution index.");

    int varIndex;
    if constexpr (RawMode) {
        varIndex = variable;
    } else {
        STORM_LOG_THROW(variableToIndexMap.count(variable) != 0, storm::exceptions::InvalidAccessException,
                        "Accessing value of unknown variable '" << variable.getName() << "'.");
        varIndex = variableToIndexMap.at(variable);
    }

    double value = 0;
    int error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_SOLUTIONNUMBER, solutionIndex);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi solution index (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    error = GRBgetdblattrelement(model, GRB_DBL_ATTR_Xn, varIndex, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    if (value > 0.5) {
        STORM_LOG_ERROR_COND(std::abs(value - 1) <= environment->getIntegerTolerance(),
                             "Illegal value for integer variable in Gurobi solution (" << value << ").");
        return true;
    } else {
        STORM_LOG_ERROR_COND(std::abs(value) <= environment->getIntegerTolerance(), "Illegal value for integer variable in Gurobi solution (" << value << ").");
        return false;
    }
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getObjectiveValue(uint64_t solutionIndex) const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidAccessException,
                        "Unable to get Gurobi solution from unoptimized model (" << GRBgeterrormsg(**environment) << ").");
    }
    STORM_LOG_ASSERT(solutionIndex < getSolutionCount(), "Invalid solution index.");

    double value = 0;
    int error = GRBsetintparam(GRBgetenv(model), GRB_INT_PAR_SOLUTIONNUMBER, solutionIndex);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi solution index (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    error = GRBgetdblattr(model, GRB_DBL_ATTR_POOLOBJVAL, &value);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi solution (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    return storm::utility::convertNumber<ValueType>(value);
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const& gap, bool relative) {
    int error = -1;
    if (relative) {
        error = GRBsetdblparam(GRBgetenv(model), GRB_DBL_PAR_MIPGAP, storm::utility::convertNumber<double>(gap));
    } else {
        error = GRBsetdblparam(GRBgetenv(model), GRB_DBL_PAR_MIPGAPABS, storm::utility::convertNumber<double>(gap));
    }
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi MILP GAP (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getMILPGap(bool relative) const {
    double relativeGap;
    int error = GRBgetdblattr(model, GRB_DBL_ATTR_MIPGAP, &relativeGap);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to get Gurobi MILP GAP (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
    auto result = storm::utility::convertNumber<ValueType>(relativeGap);
    if (relative) {
        return result;
    } else {
        return storm::utility::abs<ValueType>(result * getObjectiveValue());
    }
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::setTimeLimit(uint64_t seconds) {
    int error = GRBsetdblparam(GRBgetenv(model), GRB_DBL_PAR_TIMELIMIT, seconds);
    timeLimit.emplace(seconds);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to set Gurobi time limit (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");
}

template<typename ValueType, bool RawMode>
uint64_t GurobiLpSolver<ValueType, RawMode>::getTimeLimit() {
    STORM_LOG_THROW(timeLimit.has_value(), storm::exceptions::InvalidAccessException, "Unable to get Gurobi time limit because none was specified.");
    return timeLimit.value();
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::hasTimeLimit() {
    return timeLimit.has_value();
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::hasTimedOut() {
    if (!this->currentModelHasBeenOptimized) {
        return false;
    }
    int status = 0;

    int error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &status);
    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException,
                    "Unable to retrieve optimization status of Gurobi model (" << GRBgeterrormsg(**environment) << ", error code " << error << ").");

    return status == GRB_TIME_LIMIT;
}

#else
template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const&, std::string const&, OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const&, std::string const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const&, OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::GurobiLpSolver(std::shared_ptr<GurobiEnvironment> const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
GurobiLpSolver<ValueType, RawMode>::~GurobiLpSolver() {}

template<typename ValueType, bool RawMode>
typename GurobiLpSolver<ValueType, RawMode>::Variable GurobiLpSolver<ValueType, RawMode>::addVariable(std::string const&, VariableType const&,
                                                                                                      std::optional<ValueType> const&,
                                                                                                      std::optional<ValueType> const&, ValueType) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::update() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::addConstraint(std::string const&, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const&, Variable, bool, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::optimize() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::isOptimal() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getContinuousValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
int_fast64_t GurobiLpSolver<ValueType, RawMode>::getIntegerValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::getBinaryValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getObjectiveValue() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::writeModelToFile(std::string const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::push() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::pop() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::setMaximalSolutionCount(uint64_t) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
uint64_t GurobiLpSolver<ValueType, RawMode>::getSolutionCount() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getContinuousValue(Variable const&, uint64_t const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
int_fast64_t GurobiLpSolver<ValueType, RawMode>::getIntegerValue(Variable const&, uint64_t const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::getBinaryValue(Variable const&, uint64_t const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getObjectiveValue(uint64_t) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const&, bool) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
ValueType GurobiLpSolver<ValueType, RawMode>::getMILPGap(bool) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
void GurobiLpSolver<ValueType, RawMode>::setTimeLimit(uint64_t) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
uint64_t GurobiLpSolver<ValueType, RawMode>::getTimeLimit() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::hasTimeLimit() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

template<typename ValueType, bool RawMode>
bool GurobiLpSolver<ValueType, RawMode>::hasTimedOut() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Gurobi. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with Gurobi support.");
}

#endif

std::string toString(GurobiSolverMethod const& method) {
    switch (method) {
        case GurobiSolverMethod::AUTOMATIC:
            return "auto";
        case GurobiSolverMethod::PRIMALSIMPLEX:
            return "primal-simplex";
        case GurobiSolverMethod::DUALSIMPLEX:
            return "dual-simplex";
        case GurobiSolverMethod::BARRIER:
            return "barrier";
        case GurobiSolverMethod::CONCURRENT:
            return "concurrent";
        case GurobiSolverMethod::DETCONCURRENT:
            return "deterministic-concurrent";
        case GurobiSolverMethod::DETCONCURRENTSIMPLEX:
            return "deterministic-concurrent-simplex";
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unknown solver method.");
}

std::optional<GurobiSolverMethod> gurobiSolverMethodFromString(std::string const& method) {
    for (auto const& mt : getGurobiSolverMethods()) {
        if (toString(mt) == method) {
            return mt;
        }
    }
    return {};
}

std::vector<GurobiSolverMethod> getGurobiSolverMethods() {
    return {GurobiSolverMethod::AUTOMATIC,  GurobiSolverMethod::PRIMALSIMPLEX, GurobiSolverMethod::DUALSIMPLEX,         GurobiSolverMethod::BARRIER,
            GurobiSolverMethod::CONCURRENT, GurobiSolverMethod::DETCONCURRENT, GurobiSolverMethod::DETCONCURRENTSIMPLEX};
}

template class GurobiLpSolver<double, true>;
template class GurobiLpSolver<double, false>;
template class GurobiLpSolver<storm::RationalNumber, true>;
template class GurobiLpSolver<storm::RationalNumber, false>;
}  // namespace solver
}  // namespace storm
