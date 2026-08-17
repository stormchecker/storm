#pragma once

#include <ostream>

namespace storm {
namespace modelchecker {
/*!
 * The considered engine for region checking
 */
enum class RegionCheckEngine {
    ParameterLifting,           /*!< Parameter lifting approach */
    ExactParameterLifting,      /*!< Parameter lifting approach with exact arithmethics*/
    ValidatingParameterLifting, /*!< Parameter lifting approach with a) inexact (and fast) computation first and b) exact validation of obtained results
                                   second*/
    RobustParameterLifting,     /*!< Parameter lifting approach based on robust markov models instead of generating nondeterminism*/
};

std::ostream& operator<<(std::ostream& os, RegionCheckEngine const& regionCheckResult);
}  // namespace modelchecker
}  // namespace storm
