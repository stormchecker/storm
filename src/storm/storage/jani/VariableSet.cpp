#include "storm/storage/jani/VariableSet.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

VariableSet::VariableSet() {
    // Intentionally left empty.
}

detail::Variables<Variable> VariableSet::getBooleanVariables() {
    return detail::Variables<Variable>(booleanVariables.begin(), booleanVariables.end());
}

detail::ConstVariables<Variable> VariableSet::getBooleanVariables() const {
    return detail::ConstVariables<Variable>(booleanVariables.begin(), booleanVariables.end());
}

detail::Variables<Variable> VariableSet::getBoundedIntegerVariables() {
    return detail::Variables<Variable>(boundedIntegerVariables.begin(), boundedIntegerVariables.end());
}

detail::ConstVariables<Variable> VariableSet::getBoundedIntegerVariables() const {
    return detail::ConstVariables<Variable>(boundedIntegerVariables.begin(), boundedIntegerVariables.end());
}

detail::Variables<Variable> VariableSet::getUnboundedIntegerVariables() {
    return detail::Variables<Variable>(unboundedIntegerVariables.begin(), unboundedIntegerVariables.end());
}

detail::ConstVariables<Variable> VariableSet::getUnboundedIntegerVariables() const {
    return detail::ConstVariables<Variable>(unboundedIntegerVariables.begin(), unboundedIntegerVariables.end());
}

detail::Variables<Variable> VariableSet::getRealVariables() {
    return detail::Variables<Variable>(realVariables.begin(), realVariables.end());
}

detail::ConstVariables<Variable> VariableSet::getRealVariables() const {
    return detail::ConstVariables<Variable>(realVariables.begin(), realVariables.end());
}

detail::Variables<Variable> VariableSet::getArrayVariables() {
    return detail::Variables<Variable>(arrayVariables.begin(), arrayVariables.end());
}

detail::ConstVariables<Variable> VariableSet::getArrayVariables() const {
    return detail::ConstVariables<Variable>(arrayVariables.begin(), arrayVariables.end());
}

detail::Variables<Variable> VariableSet::getClockVariables() {
    return detail::Variables<Variable>(clockVariables.begin(), clockVariables.end());
}

detail::ConstVariables<Variable> VariableSet::getClockVariables() const {
    return detail::ConstVariables<Variable>(clockVariables.begin(), clockVariables.end());
}

detail::Variables<Variable> VariableSet::getContinuousVariables() {
    return detail::Variables<Variable>(continuousVariables.begin(), continuousVariables.end());
}

detail::ConstVariables<Variable> VariableSet::getContinuousVariables() const {
    return detail::ConstVariables<Variable>(continuousVariables.begin(), continuousVariables.end());
}

