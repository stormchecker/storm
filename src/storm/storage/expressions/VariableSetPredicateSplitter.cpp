#include "VariableSetPredicateSplitter.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {

VariableSetPredicateSplitter::VariableSetPredicateSplitter(std::set<storm::expressions::Variable> const& irrelevantVariables)
    : irrelevantVariables(irrelevantVariables) {
    // Intentionally left empty.
}

std::vector<storm::expressions::Expression> VariableSetPredicateSplitter::split(storm::expressions::Expression const& expression) {
    STORM_LOG_THROW(expression.hasBooleanType(), storm::exceptions::InvalidArgumentException, "Expected predicate of boolean type.");

    // Gather all atoms.
    resultPredicates.clear();
    expression.accept(*this, boost::none);

    // Remove all boolean literals from the atoms.
    std::vector<storm::expressions::Expression> expressionsToKeep;
    for (auto const& atom : resultPredicates) {
        if (!atom.isTrue() && !atom.isFalse()) {
            expressionsToKeep.push_back(atom);
        }
    }
    resultPredicates = std::move(expressionsToKeep);

    return resultPredicates;
}

boost::any VariableSetPredicateSplitter::visit(IfThenElseExpression const& expression, boost::any const&) {
    std::set<storm::expressions::Variable> conditionVariables;
    expression.getCondition()->gatherVariables(conditionVariables);
    bool conditionOnlyIrrelevantVariables =
        std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), conditionVariables.begin(), conditionVariables.end());

    std::set<storm::expressions::Variable> tmp;
    std::set_intersection(conditionVariables.begin(), conditionVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(),
                          std::inserter(tmp, tmp.begin()));
    bool conditionHasIrrelevantVariables = !tmp.empty();

    std::set<storm::expressions::Variable> thenVariables;
    expression.getThenExpression()->gatherVariables(thenVariables);
    bool thenOnlyIrrelevantVariables = std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), thenVariables.begin(), thenVariables.end());

    tmp.clear();
    std::set_intersection(thenVariables.begin(), thenVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(), std::inserter(tmp, tmp.begin()));
    bool thenHasIrrelevantVariables = !tmp.empty();

    std::set<storm::expressions::Variable> elseVariables;
    expression.getElseExpression()->gatherVariables(elseVariables);
    bool elseOnlyIrrelevantVariables = std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), elseVariables.begin(), elseVariables.end());

    tmp.clear();
    std::set_intersection(elseVariables.begin(), elseVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(), std::inserter(tmp, tmp.begin()));
    bool elseHasIrrelevantVariables = !tmp.empty();

    if (conditionHasIrrelevantVariables || thenHasIrrelevantVariables || elseHasIrrelevantVariables) {
        STORM_LOG_THROW(conditionOnlyIrrelevantVariables && thenOnlyIrrelevantVariables && elseOnlyIrrelevantVariables,
                        storm::exceptions::InvalidArgumentException,
                        "Cannot split expression based on variable set as variables of different type are related.");
    } else {
        resultPredicates.push_back(expression.toExpression());
    }
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    std::set<storm::expressions::Variable> leftContainedVariables;
    expression.getFirstOperand()->gatherVariables(leftContainedVariables);
    bool leftOnlyIrrelevantVariables =
        std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), leftContainedVariables.begin(), leftContainedVariables.end());

    std::set<storm::expressions::Variable> tmp;
    std::set_intersection(leftContainedVariables.begin(), leftContainedVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(),
                          std::inserter(tmp, tmp.begin()));
    bool leftHasIrrelevantVariables = !tmp.empty();

    std::set<storm::expressions::Variable> rightContainedVariables;
    expression.getSecondOperand()->gatherVariables(rightContainedVariables);
    bool rightOnlyIrrelevantVariables =
        std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), rightContainedVariables.begin(), rightContainedVariables.end());

    tmp.clear();
    std::set_intersection(rightContainedVariables.begin(), rightContainedVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(),
                          std::inserter(tmp, tmp.begin()));
    bool rightHasIrrelevantVariables = !tmp.empty();

    if (leftOnlyIrrelevantVariables && rightOnlyIrrelevantVariables) {
        return boost::any();
    }

    if (!leftHasIrrelevantVariables && !rightHasIrrelevantVariables) {
        resultPredicates.push_back(expression.toExpression());
    }

    if (!leftHasIrrelevantVariables) {
        resultPredicates.push_back(expression.getFirstOperand()->toExpression());
    } else if (!leftOnlyIrrelevantVariables) {
        return expression.getFirstOperand()->accept(*this, data);
    }

    if (!rightHasIrrelevantVariables) {
        resultPredicates.push_back(expression.getSecondOperand()->toExpression());
    } else if (!rightOnlyIrrelevantVariables) {
        return expression.getSecondOperand()->accept(*this, data);
    }
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(BinaryNumericalFunctionExpression const&, boost::any const&) {
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(BinaryRelationExpression const& expression, boost::any const&) {
    std::set<storm::expressions::Variable> leftContainedVariables;
    expression.getFirstOperand()->gatherVariables(leftContainedVariables);
    bool leftOnlyIrrelevantVariables =
        std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), leftContainedVariables.begin(), leftContainedVariables.end());

    std::set<storm::expressions::Variable> tmp;
    std::set_intersection(leftContainedVariables.begin(), leftContainedVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(),
                          std::inserter(tmp, tmp.begin()));
    bool leftHasIrrelevantVariables = !tmp.empty();

    std::set<storm::expressions::Variable> rightContainedVariables;
    expression.getSecondOperand()->gatherVariables(rightContainedVariables);
    bool rightOnlyIrrelevantVariables =
        std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), rightContainedVariables.begin(), rightContainedVariables.end());

    tmp.clear();
    std::set_intersection(rightContainedVariables.begin(), rightContainedVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(),
                          std::inserter(tmp, tmp.begin()));
    bool rightHasIrrelevantVariables = !tmp.empty();

    if (!leftHasIrrelevantVariables && !rightHasIrrelevantVariables) {
        resultPredicates.push_back(expression.toExpression());
    } else {
        STORM_LOG_THROW(leftOnlyIrrelevantVariables && rightOnlyIrrelevantVariables, storm::exceptions::InvalidArgumentException,
                        "Cannot abstract from variable set in expression as it mixes variables of different types.");
    }
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(VariableExpression const& expression, boost::any const&) {
    if (expression.hasBooleanType() && irrelevantVariables.find(expression.getVariable()) == irrelevantVariables.end()) {
        resultPredicates.push_back(expression.toExpression());
    }
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    std::set<storm::expressions::Variable> containedVariables;
    expression.gatherVariables(containedVariables);
    bool onlyIrrelevantVariables = std::includes(irrelevantVariables.begin(), irrelevantVariables.end(), containedVariables.begin(), containedVariables.end());

    if (onlyIrrelevantVariables) {
        return boost::any();
    }

    std::set<storm::expressions::Variable> tmp;
    std::set_intersection(containedVariables.begin(), containedVariables.end(), irrelevantVariables.begin(), irrelevantVariables.end(),
                          std::inserter(tmp, tmp.begin()));
    bool hasIrrelevantVariables = !tmp.empty();

    if (hasIrrelevantVariables) {
        expression.getOperand()->accept(*this, data);
    } else {
        resultPredicates.push_back(expression.toExpression());
    }
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(UnaryNumericalFunctionExpression const&, boost::any const&) {
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(BooleanLiteralExpression const&, boost::any const&) {
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(IntegerLiteralExpression const&, boost::any const&) {
    return boost::any();
}

boost::any VariableSetPredicateSplitter::visit(RationalLiteralExpression const&, boost::any const&) {
    return boost::any();
}

}  // namespace expressions
}  // namespace storm
