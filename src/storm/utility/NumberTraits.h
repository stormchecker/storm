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
};

template<>
struct NumberTraits<double> {
    static const bool SupportsExponential = true;
    static const bool IsExact = false;

    typedef uint64_t IntegerType;
};

#if defined(STORM_HAVE_CLN)
template<>
struct NumberTraits<storm::ClnRationalNumber> {
    static const bool SupportsExponential = false;
    static const bool IsExact = true;

    typedef ClnIntegerNumber IntegerType;
};
#endif

#if defined(STORM_HAVE_GMP)
template<>
struct NumberTraits<storm::GmpRationalNumber> {
    static const bool SupportsExponential = false;
    static const bool IsExact = true;

    typedef GmpIntegerNumber IntegerType;
};
#endif

template<>
struct NumberTraits<storm::Interval> {
    static const bool SupportsExponential = true;
    static const bool IsExact = false;
};

template<>
struct NumberTraits<storm::RationalInterval> {
    static const bool SupportsExponential = true;
    static const bool IsExact = true;
};

template<>
struct NumberTraits<storm::RationalFunction> {
    static const bool SupportsExponential = false;
    static const bool IsExact = true;
};
}  // namespace storm
