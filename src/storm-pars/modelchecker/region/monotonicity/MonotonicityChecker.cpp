#include "MonotonicityChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/solver/Z3SmtSolver.h"

namespace storm {
namespace analysis {
template<typename ValueType>
MonotonicityChecker<ValueType>::MonotonicityChecker(storage::SparseMatrix<ValueType> const& matrix) {
    this->matrix = matrix;
}

template<typename ValueType>
typename MonotonicityChecker<ValueType>::Monotonicity MonotonicityChecker<ValueType>::checkLocalMonotonicity(
    std::shared_ptr<Order> const& order, uint_fast64_t state, VariableType const& var, storage::ParameterRegion<ValueType> const& region) {
    // Create + fill Vector containing the Monotonicity of the transitions to the succs
    auto row = matrix.getRow(state);
    // Ignore if all entries are constant
    bool ignore = true;

    std::vector<uint_fast64_t> succs;
    std::vector<Monotonicity> succsMonUnsorted;
    std::vector<uint_fast64_t> statesIncr;
    std::vector<uint_fast64_t> statesDecr;
    bool checkAllow = true;
    for (auto entry : row) {
        auto succState = entry.getColumn();
        auto mon = checkTransitionMonRes(entry.getValue(), var, region);
        succsMonUnsorted.push_back(mon);
        succs.push_back(succState);
        ignore &= entry.getValue().isConstant();
        if (mon == Monotonicity::Incr) {
            statesIncr.push_back(succState);
        } else if (mon == Monotonicity::Decr) {
            statesDecr.push_back(succState);
        } else if (mon == Monotonicity::Not) {
            checkAllow = false;
        }
    }
    if (ignore) {
        return Monotonicity::Constant;
    }
    auto succsSorted = order->sortStates(&succs);

    uint_fast64_t succSize = succs.size();
    if (succsSorted[succSize - 1] == matrix.getColumnCount()) {
        // Maybe we can still do something
        // If one is decreasing and all others increasing, and this one is above all others or vice versa
        if (checkAllow) {
            if (statesIncr.size() == 1 && statesDecr.size() > 1) {
                auto comp = order->allAboveBelow(statesDecr, statesIncr.back());
                if (comp.first) {
                    // All decreasing states are above the increasing state, therefore decreasing
                    return Monotonicity::Decr;
                } else if (comp.second) {
                    // All decreasing states are below the increasing state, therefore increasing
                    return Monotonicity::Incr;
                }
            } else if (statesDecr.size() == 1 && statesIncr.size() > 1) {
                auto comp = order->allAboveBelow(statesDecr, statesIncr.back());
                if (comp.first) {
                    // All increasing states are below the decreasing state, therefore increasing
                    return Monotonicity::Incr;
                } else if (comp.second) {
                    // All increasing states are above the decreasing state, therefore decreasing
                    return Monotonicity::Decr;
                }
            }
        }

        return Monotonicity::Unknown;
    }

    if (succSize == 2) {
        // In this case we can ignore the last entry, as this will have a probability of 1 - the other
        succSize = 1;
    }

    // First check as long as it stays constant and either incr or decr
    bool allowedToSwap = true;
    Monotonicity localMonotonicity = Monotonicity::Constant;
    uint_fast64_t index = 0;
    while (index < succSize && localMonotonicity == Monotonicity::Constant) {
        auto itr = std::find(succs.begin(), succs.end(), succsSorted[index]);
        auto newIndex = std::distance(succs.begin(), itr);
        auto transitionMon = succsMonUnsorted[newIndex];
        localMonotonicity = transitionMon;
        if (transitionMon == Monotonicity::Not && succSize != 1) {
            localMonotonicity = Monotonicity::Unknown;
        }
        index++;
    }

    while (index < succSize && localMonotonicity != Monotonicity::Not && localMonotonicity != Monotonicity::Unknown) {
        // We get here as soon as we have seen incr/decr once
        auto itr = std::find(succs.begin(), succs.end(), succsSorted[index]);
        auto newIndex = std::distance(succs.begin(), itr);
        auto transitionMon = succsMonUnsorted[newIndex];

        if (transitionMon == Monotonicity::Not || transitionMon == Monotonicity::Unknown) {
            return Monotonicity::Unknown;
        }
        if (allowedToSwap) {
            // So far we have only seen constant and either incr or decr, but not both
            if (transitionMon != Monotonicity::Constant && transitionMon != localMonotonicity) {
                allowedToSwap = false;
            }
        } else if (!allowedToSwap) {
            // So we have been at the point where we changed from incr to decr (or decr to incr)
            if (transitionMon == localMonotonicity || transitionMon == Monotonicity::Not || transitionMon == Monotonicity::Unknown) {
                localMonotonicity = Monotonicity::Unknown;
            }
        }
        index++;
    }
    return localMonotonicity;
}

template<typename ValueType>
std::pair<bool, bool> MonotonicityChecker<ValueType>::checkDerivative(ValueType const& derivative, storage::ParameterRegion<ValueType> const& reg) {
    if (derivative.isZero()) {
        return std::pair<bool, bool>(true, true);
    }
    if (derivative.isConstant()) {
        bool monIncr = derivative.constantPart() >= 0;
        bool monDecr = derivative.constantPart() <= 0;
        return std::pair<bool, bool>(monIncr, monDecr);
    }
    bool monIncr = false;
    bool monDecr = false;

    std::shared_ptr<utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<utility::solver::MathsatSmtSolverFactory>();
    std::shared_ptr<expressions::ExpressionManager> manager(new expressions::ExpressionManager());
    solver::Z3SmtSolver s(*manager);
    std::set<VariableType> variables = derivative.gatherVariables();

    expressions::Expression exprBounds = manager->boolean(true);
    for (auto const& variable : variables) {
        auto managerVariable = manager->declareRationalVariable(variable.name());
        auto lb = utility::convertNumber<RationalNumber>(reg.getLowerBoundary(variable));
        auto ub = utility::convertNumber<RationalNumber>(reg.getUpperBoundary(variable));
        exprBounds = exprBounds && manager->rational(lb) <= managerVariable && managerVariable <= manager->rational(ub);
    }

    auto converter = expressions::RationalFunctionToExpression<ValueType>(manager);

    // < 0, so not monotone increasing. If this is unsat, then it should be monotone increasing.
    expressions::Expression exprToCheck = converter.toExpression(derivative) < manager->rational(0);
    s.add(exprBounds);
    s.add(exprToCheck);
    monIncr = s.check() == solver::SmtSolver::CheckResult::Unsat;

    // > 0, so not monotone decreasing. If this is unsat it should be monotone decreasing.
    exprToCheck = converter.toExpression(derivative) > manager->rational(0);
    s.reset();
    s.add(exprBounds);
    s.add(exprToCheck);
    monDecr = s.check() == solver::SmtSolver::CheckResult::Unsat;

    STORM_LOG_ASSERT(!(monIncr && monDecr), "Error analyzing " << derivative);

    return std::pair<bool, bool>(monIncr, monDecr);
}

/*** Private methods ***/
template<typename ValueType>
typename MonotonicityChecker<ValueType>::Monotonicity MonotonicityChecker<ValueType>::checkTransitionMonRes(
    ValueType function, typename MonotonicityChecker<ValueType>::VariableType param, typename MonotonicityChecker<ValueType>::Region region) {
    std::pair<bool, bool> res = MonotonicityChecker<ValueType>::checkDerivative(getDerivative(function, param), region);
    if (res.first && !res.second) {
        return Monotonicity::Incr;
    } else if (!res.first && res.second) {
        return Monotonicity::Decr;
    } else if (res.first && res.second) {
        return Monotonicity::Constant;
    } else {
        return Monotonicity::Not;
    }
}

template<typename ValueType>
ValueType& MonotonicityChecker<ValueType>::getDerivative(ValueType function, typename MonotonicityChecker<ValueType>::VariableType var) {
    auto& derivativeMap = derivatives[function];
    if (derivativeMap.find(var) == derivativeMap.end()) {
        derivativeMap[var] = function.derivative(var);
    }
    return derivativeMap[var];
}

template class MonotonicityChecker<RationalFunction>;
}  // namespace analysis
}  // namespace storm
