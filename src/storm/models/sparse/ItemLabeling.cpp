#include "storm/models/sparse/ItemLabeling.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/utility/macros.h"

namespace storm {
namespace models {
namespace sparse {
ItemLabeling::ItemLabeling(uint_fast64_t itemCount) : itemCount(itemCount), nameToLabelingIndexMap(), labelings() {
    // Intentionally left empty.
}

bool ItemLabeling::isStateLabeling() const {
    return false;
}
bool ItemLabeling::isChoiceLabeling() const {
    return false;
}

StateLabeling const& ItemLabeling::asStateLabeling() const {
    return dynamic_cast<StateLabeling const&>(*this);
}
StateLabeling& ItemLabeling::asStateLabeling() {
    return dynamic_cast<StateLabeling&>(*this);
}
ChoiceLabeling const& ItemLabeling::asChoiceLabeling() const {
    return dynamic_cast<ChoiceLabeling const&>(*this);
}
ChoiceLabeling& ItemLabeling::asChoiceLabeling() {
    return dynamic_cast<ChoiceLabeling&>(*this);
}

bool ItemLabeling::operator==(ItemLabeling const& other) const {
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
        if (labelings[labelIndexPair.second] != other.getItems(labelIndexPair.first)) {
            return false;
        }
    }
    return true;
}

ItemLabeling ItemLabeling::getSubItemLabeling(storm::storage::BitVector const& items) const {
    ItemLabeling result(items.getNumberOfSetBits());
    for (auto const& labelIndexPair : nameToLabelingIndexMap) {
        result.addLabel(labelIndexPair.first, labelings[labelIndexPair.second] % items);
    }
    return result;
}

void ItemLabeling::addLabel(std::string const& label) {
    addLabel(label, storage::BitVector(itemCount));
}

void ItemLabeling::removeLabel(std::string const& label) {
    auto labelIt = nameToLabelingIndexMap.find(label);
    STORM_LOG_THROW(labelIt != nameToLabelingIndexMap.end(), storm::exceptions::InvalidArgumentException, "Label '" << label << "' does not exist.");
    uint64_t labelIndex = labelIt->second;
    // Erase entry in map
    nameToLabelingIndexMap.erase(labelIt);
    // Erase label by 'swap and pop'
    std::iter_swap(labelings.begin() + labelIndex, labelings.end() - 1);
    labelings.pop_back();

    // Update index of labeling we swapped from the end
    for (auto& it : nameToLabelingIndexMap) {
        if (it.second == labelings.size()) {
            it.second = labelIndex;
            break;
        }
    }
}

void ItemLabeling::join(ItemLabeling const& other) {
    STORM_LOG_THROW(this->itemCount == other.itemCount, storm::exceptions::InvalidArgumentException,
                    "The item count of the two labelings does not match: " << this->itemCount << " vs. " << other.itemCount << ".");
    for (auto const& label : other.getLabels()) {
        if (this->containsLabel(label)) {
            this->setItems(label, this->getItems(label) | other.getItems(label));
        } else {
            this->addLabel(label, other.getItems(label));
        }
    }
}

std::set<std::string> ItemLabeling::getLabels() const {
    std::set<std::string> result;
    for (auto const& labelIndexPair : this->nameToLabelingIndexMap) {
        result.insert(labelIndexPair.first);
    }
    return result;
}

std::set<std::string> ItemLabeling::getLabelsOfItem(uint64_t item) const {
    std::set<std::string> result;
    for (auto const& labelIndexPair : this->nameToLabelingIndexMap) {
        if (this->getItemHasLabel(labelIndexPair.first, item)) {
            result.insert(labelIndexPair.first);
        }
    }
    return result;
}

void ItemLabeling::permuteItems(std::vector<uint64_t> const& inversePermutation) {
    STORM_LOG_THROW(inversePermutation.size() == itemCount, storm::exceptions::InvalidArgumentException, "Permutation does not match number of items.");
    std::vector<storm::storage::BitVector> newLabelings;
    for (storm::storage::BitVector const& source : this->labelings) {
        newLabelings.push_back(source.permute(inversePermutation));
    }

    this->labelings = newLabelings;
}

void ItemLabeling::addLabel(std::string const& label, storage::BitVector const& labeling) {
    STORM_LOG_THROW(!this->containsLabel(label), storm::exceptions::InvalidArgumentException, "Label '" << label << "' already exists.");
    STORM_LOG_THROW(labeling.size() == itemCount, storm::exceptions::InvalidArgumentException,
                    "Labeling vector has invalid size. Expected: " << itemCount << " Actual: " << labeling.size() << ".");
    nameToLabelingIndexMap.emplace(label, labelings.size());
    labelings.push_back(labeling);
}

void ItemLabeling::addLabel(std::string const& label, storage::BitVector&& labeling) {
    STORM_LOG_THROW(!this->containsLabel(label), storm::exceptions::InvalidArgumentException, "Label '" << label << "' already exists.");
    STORM_LOG_THROW(labeling.size() == itemCount, storm::exceptions::InvalidArgumentException,
                    "Labeling vector has invalid size. Expected: " << itemCount << " Actual: " << labeling.size() << ".");
    nameToLabelingIndexMap.emplace(label, labelings.size());
    labelings.emplace_back(std::move(labeling));
}

std::string ItemLabeling::addUniqueLabel(std::string const& prefix, storage::BitVector const& labeling) {
    std::string label = generateUniqueLabel(prefix);
    addLabel(label, labeling);
    return label;
}

std::string ItemLabeling::addUniqueLabel(std::string const& prefix, storage::BitVector const&& labeling) {
    std::string label = generateUniqueLabel(prefix);
    addLabel(label, labeling);
    return label;
}

bool ItemLabeling::containsLabel(std::string const& label) const {
    return nameToLabelingIndexMap.find(label) != nameToLabelingIndexMap.end();
}

void ItemLabeling::addLabelToItem(std::string const& label, uint64_t item) {
    STORM_LOG_THROW(this->containsLabel(label), storm::exceptions::InvalidArgumentException, "Label '" << label << "' unknown.");
    STORM_LOG_THROW(item < itemCount, storm::exceptions::OutOfRangeException, "Item index out of range.");
    this->labelings[nameToLabelingIndexMap.at(label)].set(item, true);
}

void ItemLabeling::removeLabelFromItem(std::string const& label, uint64_t item) {
    STORM_LOG_THROW(item < itemCount, storm::exceptions::OutOfRangeException, "Item index out of range.");
    STORM_LOG_THROW(this->getItemHasLabel(label, item), storm::exceptions::InvalidArgumentException,
                    "Item " << item << " does not have label '" << label << "'.");
    this->labelings[nameToLabelingIndexMap.at(label)].set(item, false);
}

bool ItemLabeling::getItemHasLabel(std::string const& label, uint64_t item) const {
    STORM_LOG_THROW(this->containsLabel(label), storm::exceptions::InvalidArgumentException,
                    "The label '" << label << "' is invalid for the labeling of the model.");
    return this->labelings[nameToLabelingIndexMap.at(label)].get(item);
}

std::size_t ItemLabeling::getNumberOfLabels() const {
    return labelings.size();
}

std::size_t ItemLabeling::getNumberOfItems() const {
    return itemCount;
}

storm::storage::BitVector const& ItemLabeling::getItems(std::string const& label) const {
    STORM_LOG_THROW(this->containsLabel(label), storm::exceptions::InvalidArgumentException,
                    "The label " << label << " is invalid for the labeling of the model.");
    return this->labelings[nameToLabelingIndexMap.at(label)];
}

void ItemLabeling::setItems(std::string const& label, storage::BitVector const& labeling) {
    STORM_LOG_THROW(this->containsLabel(label), storm::exceptions::InvalidArgumentException,
                    "The label " << label << " is invalid for the labeling of the model.");
    STORM_LOG_THROW(labeling.size() == itemCount, storm::exceptions::InvalidArgumentException, "Labeling vector has invalid size.");
    this->labelings[nameToLabelingIndexMap.at(label)] = labeling;
}

void ItemLabeling::setItems(std::string const& label, storage::BitVector&& labeling) {
    STORM_LOG_THROW(this->containsLabel(label), storm::exceptions::InvalidArgumentException,
                    "The label " << label << " is invalid for the labeling of the model.");
    STORM_LOG_THROW(labeling.size() == itemCount, storm::exceptions::InvalidArgumentException, "Labeling vector has invalid size.");
    this->labelings[nameToLabelingIndexMap.at(label)] = labeling;
}

void ItemLabeling::printLabelingInformationToStream(std::ostream& out) const {
    out << this->getNumberOfLabels() << " labels\n";
    for (auto const& labelIndexPair : this->nameToLabelingIndexMap) {
        out << "   * " << labelIndexPair.first << " -> " << this->labelings[labelIndexPair.second].getNumberOfSetBits() << " item(s)\n";
    }
}

void ItemLabeling::printCompleteLabelingInformationToStream(std::ostream& out) const {
    out << "Labels: \t" << this->getNumberOfLabels() << '\n';
    for (auto label : nameToLabelingIndexMap) {
        out << "Label '" << label.first << "': ";
        for (auto index : this->labelings[label.second]) {
            out << index << " ";
        }
        out << '\n';
    }
}

std::size_t ItemLabeling::hash() const {
    return 0;
}

std::ostream& operator<<(std::ostream& out, ItemLabeling const& labeling) {
    labeling.printLabelingInformationToStream(out);
    return out;
}

std::string ItemLabeling::generateUniqueLabel(const std::string& prefix) const {
    if (!containsLabel(prefix)) {
        return prefix;
    }
    unsigned int i = 0;
    std::string label;
    do {
        label = prefix + "_" + std::to_string(i);
    } while (containsLabel(label));
    return label;
}

}  // namespace sparse
}  // namespace models
}  // namespace storm
