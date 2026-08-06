#include "storm/solver/SoplexLpSolver.h"

#include <numeric>

#include "storm/exceptions/InternalException.h"
#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/LinearCoefficientVisitor.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

#ifdef STORM_HAVE_SOPLEX

using namespace soplex;

soplex::Rational to_soplex_rational(storm::RationalNumber const& in) {
    return soplex::Rational(storm::utility::convertNumber<GmpRationalNumber>(in).get_mpq_t());
}

storm::RationalNumber from_soplex_rational(soplex::Rational const& r) {
    return storm::utility::convertNumber<storm::RationalNumber>(GmpRationalNumber(r.backend().data()));
}

template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::SoplexLpSolver(std::string const& name, OptimizationDirection const& optDir) : LpSolver<ValueType, RawMode>(optDir) {
    if (std::is_same_v<ValueType, storm::RationalNumber>) {
        solver.setIntParam(SoPlex::READMODE, SoPlex::READMODE_RATIONAL);
        solver.setIntParam(SoPlex::SOLVEMODE, SoPlex::SOLVEMODE_RATIONAL);
        solver.setIntParam(SoPlex::CHECKMODE, SoPlex::CHECKMODE_RATIONAL);
        solver.setIntParam(SoPlex::SYNCMODE, SoPlex::SYNCMODE_AUTO);
        solver.setRealParam(SoPlex::FEASTOL, 0.0);
        solver.setRealParam(SoPlex::OPTTOL, 0.0);
    }
    solver.setIntParam(SoPlex::VERBOSITY, 0);
}

template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::SoplexLpSolver(std::string const& name) : SoplexLpSolver(name, OptimizationDirection::Minimize) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::SoplexLpSolver(OptimizationDirection const& optDir) : SoplexLpSolver("", optDir) {
    // Intentionally left empty.
}

template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::~SoplexLpSolver() {}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::update() const {
    // Nothing to be done.
}

