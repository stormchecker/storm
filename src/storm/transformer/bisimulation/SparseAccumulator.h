#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>

namespace storm::bisimulation {

/*!
 * Accumulates a value for each state (numbers are added up, elements are inserted into sets, c.f. WrappingSetAccumulator) and keeps track
 * of the states whose value differs from the default value, i.e., zero or the empty set. Accumulating and reading take constant time, and clearing only takes
 * time linear in the number of touched states, so a single instance can be reused for many small computations. The memory consumption is linear in the total
 * number of states, though.
 * This data structure is also known as a sparse accumulator.
 */
template<typename ValueType>
class SparseAccumulator {
   public:
    explicit SparseAccumulator(uint64_t const numStates);

    /*!
     * @return the currently stored values
     */
    std::vector<ValueType> const& getValues() const;

    /*!
     * @return the list of states currently holding a non-default value
     */
    std::vector<uint64_t> const& getNonDefaultStates() const;

    /*!
     * Adds value to the currently mapped value of the given state, i.e., inserts it into the set of that state if the values are sets.
     */
    void addValue(uint64_t const state, ValueType value);

    /*!
     * Clears the set, i.e., writes the default value for all states.
     */
    void clear();

   private:
    static ValueType defaultValue();

    std::vector<ValueType> values;           // stores the value for each state
    std::vector<uint64_t> nonDefaultStates;  // stores those states with a non-default value
};

/*!
 * Accumulates a set of indices for each state, where only the index modulo 64 is stored, namely as a bit of a 64 bit mask. Two states that received the same
 * indices thus always get the same value, whereas the values of two states that received different indices might coincide. In exchange, adding an index takes
 * constant time without allocating any memory and the values can be compared like plain integers.
 */
using WrappingSetAccumulator = SparseAccumulator<uint64_t>;

}  // namespace storm::bisimulation
