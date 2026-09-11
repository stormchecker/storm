#pragma once

#include <cstdint>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

/*!
 * Accumulates a value for each state (numbers are added up, elements are inserted into sets) and keeps track of the states whose value differs from the
 * default value, i.e., zero or the empty set. Accumulating and reading take constant time, and clearing only takes time linear in the number of touched
 * states, so a single instance can be reused for many small computations. The memory consumption is linear in the total number of states, though.
 * This data structure is also known as a sparse accumulator.
 */
template<typename ValueType>
class SparseAccumulator {
   public:
    explicit SparseAccumulator(uint64_t const numStates) : values(numStates, defaultValue()) {}

    /*!
     * @return the currently stored values
     */
    std::vector<ValueType> const& getValues() const {
        return values;
    }

    /*!
     * @return the list of states currently holding a non-default value
     */
    std::vector<uint64_t> const& getNonDefaultStates() const {
        return nonDefaultStates;
    }

    /*!
     * Adds value to the currently mapped value of the given state
     */
    template<typename T>
    void addValue(uint64_t const state, T value) {
        if constexpr (std::is_same_v<ValueType, std::set<uint64_t>>) {
            if (values[state].empty()) {
                nonDefaultStates.push_back(state);
            }
            values[state].insert(value);
        } else {
            STORM_LOG_ASSERT(!storm::utility::isZero(value), "Did not expect adding 0 probability");
            if (storm::utility::isZero(values[state])) {
                nonDefaultStates.push_back(state);
                values[state] = std::move(value);
            } else {
                values[state] += std::move(value);
            }
        }
    }

    /*!
     * Clears the set, i.e., writes the default value for all states.
     */
    void clear() {
        for (auto const& state : nonDefaultStates) {
            values[state] = defaultValue();
        }
        nonDefaultStates.clear();
    }

   private:
    static ValueType defaultValue() {
        if constexpr (std::is_same_v<ValueType, std::set<uint64_t>>) {
            return {};  // empty set
        } else {
            return storm::utility::zero<ValueType>();
        }
    }

    std::vector<ValueType> values;           // stores the value for each state
    std::vector<uint64_t> nonDefaultStates;  // stores those states with a non-default value
};

}  // namespace storm::bisimulation
