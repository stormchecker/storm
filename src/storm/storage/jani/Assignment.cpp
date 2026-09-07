#include "Assignment.h"

#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/expressions/LinearityCheckVisitor.h"
#include "storm/storage/jani/LValue.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

Assignment::Assignment(storm::jani::LValue const& lValue, storm::expressions::Expression const& expression, int64_t level)
    : lValue(lValue), expression(expression), level(level) {
    // Intentionally left empty
}

Assignment::Assignment(storm::jani::Variable const& lValue, storm::expressions::Expression const& expression, int64_t level)
    : lValue(lValue), expression(expression), level(level) {
    // Intentionally left empty
}

bool Assignment::operator==(Assignment const& other) const {
    return this->isTransient() == other.isTransient() && this->getLValue() == other.getLValue() &&
           this->getAssignedExpression().isSyntacticallyEqual(other.getAssignedExpression()) && this->getLevel() == other.getLevel();
}

bool Assignment::lValueIsVariable() const {
    return lValue.isVariable();
}

bool Assignment::lValueIsArrayAccess() const {
    return lValue.isArrayAccess();
}

storm::jani::LValue& Assignment::getLValue() {
    return lValue;
}

storm::jani::LValue const& Assignment::getLValue() const {
    return lValue;
}

storm::jani::Variable const& Assignment::getVariable() const {
    return lValue.getVariable();
}

storm::expressions::Variable const& Assignment::getExpressionVariable() const {
    return getVariable().getExpressionVariable();
}

storm::expressions::Expression const& Assignment::getAssignedExpression() const {
    return expression;
}

void Assignment::setAssignedExpression(storm::expressions::Expression const& expression) {
    this->expression = expression;
}

bool Assignment::isTransient() const {
    return lValue.isTransient();
}

void Assignment::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                            bool const substituteTranscendentalNumbers) {
    this->setAssignedExpression(substituteJaniExpression(this->getAssignedExpression(), substitution, substituteTranscendentalNumbers).simplify());
    if (lValue.isArrayAccess()) {
        std::vector<storm::expressions::Expression> substitutedExpressions;
        for (auto& index : lValue.getArrayIndexVector()) {
            substitutedExpressions.push_back(substituteJaniExpression(index, substitution, substituteTranscendentalNumbers).simplify());
        }

        lValue = LValue(lValue.getVariable(), substitutedExpressions);
    }
}

int64_t Assignment::getLevel() const {
    return level;
}

void Assignment::setLevel(int64_t level) {
    this->level = level;
}

bool Assignment::isLinear() const {
    storm::expressions::LinearityCheckVisitor linearityChecker;
    return linearityChecker.check(this->getAssignedExpression(), true);
}

std::ostream& operator<<(std::ostream& stream, Assignment const& assignment) {
    if (assignment.getLevel() != 0) {
        stream << "@" << assignment.getLevel() << ": ";
    }
    stream << assignment.getLValue() << " := " << assignment.getAssignedExpression();
    return stream;
}

bool AssignmentPartialOrderByLevelAndLValue::operator()(Assignment const& left, Assignment const& right) const {
    return left.getLevel() < right.getLevel() || (left.getLevel() == right.getLevel() && left.getLValue() < right.getLValue());
}

bool AssignmentPartialOrderByLevelAndLValue::operator()(Assignment const& left, std::shared_ptr<Assignment> const& right) const {
    return left.getLevel() < right->getLevel() || (left.getLevel() == right->getLevel() && left.getLValue() < right->getLValue());
}

bool AssignmentPartialOrderByLevelAndLValue::operator()(std::shared_ptr<Assignment> const& left, std::shared_ptr<Assignment> const& right) const {
    return left->getLevel() < right->getLevel() || (left->getLevel() == right->getLevel() && left->getLValue() < right->getLValue());
}

bool AssignmentPartialOrderByLevelAndLValue::operator()(std::shared_ptr<Assignment> const& left, Assignment const& right) const {
    return left->getLevel() < right.getLevel() || (left->getLevel() == right.getLevel() && left->getLValue() < right.getLValue());
}
}  // namespace jani
}  // namespace storm
