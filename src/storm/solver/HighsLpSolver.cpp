#include "storm/solver/HighsLpSolver.h"

#include <cmath>
#include <limits>

#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/expressions/BinaryRelationType.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/LinearCoefficientVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

#ifdef STORM_HAVE_HIGHS

namespace {

constexpr double highInfinity = std::numeric_limits<double>::infinity();

struct HighsConstraintData {
    std::vector<HighsInt> variableIndices;
    std::vector<double> coefficients;
    double rhs;
    storm::expressions::RelationType relationType;
};

template<typename ValueType, bool RawMode>
HighsConstraintData createConstraintData(typename HighsLpSolver<ValueType, RawMode>::Constraint const& constraint,
                                         std::map<storm::expressions::Variable, uint64_t> const& variableToIndexMap) {
    HighsConstraintData result;
    if constexpr (RawMode) {
        result.rhs = storm::utility::convertNumber<double>(constraint.rhs);
        result.relationType = constraint.relationType;
        result.variableIndices.reserve(constraint.lhsVariableIndices.size());
        result.coefficients.reserve(constraint.lhsCoefficients.size());
        for (auto const& variable : constraint.lhsVariableIndices) {
            result.variableIndices.push_back(static_cast<HighsInt>(variable));
        }
        for (auto const& coefficient : constraint.lhsCoefficients) {
            result.coefficients.push_back(storm::utility::convertNumber<double>(coefficient));
        }
    } else {
        STORM_LOG_THROW(constraint.isRelationalExpression(), storm::exceptions::InvalidArgumentException, "Illegal constraint is not a relational expression.");
        storm::expressions::LinearCoefficientVisitor::VariableCoefficients leftCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(0));
        storm::expressions::LinearCoefficientVisitor::VariableCoefficients rightCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(1));
        leftCoefficients.separateVariablesFromConstantPart(rightCoefficients);
        result.rhs = storm::utility::convertNumber<double>(rightCoefficients.getConstantPart());
        result.relationType = constraint.getBaseExpression().asBinaryRelationExpression().getRelationType();
        result.variableIndices.reserve(leftCoefficients.size());
        result.coefficients.reserve(leftCoefficients.size());
        for (auto const& variableCoefficientPair : leftCoefficients) {
            auto variableIndexPair = variableToIndexMap.find(variableCoefficientPair.first);
            result.variableIndices.push_back(static_cast<HighsInt>(variableIndexPair->second));
            result.coefficients.push_back(storm::utility::convertNumber<double>(variableCoefficientPair.second));
        }
    }
    return result;
}

}  // namespace

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver(std::string const&, OptimizationDirection const& optDir)
    : LpSolver<ValueType, RawMode>(optDir), nextVariableIndex(0) {
    // By default, HiGHS prints its log to the command line. We disable this as storm provides its own logging.
    highs.setOptionValue("output_flag", false);
}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver(std::string const& name) : HighsLpSolver(name, OptimizationDirection::Minimize) {}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver(OptimizationDirection const& optDir) : HighsLpSolver("", optDir) {}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver() : HighsLpSolver("", OptimizationDirection::Minimize) {}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::~HighsLpSolver() {}

