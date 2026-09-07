#include "FunctionDefinition.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

FunctionDefinition::FunctionDefinition(std::string const& name, storm::expressions::Type const& type,
                                       std::vector<storm::expressions::Variable> const& parameters, storm::expressions::Expression const& functionBody)
    : name(name), type(type), parameters(parameters), functionBody(functionBody) {
    // Intentionally left empty.
}

std::string const& FunctionDefinition::getName() const {
    return name;
}

storm::expressions::Type const& FunctionDefinition::getType() const {
    return type;
}

std::vector<storm::expressions::Variable> const& FunctionDefinition::getParameters() const {
    return parameters;
}

storm::expressions::Expression const& FunctionDefinition::getFunctionBody() const {
    return functionBody;
}

storm::expressions::Expression FunctionDefinition::call(std::vector<std::shared_ptr<storm::expressions::BaseExpression const>> const& arguments) const {
    // substitute the parameters in the function body
    STORM_LOG_THROW(arguments.size() == parameters.size(), storm::exceptions::InvalidArgumentException,
                    "The number of arguments does not match the number of parameters.");
    const bool substituteTranscendentalNumbers = true;
    std::unordered_map<storm::expressions::Variable, storm::expressions::Expression> parameterSubstitution;
    for (uint64_t i = 0; i < arguments.size(); ++i) {
        parameterSubstitution.emplace(parameters[i], arguments[i]);
    }
    return substituteJaniExpression(functionBody, parameterSubstitution, substituteTranscendentalNumbers);
}

void FunctionDefinition::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                                    bool const substituteTranscendentalNumbers) {
    this->setFunctionBody(substituteJaniExpression(this->getFunctionBody(), substitution, substituteTranscendentalNumbers));
}

void FunctionDefinition::setFunctionBody(storm::expressions::Expression const& body) {
    functionBody = body;
}
}  // namespace jani
}  // namespace storm