template<typename ValueType, bool RawMode>
typename SoplexLpSolver<ValueType, RawMode>::Variable SoplexLpSolver<ValueType, RawMode>::addVariable(std::string const& name, VariableType const& type,
                                                                                                      std::optional<ValueType> const& lowerBound,
                                                                                                      std::optional<ValueType> const& upperBound,
                                                                                                      ValueType objectiveFunctionCoefficient) {
    STORM_LOG_THROW(type == VariableType::Continuous, storm::exceptions::NotSupportedException, "Soplex LP Solver only supports variables of continuous type.");
    Variable resultVar;
    if constexpr (RawMode) {
        resultVar = nextVariableIndex;
    } else {
        resultVar = this->declareOrGetExpressionVariable(name, type);
        //  Assert whether the variable does not exist yet.
        //  Due to incremental usage (push(), pop()), a variable might be declared in the manager but not in the lp model.
        STORM_LOG_ASSERT(variableToIndexMap.count(resultVar) == 0, "Variable " << resultVar.getName() << " exists already in the model.");
        this->variableToIndexMap.emplace(resultVar, nextVariableIndex);
    }
    ++nextVariableIndex;

    if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        soplex::Rational const rat_inf(soplex::infinity);
        solver.addColRational(soplex::LPColRational(to_soplex_rational(objectiveFunctionCoefficient), variables,
                                                    upperBound.has_value() ? to_soplex_rational(*upperBound) : rat_inf,
                                                    lowerBound.has_value() ? to_soplex_rational(*lowerBound) : -rat_inf));
    } else {
        solver.addColReal(soplex::LPColReal(objectiveFunctionCoefficient, variables, upperBound.has_value() ? *upperBound : soplex::infinity,
                                            lowerBound.has_value() ? *lowerBound : -soplex::infinity));
    }
    return resultVar;
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::addConstraint(std::string const& name, Constraint const& constraint) {
    if constexpr (!RawMode) {
        STORM_LOG_TRACE("Adding constraint " << (name == "" ? std::to_string(nextConstraintIndex) : name) << " to SoplexLpSolver:\n"
                                             << "\t" << constraint);
    }
    using SoplexValueType = std::conditional_t<std::is_same_v<ValueType, storm::RationalNumber>, soplex::Rational, soplex::Real>;
    // Extract constraint data
    SoplexValueType rhs;
    storm::expressions::RelationType relationType;
    TypedDSVector row(0);
    if constexpr (RawMode) {
        if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
            rhs = to_soplex_rational(constraint.rhs);
        } else {
            rhs = constraint.rhs;
        }
        relationType = constraint.relationType;
        row = TypedDSVector(constraint.lhsVariableIndices.size());
        auto varIt = constraint.lhsVariableIndices.cbegin();
        auto varItEnd = constraint.lhsVariableIndices.cend();
        auto coefIt = constraint.lhsCoefficients.cbegin();
        for (; varIt != varItEnd; ++varIt, ++coefIt) {
            if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
                row.add(*varIt, to_soplex_rational(*coefIt));
            } else {
                row.add(*varIt, *coefIt);
            }
        }
    } else {
        STORM_LOG_THROW(constraint.getManager() == this->getManager(), storm::exceptions::InvalidArgumentException,
                        "Constraint was not built over the proper variables.");
        STORM_LOG_THROW(constraint.isRelationalExpression(), storm::exceptions::InvalidArgumentException, "Illegal constraint is not a relational expression.");

        // FIXME: We're potentially losing precision as the coefficients and the rhs are converted to double. The LinearCoefficientVisitor needs a ValueType!
        storm::expressions::LinearCoefficientVisitor::VariableCoefficients leftCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(0));
        storm::expressions::LinearCoefficientVisitor::VariableCoefficients rightCoefficients =
            storm::expressions::LinearCoefficientVisitor().getLinearCoefficients(constraint.getOperand(1));
        leftCoefficients.separateVariablesFromConstantPart(rightCoefficients);
        rhs = rightCoefficients.getConstantPart();
        relationType = constraint.getBaseExpression().asBinaryRelationExpression().getRelationType();
        row = TypedDSVector(std::distance(leftCoefficients.begin(), leftCoefficients.end()));
        for (auto const& variableCoefficientPair : leftCoefficients) {
            auto variableIndexPair = this->variableToIndexMap.find(variableCoefficientPair.first);
            row.add(variableIndexPair->second, leftCoefficients.getCoefficient(variableIndexPair->first));
        }
    }

    // Determine the type of the constraint and add it properly.
    SoplexValueType l, r;
    switch (relationType) {
        case storm::expressions::RelationType::Less:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SoPlex only supports nonstrict inequalities.");
            break;
        case storm::expressions::RelationType::LessOrEqual:
            l = -soplex::infinity;
            r = rhs;
            break;
        case storm::expressions::RelationType::Greater:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SoPlex only supports nonstrict inequalities.");
            break;
        case storm::expressions::RelationType::GreaterOrEqual:
            l = rhs;
            r = soplex::infinity;
            break;
        case storm::expressions::RelationType::Equal:
            l = rhs;
            r = rhs;
            break;
        default:
            STORM_LOG_ASSERT(false, "Illegal operator in LP solver constraint.");
    }
    if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        solver.addRowRational(LPRowRational(l, row, r));
    } else {
        solver.addRowReal(LPRow(l, row, r));
    }
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const&, Variable, bool, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Indicator Constraints not supported for SoPlex.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::optimize() const {
    // First incorporate all recent changes.
    this->update();
    //
    if (this->getOptimizationDirection() == storm::solver::OptimizationDirection::Minimize) {
        solver.setIntParam(SoPlex::OBJSENSE, SoPlex::OBJSENSE_MINIMIZE);
    } else {
        solver.setIntParam(SoPlex::OBJSENSE, SoPlex::OBJSENSE_MAXIMIZE);
    }

    status = solver.optimize();
    STORM_LOG_TRACE("soplex status " << status);
    if (status == soplex::SPxSolver::ERROR) {
        STORM_LOG_THROW(false, storm::exceptions::InternalException, "Soplex failed.");
    } else if (status == soplex::SPxSolver::UNKNOWN) {
        STORM_LOG_THROW(false, storm::exceptions::InternalException, "Soplex gives up on this problem.");
    }
    this->currentModelHasBeenOptimized = true;
}

template<typename ValueType, bool RawMode>
bool SoplexLpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to SoplexLpSolver<ValueType, RawMode>::isInfeasible: model has not been optimized.");

    return (status == soplex::SPxSolver::INFEASIBLE);
}

template<typename ValueType, bool RawMode>
bool SoplexLpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(this->currentModelHasBeenOptimized, storm::exceptions::InvalidStateException,
                    "Illegal call to SoplexLpSolver<ValueType, RawMode>::isUnbounded: model has not been optimized.");

    return (status == soplex::SPxSolver::UNBOUNDED);
}

template<typename ValueType, bool RawMode>
bool SoplexLpSolver<ValueType, RawMode>::isOptimal() const {
    if (!this->currentModelHasBeenOptimized) {
        return false;
    }
    return (status == soplex::SPxSolver::OPTIMAL);
}

