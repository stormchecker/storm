#pragma once

#include <unordered_map>

#include "storm/storage/BoostTypes.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace storage {
// fwd
class BitVector;

/*!
 * This class represents a maximal end-component of a nondeterministic model.
 */
class MaximalEndComponent {
   public:
    typedef storm::storage::FlatSet<sparse::state_type> set_type;
    typedef std::unordered_map<uint_fast64_t, set_type> map_type;
    typedef map_type::iterator iterator;
    typedef map_type::const_iterator const_iterator;

    /*!
     * Creates an empty MEC.
     */
    MaximalEndComponent();

    /*!
     * Creates an MEC by copying the given one.
     *
     * @param other The MEC to copy.
     */
    MaximalEndComponent(MaximalEndComponent const& other);

    /*!
     * Assigns the contents of the given MEC to the current one via copying.
     *
     * @param other The MEC whose contents to copy.
     */
    MaximalEndComponent& operator=(MaximalEndComponent const& other);

    /*!
     * Creates an MEC by moving the given one.
     *
     * @param other The MEC to move.
     */
    MaximalEndComponent(MaximalEndComponent&& other);

    /*!
     * Assigns the contents of the given MEC to the current one via moving.
     *
     * @param other The MEC whose contents to move.
     */
    MaximalEndComponent& operator=(MaximalEndComponent&& other);

    /*!
     * @return true iff this is the same MEC as other
     */
    bool operator==(MaximalEndComponent const& other) const;

    /*!
     * @return true iff this is a different MEC as other
     */
    bool operator!=(MaximalEndComponent const& other) const;

    /*!
     * Adds the given state and the given choices to the MEC.
     *
     * @param state The state for which to add the choices.
     * @param choices The choices to add for the state.
     */
    void addState(uint_fast64_t state, set_type const& choices);

    /*!
     * Adds the given state and the given choices to the MEC.
     *
     * @param state The state for which to add the choices.
     * @param choices The choices to add for the state.
     */
    void addState(uint_fast64_t state, set_type&& choices);

    /*!
     * @return The number of states in this mec.
     */
    std::size_t size() const;

    /*!
     * Retrieves the choices for the given state that are contained in this MEC under the assumption that the
     * state is in the MEC.
     *
     * @param state The state for which to retrieve the choices.
     * @return A set of choices of the state in the MEC.
     */
    set_type const& getChoicesForState(uint_fast64_t state) const;

    /*!
     * Retrieves the choices for the given state that are contained in this MEC under the assumption that the
     * state is in the MEC.
     *
     * @param state The state for which to retrieve the choices.
     * @return A set of choices of the state in the MEC.
     */
    set_type& getChoicesForState(uint_fast64_t state);

    /*!
     * Removes the given state and all of its choices from the MEC.
     *
     * @param state The state to remove froom the MEC.
     */
    void removeState(uint_fast64_t state);

    /*!
     * Retrieves whether the given state is contained in this MEC.
     *
     * @param state The state for which to query membership in the MEC.
     * @return True if the given state is contained in the MEC.
     */
    bool containsState(uint_fast64_t state) const;

    /*!
     * Retrieves whether at least one of the given states is contained in this MEC.
     *
     * @param stateSet The states for which to query membership in the MEC.
     * @return True if any of the given states is contained in the MEC.
     */
    bool containsAnyState(storm::storage::BitVector stateSet) const;

    /*!
     * Retrieves whether the given choice for the given state is contained in the MEC.
     *
     * @param state The state for which to check whether the given choice is contained in the MEC.
     * @param choice The choice for which to check whether it is contained in the MEC.
     * @return True if the given choice is contained in the MEC.
     */
    bool containsChoice(uint_fast64_t state, uint_fast64_t choice) const;

    /*!
     * Retrieves the set of states contained in the MEC.
     *
     * @return The set of states contained in the MEC.
     */
    set_type getStateSet() const;

    /*!
     * Retrieves an iterator that points to the first state and its choices in the MEC.
     *
     * @return An iterator that points to the first state and its choices in the MEC.
     */
    iterator begin();

    /*!
     * Retrieves an iterator that points past the last state and its choices in the MEC.
     *
     * @return An iterator that points past the last state and its choices in the MEC.
     */
    iterator end();

    /*!
     * Retrieves an iterator that points to the first state and its choices in the MEC.
     *
     * @return An iterator that points to the first state and its choices in the MEC.
     */
    const_iterator begin() const;

    /*!
     * Retrieves an iterator that points past the last state and its choices in the MEC.
     *
     * @return An iterator that points past the last state and its choices in the MEC.
     */
    const_iterator end() const;

    // Declare the streaming operator as a friend function.
    friend std::ostream& operator<<(std::ostream& out, MaximalEndComponent const& component);

   private:
    // This stores the mapping from states contained in the MEC to the choices in this MEC.
    map_type stateToChoicesMapping;
};
}  // namespace storage
}  // namespace storm
