#include "storm/models/sparse/ChoiceLabeling.h"

namespace storm {
namespace models {
namespace sparse {
ChoiceLabeling::ChoiceLabeling(uint_fast64_t choiceCount) : ItemLabeling(choiceCount) {
    // Intentionally left empty.
}

ChoiceLabeling::ChoiceLabeling(ItemLabeling const& itemLab) : ItemLabeling(itemLab) {
    // Intentionally left empty.
}

ChoiceLabeling::ChoiceLabeling(ItemLabeling const&& itemLab) : ItemLabeling(itemLab) {
    // Intentionally left empty.
}

bool ChoiceLabeling::isChoiceLabeling() const {
    return true;
}

bool ChoiceLabeling::operator==(ChoiceLabeling const& other) const {
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
        if (labelings[labelIndexPair.second] != other.getChoices(labelIndexPair.first)) {
            return false;
        }
    }
    return true;
}

ChoiceLabeling ChoiceLabeling::getSubLabeling(storm::storage::BitVector const& choices) const {
    return ChoiceLabeling(ItemLabeling::getSubItemLabeling(choices));
}

std::set<std::string> ChoiceLabeling::getLabelsOfChoice(uint64_t choice) const {
    return this->getLabelsOfItem(choice);
}

void ChoiceLabeling::addLabelToChoice(std::string const& label, uint64_t choice) {
    return ItemLabeling::addLabelToItem(label, choice);
}

bool ChoiceLabeling::getChoiceHasLabel(std::string const& label, uint64_t choice) const {
    return this->getItemHasLabel(label, choice);
}

storm::storage::BitVector const& ChoiceLabeling::getChoices(std::string const& label) const {
    return this->getItems(label);
}

void ChoiceLabeling::setChoices(std::string const& label, storage::BitVector const& labeling) {
    this->setItems(label, labeling);
}

void ChoiceLabeling::setChoices(std::string const& label, storage::BitVector&& labeling) {
    this->setItems(label, labeling);
}

std::ostream& operator<<(std::ostream& out, ChoiceLabeling const& labeling) {
    out << "Choice ";
    labeling.printLabelingInformationToStream(out);
    return out;
}

}  // namespace sparse
}  // namespace models
}  // namespace storm
