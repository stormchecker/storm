#pragma once

#include <ostream>

#include <boost/container/container_fwd.hpp>

#include "storm/storage/BoostTypes.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace storage {

// Typedef the most common state container.
typedef storm::storage::FlatSet<sparse::state_type> FlatSetStateContainer;

std::ostream& operator<<(std::ostream& out, FlatSetStateContainer const& block);

class StateBlock {
   public:
    typedef FlatSetStateContainer container_type;
    typedef container_type::value_type value_type;
    static_assert(std::is_same<value_type, sparse::state_type>::value, "Illegal value type of container.");
    typedef container_type::iterator iterator;
    typedef container_type::const_iterator const_iterator;

    // Default constructors.
    StateBlock() = default;
    StateBlock(StateBlock const& other) = default;
    StateBlock(StateBlock&& other) = default;
    StateBlock& operator=(StateBlock const& other) = default;
    StateBlock& operator=(StateBlock&& other) = default;
    /*!
     * Creates a state block and inserts all elements in the given range.
     *
     * @param first The first element of the range to insert.
     * @param last The last element of the range (that is itself not inserted).
     * @param sortedAndUnique If set to true, the input range is assumed to be sorted and duplicate-free.
     */
    template<typename InputIterator>
    StateBlock(InputIterator first, InputIterator last, bool sortedAndUnique = false) {
        if (sortedAndUnique) {
            this->states = container_type(boost::container::ordered_unique_range_t(), first, last);
        } else {
            this->states = container_type(first, last);
        }
    }

    /*!
     * Constructs a state block from the given initializer list.
     *
     * @param list The list of states to add to this state block.
     */
    StateBlock(std::initializer_list<sparse::state_type> list) : states(list.begin(), list.end()) {
        // Intentionally left empty.
    }

    /*!
     * Checks whether the two state blocks contain exactly the same states.
     *
     * @param other The state block with which to compare the current one.
     * @return True iff the two state blocks contain exactly the same states.
     */
    bool operator==(StateBlock const& other) const {
        return this->states == other.states;
    }

    /*!
     * Returns an iterator to the states in this SCC.
     *
     * @return An iterator to the states in this SCC.
     */
    iterator begin();

    /*!
     * Returns a const iterator to the states in this SCC.
     *
     * @return A const iterator to the states in this SCC.
     */
    const_iterator begin() const;

    /*!
     * Returns a const iterator to the states in this SCC.
     *
     * @return A const iterator to the states in this SCC.
     */
    const_iterator cbegin() const {
        return this->begin();
    }

    /*!
     * Returns an iterator that points one past the end of the states in this SCC.
     *
     * @return An iterator that points one past the end of the states in this SCC.
     */
    iterator end();

    /*!
     * Returns a const iterator that points one past the end of the states in this SCC.
     *
     * @return A const iterator that points one past the end of the states in this SCC.
     */
    const_iterator end() const;

    /*!
     * Returns a const iterator that points one past the end of the states in this SCC.
     *
     * @return A const iterator that points one past the end of the states in this SCC.
     */
    const_iterator cend() const {
        return this->end();
    }

    /*!
     * Retrieves whether the given state is in the SCC.
     *
     * @param state The state for which to query membership.
     */
    bool containsState(value_type const& state) const;

    /*!
     * Inserts the given element into this SCC.
     *
     * @param state The state to add to this SCC.
     */
    void insert(value_type const& state);

    /*!
     * Inserts the given element into this SCC.
     *
     * @param state The state to add to this SCC.
     */
    iterator insert(container_type::const_iterator iterator, value_type const& state);

    /*!
     * Removes the given element from this SCC.
     *
     * @param state The element to remove.
     */
    void erase(value_type const& state);

    /*!
     * Retrieves the number of states in this SCC.
     *
     * @return The number of states in this SCC.
     */
    std::size_t size() const;

    /*!
     * Retrieves whether this SCC is empty.
     *
     * @return True iff the SCC is empty.
     */
    bool empty() const;

    /*!
     * Retrieves the set of states contained in the StateBlock.
     *
     * @return The set of states contained in the StateBlock.
     */
    container_type const& getStates() const;

   private:
    // The container that holds the states.
    container_type states;
};

/*!
 * Writes a string representation of the state block to the given output stream.
 *
 * @param out The output stream to write to.
 * @param block The block to print to the stream.
 * @return The given output stream.
 */
std::ostream& operator<<(std::ostream& out, StateBlock const& block);
}  // namespace storage
}  // namespace storm
