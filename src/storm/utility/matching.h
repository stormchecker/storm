#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "storm/storage/BitVector.h"

namespace storm::utility {
/*
 * Finds a bijective mapping f: {0,...,n-1} -> {0,...,n-1} such that hasEdge(v, f(v)) is true for all v.
 * If no such mapping exists, returns std::nullopt.
 * The algorithm  finds a maximum matching in the bipartite graph defined by hasEdge
 */
template<std::predicate<uint64_t, uint64_t> EdgePredicate>
std::optional<std::vector<uint64_t>> findPerfectMatching(uint64_t const n, EdgePredicate&& hasEdge) {
    // We follow Kuhn's augmenting path algorithm to find a maximum matching.
    constexpr uint64_t None = std::numeric_limits<uint64_t>::max();
    std::vector<uint64_t> f(n, None);  // f[v] = image vertex assigned to v
    storm::storage::BitVector available(n, true);
    // Try to find an augmenting path that matches image vertex u.
    // (The lambda takes itself as a parameter so it can recurse)
    auto augment = [&](auto& self, uint64_t const u) -> bool {
        for (uint64_t const v : available) {
            if (hasEdge(v, u)) {
                available.set(v, false);
                // v is free, or its current partner can be re-matched elsewhere
                if (f[v] == None || self(self, f[v])) {
                    f[v] = u;
                    return true;
                }
            }
        }
        return false;
    };

    // Match every image vertex (a matching saturating the image side).
    for (uint64_t u = 0; u < n; ++u) {
        available.fill();
        if (!augment(augment, u)) {
            return std::nullopt;  // Hall's condition violated
        }
    }
    return f;
}
}  // namespace storm::utility
