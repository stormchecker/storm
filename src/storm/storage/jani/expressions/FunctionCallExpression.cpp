#include "FunctionCallExpression.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace expressions {

FunctionCallExpression::FunctionCallExpression(ExpressionManager const& manager, Type const& type, std::string const& functionIdentifier,
                                               std::vector<std::shared_ptr<BaseExpression const>> const& arguments)
    : BaseExpression(manager, type), identifier(functionIdentifier), arguments(arguments) {
    // Intentionally left empty
}

void FunctionCallExpression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    for (auto const& a : arguments) {
        a->gatherVariables(variables);
    }
}

bool FunctionCallExpression::containsVariables() const {
    for (auto const& a : arguments) {
        if (a->containsVariables()) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<BaseExpression const> FunctionCallExpression::simplify() const {
    std::vector<std::shared_ptr<BaseExpression const>> simplifiedArguments;
    simplifiedArguments.reserve(arguments.size());
    for (auto const& a : arguments) {
        simplifiedArguments.push_back(a->simplify());
    }
    return std::shared_ptr<BaseExpression const>(new FunctionCallExpression(getManager(), getType(), identifier, simplifiedArguments));
}

boost::any FunctionCallExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    auto janiVisitor = dynamic_cast<JaniExpressionVisitor*>(&visitor);
    STORM_LOG_THROW(janiVisitor != nullptr, storm::exceptions::UnexpectedException, "Visitor of jani expression should be of type JaniVisitor.");
    return janiVisitor->visit(*this, data);
}

void FunctionCallExpression::printToStream(std::ostream& stream) const {
    stream << identifier;
    if (getNumberOfArguments() > 0) {
        stream << "(";
        bool first = true;
        for (auto const& a : arguments) {
            if (!first) {
                stream << ", ";
            }
            first = false;
            stream << *a;
        }
        stream << ")";
    }
}

std::string const& FunctionCallExpression::getFunctionIdentifier() const {
    return identifier;
}

uint64_t FunctionCallExpression::getNumberOfArguments() const {
    return arguments.size();
}

std::shared_ptr<BaseExpression const> FunctionCallExpression::getArgument(uint64_t i) const {
    STORM_LOG_THROW(i < arguments.size(), storm::exceptions::InvalidArgumentException,
                    "Tried to access the argument with index " << i << " of a function call with " << arguments.size() << " arguments.");
    return arguments[i];
}

std::vector<std::shared_ptr<BaseExpression const>> const& FunctionCallExpression::getArguments() const {
    return arguments;
}

}  // namespace expressions
}  // namespace storm