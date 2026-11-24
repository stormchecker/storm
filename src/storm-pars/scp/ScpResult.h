#pragma once

#include <cstdint>

#include "storm-pars/utility/FeasibilityResult.h"

namespace storm::pars::scp {

/// How a search ended. SCP is incomplete, so a search that finds no witness (NotFound) does not imply that none exists.
enum class ScpStatus { Feasible, NotFound, MaxIterationsReached, Timeout };

/*!
 * The outcome of an SCP search at ConstantType precision (double or storm::RationalNumber): the best point found
 * (`valuation` and `value`, see FeasibilityResult), together with how the search ended.
 *
 * Only for status == Feasible has `valuation` been verified by model-checking the instantiated
 * model at ConstantType precision; for the other statuses it is a best-effort point.
 */
template<typename ConstantType>
struct ScpResult : FeasibilityResult<ConstantType> {
    ScpStatus status;
    /// The iteration at which (`valuation`, `value`) was established; 0 for the initial point.
    uint64_t iterations;
};

}  // namespace storm::pars::scp
