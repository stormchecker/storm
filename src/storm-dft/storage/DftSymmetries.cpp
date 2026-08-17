#include "DftSymmetries.h"

#include "storm/utility/macros.h"

namespace storm::dft {
namespace storage {

DftSymmetries::DftSymmetries(std::map<size_t, std::vector<std::vector<size_t>>> groups) : groups(groups) {
    std::vector<size_t> sortedGroups;
    for (auto const& cl : groups) {
        sortedGroups.push_back(cl.first);
    }
    // Sort by length of symmetry or (if equal) by lower first element
    std::sort(sortedGroups.begin(), sortedGroups.end(), [&](const size_t left, const size_t right) {
        return groups.at(left).size() < groups.at(right).size() ||
               (groups.at(left).size() == groups.at(right).size() && groups.at(left).front().front() < groups.at(right).front().front());
    });

    // Sort hierarchical
    while (!sortedGroups.empty()) {
        // Insert largest element
        size_t currentRoot = sortedGroups.back();
        sortedGroups.pop_back();
        sortHierarchical(currentRoot, sortedGroups);
        sortedSymmetries.push_back(currentRoot);
    }
}

size_t DftSymmetries::nrSymmetries() const {
    return groups.size();
}

std::vector<size_t>::const_iterator DftSymmetries::begin() const {
    return sortedSymmetries.begin();
}

std::vector<size_t>::const_iterator DftSymmetries::end() const {
    return sortedSymmetries.end();
}

std::vector<std::vector<size_t>> const& DftSymmetries::getSymmetryGroup(size_t index) const {
    STORM_LOG_ASSERT(groups.count(index) > 0, "Invalid index.");
    return groups.at(index);
}

bool DftSymmetries::existsInFirstSymmetry(size_t index, size_t value) const {
    for (std::vector<size_t> symmetry : getSymmetryGroup(index)) {
        if (symmetry.front() == value) {
            return true;
        }
    }
    return false;
}

bool DftSymmetries::existsInSymmetry(size_t index, size_t value) const {
    for (std::vector<size_t> symmetry : getSymmetryGroup(index)) {
        for (size_t index : symmetry) {
            if (index == value) {
                return true;
            }
        }
    }
    return false;
}

int DftSymmetries::applySymmetry(std::vector<std::vector<size_t>> const& symmetry, size_t value, size_t index) const {
    for (std::vector<size_t> element : symmetry) {
        if (element[0] == value) {
            return element[index];
        }
    }
    return -1;
}

std::vector<std::vector<size_t>> DftSymmetries::createSymmetry(std::vector<std::vector<size_t>> parentSymmetry, std::vector<std::vector<size_t>> childSymmetry,
                                                               size_t index) {
    std::vector<std::vector<size_t>> result;
    for (std::vector<size_t> childSym : childSymmetry) {
        std::vector<size_t> symmetry;
        for (size_t child : childSym) {
            int bijectionValue = applySymmetry(parentSymmetry, child, index);
            if (bijectionValue >= 0) {
                symmetry.push_back(bijectionValue);
            } else {
                STORM_LOG_ASSERT(result.empty(), "Returning inconsistent result.");
                return result;
            }
        }
        result.push_back(symmetry);
    }
    return result;
}

void DftSymmetries::sortHierarchical(size_t parent, std::vector<size_t>& candidates) {
    // Find subsymmetries of current symmetry
    std::vector<size_t> children;
    for (int i = candidates.size() - 1; i >= 0; --i) {
        size_t currentRoot = candidates[i];
        if (existsInSymmetry(parent, currentRoot)) {
            // Is child
            STORM_LOG_TRACE(currentRoot << " is child of " << parent);
            children.insert(children.begin(), currentRoot);
            candidates.erase(candidates.begin() + i);  // Removing is okay as i is decremented
        }
    }

    // Find child symmetries which are created by parent and other child symmetries
    for (size_t i = 0; i < children.size(); ++i) {
        // Iterate over all possible symmetry groups
        for (size_t index = 1; index < groups.at(parent).front().size(); ++index) {
            std::vector<std::vector<size_t>> possibleSymmetry = createSymmetry(groups.at(parent), groups.at(children[i]), index);
            if (possibleSymmetry.empty()) {
                // No symmetry created
                break;
            }
            for (size_t j = 0; j < children.size(); /*manual increment*/) {
                if (j == i) {
                    // Ignore identity
                    ++j;
                    continue;
                }
                if (possibleSymmetry == groups.at(children[j])) {
                    STORM_LOG_TRACE("Child " << children[j] << " ignored as created by symmetries " << parent << " and " << children[i]);
                    groups.erase(children[j]);
                    children.erase(children.begin() + j);
                    // Update iterator
                    if (i > j) {
                        --i;
                    }
                } else {
                    // Consider next element
                    ++j;
                }
            }
        }
    }

    // Apply sorting recursively
    while (!children.empty()) {
        // Insert largest element
        size_t largestChild = children.back();
        children.pop_back();
        sortHierarchical(largestChild, children);
        sortedSymmetries.push_back(largestChild);
    }
}

std::ostream& operator<<(std::ostream& os, DftSymmetries const& symmetries) {
    for (size_t index : symmetries.sortedSymmetries) {
        os << "Symmetry group for " << index << '\n';
        for (auto const& eqClass : symmetries.groups.at(index)) {
            for (auto const& i : eqClass) {
                os << i << " ";
            }
            os << '\n';
        }
    }
    return os;
}

}  // namespace storage
}  // namespace storm::dft
