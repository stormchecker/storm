#include "storm-dft/storage/FailableElements.h"

#include <sstream>

#include "storm-dft/storage/DFT.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace storage {

FailableElements::const_iterator::const_iterator(bool dependency, bool conflicting, storm::storage::BitVector::const_iterator const& iterBE,
                                                 std::list<size_t>::const_iterator const& iterDependency, std::list<size_t>::const_iterator nonConflictEnd,
                                                 std::list<size_t>::const_iterator conflictBegin)
    : dependency(dependency), conflicting(conflicting), itBE(iterBE), itDep(iterDependency), nonConflictEnd(nonConflictEnd), conflictBegin(conflictBegin) {
    STORM_LOG_ASSERT(conflicting || itDep != nonConflictEnd, "No non-conflicting dependencies present.");
}

FailableElements::const_iterator& FailableElements::const_iterator::operator++() {
    if (dependency) {
        ++itDep;
        if (!conflicting && itDep == nonConflictEnd) {
            // All non-conflicting dependencies considered -> start with conflicting ones
            conflicting = true;
            itDep = conflictBegin;
        }
    } else {
        ++itBE;
    }
    return *this;
}

uint_fast64_t FailableElements::const_iterator::operator*() const {
    if (dependency) {
        return *itDep;
    } else {
        return *itBE;
    }
}

bool FailableElements::const_iterator::operator!=(const_iterator const& other) const {
    if (dependency != other.dependency) {
        return true;
    }
    if (dependency) {
        if (conflicting != other.conflicting) {
            return true;
        } else {
            return itDep != other.itDep;
        }
    } else {
        return itBE != other.itBE;
    }
}

bool FailableElements::const_iterator::operator==(const_iterator const& other) const {
    return !(*this != other);
}

bool FailableElements::const_iterator::isFailureDueToDependency() const {
    return dependency;
}

bool FailableElements::const_iterator::isConflictingDependency() const {
    return conflicting;
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const> FailableElements::const_iterator::asBE(
    storm::dft::storage::DFT<ValueType> const& dft) const {
    size_t nextFailId = **this;
    STORM_LOG_ASSERT(!isFailureDueToDependency(), "The current iterator is not a BE failure but a dependency failure.");
    return dft.getBasicElement(nextFailId);
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::elements::DFTDependency<ValueType> const> FailableElements::const_iterator::asDependency(
    storm::dft::storage::DFT<ValueType> const& dft) const {
    size_t nextFailId = **this;
    STORM_LOG_ASSERT(isFailureDueToDependency(), "The current iterator is not a dependency failure but a BE failure.");
    return dft.getDependency(nextFailId);
}

void FailableElements::addBE(size_t id) {
    currentlyFailableBE.set(id);
}

void FailableElements::addDependency(size_t id, bool isConflicting) {
    std::list<size_t>& failableList = (isConflicting ? failableConflictingDependencies : failableNonconflictingDependencies);
    for (auto it = failableList.begin(); it != failableList.end(); ++it) {
        if (*it > id) {
            failableList.insert(it, id);
            return;
        } else if (*it == id) {
            // Dependency already contained
            return;
        }
    }
    failableList.push_back(id);
}

void FailableElements::removeBE(size_t id) {
    currentlyFailableBE.set(id, false);
}

void FailableElements::removeDependency(size_t id) {
    auto iter = std::find(failableConflictingDependencies.begin(), failableConflictingDependencies.end(), id);
    if (iter != failableConflictingDependencies.end()) {
        failableConflictingDependencies.erase(iter);
        return;
    }
    iter = std::find(failableNonconflictingDependencies.begin(), failableNonconflictingDependencies.end(), id);
    if (iter != failableNonconflictingDependencies.end()) {
        failableNonconflictingDependencies.erase(iter);
    }
}

void FailableElements::clear() {
    currentlyFailableBE.clear();
    failableConflictingDependencies.clear();
    failableNonconflictingDependencies.clear();
}

FailableElements::const_iterator FailableElements::begin(bool forceBE) const {
    bool dependency = hasDependencies() && !forceBE;
    bool conflicting = failableNonconflictingDependencies.empty();
    auto itDep = conflicting ? failableConflictingDependencies.begin() : failableNonconflictingDependencies.begin();
    return FailableElements::const_iterator(dependency, conflicting, currentlyFailableBE.begin(), itDep, failableNonconflictingDependencies.end(),
                                            failableConflictingDependencies.begin());
}

FailableElements::const_iterator FailableElements::end(bool forceBE) const {
    bool dependency = hasDependencies() && !forceBE;
    return FailableElements::const_iterator(dependency, true, currentlyFailableBE.end(), failableConflictingDependencies.end(),
                                            failableNonconflictingDependencies.end(), failableConflictingDependencies.begin());
}

bool FailableElements::hasDependencies() const {
    return !failableConflictingDependencies.empty() || !failableNonconflictingDependencies.empty();
}

bool FailableElements::hasBEs() const {
    return !currentlyFailableBE.empty();
}

std::string FailableElements::getCurrentlyFailableString(bool forceBE) const {
    std::stringstream stream;
    stream << "{";
    if (hasDependencies() && !forceBE) {
        stream << "Dependencies: ";
    }
    for (auto it = begin(forceBE); it != end(forceBE); ++it) {
        stream << *it << ", ";
    }
    stream << "}";
    return stream.str();
}

// Explicit instantiations.
template std::shared_ptr<storm::dft::storage::elements::DFTBE<double> const> FailableElements::const_iterator::asBE(
    storm::dft::storage::DFT<double> const& dft) const;
template std::shared_ptr<storm::dft::storage::elements::DFTDependency<double> const> FailableElements::const_iterator::asDependency(
    storm::dft::storage::DFT<double> const& dft) const;

template std::shared_ptr<storm::dft::storage::elements::DFTBE<storm::RationalFunction> const> FailableElements::const_iterator::asBE(
    storm::dft::storage::DFT<storm::RationalFunction> const& dft) const;
template std::shared_ptr<storm::dft::storage::elements::DFTDependency<storm::RationalFunction> const> FailableElements::const_iterator::asDependency(
    storm::dft::storage::DFT<storm::RationalFunction> const& dft) const;

}  // namespace storage
}  // namespace storm::dft