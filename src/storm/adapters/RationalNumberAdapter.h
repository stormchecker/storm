#pragma once

#include "storm/adapters/RationalNumberForward.h"

#if defined(STORM_HAVE_CLN)
#include <cln/cln.h>
#endif

#if defined(STORM_HAVE_GMP)
// Disable potential warning on newer AppleClang versions
#if __GNUC__ && defined(__has_warning)
#if __has_warning("-Wdeprecated-literal-operator")
#define SUPPRESSING
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#endif

#include <gmpxx.h>

#ifdef SUPPRESSING
#undef SUPPRESSING
#pragma GCC diagnostic pop
#endif

#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/numbers/numbers.h>
#pragma clang diagnostic pop

#if defined(STORM_HAVE_CLN)
namespace cln {
inline size_t hash_value(cl_RA const& n) {
    std::hash<cln::cl_RA> h;
    return h(n);
}
}  // namespace cln
#endif

#if defined(STORM_HAVE_GMP)
inline size_t hash_value(mpq_class const& q) {
    std::hash<mpq_class> h;
    return h(q);
}
#endif
