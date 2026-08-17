#pragma once

#include <set>

#include "storm/models/sparse/ItemLabeling.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class manages the labeling of the state space with a number of (atomic) labels.
 */
class StateLabeling : public ItemLabeling {
   public:
    /*!
     * Constructs an empty labeling for the given number of states.
     *
     * @param stateCount The number of states for which this labeling can hold the labels.
     */
    StateLabeling(uint_fast64_t stateCount = 0);

    StateLabeling(StateLabeling const& other) = default;
    StateLabeling(ItemLabeling const& other);
    StateLabeling(ItemLabeling const&& other);
    StateLabeling& operator=(StateLabeling const& other) = default;

    virtual bool isStateLabeling() const override;

    /*!
     * Checks whether the two labelings are equal.
     *
     * @param other The labeling with which the current one is compared.
     * @return True iff the labelings are equal.
     */
    bool operator==(StateLabeling const& other) const;

    /*!
     * Retrieves the sub labeling that represents the same labeling as the current one for all selected states.
     *
     * @param states The selected set of states.
     */
    StateLabeling getSubLabeling(storm::storage::BitVector const& states) const;

    /*!
     * Retrieves the set of labels attached to the given state.
     *
     * @param state The state for which to retrieve the labels.
     * @return The labels attached to the given state.
     */
    std::set<std::string> getLabelsOfState(storm::storage::sparse::state_type state) const;

    /*!
     * Adds a label to a given state.
     *
     * @param label The name of the label to add.
     * @param state The index of the state to label.
     */
    void addLabelToState(std::string const& label, storm::storage::sparse::state_type state);

    /*!
     * Removes a label from a given state.
     *
     * @param label The name of the label to remove.
     * @param state The index of the state.
     */
    void removeLabelFromState(std::string const& label, storm::storage::sparse::state_type state);

    /*!
     * Checks whether a given state is labeled with the given label.
     *
     * @param label The name of the label.
     * @param state The index of the state to check.
     * @return True if the node is labeled with the label, false otherwise.
     */
    bool getStateHasLabel(std::string const& label, storm::storage::sparse::state_type state) const;

    /*!
     * Returns the labeling of states associated with the given label.
     *
     * @param label The name of the label.
     * @return A bit vector that represents the labeling of the states with the given label.
     */
    storm::storage::BitVector const& getStates(std::string const& label) const;

    /*!
     * Sets the labeling of states associated with the given label.
     *
     * @param label The name of the label.
     * @param labeling A bit vector that represents the set of states that will get this label.
     */
    void setStates(std::string const& label, storage::BitVector const& labeling);

    /*!
     * Sets the labeling of states associated with the given label.
     *
     * @param label The name of the label.
     * @param labeling A bit vector that represents the set of states that will get this label.
     */
    void setStates(std::string const& label, storage::BitVector&& labeling);

    friend std::ostream& operator<<(std::ostream& out, StateLabeling const& labeling);
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