template<typename ValueType, bool RawMode>
ValueType SoplexLpSolver<ValueType, RawMode>::getContinuousValue(Variable const& variable) const {
    ensureSolved();

    uint64_t varIndex;
    if constexpr (RawMode) {
        varIndex = variable;
    } else {
        STORM_LOG_THROW(variableToIndexMap.count(variable) != 0, storm::exceptions::InvalidAccessException,
                        "Accessing value of unknown variable '" << variable.getName() << "'.");
        varIndex = variableToIndexMap.at(variable);
    }
    STORM_LOG_ASSERT(varIndex < nextVariableIndex, "Variable Index exceeds highest value.");

    if (primalSolution.dim() == 0) {
        primalSolution = TypedDVector(nextVariableIndex);
        if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
            solver.getPrimalRational(primalSolution);
        } else {
            solver.getPrimal(primalSolution);
        }
    }
    if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        return storm::utility::convertNumber<ValueType>(from_soplex_rational(primalSolution[varIndex]));
    } else {
        return primalSolution[varIndex];
    }
}

template<>
double SoplexLpSolver<double>::getObjectiveValue() const {
    ensureSolved();
    return solver.objValueReal();
}

template<typename ValueType, bool RawMode>
ValueType SoplexLpSolver<ValueType, RawMode>::getObjectiveValue() const {
    ensureSolved();
    return storm::utility::convertNumber<ValueType>(from_soplex_rational(solver.objValueRational()));
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::ensureSolved() const {
    if (!this->isOptimal()) {
        STORM_LOG_THROW(!this->isInfeasible(), storm::exceptions::InvalidAccessException, "Unable to get Soplex solution from infeasible model.");
        STORM_LOG_THROW(!this->isUnbounded(), storm::exceptions::InvalidAccessException, "Unable to get Soplex solution from unbounded model.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to get Soplex solution from unoptimized model.");
    }
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::writeModelToFile(std::string const& filename) const {
    if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        solver.writeFileRational(filename.c_str(), nullptr, nullptr, nullptr);
    } else {
        solver.writeFileReal(filename.c_str(), nullptr, nullptr, nullptr);
    }
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::push() {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Push/Pop not supported on SoPlex.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::pop() {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Push/Pop not supported on SoPlex.");
}

#else
template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::SoplexLpSolver(std::string const&, OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::SoplexLpSolver(std::string const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::SoplexLpSolver(OptimizationDirection const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
SoplexLpSolver<ValueType, RawMode>::~SoplexLpSolver() {}

template<typename ValueType, bool RawMode>
typename SoplexLpSolver<ValueType, RawMode>::Variable SoplexLpSolver<ValueType, RawMode>::addVariable(std::string const&, VariableType const&,
                                                                                                      std::optional<ValueType> const&,
                                                                                                      std::optional<ValueType> const&, ValueType) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::update() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::addConstraint(std::string const&, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::addIndicatorConstraint(std::string const&, Variable, bool, Constraint const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::optimize() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
bool SoplexLpSolver<ValueType, RawMode>::isInfeasible() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
bool SoplexLpSolver<ValueType, RawMode>::isUnbounded() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
bool SoplexLpSolver<ValueType, RawMode>::isOptimal() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
ValueType SoplexLpSolver<ValueType, RawMode>::getContinuousValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
ValueType SoplexLpSolver<ValueType, RawMode>::getObjectiveValue() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::writeModelToFile(std::string const&) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::push() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::pop() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for Soplex. Yet, a method was called that requires this support. Please choose a "
                    "version of support with Soplex support.");
}
#endif

template<typename ValueType, bool RawMode>
int_fast64_t SoplexLpSolver<ValueType, RawMode>::getIntegerValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SoPlex does not support integer variables.");
}

template<typename ValueType, bool RawMode>
bool SoplexLpSolver<ValueType, RawMode>::getBinaryValue(Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SoPlex does not support binary variables.");
}

template<typename ValueType, bool RawMode>
void SoplexLpSolver<ValueType, RawMode>::setMaximalMILPGap(ValueType const&, bool) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SoPlex does not support integer variables.");
}

template<typename ValueType, bool RawMode>
ValueType SoplexLpSolver<ValueType, RawMode>::getMILPGap(bool) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SoPlex does not support integer variables.");
}

template class SoplexLpSolver<double, false>;
template class SoplexLpSolver<storm::RationalNumber, false>;
template class SoplexLpSolver<double, true>;
template class SoplexLpSolver<storm::RationalNumber, true>;
}  // namespace solver
}  // namespace storm
