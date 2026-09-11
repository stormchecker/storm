#pragma once

#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionForward.h"
#include "storm/adapters/RationalNumberForward.h"

#include <cstdint>

namespace storm {
template<typename ValueType>
struct NumberTraits {
    static const bool SupportsExponential = false;
    static const bool IsExact = false;
    /// Whether the type can represent +/- infinity itself.
    static const bool HasInfinity = false;
};

template<>
struct NumberTraits<double> {
    static const bool SupportsExponential = true;
    static const bool IsExact = false;
    static const bool HasInfinity = true;

    typedef uint64_t IntegerType;
};

#if defined(STORM_HAVE_CLN)
template<>
struct NumberTraits<storm::ClnRationalNumber> {
    static const bool SupportsExponential = false;
    static const bool IsExact = true;
    static const bool HasInfinity = false;

    typedef ClnIntegerNumber IntegerType;
};
#endif

#if defined(STORM_HAVE_GMP)
template<>
struct NumberTraits<storm::GmpRationalNumber> {
    static const bool SupportsExponential = false;
    static const bool IsExact = true;
    static const bool HasInfinity = false;

    typedef GmpIntegerNumber IntegerType;
};
#endif

template<>
struct NumberTraits<storm::Interval> {
    static const bool SupportsExponential = true;
    static const bool IsExact = false;
    static const bool HasInfinity = false;
};

template<>
struct NumberTraits<storm::RationalInterval> {
    static const bool SupportsExponential = true;
    static const bool IsExact = true;
    static const bool HasInfinity = false;
};

template<>
struct NumberTraits<storm::RationalFunction> {
    static const bool SupportsExponential = false;
    static const bool IsExact = true;
    static const bool HasInfinity = false;
};
}  // namespace storm
