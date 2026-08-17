#include "storm/storage/jani/LValue.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/visitor/JaniSyntacticalEqualityCheckVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

LValue::LValue(storm::jani::Variable const& variable) : variable(&variable) {
    // Intentionally left empty
}

LValue::LValue(storm::jani::Variable const& variable, std::vector<storm::expressions::Expression> const& index) : variable(&variable), arrayIndexVector(index) {
    STORM_LOG_THROW(variable.getType().isArrayType(), storm::exceptions::NotSupportedException, "Expecting an array Variable.");
    // Intentionally left empty
}

bool LValue::isVariable() const {
    return arrayIndexVector.empty();
}

bool LValue::isArray() const {
    return variable->getType().isArrayType();
}

storm::jani::Variable const& LValue::getVariable() const {
    return *variable;
}

bool LValue::isArrayAccess() const {
    return !isVariable();
}

bool LValue::isFullArrayAccess() const {
    return isArrayAccess() && variable->getType().asArrayType().getNestingDegree() == arrayIndexVector.size();
}

std::vector<storm::expressions::Expression>& LValue::getArrayIndexVector() {
    STORM_LOG_ASSERT(isArray(), "Tried to get the array index of an LValue that does not refer to an array.");
    return arrayIndexVector;
}

std::vector<storm::expressions::Expression> const& LValue::getArrayIndexVector() const {
    STORM_LOG_ASSERT(isArray(), "Tried to get the array index of an LValue that does not refer to an array.");
    return arrayIndexVector;
}

bool LValue::arrayIndexContainsVariable() const {
    STORM_LOG_ASSERT(isArray(), "Tried to check for variables in the array index of an LValue that does not refer to an array.");
    for (auto const& expr : arrayIndexVector) {
        if (expr.containsVariables()) {
            return true;
        }
    }
    return false;
}

void LValue::setArrayIndex(std::vector<storm::expressions::Expression> const& newIndex) {
    STORM_LOG_ASSERT(isArray(), "Tried to set the array index of an LValue that does not refer to an array.");
    arrayIndexVector = newIndex;
}

void LValue::addArrayAccessIndex(storm::expressions::Expression const& index) {
    STORM_LOG_ASSERT(isArray(), "Tried to add an array access index to an LValue that doesn't consider an array.");
    arrayIndexVector.push_back(index);
}

bool LValue::isTransient() const {
    return variable->isTransient();
}

LValue LValue::changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping) const {
    auto it = remapping.find(variable);
    if (it == remapping.end()) {
        return *this;
    } else {
        if (isArrayAccess()) {
            return LValue(it->second, arrayIndexVector);
        } else {
            return LValue(it->second);
        }
    }
}

std::string LValue::getName() const {
    std::string result = getVariable().getName();
    if (isArrayAccess()) {
        for (auto const& i : arrayIndexVector) {
            result += "[" + i.toString() + "]";
        }
    }
    return result;
}

bool LValue::operator<(LValue const& other) const {
    if (isVariable()) {
        return !other.isVariable() || variable->getExpressionVariable() < other.getVariable().getExpressionVariable();
    } else {
        STORM_LOG_ASSERT(isArrayAccess(), "Unhandled LValue.");
        if (other.isVariable()) {
            return false;
        }
        STORM_LOG_ASSERT(other.isArrayAccess(), "Unhandled LValue.");
        if (getVariable().getExpressionVariable() < other.getVariable().getExpressionVariable()) {
            return true;
        } else if (other.getVariable().getExpressionVariable() < getVariable().getExpressionVariable()) {
            return false;
        } else {
            return std::lexicographical_compare(arrayIndexVector.begin(), arrayIndexVector.end(), other.getArrayIndexVector().begin(),
                                                other.getArrayIndexVector().end(), std::less<storm::expressions::Expression>());
        }
    }
}

bool LValue::operator==(LValue const& other) const {
    if (getVariable().getExpressionVariable() == other.getVariable().getExpressionVariable()) {
        if (isArrayAccess()) {
            if (other.isArrayAccess() && arrayIndexVector.size() == other.getArrayIndexVector().size()) {
                storm::expressions::JaniSyntacticalEqualityCheckVisitor checker;
                for (uint64_t i = 0; i < arrayIndexVector.size(); ++i) {
                    if (!checker.isSyntacticallyEqual(arrayIndexVector[i], other.getArrayIndexVector()[i])) {
                        return false;
                    }
                }
                return true;
            } else {
                return false;
            }
        } else {
            STORM_LOG_ASSERT(isVariable(), "Unhandled kind of LValue.");
            return other.isVariable();
        }
    } else {
        return false;
    }
}

std::ostream& operator<<(std::ostream& stream, LValue const& lValue) {
    stream << lValue.getName();
    return stream;
}

}  // namespace jani
}  // namespace storm