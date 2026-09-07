#include "ReduceVertexCloud.h"

#include "storm/utility/Stopwatch.h"
#include "storm/utility/constants.h"
#undef _DEBUG_REDUCE_VERTEX_CLOUD

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
std::string toString(std::map<uint64_t, ValueType> const& point) {
    std::stringstream sstr;
    bool first = true;
    for (auto const& entry : point) {
        if (first) {
            first = false;
        } else {
            sstr << ", ";
        }
        sstr << entry.first << " : " << entry.second;
    }
    return sstr.str();
}

template<typename ValueType>
std::pair<storm::storage::BitVector, bool> ReduceVertexCloud<ValueType>::eliminate(std::vector<std::map<uint64_t, ValueType>> const& input,
                                                                                   uint64_t maxdimension) {
    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager = std::make_shared<storm::expressions::ExpressionManager>();
    std::vector<storm::storage::BitVector> supports;
    std::vector<storm::expressions::Variable> weightVariables;
    std::vector<storm::expressions::Expression> weightVariableExpressions;

    for (uint64_t pointIndex = 0; pointIndex < input.size(); ++pointIndex) {
        // Compute the support vectors to quickly determine which input points could be relevant.
        supports.push_back(storm::storage::BitVector(maxdimension));
        for (auto const& entry : input[pointIndex]) {
            supports.back().set(entry.first, true);
        }
        // Add a weight variable for each input point
        weightVariables.push_back(expressionManager->declareRationalVariable("w_" + std::to_string(pointIndex)));
        // For convenience and performance, obtain the expression.
        weightVariableExpressions.push_back(weightVariables.back().getExpression());
    }

    std::unique_ptr<storm::solver::SmtSolver> smtSolver = smtSolverFactory->create(*expressionManager);
    for (auto const& weightVariableExpr : weightVariableExpressions) {
        // smtSolver->add((weightVariableExpr == expressionManager->rational(0.0)) || (weightVariableExpr > expressionManager->rational(0.00001)));
        smtSolver->add((weightVariableExpr >= expressionManager->rational(0.0)));
        smtSolver->add(weightVariableExpr < expressionManager->rational(1.0));
    }
    if (storm::utility::isZero(wiggle)) {
        smtSolver->add(storm::expressions::sum(weightVariableExpressions) <= expressionManager->rational(1));
    } else {
        smtSolver->add(storm::expressions::sum(weightVariableExpressions) <= expressionManager->rational(1 + wiggle));
        smtSolver->add(storm::expressions::sum(weightVariableExpressions) >= expressionManager->rational(1 - wiggle));
    }

    storm::utility::Stopwatch solverTime;
    storm::utility::Stopwatch totalTime(true);
    storm::storage::BitVector vertices(input.size());
    for (uint64_t pointIndex = 0; pointIndex < input.size(); ++pointIndex) {
#ifdef _DEBUG_REUCE_VERTEX_CLOUD
        std::cout << pointIndex << " out of " << input.size() << '\n';
#endif
        smtSolver->push();
        std::map<uint64_t, std::vector<storm::expressions::Expression>> dimensionTerms;
        for (auto const& entry : input[pointIndex]) {
            dimensionTerms[entry.first] = {expressionManager->rational(-entry.second)};
        }
        for (uint64_t potentialSupport = 0; potentialSupport < input.size(); ++potentialSupport) {
            if (pointIndex == potentialSupport) {
                smtSolver->add(weightVariableExpressions[potentialSupport] == expressionManager->rational(0.0));
            } else if (potentialSupport < pointIndex && !vertices.get(potentialSupport)) {
                smtSolver->add(weightVariableExpressions[potentialSupport] == expressionManager->rational(0.0));
            } else if (supports[potentialSupport].isSubsetOf(supports[pointIndex])) {
                for (auto const& entry : input[potentialSupport]) {
                    dimensionTerms[entry.first].push_back(weightVariableExpressions[potentialSupport] * expressionManager->rational(entry.second));
                }
            } else {
                smtSolver->add(weightVariableExpressions[potentialSupport] == expressionManager->rational(0.0));
            }
        }
        for (auto const& entry : dimensionTerms) {
            smtSolver->add(storm::expressions::sum(entry.second) == expressionManager->rational(0.0));
        }

        solverTime.start();
        auto result = smtSolver->check();
        solverTime.stop();
        if (result == storm::solver::SmtSolver::CheckResult::Unsat) {
#ifdef _DEBUG_REDUCE_VERTEX_CLOUD
            if (input[pointIndex].size() == 2) {
                std::cout << "point " << toString(input[pointIndex]) << " is a vertex:";
                std::cout << smtSolver->getSmtLibString() << '\n';
            }
#endif
            vertices.set(pointIndex, true);
        }
#ifdef _DEBUG_REDUCE_VERTEX_CLOUD
        else {
            std::cout << "point " << toString(input[pointIndex]) << " is a convex combination of ";
            auto val = smtSolver->getModelAsValuation();
            uint64_t varIndex = 0;
            for (auto const& wvar : weightVariables) {
                if (!storm::utility::isZero(val.getRationalValue(wvar))) {
                    std::cout << toString(input[varIndex]) << " (weight: " << val.getRationalValue(wvar) << ")";
                }
                varIndex++;
            }
            std::cout << '\n';
        }
        if (timeOut >)
#endif
            if (timeOut > 0 && static_cast<uint64_t>(totalTime.getTimeInMilliseconds()) > timeOut) {
                for (uint64_t remainingPoint = pointIndex + 1; remainingPoint < input.size(); ++remainingPoint) {
                    vertices.set(remainingPoint);
                }
                return {vertices, true};
            }
        smtSolver->pop();
#ifdef _DEBUG_REDUCE_VERTEX_CLOUD
        std::cout << "Solver time " << solverTime.getTimeInMilliseconds() << '\n';
        std::cout << "Total time " << totalTime.getTimeInMilliseconds() << '\n';
#endif
    }
    return {vertices, false};
}

template class ReduceVertexCloud<double>;
template class ReduceVertexCloud<storm::RationalNumber>;
}  // namespace geometry
}  // namespace storage
}  // namespace storm
