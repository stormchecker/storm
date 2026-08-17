#pragma once

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename T>
T squaredEuclideanDistance(std::vector<T> const& p, std::vector<T> const& q) {
    STORM_LOG_ASSERT(p.size() == q.size(), "Invalid dimensions of input vectors.");
    T squaredSum = storm::utility::zero<T>();
    auto pIt = p.begin();
    auto pItE = p.end();
    auto qIt = q.begin();
    for (; pIt != pItE; ++pIt, ++qIt) {
        T diff = *pIt - *qIt;
        squaredSum += diff * diff;
    }
    return squaredSum;
}

}  // namespace geometry
}  // namespace storage
}  // namespace storm