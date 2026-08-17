#pragma once

#include "storm/adapters/RationalFunctionForward.h"

#include <map>
#include <set>

namespace storm {
namespace utility {
namespace parametric {

/*!
 * Access the type of variables from a given function type
 */
template<typename FunctionType>
struct VariableType {
    typedef void type;
};

/*!
 * Acess the type of coefficients from a given function type
 */
template<typename FunctionType>
struct CoefficientType {
    typedef void type;
};

template<>
struct VariableType<storm::RationalFunction> {
    typedef storm::RationalFunctionVariable type;
};
template<>
struct CoefficientType<storm::RationalFunction> {
    typedef storm::RationalFunctionCoefficient type;
};

template<typename FunctionType>
using VariableType_t = typename VariableType<FunctionType>::type;
template<typename FunctionType>
using CoefficientType_t = typename CoefficientType<FunctionType>::type;

template<typename FunctionType>
using Valuation = std::map<typename VariableType<FunctionType>::type, typename CoefficientType<FunctionType>::type>;

/*!
 * Evaluates the given function wrt. the given valuation and returns the required type.
 */
template<typename ReturnType, typename FunctionType>
ReturnType evaluate(FunctionType const& function, Valuation<FunctionType> const& valuation);

/*!
 * Evaluates the given function wrt. the given valuation
 */
template<typename FunctionType>
FunctionType substitute(FunctionType const& function, Valuation<FunctionType> const& valuation);

/*!
 *  Add all variables that occur in the given function to the the given set
 */
template<typename FunctionType>
void gatherOccurringVariables(FunctionType const& function, std::set<typename VariableType<FunctionType>::type>& variableSet);

/*!
 *  Checks whether the function is linear (in one parameter)
 */
template<typename FunctionType>
bool isLinear(FunctionType const& function);

/*!
 *  Checks whether the function is a multilinear polynomial, i.e., a polynomial which only considers variables with exponent at most 1
 */
template<typename FunctionType>
bool isMultiLinearPolynomial(FunctionType const& function);

}  // namespace parametric
}  // namespace utility
}  // namespace storm
