#pragma once

#include <iostream>
// Include these utility headers so we can access utility function from Eigen.
#include "storm/utility/constants.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Walloca"
#pragma clang diagnostic ignored "-Wunknown-warning-option"  // AppleClang 15 does not recognize -Wdeprecated-redundant-constexpr-static-def
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wunused-template"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Walloc-size-larger-than="
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

// Finally include the parts of Eigen we need.
// Make sure to include our patched version of Eigen (and not a pre-installed one e.g. located at /usr/include)
#include <StormEigen/Eigen/Dense>
#include <StormEigen/Eigen/Sparse>
#include <StormEigen/unsupported/Eigen/IterativeSolvers>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
