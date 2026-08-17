#include "storm/storage/MaximalEndComponent.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

std::ostream& operator<<(std::ostream& out, storm::storage::FlatSet<uint_fast64_t> const& block);

MaximalEndComponent::MaximalEndComponent() : stateToChoicesMapping() {
    // Intentionally left empty.
}

MaximalEndComponent::MaximalEndComponent(MaximalEndComponent const& other) : stateToChoicesMapping(other.stateToChoicesMapping) {
    // Intentionally left empty.
}

MaximalEndComponent& MaximalEndComponent::operator=(MaximalEndComponent const& other) {
    stateToChoicesMapping = other.stateToChoicesMapping;
    return *this;
}

MaximalEndComponent::MaximalEndComponent(MaximalEndComponent&& other) : stateToChoicesMapping(std::move(other.stateToChoicesMapping)) {
    // Intentionally left empty.
}

MaximalEndComponent& MaximalEndComponent::operator=(MaximalEndComponent&& other) {
    stateToChoicesMapping = std::move(other.stateToChoicesMapping);
    return *this;
}

bool MaximalEndComponent::operator==(MaximalEndComponent const& other) const {
    return stateToChoicesMapping == other.stateToChoicesMapping;
}

bool MaximalEndComponent::operator!=(MaximalEndComponent const& other) const {
    return stateToChoicesMapping != other.stateToChoicesMapping;
}

void MaximalEndComponent::addState(uint_fast64_t state, set_type const& choices) {
    stateToChoicesMapping[state] = choices;
}

void MaximalEndComponent::addState(uint_fast64_t state, set_type&& choices) {
    stateToChoicesMapping.emplace(state, std::move(choices));
}

std::size_t MaximalEndComponent::size() const {
    return stateToChoicesMapping.size();
}

MaximalEndComponent::set_type const& MaximalEndComponent::getChoicesForState(uint_fast64_t state) const {
    auto stateChoicePair = stateToChoicesMapping.find(state);

    STORM_LOG_THROW(stateChoicePair != stateToChoicesMapping.end(), storm::exceptions::InvalidStateException,
                    "Invalid call to MaximalEndComponent::getChoicesForState: cannot retrieve choices for state not contained in MEC.");

    return stateChoicePair->second;
}

MaximalEndComponent::set_type& MaximalEndComponent::getChoicesForState(uint_fast64_t state) {
    auto stateChoicePair = stateToChoicesMapping.find(state);

    STORM_LOG_THROW(stateChoicePair != stateToChoicesMapping.end(), storm::exceptions::InvalidStateException,
                    "Invalid call to MaximalEndComponent::getChoicesForState: cannot retrieve choices for state not contained in MEC.");

    return stateChoicePair->second;
}

bool MaximalEndComponent::containsState(uint_fast64_t state) const {
    auto stateChoicePair = stateToChoicesMapping.find(state);

    if (stateChoicePair == stateToChoicesMapping.end()) {
        return false;
    }
    return true;
}

bool MaximalEndComponent::containsAnyState(storm::storage::BitVector stateSet) const {
    // TODO: iteration over unordered_map is potentially inefficient?
    for (auto const& stateChoicesPair : stateToChoicesMapping) {
        if (stateSet.get(stateChoicesPair.first)) {
            return true;
        }
    }
    return false;
}

void MaximalEndComponent::removeState(uint_fast64_t state) {
    auto stateChoicePair = stateToChoicesMapping.find(state);

    STORM_LOG_THROW(stateChoicePair != stateToChoicesMapping.end(), storm::exceptions::InvalidStateException,
                    "Invalid call to MaximalEndComponent::removeState: cannot remove state not contained in MEC.");

    stateToChoicesMapping.erase(stateChoicePair);
}

bool MaximalEndComponent::containsChoice(uint_fast64_t state, uint_fast64_t choice) const {
    auto stateChoicePair = stateToChoicesMapping.find(state);

    STORM_LOG_THROW(stateChoicePair != stateToChoicesMapping.end(), storm::exceptions::InvalidStateException,
                    "Invalid call to MaximalEndComponent::containsChoice: cannot obtain choices for state not contained in MEC.");

    return stateChoicePair->second.find(choice) != stateChoicePair->second.end();
}

MaximalEndComponent::set_type MaximalEndComponent::getStateSet() const {
    set_type states;
    states.reserve(stateToChoicesMapping.size());

    for (auto const& stateChoicesPair : stateToChoicesMapping) {
        states.insert(stateChoicesPair.first);
    }

    return states;
}

std::ostream& operator<<(std::ostream& out, MaximalEndComponent const& component) {
    out << "{";
    for (auto const& stateChoicesPair : component.stateToChoicesMapping) {
        out << "{" << stateChoicesPair.first << ", " << stateChoicesPair.second << "}";
    }
    out << "}";

    return out;
}

MaximalEndComponent::iterator MaximalEndComponent::begin() {
    return stateToChoicesMapping.begin();
}

MaximalEndComponent::iterator MaximalEndComponent::end() {
    return stateToChoicesMapping.end();
}

MaximalEndComponent::const_iterator MaximalEndComponent::begin() const {
    return stateToChoicesMapping.begin();
}

MaximalEndComponent::const_iterator MaximalEndComponent::end() const {
    return stateToChoicesMapping.end();
}
}  // namespace storage
}  // namespace storm
