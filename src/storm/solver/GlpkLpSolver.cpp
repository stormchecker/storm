#include "storm/solver/GlpkLpSolver.h"

#include <cmath>

#include "storm/environment/solver/GlpkSolverEnvironment.h"
#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/expressions/BinaryRelationType.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/LinearCoefficientVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

#ifdef STORM_HAVE_GLPK
template<typename ValueType, bool RawMode>
GlpkLpSolver<ValueType, RawMode>::GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug, std::string const& name,
                                               OptimizationDirection const& optDir)
    : LpSolver<ValueType, RawMode>(optDir),
      lp(nullptr),
      variableToIndexMap(),
      modelContainsIntegerVariables(false),
      integerTolerance(glpkSettings.getIntegerTolerance()),
      milpPresolverEnabled(glpkSettings.isMILPPresolverEnabled()),
      isInfeasibleFlag(false),
      isUnboundedFlag(false) {
    // Create the LP problem for glpk.
    lp = glp_create_prob();

    // Set its name and model sense.
    glp_set_prob_name(lp, name.c_str());

    // Set whether the glpk output shall be printed to the command line.
    glp_term_out(debug || glpkSettings.isOutputSet() ? GLP_ON : GLP_OFF);

    // Set the maximal allowed MILP gap to its default value
    glp_iocp* defaultParameters = new glp_iocp();
    glp_init_iocp(defaultParameters);
    this->maxMILPGap = defaultParameters->mip_gap;
    this->maxMILPGapRelative = true;
}

#else

template<typename ValueType, bool RawMode>
GlpkLpSolver<ValueType, RawMode>::GlpkLpSolver(storm::GlpkSolverEnvironment const&, bool, std::string const&, OptimizationDirection const&) {
    // Throw nothing in a constructor.
}
#endif

template<typename ValueType, bool RawMode>
GlpkLpSolver<ValueType, RawMode>::GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug, std::string const& name)
    : GlpkLpSolver(glpkSettings, debug, name, OptimizationDirection::Minimize) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
GlpkLpSolver<ValueType, RawMode>::GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug)
    : GlpkLpSolver(glpkSettings, debug, "", OptimizationDirection::Minimize) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
GlpkLpSolver<ValueType, RawMode>::GlpkLpSolver(storm::GlpkSolverEnvironment const& glpkSettings, bool debug, OptimizationDirection const& optDir)
    : GlpkLpSolver(glpkSettings, debug, "", optDir) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
GlpkLpSolver<ValueType, RawMode>::~GlpkLpSolver() {
#ifdef STORM_HAVE_GLPK
    // Dispose of all objects allocated dynamically by glpk.
    glp_delete_prob(this->lp);
    glp_free_env();
#endif
}

