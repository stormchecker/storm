#include "storm/generator/StateBehavior.h"

#include <algorithm>
#include <utility>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/macros.h"

namespace storm {
namespace generator {

template<typename ValueType, typename StateType>
StateBehavior<ValueType, StateType>::StateBehavior() : numberOfChoices(0), expanded(false) {
    // Intentionally left empty.
}

template<typename ValueType, typename StateType>
StateBehavior<ValueType, StateType>::StateBehavior(StateBehavior const& other)
    : choices(other.choices.begin(), other.choices.begin() + other.numberOfChoices),
      numberOfChoices(other.numberOfChoices),
      stateRewards(other.stateRewards),
      expanded(other.expanded) {
    // Intentionally left empty. Note that we only copy the choices that are actually part of the behavior.
}

template<typename ValueType, typename StateType>
StateBehavior<ValueType, StateType>::StateBehavior(StateBehavior&& other) noexcept
    : choices(std::move(other.choices)),
      numberOfChoices(std::exchange(other.numberOfChoices, 0)),
      stateRewards(std::move(other.stateRewards)),
      expanded(std::exchange(other.expanded, false)) {
    // Make sure that other is in a well-defined (empty) state.
    other.choices.clear();
    other.stateRewards.clear();
}

template<typename ValueType, typename StateType>
StateBehavior<ValueType, StateType>& StateBehavior<ValueType, StateType>::operator=(StateBehavior&& other) noexcept {
    if (this != &other) {
        choices = std::move(other.choices);
        numberOfChoices = std::exchange(other.numberOfChoices, 0);
        stateRewards = std::move(other.stateRewards);
        expanded = std::exchange(other.expanded, false);
        other.choices.clear();
        other.stateRewards.clear();
    }
    return *this;
}

template<typename ValueType, typename StateType>
StateBehavior<ValueType, StateType>& StateBehavior<ValueType, StateType>::operator=(StateBehavior const& other) {
    if (this != &other) {
        // Copy the active choices into the already existing choices (this reuses their memory)
        if (choices.size() < other.numberOfChoices) {
            choices.resize(other.numberOfChoices);
        }
        std::copy(other.choices.begin(), other.choices.begin() + other.numberOfChoices, choices.begin());
        numberOfChoices = other.numberOfChoices;
        stateRewards = other.stateRewards;
        expanded = other.expanded;
    }
    return *this;
}

template<typename ValueType, typename StateType>
void StateBehavior<ValueType, StateType>::clear() {
    clearChoices();
    stateRewards.clear();
    expanded = false;
}

template<typename ValueType, typename StateType>
void StateBehavior<ValueType, StateType>::clearChoices() {
    numberOfChoices = 0;
}

template<typename ValueType, typename StateType>
void StateBehavior<ValueType, StateType>::addChoice(Choice<ValueType, StateType>&& choice) {
    if (numberOfChoices < choices.size()) {
        choices[numberOfChoices] = std::move(choice);
    } else {
        choices.push_back(std::move(choice));
    }
    ++numberOfChoices;
}

template<typename ValueType, typename StateType>
Choice<ValueType, StateType>& StateBehavior<ValueType, StateType>::startNewChoice(uint_fast64_t actionIndex, bool markovian) {
    if (numberOfChoices < choices.size()) {
        choices[numberOfChoices].reset(actionIndex, markovian);
    } else {
        choices.emplace_back(actionIndex, markovian);
    }
    return choices[numberOfChoices++];
}

template<typename ValueType, typename StateType>
void StateBehavior<ValueType, StateType>::removeLastChoices(std::size_t numberOfChoicesToRemove) {
    STORM_LOG_ASSERT(numberOfChoicesToRemove <= numberOfChoices, "Cannot remove more choices than available.");
    numberOfChoices -= numberOfChoicesToRemove;
}

template<typename ValueType, typename StateType>
void StateBehavior<ValueType, StateType>::addStateReward(ValueType const& stateReward) {
    stateRewards.push_back(stateReward);
}

template<typename ValueType, typename StateType>
void StateBehavior<ValueType, StateType>::addStateRewards(std::vector<ValueType>&& stateRewards) {
    this->stateRewards = std::move(stateRewards);
}

template<typename ValueType, typename StateType>
void StateBehavior<ValueType, StateType>::setExpanded(bool newValue) {
    this->expanded = newValue;
}

template<typename ValueType, typename StateType>
bool StateBehavior<ValueType, StateType>::wasExpanded() const {
    return expanded;
}

template<typename ValueType, typename StateType>
bool StateBehavior<ValueType, StateType>::empty() const {
    return numberOfChoices == 0;
}

template<typename ValueType, typename StateType>
typename std::vector<Choice<ValueType, StateType>>::const_iterator StateBehavior<ValueType, StateType>::begin() const {
    return choices.begin();
}

template<typename ValueType, typename StateType>
typename std::vector<Choice<ValueType, StateType>>::const_iterator StateBehavior<ValueType, StateType>::end() const {
    return choices.begin() + numberOfChoices;
}

template<typename ValueType, typename StateType>
std::span<Choice<ValueType, StateType> const> StateBehavior<ValueType, StateType>::getChoices() const {
    return std::span<Choice<ValueType, StateType> const>(choices.data(), numberOfChoices);
}

template<typename ValueType, typename StateType>
std::span<Choice<ValueType, StateType>> StateBehavior<ValueType, StateType>::getChoices() {
    return std::span<Choice<ValueType, StateType>>(choices.data(), numberOfChoices);
}

template<typename ValueType, typename StateType>
std::vector<ValueType> const& StateBehavior<ValueType, StateType>::getStateRewards() const {
    return stateRewards;
}

template<typename ValueType, typename StateType>
std::vector<ValueType>& StateBehavior<ValueType, StateType>::getStateRewards() {
    return stateRewards;
}

template<typename ValueType, typename StateType>
std::size_t StateBehavior<ValueType, StateType>::getNumberOfChoices() const {
    return numberOfChoices;
}

template class StateBehavior<double>;
template class StateBehavior<storm::RationalNumber>;
template class StateBehavior<storm::RationalFunction>;
template class StateBehavior<storm::Interval>;
template class StateBehavior<storm::RationalInterval>;
}  // namespace generator
}  // namespace storm
