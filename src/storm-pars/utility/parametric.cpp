#include "storm-pars/utility/parametric.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/constants.h"

namespace storm {
namespace utility {
namespace parametric {

// Partially instantiated function
template<typename ReturnType>
ReturnType evaluateRationalFunction(storm::RationalFunction const& function, Valuation<storm::RationalFunction> const& valuation) {
    if constexpr (std::is_same<ReturnType, typename CoefficientType<storm::RationalFunction>::type>::value) {
        return function.evaluate(valuation);
    } else {
        return storm::utility::convertNumber<ReturnType>(function.evaluate(valuation));
    }
}

template<>
double evaluate(storm::RationalFunction const& function, Valuation<storm::RationalFunction> const& valuation) {
    return evaluateRationalFunction<double>(function, valuation);
}

#if defined(STORM_HAVE_CLN)
template<>
ClnRationalNumber evaluate(storm::RationalFunction const& function, Valuation<storm::RationalFunction> const& valuation) {
    return evaluateRationalFunction<ClnRationalNumber>(function, valuation);
}
#endif

#if defined(STORM_HAVE_GMP)
template<>
GmpRationalNumber evaluate(storm::RationalFunction const& function, Valuation<storm::RationalFunction> const& valuation) {
    return evaluateRationalFunction<GmpRationalNumber>(function, valuation);
}
#endif

template<>
typename storm::RationalFunction substitute<storm::RationalFunction>(storm::RationalFunction const& function,
                                                                     Valuation<storm::RationalFunction> const& valuation) {
    return function.substitute(valuation);
}

template<>
void gatherOccurringVariables<storm::RationalFunction>(storm::RationalFunction const& function,
                                                       std::set<typename VariableType<storm::RationalFunction>::type>& variableSet) {
    function.gatherVariables(variableSet);
}

template<>
bool isLinear<storm::RationalFunction>(storm::RationalFunction const& function) {
    return storm::utility::isConstant(function.denominator()) && function.nominator().isLinear();
}

template<>
bool isMultiLinearPolynomial<storm::RationalFunction>(storm::RationalFunction const& function) {
    if (!storm::utility::isConstant(function.denominator())) {
        return false;
    }
    auto varInfos = function.nominator().getVarInfo<false>();
    for (auto const& varInfo : varInfos) {
        if (varInfo.second.maxDegree() > 1) {
            return false;
        }
    }
    return true;
}
}  // namespace parametric
}  // namespace utility
}  // namespace storm
