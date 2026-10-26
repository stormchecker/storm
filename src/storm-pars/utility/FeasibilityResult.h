#pragma once

#include <map>

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm::pars {

/*!
 * The result of a feasibility synthesis: a parameter valuation and the value at the initial state under
 * this valuation, in ConstantType precision. The value is extended so that infinite rewards are represented.
 */
template<typename ConstantType>
struct FeasibilityResult {
    storm::utility::ExtendedValueType<ConstantType> value;
    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
};

}  // namespace storm::pars
