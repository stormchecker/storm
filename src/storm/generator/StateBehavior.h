#pragma once

#include <cstdint>
#include <span>

#include "storm/generator/Choice.h"

namespace storm {
namespace generator {

template<typename ValueType, typename StateType = uint32_t>
class StateBehavior {
   public:
    /*!
     * Creates an empty behavior, i.e. the state was not yet expanded.
     */
    StateBehavior();

    StateBehavior(StateBehavior const& other);
    /*!
     * Moves the content of other into this. Afterwards, other is an empty behavior (as if it was cleared).
     */
    StateBehavior(StateBehavior&& other) noexcept;
    StateBehavior& operator=(StateBehavior const& other);
    StateBehavior& operator=(StateBehavior&& other) noexcept;

    /*!
     * Resets this to an empty behavior, i.e. the state was not yet expanded.
     * The allocated memory (including that of the choices) is kept for reuse.
     */
    void clear();

    /*!
     * Removes all choices (but keeps the state rewards and the allocated memory of the choices for reuse).
     */
    void clearChoices();

    /*!
     * Adds the given choice to the behavior of the state.
     */
    void addChoice(Choice<ValueType, StateType>&& choice);

    /*!
     * Adds a new empty choice with the given action index and returns a reference to it.
     * The memory of previously removed choices is reused if possible.
     * @note The returned reference is invalidated when another choice is added.
     */
    Choice<ValueType, StateType>& startNewChoice(uint_fast64_t actionIndex = 0, bool markovian = false);

    /*!
     * Removes the last given number of choices.
     */
    void removeLastChoices(std::size_t numberOfChoices);

    /*!
     * Adds the given state reward to the behavior of the state.
     */
    void addStateReward(ValueType const& stateReward);

    /*!
     * Adds the given state rewards to the behavior of the state.
     */
    void addStateRewards(std::vector<ValueType>&& stateRewards);

    /*!
     * Sets whether the state was expanded.
     */
    void setExpanded(bool newValue = true);

    /*!
     * Retrieves whether the state was expanded.
     */
    bool wasExpanded() const;

    /*!
     * Retrieves whether the behavior is empty in the sense that there are no available choices.
     */
    bool empty() const;

    /*!
     * Retrieves an iterator to the choices available in the behavior.
     */
    typename std::vector<Choice<ValueType, StateType>>::const_iterator begin() const;

    /*!
     * Retrieves an iterator past the choices available in the behavior.
     */
    typename std::vector<Choice<ValueType, StateType>>::const_iterator end() const;

    /*!
     * Retrieves the choices.
     */
    std::span<Choice<ValueType, StateType> const> getChoices() const;

    /*!
     * Retrieves the choices.
     */
    std::span<Choice<ValueType, StateType>> getChoices();

    /*!
     * Retrieves the list of state rewards under selected reward models.
     */
    std::vector<ValueType> const& getStateRewards() const;

    /*!
     * Retrieves the list of state rewards under selected reward models. The rewards can be modified in place.
     */
    std::vector<ValueType>& getStateRewards();

    /*!
     * Retrieves the number of choices in the behavior.
     */
    std::size_t getNumberOfChoices() const;

   private:
    // The storage for the choices. Only the first numberOfChoices entries are considered to be part of this behavior; the remaining entries are kept for reuse.
    std::vector<Choice<ValueType, StateType>> choices;

    // The number of choices available in the state.
    std::size_t numberOfChoices;

    // The state rewards (under the different, selected reward models) of the state.
    std::vector<ValueType> stateRewards;

    // A flag indicating whether the state was actually expanded.
    bool expanded;
};

}  // namespace generator
}  // namespace storm
