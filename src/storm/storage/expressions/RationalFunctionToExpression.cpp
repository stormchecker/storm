#include "RationalFunctionToExpression.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/constants.h"

namespace storm {
namespace expressions {
template<typename ValueType>
RationalFunctionToExpression<ValueType>::RationalFunctionToExpression(std::shared_ptr<ExpressionManager> manager) : manager(manager) {
    // Intentionally left empty.
}

template<typename ValueType>
std::shared_ptr<ExpressionManager> RationalFunctionToExpression<ValueType>::getManager() {
    return manager;
}

template<typename ValueType>
Expression RationalFunctionToExpression<ValueType>::toExpression(ValueType function) {
    function.simplify();
    auto varsFunction = function.gatherVariables();
    for (auto var : varsFunction) {
        auto varsManager = manager->getVariables();
        bool found = find_if(varsManager.begin(), varsManager.end(), [&](auto val) -> bool { return val.getName() == var.name(); }) != varsManager.end();
        if (!found) {
            manager->declareRationalVariable(var.name());
        }
    }

    auto denominator = function.denominator();
    if (!denominator.isConstant()) {
        STORM_LOG_DEBUG("Expecting the denominator to be constant");
    }

    storm::expressions::Expression result;
    if (function.isConstant()) {
        result = manager->rational(storm::utility::convertNumber<storm::RationalNumber, storm::RationalFunctionCoefficient>(function.constantPart()));
    } else {
        auto nominator = function.nominatorAsPolynomial().polynomialWithCoefficient();
        result = manager->rational(storm::utility::convertNumber<storm::RationalNumber, storm::RationalFunctionCoefficient>(nominator.constantPart()));
        for (auto itr = nominator.begin(); itr != nominator.end(); ++itr) {
            varsFunction.clear();
            itr->gatherVariables(varsFunction);

            storm::expressions::Expression nominatorPartExpr =
                manager->rational(storm::utility::convertNumber<storm::RationalNumber, storm::RationalFunctionCoefficient>(itr->coeff()));
            for (auto var : varsFunction) {
                nominatorPartExpr =
                    nominatorPartExpr *
                    storm::expressions::pow(manager->getVariable(var.name()),
                                            manager->rational(storm::utility::convertNumber<storm::RationalNumber>(itr->monomial()->exponentOfVariable(var))));
            }
            if (varsFunction.size() >= 1) {
                result = result + nominatorPartExpr;
            }
        }
        storm::expressions::Expression denominatorVal =
            manager->rational(storm::utility::convertNumber<storm::RationalNumber, storm::RationalFunctionCoefficient>(denominator.constantPart()));
        result = result / denominatorVal;
    }

    return result;
}

template class RationalFunctionToExpression<storm::RationalFunction>;
}  // namespace expressions
}  // namespace storm
