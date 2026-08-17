#pragma once

#include <iostream>
#include <set>
#include <string>
#include <unordered_map>

#include "storm/storage/BitVector.h"

namespace storm {
namespace models {
namespace sparse {

class StateLabeling;
class ChoiceLabeling;

/*!
 * A base class managing the labeling of items with a number of (atomic) labels.
 */
class ItemLabeling {
   public:
    /*!
     * Constructs an empty labeling for the given number of items.
     *
     * @param itemCount The number of items for which this labeling can hold the labels.
     */
    explicit ItemLabeling(uint64_t itemCount = 0);

    ItemLabeling(ItemLabeling const& other) = default;
    ItemLabeling& operator=(ItemLabeling const& other) = default;

    virtual ~ItemLabeling() = default;

    virtual bool isStateLabeling() const;
    virtual bool isChoiceLabeling() const;

    StateLabeling const& asStateLabeling() const;
    StateLabeling& asStateLabeling();
    ChoiceLabeling const& asChoiceLabeling() const;
    ChoiceLabeling& asChoiceLabeling();

    /*!
     * Checks whether the two labelings are equal.
     *
     * @param other The labeling with which the current one is compared.
     * @return True iff the labelings are equal.
     */
    bool operator==(ItemLabeling const& other) const;

    /*!
     * Adds a new label to the labelings. Initially, no item is labeled with this label.
     *
     * @param label The name of the new label.
     */
    void addLabel(std::string const& label);

    /*!
     * Removes a label from the labelings.
     *
     * @param label The name of the label to remove.
     */
    void removeLabel(std::string const& label);

    /*!
     * Retrieves the set of labels contained in this labeling.
     *
     * @return The set of known labels.
     */
    std::set<std::string> getLabels() const;

    /*!
     * Creates a new label and attaches it to the given items. Note that the dimension of given labeling must
     * match the number of items for which this item labeling is valid.
     *
     * @param label The new label.
     * @param labeling A bit vector that indicates whether or not the new label is attached to a item.
     */
    void addLabel(std::string const& label, storage::BitVector const& labeling);

    /*!
     * Creates a new label and attaches it to the given items. Note that the dimension of given labeling must
     * match the number of items for which this item labeling is valid.
     *
     * @param label The new label.
     * @param labeling A bit vector that indicates whether or not the new label is attached to a item.
     */
    void addLabel(std::string const& label, storage::BitVector&& labeling);

    /*!
     * Creates a new label with a unique name, derived from the prefix,
     * and attaches it to the given items. Note that the dimension of given labeling must
     * match the number of items for which this item labeling is valid.
     *
     * @param prefix A prefix to use for the new label.
     * @param labeling A bit vector that indicates whether or not the new label is attached to a item.
     * @return the new label
     */
    std::string addUniqueLabel(std::string const& prefix, storage::BitVector const& labeling);

    /*!
     * Creates a new label with a unique name, derived from the prefix,
     * and attaches it to the given items. Note that the dimension of given labeling must
     * match the number of items for which this item labeling is valid.
     *
     * @param prefix A prefix to use for the new label.
     * @param labeling A bit vector that indicates whether or not the new label is attached to a item.
     * @return the new label
     */
    std::string addUniqueLabel(std::string const& prefix, storage::BitVector const&& labeling);

    /*!
     * Adds all labels from the other labeling to this labeling.
     * It is assumed that both labelings have the same itemcount.
     * If a label is present in both labellings, we take the union of the corresponding item sets.
     */
    void join(ItemLabeling const& other);

    /*!
     * Checks whether a label is registered within this labeling.
     *
     * @return True if the label is known, false otherwise.
     */
    bool containsLabel(std::string const& label) const;

    /*!
     * Returns the number of labels managed by this object.
     *
     * @return The number of labels.
     */
    std::size_t getNumberOfLabels() const;

    /*!
     * Returns the number of items managed by this object.
     *
     * @return The number of labels.
     */
    std::size_t getNumberOfItems() const;

    void permuteItems(std::vector<uint64_t> const& inversePermutation);

    virtual std::size_t hash() const;

    /*!
     * Prints information about the labeling to the specified stream.
     *
     * @param out The stream the information is to be printed to.
     */
    void printLabelingInformationToStream(std::ostream& out = std::cout) const;

    /*!
     * Prints the complete labeling to the specified stream.
     *
     * @param out The stream the information is to be printed to.
     */
    void printCompleteLabelingInformationToStream(std::ostream& out = std::cout) const;

    friend std::ostream& operator<<(std::ostream& out, ItemLabeling const& labeling);

   protected:
    /*!
     * Retrieves the sub labeling that represents the same labeling as the current one for all selected items.
     *
     * @param items The selected set of items.
     */
    ItemLabeling getSubItemLabeling(storm::storage::BitVector const& items) const;

    /*!
     * Retrieves the set of labels attached to the given item.
     *
     * @param item The item for which to retrieve the labels.
     * @return The labels attached to the given item.
     */
    virtual std::set<std::string> getLabelsOfItem(uint64_t item) const;

    /*!
     * Checks whether a given item is labeled with the given label.
     *
     * @param label The name of the label.
     * @param item The index of the item to check.
     * @return True if the node is labeled with the label, false otherwise.
     */
    virtual bool getItemHasLabel(std::string const& label, uint64_t item) const;

    /*!
     * Returns the labeling of items associated with the given label.
     *
     * @param label The name of the label.
     * @return A bit vector that represents the labeling of the items with the given label.
     */
    virtual storm::storage::BitVector const& getItems(std::string const& label) const;

    /*!
     * Sets the labeling of items associated with the given label.
     *
     * @param label The name of the label.
     * @param labeling A bit vector that represents the set of items that will get this label.
     */
    virtual void setItems(std::string const& label, storage::BitVector const& labeling);

    /*!
     * Sets the labeling of items associated with the given label.
     *
     * @param label The name of the label.
     * @param labeling A bit vector that represents the set of items that will get this label.
     */
    virtual void setItems(std::string const& label, storage::BitVector&& labeling);

    /*!
     * Adds a label to a given item.
     *
     * @param label The name of the label to add.
     * @param item The index of the item to label.
     */
    virtual void addLabelToItem(std::string const& label, uint64_t item);

    /*!
     * Removes a label from a given item.
     *
     * @param label The name of the label to remove.
     * @param item The index of the item.
     */
    virtual void removeLabelFromItem(std::string const& label, uint64_t item);

    // The number of items for which this object can hold the labeling.
    uint64_t itemCount;

    // A mapping from labels to the index of the corresponding bit vector in the vector.
    std::unordered_map<std::string, uint64_t> nameToLabelingIndexMap;

    // A vector that holds the labeling for all known labels.
    std::vector<storm::storage::BitVector> labelings;

    /*!
     * Generate a unique, previously unused label from the given prefix string.
     */
    std::string generateUniqueLabel(const std::string& prefix) const;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
