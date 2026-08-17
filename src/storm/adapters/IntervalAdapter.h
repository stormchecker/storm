#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/interval/Interval.h>
#pragma clang diagnostic pop

#include "storm/adapters/IntervalForward.h"

namespace carl {
template<typename Number>
inline size_t hash_value(carl::Interval<Number> const& i) {
    std::hash<carl::Interval<Number>> h;
    return h(i);
}
}  // namespace carl