std::vector<std::shared_ptr<Variable>>& VariableSet::getVariableVectorForType(JaniType const& type) {
    if (type.isBasicType()) {
        switch (type.asBasicType().get()) {
            case BasicType::Type::Bool:
                return booleanVariables;
            case BasicType::Type::Int:
                return unboundedIntegerVariables;
            case BasicType::Type::Real:
                return realVariables;
        }
    } else if (type.isBoundedType()) {
        switch (type.asBoundedType().getBaseType()) {
            case BoundedType::BaseType::Int:
                return boundedIntegerVariables;
            case BoundedType::BaseType::Real:
                STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Cannot add variable of bounded real type.");
        }
    } else if (type.isArrayType()) {
        return arrayVariables;
    } else if (type.isClockType()) {
        return clockVariables;
    } else if (type.isContinuousType()) {
        return continuousVariables;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unhandled variable type" << type << ".");
}

Variable const& VariableSet::addVariable(Variable const& variable) {
    STORM_LOG_THROW(!this->hasVariable(variable.getName()), storm::exceptions::WrongFormatException,
                    "Cannot add variable with name '" << variable.getName() << "', because a variable with that name already exists.");
    std::shared_ptr<Variable> newVariable = variable.clone();
    variables.push_back(newVariable);
    if (variable.isTransient()) {
        transientVariables.push_back(newVariable);
    }
    nameToVariable.emplace(variable.getName(), variable.getExpressionVariable());
    variableToVariable.emplace(variable.getExpressionVariable(), newVariable);

    auto& variableVectorForType = getVariableVectorForType(variable.getType());
    variableVectorForType.push_back(newVariable);
    return *newVariable;
}

std::vector<std::shared_ptr<Variable>> VariableSet::dropAllArrayVariables() {
    if (!arrayVariables.empty()) {
        for (auto const& arrVar : arrayVariables) {
            nameToVariable.erase(arrVar->getName());
            variableToVariable.erase(arrVar->getExpressionVariable());
        }
        std::vector<std::shared_ptr<Variable>> newVariables;
        for (auto const& v : variables) {
            if (!v->getType().isArrayType()) {
                newVariables.push_back(v);
            }
        }
        variables = std::move(newVariables);
        newVariables.clear();
        for (auto const& v : transientVariables) {
            if (!v->getType().isArrayType()) {
                newVariables.push_back(v);
            }
        }
        transientVariables = std::move(newVariables);
    }

    std::vector<std::shared_ptr<Variable>> result = std::move(arrayVariables);
    arrayVariables.clear();
    return result;
}

bool VariableSet::hasVariable(std::string const& name) const {
    return nameToVariable.find(name) != nameToVariable.end();
}

bool VariableSet::hasVariable(Variable const& var) const {
    return hasVariable(var.getName());
}

Variable const& VariableSet::getVariable(std::string const& name) const {
    auto it = nameToVariable.find(name);
    STORM_LOG_THROW(it != nameToVariable.end(), storm::exceptions::InvalidArgumentException, "Unable to retrieve unknown variable '" << name << "'.");
    return getVariable(it->second);
}

template<typename VarType>
void eraseFromVariableVector(std::vector<std::shared_ptr<VarType>>& varVec, storm::expressions::Variable const& variable) {
    for (auto vIt = varVec.begin(); vIt != varVec.end(); ++vIt) {
        if ((*vIt)->getExpressionVariable() == variable) {
            varVec.erase(vIt);
            break;
        }
    }
}

std::shared_ptr<Variable> VariableSet::eraseVariable(storm::expressions::Variable const& variable) {
    auto vToVIt = variableToVariable.find(variable);
    STORM_LOG_THROW(vToVIt != variableToVariable.end(), storm::exceptions::InvalidArgumentException,
                    "Unable to erase unknown variable '" << variable.getName() << "'.");
    std::shared_ptr<Variable> janiVar = std::move(vToVIt->second);
    variableToVariable.erase(vToVIt);

    nameToVariable.erase(janiVar->getName());
    eraseFromVariableVector(variables, variable);
    eraseFromVariableVector(getVariableVectorForType(janiVar->getType()), variable);
    if (janiVar->isTransient()) {
        eraseFromVariableVector(transientVariables, variable);
    }
    return janiVar;
}

typename detail::Variables<Variable>::iterator VariableSet::begin() {
    return detail::Variables<Variable>::make_iterator(variables.begin());
}

typename detail::ConstVariables<Variable>::iterator VariableSet::begin() const {
    return detail::ConstVariables<Variable>::make_iterator(variables.begin());
}

typename detail::Variables<Variable>::iterator VariableSet::end() {
    return detail::Variables<Variable>::make_iterator(variables.end());
}

detail::ConstVariables<Variable>::iterator VariableSet::end() const {
    return detail::ConstVariables<Variable>::make_iterator(variables.end());
}

Variable const& VariableSet::getVariable(storm::expressions::Variable const& variable) const {
    auto it = variableToVariable.find(variable);
    STORM_LOG_THROW(it != variableToVariable.end(), storm::exceptions::InvalidArgumentException,
                    "Unable to retrieve unknown variable '" << variable.getName() << "'.");
    return *it->second;
}

bool VariableSet::hasVariable(storm::expressions::Variable const& variable) const {
    return variableToVariable.find(variable) != variableToVariable.end();
}

bool VariableSet::hasTransientVariable() const {
    for (auto const& variable : variables) {
        if (variable->isTransient()) {
            return true;
        }
    }
    return false;
}

bool VariableSet::containsBooleanVariable() const {
    return !booleanVariables.empty();
}

bool VariableSet::containsBoundedIntegerVariable() const {
    return !boundedIntegerVariables.empty();
}

bool VariableSet::containsUnboundedIntegerVariables() const {
    return !unboundedIntegerVariables.empty();
}

bool VariableSet::containsRealVariables() const {
    return !realVariables.empty();
}

bool VariableSet::containsArrayVariables() const {
    return !arrayVariables.empty();
}

bool VariableSet::containsClockVariables() const {
    return !clockVariables.empty();
}

bool VariableSet::containsContinuousVariables() const {
    return !continuousVariables.empty();
}

bool VariableSet::containsNonTransientRealVariables() const {
    for (auto const& variable : realVariables) {
        if (!variable->isTransient()) {
            return true;
        }
    }
    return false;
}

bool VariableSet::containsNonTransientUnboundedIntegerVariables() const {
    for (auto const& variable : unboundedIntegerVariables) {
        if (!variable->isTransient()) {
            return true;
        }
    }
    return false;
}

bool VariableSet::empty() const {
    return !(containsBooleanVariable() || containsBoundedIntegerVariable() || containsUnboundedIntegerVariables() || containsRealVariables() ||
             containsArrayVariables() || containsClockVariables());
}

uint64_t VariableSet::getNumberOfVariables() const {
    return variables.size();
}

uint64_t VariableSet::getNumberOfNontransientVariables() const {
    return getNumberOfVariables() - getNumberOfTransientVariables();
}

uint_fast64_t VariableSet::getNumberOfTransientVariables() const {
    uint_fast64_t result = 0;
    for (auto const& variable : variables) {
        if (variable->isTransient()) {
            ++result;
        }
    }
    return result;
}

uint_fast64_t VariableSet::getNumberOfRealTransientVariables() const {
    uint_fast64_t result = 0;
    for (auto const& variable : realVariables) {
        if (variable->isTransient()) {
            ++result;
        }
    }
    return result;
}

uint_fast64_t VariableSet::getNumberOfUnboundedIntegerTransientVariables() const {
    uint_fast64_t result = 0;
    for (auto const& variable : unboundedIntegerVariables) {
        if (variable->isTransient()) {
            ++result;
        }
    }
    return result;
}

uint_fast64_t VariableSet::getNumberOfNumericalTransientVariables() const {
    uint_fast64_t result = 0;
    for (auto const& variable : transientVariables) {
        auto const& type = variable->getType();
        if (type.isBasicType() && (type.asBasicType().isIntegerType() || type.asBasicType().isRealType())) {
            ++result;
        } else if (type.isBoundedType() && (type.asBoundedType().isIntegerType() || type.asBoundedType().isRealType())) {
            ++result;
        }
    }
    return result;
}

typename detail::ConstVariables<Variable> VariableSet::getTransientVariables() const {
    return detail::ConstVariables<Variable>(transientVariables.begin(), transientVariables.end());
}

bool VariableSet::containsVariablesInBoundExpressionsOrInitialValues(std::set<storm::expressions::Variable> const& variables) const {
    for (auto const& variable : this->variables) {
        if (variable->hasInitExpression() && variable->getInitExpression().containsVariable(variables)) {
            return true;
        }
        auto const& varType = variable->getType();
        auto const& type = varType.isArrayType() ? varType.asArrayType().getBaseTypeRecursive() : varType;
        if (type.isBoundedType()) {
            auto const& boundedType = type.asBoundedType();
            if (boundedType.hasLowerBound() && boundedType.getLowerBound().containsVariable(variables)) {
                return true;
            }
            if (boundedType.hasUpperBound() && boundedType.getUpperBound().containsVariable(variables)) {
                return true;
            }
        }
    }
    return false;
}

std::map<std::string, std::reference_wrapper<Variable const>> VariableSet::getNameToVariableMap() const {
    std::map<std::string, std::reference_wrapper<Variable const>> result;

    for (auto const& variable : variables) {
        result.emplace(variable->getName(), *variable);
    }

    return result;
}

void VariableSet::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                             bool const substituteTranscendentalNumbers) {
    for (auto& variable : variables) {
        variable->substitute(substitution, substituteTranscendentalNumbers);
    }
}

void VariableSet::substituteExpressionVariables(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) {
    for (auto& variable : variables) {
        auto varIt = substitution.find(variable->getExpressionVariable());
        if (varIt != substitution.end()) {
            STORM_LOG_ASSERT(varIt->second.isVariable(), "Expected that variables are only substituted by other variables. However, we substitute "
                                                             << varIt->first.getName() << " by " << varIt->second << ".");
            variable->setExpressionVariable(varIt->second.getBaseExpression().asVariableExpression().getVariable());
        }
    }
}
}  // namespace jani
}  // namespace storm
