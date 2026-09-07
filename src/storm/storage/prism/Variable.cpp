#include "Variable.h"

#include <map>

#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace prism {
Variable::Variable(storm::expressions::Variable const& variable, storm::expressions::Expression const& initialValueExpression, bool observable,
                   std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), variable(variable), initialValueExpression(initialValueExpression), observable(observable) {
    // Nothing to do here.
}

Variable::Variable(storm::expressions::ExpressionManager& manager, Variable const& oldVariable, std::string const& newName,
                   std::map<storm::expressions::Variable, storm::expressions::Expression> const& renaming, bool observable, std::string const& filename,
                   uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber),
      variable(manager.declareVariable(newName, oldVariable.variable.getType())),
      initialValueExpression(oldVariable.getInitialValueExpression().substitute(renaming)),
      observable(observable) {
    // Intentionally left empty.
}

std::string const& Variable::getName() const {
    return this->variable.getName();
}

bool Variable::hasInitialValue() const {
    return this->initialValueExpression.isInitialized();
}

storm::expressions::Expression const& Variable::getInitialValueExpression() const {
    return this->initialValueExpression;
}

void Variable::setInitialValueExpression(storm::expressions::Expression const& initialValueExpression) {
    this->initialValueExpression = initialValueExpression;
}

storm::expressions::Variable const& Variable::getExpressionVariable() const {
    return this->variable;
}

storm::expressions::Expression Variable::getExpression() const {
    return variable.getExpression();
}

bool Variable::isObservable() const {
    return this->observable;
}

}  // namespace prism
}  // namespace storm
