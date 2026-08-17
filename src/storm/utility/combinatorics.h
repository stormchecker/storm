#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace storm {
namespace utility {
namespace combinatorics {

template<typename IteratorType>
void forEach(std::vector<IteratorType> const& its, std::vector<IteratorType> const& ites,
             std::function<void(uint64_t, decltype(*std::declval<IteratorType>()))> const& setValueCallback,
             std::function<bool()> const& newCombinationCallback) {
    STORM_LOG_ASSERT(its.size() == ites.size(), "Iterator begin/end mismatch.");

    if (its.size() == 0) {
        return;
    }

    bool allNonEmpty = true;
    for (uint64_t index = 0; index < its.size(); ++index) {
        if (its[index] == ites[index]) {
            allNonEmpty = false;
            break;
        }
    }
    if (!allNonEmpty) {
        return;
    }

    std::vector<IteratorType> currentIterators(its);

    // Fill the initial combination.
    for (uint64_t index = 0; index < currentIterators.size(); ++index) {
        setValueCallback(index, *currentIterators[index]);
    }

    // Enumerate all combinations until the callback yields false (or there are no more combinations).
    while (true) {
        bool cont = newCombinationCallback();
        if (!cont) {
            break;
        }

        uint64_t index = 0;
        for (; index < currentIterators.size(); ++index) {
            ++currentIterators[index];
            if (currentIterators[index] == ites[index]) {
                currentIterators[index] = its[index];
            } else {
                break;
            }
        }

        // If we are at the end, leave the loop.
        if (index == currentIterators.size()) {
            break;
        } else {
            for (uint64_t j = 0; j <= index; ++j) {
                setValueCallback(j, *currentIterators[j]);
            }
        }
    }
}

}  // namespace combinatorics
}  // namespace utility
}  // namespace storm
