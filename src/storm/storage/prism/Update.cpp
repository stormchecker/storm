#include "storm/storage/prism/Update.h"

#include <algorithm>
#include <boost/algorithm/string/join.hpp>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace prism {
Update::Update(uint_fast64_t globalIndex, storm::expressions::Expression const& likelihoodExpression, std::vector<storm::prism::Assignment> const& assignments,
               std::string const& filename, uint_fast64_t lineNumber)
    : Update(globalIndex, std::make_pair(likelihoodExpression, storm::expressions::Expression()), assignments, filename, lineNumber) {
    // Intentionally left empty.
}

/*!
 * helper to assert valid likelihood expressions
 */
bool isValidLikelihoodSimpl(storm::expressions::Expression const& simplifiedExpression) {
    return !simplifiedExpression.isLiteral() || simplifiedExpression.evaluateAsRational() >= 0;
}

bool isValidLikelihood(storm::expressions::Expression const& expression) {
    if (!expression.isInitialized()) {
        return true;  // Uninitialized expressions are considered valid.
    }
    auto simplified = expression.simplify();
    return isValidLikelihoodSimpl(simplified);
}

Update::Update(uint_fast64_t globalIndex, ExpressionPair const& likelihoodExpressionInterval, std::vector<storm::prism::Assignment> const& assignments,
               std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber),
      likelihoodExpressions(likelihoodExpressionInterval),
      assignments(assignments),
      variableToAssignmentIndexMap(),
      globalIndex(globalIndex) {
    STORM_LOG_ASSERT(likelihoodExpressions.first.isInitialized(), "LikelihoodExpression must be initialized.");
    // Note: likelihoodExpressions.second might be uninitialized in which case we're having a non-interval likelihood
    STORM_LOG_ASSERT(isValidLikelihood(likelihoodExpressions.first),
                     "Negative likelihood expressions are not allowed. Got " << likelihoodExpressions.first << ".");
    STORM_LOG_ASSERT(isValidLikelihood(likelihoodExpressions.second),
                     "Negative likelihood expressions are not allowed. Got " << likelihoodExpressions.second << ".");
    std::sort(this->assignments.begin(), this->assignments.end(), [](storm::prism::Assignment const& assignment1, storm::prism::Assignment const& assignment2) {
        bool smaller = assignment1.getVariable().getType().isBooleanType() && !assignment2.getVariable().getType().isBooleanType();
        if (!smaller) {
            smaller = assignment1.getVariable() < assignment2.getVariable();
        }
        return smaller;
    });
    this->createAssignmentMapping();
}

bool Update::isLikelihoodInterval() const {
    return likelihoodExpressions.second.isInitialized();
}

storm::expressions::Expression const& Update::getLikelihoodExpression() const {
    STORM_LOG_ASSERT(!isLikelihoodInterval(), "Cannot retrieve likelihood expression of an interval update.");
    return likelihoodExpressions.first;
}

typename Update::ExpressionPair const& Update::getLikelihoodExpressionInterval() const {
    STORM_LOG_ASSERT(isLikelihoodInterval(), "Cannot retrieve likelihood expressions of non-interval update.");
    return likelihoodExpressions;
}

std::size_t Update::getNumberOfAssignments() const {
    return this->assignments.size();
}

std::vector<storm::prism::Assignment> const& Update::getAssignments() const {
    return this->assignments;
}

std::vector<storm::prism::Assignment>& Update::getAssignments() {
    return this->assignments;
}

storm::prism::Assignment const& Update::getAssignment(std::string const& variableName) const {
    auto const& variableIndexPair = this->variableToAssignmentIndexMap.find(variableName);
    STORM_LOG_THROW(variableIndexPair != this->variableToAssignmentIndexMap.end(), storm::exceptions::OutOfRangeException,
                    "Variable '" << variableName << "' is not assigned in update.");
    return this->getAssignments()[variableIndexPair->second];
}

std::map<storm::expressions::Variable, storm::expressions::Expression> Update::getAsVariableToExpressionMap() const {
    std::map<storm::expressions::Variable, storm::expressions::Expression> result;

    for (auto const& assignment : this->getAssignments()) {
        result.emplace(assignment.getVariable(), assignment.getExpression());
    }

    return result;
}

uint_fast64_t Update::getGlobalIndex() const {
    return this->globalIndex;
}

void Update::createAssignmentMapping() {
    this->variableToAssignmentIndexMap.clear();
    for (uint_fast64_t assignmentIndex = 0; assignmentIndex < this->getAssignments().size(); ++assignmentIndex) {
        this->variableToAssignmentIndexMap[this->getAssignments()[assignmentIndex].getVariableName()] = assignmentIndex;
    }
}

