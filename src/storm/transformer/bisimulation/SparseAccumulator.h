#pragma once

#include <cstdint>
#include <set>
#include <type_traits>
#include <vector>

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
    /// The type of the values that can be added: numbers for numeric value types, elements for sets.
    using AddedValueType = std::conditional_t<std::is_same_v<ValueType, std::set<uint64_t>>, uint64_t, ValueType>;

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
     * Adds value to the currently mapped value of the given state
     */
    void addValue(uint64_t const state, AddedValueType value);

    /*!
     * Clears the set, i.e., writes the default value for all states.
     */
    void clear();

   private:
    static ValueType defaultValue();

    std::vector<ValueType> values;           // stores the value for each state
    std::vector<uint64_t> nonDefaultStates;  // stores those states with a non-default value
};

}  // namespace storm::bisimulation
