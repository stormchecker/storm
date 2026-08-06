#pragma once
#include <boost/optional.hpp>
#include <string>
namespace storm {
namespace derivative {
/**
 * GradientDescentConstraintMethod is the method for mitigating constraints
 * that the GradientDescentInstantiationSearcher uses.
 */
enum class GradientDescentConstraintMethod {
    PROJECT_WITH_GRADIENT,  //< The default.
    PROJECT,
    PENALTY_QUADRATIC,
    BARRIER_LOGARITHMIC,
    BARRIER_INFINITY,
    LOGISTIC_SIGMOID
};
}  // namespace derivative
}  // namespace storm