Update Update::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    std::vector<Assignment> newAssignments;
    newAssignments.reserve(this->getNumberOfAssignments());
    for (auto const& assignment : this->getAssignments()) {
        newAssignments.emplace_back(assignment.substitute(substitution));
    }
    auto subsAndSimpl = [&substitution](storm::expressions::Expression const& expr) {
        auto res = expr.substitute(substitution).simplify();
        STORM_LOG_THROW(isValidLikelihoodSimpl(res), storm::exceptions::IllegalArgumentException,
                        "Substitution yielding negative probabilities in '" << expr << "' are not allowed.");
        return res;
    };
    if (isLikelihoodInterval()) {
        ExpressionPair newLikelihoodExpression(subsAndSimpl(this->getLikelihoodExpressionInterval().first),
                                               subsAndSimpl(this->getLikelihoodExpressionInterval().second));
        return Update(this->getGlobalIndex(), newLikelihoodExpression, newAssignments, this->getFilename(), this->getLineNumber());
    } else {
        auto newLikelihoodExpression = subsAndSimpl(this->getLikelihoodExpression());
        return Update(this->getGlobalIndex(), newLikelihoodExpression, newAssignments, this->getFilename(), this->getLineNumber());
    }
}

Update Update::substituteNonStandardPredicates() const {
    std::vector<Assignment> newAssignments;
    newAssignments.reserve(this->getNumberOfAssignments());
    for (auto const& assignment : this->getAssignments()) {
        newAssignments.emplace_back(assignment.substituteNonStandardPredicates());
    }
    auto subsAndSimpl = [](storm::expressions::Expression const& expr) {
        auto res = expr.substituteNonStandardPredicates().simplify();
        STORM_LOG_THROW(isValidLikelihoodSimpl(res), storm::exceptions::IllegalArgumentException,
                        "Predicate substitution yielding negative probabilities in '" << expr << "' are not allowed.");
        return res;
    };
    if (isLikelihoodInterval()) {
        ExpressionPair newLikelihoodExpression(subsAndSimpl(this->getLikelihoodExpressionInterval().first),
                                               subsAndSimpl(this->getLikelihoodExpressionInterval().second));
        return Update(this->getGlobalIndex(), newLikelihoodExpression, newAssignments, this->getFilename(), this->getLineNumber());
    } else {
        auto newLikelihoodExpression = subsAndSimpl(this->getLikelihoodExpression());
        return Update(this->getGlobalIndex(), newLikelihoodExpression, newAssignments, this->getFilename(), this->getLineNumber());
    }
}

Update Update::removeIdentityAssignments() const {
    std::vector<Assignment> newAssignments;
    newAssignments.reserve(getNumberOfAssignments());
    for (auto const& ass : this->assignments) {
        if (!ass.isIdentity()) {
            newAssignments.push_back(ass);
        }
    }
    return Update(this->globalIndex, this->likelihoodExpressions, newAssignments, getFilename(), getLineNumber());
}

Update Update::simplify() const {
    std::vector<Assignment> newAssignments;
    newAssignments.reserve(getNumberOfAssignments());
    for (auto const& ass : this->assignments) {
        Assignment newAssignment(ass.getVariable(), ass.getExpression().simplify().reduceNesting(), ass.getFilename(), ass.getLineNumber());
        if (!newAssignment.isIdentity()) {
            newAssignments.push_back(std::move(newAssignment));
        }
    }
    if (isLikelihoodInterval()) {
        ExpressionPair simplLikelihoodExpr(getLikelihoodExpressionInterval().first.simplify().reduceNesting(),
                                           getLikelihoodExpressionInterval().second.simplify().reduceNesting());
        return Update(this->globalIndex, simplLikelihoodExpr, newAssignments, getFilename(), getLineNumber());
    } else {
        return Update(this->globalIndex, getLikelihoodExpression().simplify().reduceNesting(), newAssignments, getFilename(), getLineNumber());
    }
}

std::ostream& operator<<(std::ostream& stream, Update const& update) {
    if (update.isLikelihoodInterval()) {
        stream << "[" << update.getLikelihoodExpressionInterval().first << ", " << update.getLikelihoodExpressionInterval().second << "] : ";
    } else {
        stream << update.getLikelihoodExpression() << " : ";
    }
    if (update.getAssignments().empty()) {
        stream << "true";
    } else {
        std::vector<std::string> assignmentsAsStrings;
        std::for_each(update.getAssignments().cbegin(), update.getAssignments().cend(), [&assignmentsAsStrings](Assignment const& assignment) {
            std::stringstream sstream;
            sstream << assignment;
            assignmentsAsStrings.push_back(sstream.str());
        });
        stream << boost::algorithm::join(assignmentsAsStrings, " & ");
    }
    return stream;
}

}  // namespace prism
}  // namespace storm
