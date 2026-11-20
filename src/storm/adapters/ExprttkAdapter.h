#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"

// exprtk should be case sensitive in our case.
#define exprtk_disable_caseinsensitivity

// exprtk's template instantiations must not leak out of libstorm.
#pragma GCC visibility push(hidden)
#include "exprtk.hpp"
#pragma GCC visibility pop

#pragma clang diagnostic pop
