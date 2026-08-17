#include "storm/models/sparse/StateLabeling.h"

namespace storm {
namespace models {
namespace sparse {
StateLabeling::StateLabeling(uint_fast64_t stateCount) : ItemLabeling(stateCount) {
    // Intentionally left empty.
}

StateLabeling::StateLabeling(ItemLabeling const& itemLab) : ItemLabeling(itemLab) {
    // Intentionally left empty.
}

StateLabeling::StateLabeling(ItemLabeling const&& itemLab) : ItemLabeling(itemLab) {
    // Intentionally left empty.
}

bool StateLabeling::isStateLabeling() const {
    return true;
}

bool StateLabeling::operator==(StateLabeling const& other) const {
    if (itemCount != other.itemCount) {
        return false;
    }
    if (this->getNumberOfLabels() != other.getNumberOfLabels()) {
        return false;
    }
    for (auto const& labelIndexPair : this->nameToLabelingIndexMap) {
        if (!other.containsLabel(labelIndexPair.first)) {
            return false;
        }
        if (labelings[labelIndexPair.second] != other.getStates(labelIndexPair.first)) {
            return false;
        }
    }
    return true;
}

StateLabeling StateLabeling::getSubLabeling(storm::storage::BitVector const& states) const {
    return StateLabeling(ItemLabeling::getSubItemLabeling(states));
}

std::set<std::string> StateLabeling::getLabelsOfState(storm::storage::sparse::state_type state) const {
    return ItemLabeling::getLabelsOfItem(state);
}

void StateLabeling::addLabelToState(std::string const& label, storm::storage::sparse::state_type state) {
    ItemLabeling::addLabelToItem(label, state);
}

void StateLabeling::removeLabelFromState(std::string const& label, storm::storage::sparse::state_type state) {
    ItemLabeling::removeLabelFromItem(label, state);
}

bool StateLabeling::getStateHasLabel(std::string const& label, storm::storage::sparse::state_type state) const {
    return ItemLabeling::getItemHasLabel(label, state);
}

storm::storage::BitVector const& StateLabeling::getStates(std::string const& label) const {
    return ItemLabeling::getItems(label);
}

void StateLabeling::setStates(std::string const& label, storage::BitVector const& labeling) {
    ItemLabeling::setItems(label, labeling);
}

void StateLabeling::setStates(std::string const& label, storage::BitVector&& labeling) {
    ItemLabeling::setItems(label, labeling);
}

std::ostream& operator<<(std::ostream& out, StateLabeling const& labeling) {
    labeling.printLabelingInformationToStream(out);
    return out;
}

}  // namespace sparse
}  // namespace models
}  // namespace storm