template<typename ValueType, bool RawMode>
int getGlpkType(typename GlpkLpSolver<ValueType, RawMode>::VariableType const& type) {
#ifdef STORM_HAVE_GLPK
    switch (type) {
        case GlpkLpSolver<ValueType, RawMode>::VariableType::Continuous:
            return GLP_CV;
        case GlpkLpSolver<ValueType, RawMode>::VariableType::Integer:
            return GLP_IV;
        case GlpkLpSolver<ValueType, RawMode>::VariableType::Binary:
            return GLP_BV;
    }
    STORM_LOG_ASSERT(false, "Unexpected variable type.");
    return -1;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
typename GlpkLpSolver<ValueType, RawMode>::Variable GlpkLpSolver<ValueType, RawMode>::addVariable(std::string const& name, VariableType const& type,
                                                                                                  std::optional<ValueType> const& lowerBound,
                                                                                                  std::optional<ValueType> const& upperBound,
                                                                                                  ValueType objectiveFunctionCoefficient) {
#ifdef STORM_HAVE_GLPK
    Variable resultVar;
    if constexpr (RawMode) {
        resultVar = variableToIndexMap.size();
    } else {
        resultVar = this->declareOrGetExpressionVariable(name, type);
        //  Assert whether the variable does not exist yet.
        //  Due to incremental usage (push(), pop()), a variable might be declared in the manager but not in the lp model.
        STORM_LOG_ASSERT(variableToIndexMap.count(resultVar) == 0, "Variable " << resultVar.getName() << " exists already in the model.");
    }

    int boundType;
    if (lowerBound.has_value()) {
        boundType = upperBound.has_value() ? GLP_DB : GLP_LO;
    } else {
        boundType = upperBound.has_value() ? GLP_UP : GLP_FR;
    }

    if (type == VariableType::Integer || type == VariableType::Binary) {
        this->modelContainsIntegerVariables = true;
    }

    // Create the variable in glpk.
    int variableIndex = glp_add_cols(this->lp, 1);
    glp_set_col_name(this->lp, variableIndex, name.c_str());
    glp_set_col_bnds(lp, variableIndex, boundType, lowerBound.has_value() ? storm::utility::convertNumber<double>(*lowerBound) : 0.0,
                     upperBound.has_value() ? storm::utility::convertNumber<double>(*upperBound) : 0.0);
    glp_set_col_kind(this->lp, variableIndex, getGlpkType<ValueType, RawMode>(type));
    glp_set_obj_coef(this->lp, variableIndex, storm::utility::convertNumber<double>(objectiveFunctionCoefficient));

    if constexpr (RawMode) {
        this->variableToIndexMap.push_back(variableIndex);
    } else {
        this->variableToIndexMap.emplace(resultVar, variableIndex);
        if (!incrementalData.empty()) {
            incrementalData.back().variables.push_back(resultVar);
        }
    }

    return resultVar;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::update() const {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::addConstraint(std::string const& name, Constraint const& constraint) {
#ifdef STORM_HAVE_GLPK
    // Add the row that will represent this constraint.
    int constraintIndex = glp_add_rows(this->lp, 1);
    glp_set_row_name(this->lp, constraintIndex, name.c_str());

    // Extract constraint data
    double rhs;
    storm::expressions::RelationType relationType;
    // glpk uses 1-based indexing (wtf!?)...
    std::vector<int> variableIndices(1, -1);
    std::vector<double> coefficients(1, 0.0);
    if constexpr (RawMode) {
        rhs = storm::utility::convertNumber<double>(constraint.rhs);
        relationType = constraint.relationType;
        variableIndices.reserve(constraint.lhsVariableIndices.size() + 1);
        for (auto const& var : constraint.lhsVariableIndices) {
            variableIndices.push_back(this->variableToIndexMap.at(var));
        }
        coefficients.reserve(constraint.lhsCoefficients.size() + 1);
        for (auto const& coef : constraint.lhsCoefficients) {
            coefficients.push_back(storm::utility::convertNumber<double>(coef));
        }
    } else {
        STORM_LOG_THROW(constraint.getManager() == this->getManager(), storm::exceptions::InvalidArgumentException,
                        "Constraint was not built over the proper variables.");
        STORM_LOG_THROW(constraint.isRelationalExpression(), storm::exceptions::InvalidArgumentException, "Illegal constraint is not a relational expression.");

        storm::expressions::LinearCoefficientVisitor::VariableCoefficients leftCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(0));
        storm::expressions::LinearCoefficientVisitor::VariableCoefficients rightCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(1));
        leftCoefficients.separateVariablesFromConstantPart(rightCoefficients);
        rhs = rightCoefficients.getConstantPart();
        relationType = constraint.getBaseExpression().asBinaryRelationExpression().getRelationType();
        int len = std::distance(leftCoefficients.begin(), leftCoefficients.end());
        variableIndices.reserve(len + 1);
        coefficients.reserve(len + 1);
        for (auto const& variableCoefficientPair : leftCoefficients) {
            auto variableIndexPair = this->variableToIndexMap.find(variableCoefficientPair.first);
            variableIndices.push_back(variableIndexPair->second);
            coefficients.push_back(variableCoefficientPair.second);
        }
    }

    // Determine the type of the constraint and add it properly.
    switch (relationType) {
        case storm::expressions::RelationType::Less:
            glp_set_row_bnds(this->lp, constraintIndex, GLP_UP, 0, rhs - this->integerTolerance);
            break;
        case storm::expressions::RelationType::LessOrEqual:
            glp_set_row_bnds(this->lp, constraintIndex, GLP_UP, 0, rhs);
            break;
        case storm::expressions::RelationType::Greater:
            glp_set_row_bnds(this->lp, constraintIndex, GLP_LO, rhs + this->integerTolerance, 0);
            break;
        case storm::expressions::RelationType::GreaterOrEqual:
            glp_set_row_bnds(this->lp, constraintIndex, GLP_LO, rhs, 0);
            break;
        case storm::expressions::RelationType::Equal:
            glp_set_row_bnds(this->lp, constraintIndex, GLP_FX, rhs, rhs);
            break;
        default:
            STORM_LOG_ASSERT(false, "Illegal operator in LP solver constraint.");
    }

    // Add the constraints
    glp_set_mat_row(this->lp, constraintIndex, variableIndices.size() - 1, variableIndices.data(), coefficients.data());

    this->currentModelHasBeenOptimized = false;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const&, Variable, bool, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Indicator constraints are not supported for GLPK.");
}

#ifdef STORM_HAVE_GLPK
// Method used within the MIP solver to terminate early
void callback(glp_tree* t, void* info) {
    auto& mipgap = *static_cast<std::pair<double, bool>*>(info);
    double actualRelativeGap = glp_ios_mip_gap(t);
    double factor = storm::utility::one<double>();
    if (!mipgap.second) {
        // Compute absolute gap
        factor = storm::utility::abs(glp_mip_obj_val(glp_ios_get_prob(t))) + DBL_EPSILON;
        assert(factor >= 0.0);
    }
    if (actualRelativeGap * factor <= mipgap.first) {
        // Terminate early
        mipgap.first = actualRelativeGap;
        mipgap.second = true;  // The gap is relative.
        glp_ios_terminate(t);
    }
}
#endif

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::optimize() const {
#ifdef STORM_HAVE_GLPK
    // First, reset the flags.
    this->isInfeasibleFlag = false;
    this->isUnboundedFlag = false;

    // Start by setting the model sense.
    glp_set_obj_dir(this->lp, this->getOptimizationDirection() == OptimizationDirection::Minimize ? GLP_MIN : GLP_MAX);

    int error = 0;
    if (this->modelContainsIntegerVariables) {
        glp_iocp* parameters = new glp_iocp();
        glp_init_iocp(parameters);
        parameters->tol_int = this->integerTolerance;
        this->isInfeasibleFlag = false;
        if (this->milpPresolverEnabled) {
            parameters->presolve = GLP_ON;
        } else {
            // Without presolving, we solve the relaxed model first. This is required because
            // glp_intopt requires that either presolving is enabled or an optimal initial basis is provided.
            error = glp_simplex(this->lp, nullptr);
            STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to optimize relaxed glpk model (" << error << ").");
            // If the relaxed model is already not feasible, we don't have to solve the actual model.
            if (glp_get_status(this->lp) == GLP_INFEAS || glp_get_status(this->lp) == GLP_NOFEAS) {
                this->isInfeasibleFlag = true;
            }
            // If the relaxed model is unbounded, there could still be no feasible integer solution.
            // However, since we can not provide an optimal initial basis, we will need to enable presolving
            if (glp_get_status(this->lp) == GLP_UNBND) {
                parameters->presolve = GLP_ON;
            } else {
                parameters->presolve = GLP_OFF;
            }
        }
        if (!this->isInfeasibleFlag) {
            // Check whether we allow sub-optimal solutions via a non-zero MIP gap.
            // parameters->mip_gap = this->maxMILPGap; (only works for relative values. Also, we need to obtain the actual gap anyway.
            std::pair<double, bool> mipgap(this->maxMILPGap, this->maxMILPGapRelative);
            if (!storm::utility::isZero(this->maxMILPGap)) {
                parameters->cb_func = &callback;
                parameters->cb_info = &mipgap;
            }

            // Invoke mip solving
            error = glp_intopt(this->lp, parameters);
            int status = glp_mip_status(this->lp);
            delete parameters;

            // mipgap.first has been set to the achieved mipgap (either within the callback function or because it has been set to this->maxMILPGap)
            this->actualRelativeMILPGap = mipgap.first;

            // In case the error is caused by an infeasible problem, we do not want to view this as an error and
            // reset the error code.
            if (error == GLP_ENOPFS || status == GLP_NOFEAS) {
                this->isInfeasibleFlag = true;
                error = 0;
            } else if (error == GLP_ENODFS) {
                this->isUnboundedFlag = true;
                error = 0;
            } else if (error == GLP_ESTOP) {
                // Early termination due to achieved MIP Gap. That's fine.
                error = 0;
            } else if (error == GLP_EBOUND) {
                STORM_LOG_THROW(false, storm::exceptions::InvalidStateException,
                                "The bounds of some variables are illegal. Note that glpk only accepts integer bounds for integer variables.");
            }
        }
    } else {
        error = glp_simplex(this->lp, nullptr);
    }

    STORM_LOG_THROW(error == 0, storm::exceptions::InvalidStateException, "Unable to optimize glpk model (" << error << ").");
    this->currentModelHasBeenOptimized = true;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
bool GlpkLpSolver<ValueType, RawMode>::isInfeasible() const {
#ifdef STORM_HAVE_GLPK
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to GlpkLpSolver::isInfeasible: model has not been optimized.");

    if (this->modelContainsIntegerVariables) {
        return isInfeasibleFlag;
    } else {
        return glp_get_status(this->lp) == GLP_INFEAS || glp_get_status(this->lp) == GLP_NOFEAS;
    }
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
bool GlpkLpSolver<ValueType, RawMode>::isUnbounded() const {
#ifdef STORM_HAVE_GLPK
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to GlpkLpSolver::isUnbounded: model has not been optimized.");

    if (this->modelContainsIntegerVariables) {
        return isUnboundedFlag;
    } else {
        return glp_get_status(this->lp) == GLP_UNBND;
    }
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
bool GlpkLpSolver<ValueType, RawMode>::isOptimal() const {
    if (!this->currentModelHasBeenOptimized) {
        return false;
    }

    return !isInfeasible() && !isUnbounded();
}

template<typename ValueType, bool RawMode>
ValueType GlpkLpSolver<ValueType, RawMode>::getContinuousValue(Variable const& variable) const {
#ifdef STORM_HAVE_GLPK
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from infeasible model.");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unbounded model.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unoptimized model.");
    }

    int variableIndex = variableToIndexMap.at(variable);

    double value = 0;
    if (this->modelContainsIntegerVariables) {
        value = glp_mip_col_val(this->lp, static_cast<int>(variableIndex));
    } else {
        value = glp_get_col_prim(this->lp, static_cast<int>(variableIndex));
    }
    return storm::utility::convertNumber<ValueType>(value);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
int64_t GlpkLpSolver<ValueType, RawMode>::getIntegerValue(Variable const& variable) const {
#ifdef STORM_HAVE_GLPK
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from infeasible model.");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unbounded model.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unoptimized model.");
    }

    int variableIndex = variableToIndexMap.at(variable);

    double value = 0;
    if (this->modelContainsIntegerVariables) {
        value = glp_mip_col_val(this->lp, variableIndex);
    } else {
        value = glp_get_col_prim(this->lp, variableIndex);
    }

    double roundedValue = std::round(value);
    double diff = std::abs(roundedValue - value);
    STORM_LOG_ERROR_COND(diff <= this->integerTolerance,
                         "Illegal value for integer variable in GLPK solution (" << value << "). Difference to nearest int is " << diff);
    return static_cast<int_fast64_t>(roundedValue);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
bool GlpkLpSolver<ValueType, RawMode>::getBinaryValue(Variable const& variable) const {
#ifdef STORM_HAVE_GLPK
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from infeasible model.");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unbounded model.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unoptimized model.");
    }

    int variableIndex = variableToIndexMap.at(variable);

    double value = 0;
    if (this->modelContainsIntegerVariables) {
        value = glp_mip_col_val(this->lp, variableIndex);
    } else {
        value = glp_get_col_prim(this->lp, variableIndex);
    }

    if (value > 0.5) {
        STORM_LOG_ERROR_COND(std::abs(value - 1.0) <= this->integerTolerance, "Illegal value for binary variable in GLPK solution (" << value << ").");
        return true;
    } else {
        STORM_LOG_ERROR_COND(std::abs(value) <= this->integerTolerance, "Illegal value for binary variable in GLPK solution (" << value << ").");
        return false;
    }

    return static_cast<bool>(value);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
ValueType GlpkLpSolver<ValueType, RawMode>::getObjectiveValue() const {
#ifdef STORM_HAVE_GLPK
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from infeasible model.");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unbounded model.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get glpk solution from unoptimized model.");
    }

    double value = 0;
    if (this->modelContainsIntegerVariables) {
        value = glp_mip_obj_val(this->lp);
    } else {
        value = glp_get_obj_val(this->lp);
    }

    return storm::utility::convertNumber<ValueType>(value);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::writeModelToFile(std::string const& filename) const {
#ifdef STORM_HAVE_GLPK
    glp_write_lp(this->lp, nullptr, filename.c_str());
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::push() {
#ifdef STORM_HAVE_GLPK
    if constexpr (RawMode) {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Incremental LP solving not supported in raw mode.");
    } else {
        IncrementalLevel lvl;
        lvl.firstConstraintIndex = glp_get_num_rows(this->lp) + 1;
        incrementalData.push_back(lvl);
    }
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::pop() {
#ifdef STORM_HAVE_GLPK
    if constexpr (RawMode) {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Incremental LP solving not supported in raw mode.");
    } else {
        if (incrementalData.empty()) {
            STORM_LOG_ERROR("Tried to pop from a solver without pushing before.");
        } else {
            IncrementalLevel const& lvl = incrementalData.back();
            // Since glpk uses 1-based indexing, we need to prepend an additional index
            std::vector<int> indicesToBeRemoved = storm::utility::vector::buildVectorForRange(lvl.firstConstraintIndex - 1, glp_get_num_rows(this->lp) + 1);
            if (indicesToBeRemoved.size() > 1) {
                glp_del_rows(this->lp, indicesToBeRemoved.size() - 1, indicesToBeRemoved.data());
            }
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
                // Since glpk uses 1-based indexing, we need to prepend an additional index
                std::vector<int> indicesToBeRemoved = storm::utility::vector::buildVectorForRange(firstIndex - 1, glp_get_num_cols(this->lp) + 1);
                glp_del_cols(this->lp, indicesToBeRemoved.size() - 1, indicesToBeRemoved.data());
            }
            incrementalData.pop_back();
            update();
            // Check whether we need to adapt the current basis (i.e. the number of basic variables does not equal the number of constraints)
            int n = glp_get_num_rows(lp);
            int m = glp_get_num_cols(lp);
            int nb(0), mb(0);
            for (int i = 1; i <= n; ++i) {
                if (glp_get_row_stat(lp, i) == GLP_BS) {
                    ++nb;
                }
            }
            for (int j = 1; j <= m; ++j) {
                if (glp_get_col_stat(lp, j) == GLP_BS) {
                    ++mb;
                }
            }
            if (n != (nb + mb)) {
                glp_std_basis(this->lp);
            }
        }
    }
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GLPK. Yet, a method was called that requires this support. Please choose a "
                    "version with GLPK support.");
#endif
}

template<typename ValueType, bool RawMode>
void GlpkLpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const& gap, bool relative) {
    this->maxMILPGap = storm::utility::convertNumber<double>(gap);
    this->maxMILPGapRelative = relative;
}

template<typename ValueType, bool RawMode>
ValueType GlpkLpSolver<ValueType, RawMode>::getMILPGap(bool relative) const {
    STORM_LOG_ASSERT(this->isOptimal(), "Asked for the MILP gap although there is no (bounded) solution.");
    if (relative) {
        return storm::utility::convertNumber<ValueType>(this->actualRelativeMILPGap);
    } else {
        return storm::utility::abs<ValueType>(storm::utility::convertNumber<ValueType>(this->actualRelativeMILPGap) * getObjectiveValue());
    }
}

template class GlpkLpSolver<double, true>;
template class GlpkLpSolver<double, false>;
template class GlpkLpSolver<storm::RationalNumber, true>;
template class GlpkLpSolver<storm::RationalNumber, false>;

}  // namespace solver
}  // namespace storm
