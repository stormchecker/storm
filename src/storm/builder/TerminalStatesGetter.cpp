#include "storm/builder/TerminalStatesGetter.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/Formulas.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace builder {
void getTerminalStatesFromFormula(storm::logic::Formula const& formula,
                                  std::function<void(storm::expressions::Expression const&, bool)> const& terminalExpressionCallback,
                                  std::function<void(std::string const&, bool)> const& terminalLabelCallback) {
    auto isAtomic = [](auto const& f) { return f.isAtomicExpressionFormula() || f.isAtomicLabelFormula() || f.isBooleanLiteralFormula(); };
    if (formula.isAtomicExpressionFormula()) {
        terminalExpressionCallback(formula.asAtomicExpressionFormula().getExpression(), true);
    } else if (formula.isAtomicLabelFormula()) {
        terminalLabelCallback(formula.asAtomicLabelFormula().getLabel(), true);
    } else if (formula.isEventuallyFormula()) {
        storm::logic::Formula const& sub = formula.asEventuallyFormula().getSubformula();
        if (isAtomic(sub)) {
            getTerminalStatesFromFormula(sub, terminalExpressionCallback, terminalLabelCallback);
        }
    } else if (formula.isUntilFormula()) {
        storm::logic::Formula const& right = formula.asUntilFormula().getRightSubformula();
        storm::logic::Formula const& left = formula.asUntilFormula().getLeftSubformula();
        if (isAtomic(left) && isAtomic(right)) {
            getTerminalStatesFromFormula(right, terminalExpressionCallback, terminalLabelCallback);
            if (left.isAtomicExpressionFormula()) {
                terminalExpressionCallback(left.asAtomicExpressionFormula().getExpression(), false);
            } else if (left.isAtomicLabelFormula()) {
                terminalLabelCallback(left.asAtomicLabelFormula().getLabel(), false);
            }
        }
    } else if (formula.isBoundedUntilFormula() && !formula.asBoundedUntilFormula().hasMultiDimensionalSubformulas()) {
        storm::logic::BoundedUntilFormula const& boundedUntil = formula.asBoundedUntilFormula();
        storm::logic::Formula const& right = boundedUntil.getRightSubformula();
        storm::logic::Formula const& left = boundedUntil.getLeftSubformula();
        if (isAtomic(left) && isAtomic(right)) {
            bool hasLowerBound = false;
            for (uint64_t i = 0; i < boundedUntil.getDimension(); ++i) {
                if (boundedUntil.hasLowerBound(i) &&
                    (boundedUntil.getLowerBound(i).containsVariables() || !storm::utility::isZero(boundedUntil.getLowerBound(i).evaluateAsRational()))) {
                    hasLowerBound = true;
                    break;
                }
            }
            if (!hasLowerBound) {
                getTerminalStatesFromFormula(right, terminalExpressionCallback, terminalLabelCallback);
            }
            if (left.isAtomicExpressionFormula()) {
                terminalExpressionCallback(left.asAtomicExpressionFormula().getExpression(), false);
            } else if (left.isAtomicLabelFormula()) {
                terminalLabelCallback(left.asAtomicLabelFormula().getLabel(), false);
            }
        }
    } else if (formula.isProbabilityOperatorFormula()) {
        storm::logic::Formula const& sub = formula.asProbabilityOperatorFormula().getSubformula();
        if (sub.isEventuallyFormula() || sub.isUntilFormula() || sub.isBoundedUntilFormula()) {
            getTerminalStatesFromFormula(sub, terminalExpressionCallback, terminalLabelCallback);
        }
    } else if (formula.isRewardOperatorFormula() || formula.isTimeOperatorFormula()) {
        storm::logic::Formula const& sub = formula.asOperatorFormula().getSubformula();
        if (sub.isEventuallyFormula()) {
            getTerminalStatesFromFormula(sub, terminalExpressionCallback, terminalLabelCallback);
        }
    }
}

void TerminalStates::clear() {
    terminalExpressions.clear();
    negatedTerminalExpressions.clear();
    terminalLabels.clear();
    negatedTerminalLabels.clear();
}

bool TerminalStates::empty() const {
    return terminalExpressions.empty() && negatedTerminalExpressions.empty() && terminalLabels.empty() && negatedTerminalLabels.empty();
}

storm::expressions::Expression TerminalStates::asExpression(
    std::function<storm::expressions::Expression(std::string const&)> const& labelToExpressionMap) const {
    auto allTerminalExpressions = terminalExpressions;
    for (auto const& e : negatedTerminalExpressions) {
        allTerminalExpressions.push_back(!e);
    }
    for (auto const& l : terminalLabels) {
        allTerminalExpressions.push_back(labelToExpressionMap(l));
    }
    for (auto const& l : negatedTerminalLabels) {
        allTerminalExpressions.push_back(!labelToExpressionMap(l));
    }
    STORM_LOG_ASSERT(!allTerminalExpressions.empty(), "Unable to convert empty terminal state set to expression.");
    return storm::expressions::disjunction(allTerminalExpressions);
}

TerminalStates getTerminalStatesFromFormula(storm::logic::Formula const& formula) {
    TerminalStates result;
    getTerminalStatesFromFormula(
        formula,
        [&result](storm::expressions::Expression const& expr, bool inverted) {
            if (inverted) {
                result.terminalExpressions.push_back(expr);
            } else {
                result.negatedTerminalExpressions.push_back(expr);
            }
        },
        [&result](std::string const& label, bool inverted) {
            if (inverted) {
                result.terminalLabels.push_back(label);
            } else {
                result.negatedTerminalLabels.push_back(label);
            }
        });
    return result;
}

}  // namespace builder
}  // namespace storm