template<typename ValueType, bool RawMode>
typename HighsLpSolver<ValueType, RawMode>::Variable HighsLpSolver<ValueType, RawMode>::addVariable(std::string const& name, VariableType const& type,
                                                                                                    std::optional<ValueType> const& lowerBound,
                                                                                                    std::optional<ValueType> const& upperBound,
                                                                                                    ValueType objectiveFunctionCoefficient) {
    Variable resultVar;
    if constexpr (RawMode) {
        resultVar = nextVariableIndex;
    } else {
        resultVar = this->declareOrGetExpressionVariable(name, type);
        STORM_LOG_ASSERT(variableToIndexMap.count(resultVar) == 0, "Variable " << resultVar.getName() << " exists already in the model.");
        this->variableToIndexMap.emplace(resultVar, nextVariableIndex);
    }

    double lower = lowerBound.has_value() ? storm::utility::convertNumber<double>(*lowerBound) : -highInfinity;
    double upper = upperBound.has_value() ? storm::utility::convertNumber<double>(*upperBound) : highInfinity;
    if (type == VariableType::Binary) {
        lower = 0.0;
        upper = 1.0;
    }

    HighsStatus addColStatus = highs.addCol(storm::utility::convertNumber<double>(objectiveFunctionCoefficient), lower, upper, 0, nullptr, nullptr);
    STORM_LOG_THROW(addColStatus != HighsStatus::kError, storm::exceptions::InvalidStateException, "Unable to add variable to HiGHS model.");
    HighsInt column = static_cast<HighsInt>(nextVariableIndex);

    if (type != VariableType::Continuous) {
        HighsStatus integralityStatus = highs.changeColIntegrality(column, HighsVarType::kInteger);
        STORM_LOG_THROW(integralityStatus != HighsStatus::kError, storm::exceptions::InvalidStateException, "Unable to set integrality of HiGHS variable.");
    }
    if (!name.empty()) {
        highs.passColName(column, name);
    }

    variableBounds.emplace_back(lower, upper);
    ++nextVariableIndex;
    return resultVar;
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::update() const {
    // HiGHS accepts incremental changes to the model at any point in time, so there is nothing to do here.
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::addConstraint(std::string const&, Constraint const& constraint) {
    if constexpr (!RawMode) {
        STORM_LOG_ASSERT(constraint.getManager() == this->getManager(), "Constraint was not built over the proper variables.");
    }

    auto constraintData = createConstraintData<ValueType, RawMode>(constraint, this->variableToIndexMap);

    double lower, upper;
    switch (constraintData.relationType) {
        case storm::expressions::RelationType::Less:
        case storm::expressions::RelationType::Greater:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "HiGHS only supports nonstrict inequalities.");
            break;
        case storm::expressions::RelationType::LessOrEqual:
            lower = -highInfinity;
            upper = constraintData.rhs;
            break;
        case storm::expressions::RelationType::GreaterOrEqual:
            lower = constraintData.rhs;
            upper = highInfinity;
            break;
        case storm::expressions::RelationType::Equal:
            lower = constraintData.rhs;
            upper = constraintData.rhs;
            break;
        default:
            STORM_LOG_ASSERT(false, "Illegal operator in LP solver constraint.");
    }
    highs.addRow(lower, upper, constraintData.variableIndices.size(), constraintData.variableIndices.data(), constraintData.coefficients.data());
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const&, Variable indicatorVariable, bool indicatorValue,
                                                               Constraint const& constraint) {
    if constexpr (RawMode) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Indicator constraints not implemented in RawMode.");
    } else {
        STORM_LOG_ASSERT(this->variableToIndexMap.count(indicatorVariable) > 0, "Indicator Variable " << indicatorVariable.getName() << " unknown to solver.");
        STORM_LOG_ASSERT(indicatorVariable.hasIntegerType(), "Indicator Variable " << indicatorVariable.getName() << " has unexpected type.");
        STORM_LOG_ASSERT(constraint.getManager() == this->getManager(), "Constraint was not built over the proper variables.");

        auto constraintData = createConstraintData<ValueType, RawMode>(constraint, this->variableToIndexMap);
        if (constraintData.relationType == storm::expressions::RelationType::Less || constraintData.relationType == storm::expressions::RelationType::Greater) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "HiGHS only supports nonstrict inequalities.");
        }

        // HiGHS does not support indicator constraints natively. Instead, we apply a big-M reformulation that reuses the binary indicator variable.
        // To this end, we compute bounds on the activity of the constraint's linear expression over the box given by the variable bounds.
        double minActivity = 0.0;
        double maxActivity = 0.0;
        for (std::size_t i = 0; i < constraintData.variableIndices.size(); ++i) {
            double coefficient = constraintData.coefficients[i];
            auto const& bounds = variableBounds[constraintData.variableIndices[i]];
            double lowerBound = bounds.first;
            double upperBound = bounds.second;
            if (coefficient > 0) {
                maxActivity += coefficient * (std::isfinite(upperBound) ? upperBound : highInfinity);
                minActivity += coefficient * (std::isfinite(lowerBound) ? lowerBound : -highInfinity);
            } else if (coefficient < 0) {
                maxActivity += coefficient * (std::isfinite(lowerBound) ? lowerBound : -highInfinity);
                minActivity += coefficient * (std::isfinite(upperBound) ? upperBound : highInfinity);
            }
        }

        HighsInt indicatorIndex = static_cast<HighsInt>(this->variableToIndexMap.at(indicatorVariable));

        auto addIndicatorRow = [this, &constraintData, indicatorIndex](double mCoefficient, double newRhs, bool isLessEqual) {
            std::vector<HighsInt> variableIndices = constraintData.variableIndices;
            std::vector<double> coefficients = constraintData.coefficients;
            variableIndices.push_back(indicatorIndex);
            coefficients.push_back(mCoefficient);
            double lower = -highInfinity;
            double upper = highInfinity;
            if (isLessEqual) {
                upper = newRhs;
            } else {
                lower = newRhs;
            }
            highs.addRow(lower, upper, variableIndices.size(), variableIndices.data(), coefficients.data());
        };

        auto addSingleIndicatorConstraint = [&](bool isLessEqual, bool indicatorValue) {
            double m;
            if (isLessEqual) {
                m = maxActivity - constraintData.rhs;
            } else {
                m = constraintData.rhs - minActivity;
            }
            STORM_LOG_THROW(std::isfinite(m), storm::exceptions::NotSupportedException,
                            "Indicator constraints over unbounded variables are not supported by the HiGHS solver.");
            double mCoefficient = (indicatorValue == isLessEqual) ? m : -m;
            double newRhs = constraintData.rhs;
            if (indicatorValue) {
                newRhs += isLessEqual ? m : -m;
            }
            addIndicatorRow(mCoefficient, newRhs, isLessEqual);
        };

        switch (constraintData.relationType) {
            case storm::expressions::RelationType::LessOrEqual:
                addSingleIndicatorConstraint(true, indicatorValue);
                break;
            case storm::expressions::RelationType::GreaterOrEqual:
                addSingleIndicatorConstraint(false, indicatorValue);
                break;
            case storm::expressions::RelationType::Equal:
                addSingleIndicatorConstraint(true, indicatorValue);
                addSingleIndicatorConstraint(false, indicatorValue);
                break;
            default:
                STORM_LOG_ASSERT(false, "Illegal operator in LP solver constraint.");
        }
    }
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::optimize() const {
    // First incorporate all recent changes.
    this->update();

    // Set the model sense.
    highs.changeObjectiveSense(this->getOptimizationDirection() == OptimizationDirection::Minimize ? ObjSense::kMinimize : ObjSense::kMaximize);

    // Then we actually optimize the model.
    HighsStatus status = highs.run();
    STORM_LOG_THROW(status != HighsStatus::kError, storm::exceptions::InvalidStateException, "Unable to optimize the model with HiGHS.");

    this->currentModelHasBeenOptimized = true;
    modelStatus = highs.getModelStatus();
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to HighsLpSolver<ValueType, RawMode>::isInfeasible: model has not been optimized.");
    return modelStatus == HighsModelStatus::kInfeasible;
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to HighsLpSolver<ValueType, RawMode>::isUnbounded: model has not been optimized.");
    return modelStatus == HighsModelStatus::kUnbounded || modelStatus == HighsModelStatus::kUnboundedOrInfeasible;
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::isOptimal() const {
    if (!this->currentModelHasBeenOptimized) {
        return false;
    }
    return modelStatus == HighsModelStatus::kOptimal;
}

template<typename ValueType, bool RawMode>
ValueType HighsLpSolver<ValueType, RawMode>::getContinuousValue(Variable const& variable) const {
    STORM_LOG_THROW(this->isOptimal(), storm::exceptions::InvalidAccessException,
                    "Unable to get HiGHS solution from a model that has not been solved optimally.");

    uint64_t variableIndex;
    if constexpr (RawMode) {
        variableIndex = variable;
    } else {
        STORM_LOG_THROW(variableToIndexMap.count(variable) != 0, storm::exceptions::InvalidAccessException,
                        "Accessing value of unknown variable '" << variable.getName() << "'.");
        variableIndex = variableToIndexMap.at(variable);
    }
    STORM_LOG_ASSERT(variableIndex < nextVariableIndex, "Variable Index exceeds highest value.");

    return storm::utility::convertNumber<ValueType>(highs.getSolution().col_value[variableIndex]);
}

template<typename ValueType, bool RawMode>
int_fast64_t HighsLpSolver<ValueType, RawMode>::getIntegerValue(Variable const& variable) const {
    STORM_LOG_THROW(this->isOptimal(), storm::exceptions::InvalidAccessException,
                    "Unable to get HiGHS solution from a model that has not been solved optimally.");

    uint64_t variableIndex;
    if constexpr (RawMode) {
        variableIndex = variable;
    } else {
        STORM_LOG_THROW(variableToIndexMap.count(variable) != 0, storm::exceptions::InvalidAccessException,
                        "Accessing value of unknown variable '" << variable.getName() << "'.");
        variableIndex = variableToIndexMap.at(variable);
    }
    STORM_LOG_ASSERT(variableIndex < nextVariableIndex, "Variable Index exceeds highest value.");

    return std::llround(highs.getSolution().col_value[variableIndex]);
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::getBinaryValue(Variable const& variable) const {
    STORM_LOG_THROW(this->isOptimal(), storm::exceptions::InvalidAccessException,
                    "Unable to get HiGHS solution from a model that has not been solved optimally.");

    uint64_t variableIndex;
    if constexpr (RawMode) {
        variableIndex = variable;
    } else {
        STORM_LOG_THROW(variableToIndexMap.count(variable) != 0, storm::exceptions::InvalidAccessException,
                        "Accessing value of unknown variable '" << variable.getName() << "'.");
        variableIndex = variableToIndexMap.at(variable);
    }
    STORM_LOG_ASSERT(variableIndex < nextVariableIndex, "Variable Index exceeds highest value.");

    return highs.getSolution().col_value[variableIndex] > 0.5;
}

template<typename ValueType, bool RawMode>
ValueType HighsLpSolver<ValueType, RawMode>::getObjectiveValue() const {
    STORM_LOG_THROW(this->isOptimal(), storm::exceptions::InvalidAccessException,
                    "Unable to get HiGHS solution from a model that has not been solved optimally.");
    return storm::utility::convertNumber<ValueType>(highs.getObjectiveValue());
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::writeModelToFile(std::string const& filename) const {
    HighsStatus status = highs.writeModel(filename);
    STORM_LOG_THROW(status != HighsStatus::kError, storm::exceptions::InvalidStateException, "Unable to write HiGHS model to file '" << filename << "'.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::push() {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Push/Pop not supported for HiGHS.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::pop() {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Push/Pop not supported for HiGHS.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const& gap, bool relative) {
    double gapAsDouble = storm::utility::convertNumber<double>(gap);
    HighsStatus status = relative ? highs.setOptionValue("mip_rel_gap", gapAsDouble) : highs.setOptionValue("mip_abs_gap", gapAsDouble);
    STORM_LOG_THROW(status != HighsStatus::kError, storm::exceptions::InvalidStateException, "Unable to set HiGHS MILP gap.");
}

template<typename ValueType, bool RawMode>
ValueType HighsLpSolver<ValueType, RawMode>::getMILPGap(bool relative) const {
    auto const& info = highs.getInfo();
    // HiGHS reports the relative MILP gap as a percentage.
    double relativeGap = info.mip_gap / 100.0;
    auto result = storm::utility::convertNumber<ValueType>(relativeGap);
    if (relative) {
        return result;
    } else {
        return storm::utility::abs<ValueType>(result * getObjectiveValue());
    }
}

#else

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver(std::string const&, OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver(std::string const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver(OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::HighsLpSolver() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
HighsLpSolver<ValueType, RawMode>::~HighsLpSolver() {}

template<typename ValueType, bool RawMode>
typename HighsLpSolver<ValueType, RawMode>::Variable HighsLpSolver<ValueType, RawMode>::addVariable(std::string const&, VariableType const&,
                                                                                                    std::optional<ValueType> const&,
                                                                                                    std::optional<ValueType> const&, ValueType) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::update() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::addConstraint(std::string const&, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const&, Variable, bool, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::optimize() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::isOptimal() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
ValueType HighsLpSolver<ValueType, RawMode>::getContinuousValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
int_fast64_t HighsLpSolver<ValueType, RawMode>::getIntegerValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
bool HighsLpSolver<ValueType, RawMode>::getBinaryValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
ValueType HighsLpSolver<ValueType, RawMode>::getObjectiveValue() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::writeModelToFile(std::string const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::push() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::pop() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
void HighsLpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const&, bool) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

template<typename ValueType, bool RawMode>
ValueType HighsLpSolver<ValueType, RawMode>::getMILPGap(bool) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for HiGHS. Yet, a method was called that requires this support. Please choose a "
                    "version of storm with HiGHS support.");
}

#endif

template class HighsLpSolver<double, true>;
template class HighsLpSolver<double, false>;

}  // namespace solver
}  // namespace storm
