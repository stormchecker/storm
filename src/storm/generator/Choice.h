#pragma once

#include <cstdint>
#include <functional>
#include <set>

#include <boost/any.hpp>
#include <boost/optional.hpp>

#include "storm/storage/Distribution.h"
#include "storm/storage/PlayerIndex.h"

namespace storm {
namespace generator {

// A structure holding information about a particular choice.
template<typename ValueType, typename StateType = uint32_t>
struct Choice {
   public:
    Choice(uint_fast64_t actionIndex = 0, bool markovian = false);

    Choice(Choice const& other) = default;
    Choice& operator=(Choice const& other) = default;
    Choice(Choice&& other) = default;
    Choice& operator=(Choice&& other) = default;

    /*!
     * Adds the given choice to the current one.
     */
    void add(Choice const& other);

    /**
     * Given a value q, find the event in the ordered distribution that corresponds to this prob.
     * Example: Given a (sub)distribution { x -> 0.4, y -> 0.3, z -> 0.2 },
     * A value q in [0,0.4] yields x, q in [0.4, 0.7] yields y, and q in [0.7, 0.9] yields z.
     * Any other value for q yields undefined behavior.
     *
     * @param quantile q, a value in the CDF.
     * @return A state
     */
    StateType sampleFromDistribution(ValueType const& quantile) const;

    /*!
     * Returns an iterator to the distribution associated with this choice.
     *
     * @return An iterator to the first element of the distribution.
     */
    typename storm::storage::Distribution<ValueType, StateType>::iterator begin();

    /*!
     * Returns an iterator to the distribution associated with this choice.
     *
     * @return An iterator to the first element of the distribution.
     */
    typename storm::storage::Distribution<ValueType, StateType>::const_iterator begin() const;

    /*!
     * Returns an iterator past the end of the distribution associated with this choice.
     *
     * @return An iterator past the end of the distribution.
     */
    typename storm::storage::Distribution<ValueType, StateType>::iterator end();

    /*!
     * Returns an iterator past the end of the distribution associated with this choice.
     *
     * @return An iterator past the end of the distribution.
     */
    typename storm::storage::Distribution<ValueType, StateType>::const_iterator end() const;

    /*!
     * Inserts the contents of this object to the given output stream.
     *
     * @param out The stream in which to insert the contents.
     */
    template<typename ValueTypePrime, typename StateTypePrime>
    friend std::ostream& operator<<(std::ostream& out, Choice<ValueTypePrime, StateTypePrime> const& choice);

    /*!
     * Adds the given label to the labels associated with this choice.
     *
     * @param label The label to associate with this choice.
     */
    void addLabel(std::string const& label);

    /*!
     * Adds the given label set to the labels associated with this choice.
     *
     * @param labelSet The label set to associate with this choice.
     */
    void addLabels(std::set<std::string> const& labels);

    /*!
     * Returns whether there are labels defined for this choice
     */
    bool hasLabels() const;

    /*!
     * Retrieves the set of labels associated with this choice.
     *
     * @return The set of labels associated with this choice.
     */
    std::set<std::string> const& getLabels() const;

    /*!
     * Sets the players index
     *
     * @param The player index associated with this choice.
     */
    void setPlayerIndex(storm::storage::PlayerIndex const& playerIndex);

    /*!
     * Returns whether there is an index for the player defined for this choice.
     */
    bool hasPlayerIndex() const;

    /*!
     * Retrieves the players index associated with this choice
     *
     * @return The player index associated with this choice.
     */
    storm::storage::PlayerIndex const& getPlayerIndex() const;

    /*!
     * Adds the given data that specifies the origin of this choice w.r.t. the model specification
     */
    void addOriginData(boost::any const& data);

    /*!
     * Returns whether there is origin data defined for this choice
     */
    bool hasOriginData() const;

    /*!
     * Returns the origin data associated with this choice.
     */
    boost::any const& getOriginData() const;

    /*!
     * Retrieves the index of the action of this choice.
     *
     * @return The index of the action of this choice.
     */
    uint_fast64_t getActionIndex() const;

    /*!
     * Retrieves the total mass of this choice.
     *
     * @return The total mass.
     */
    ValueType getTotalMass() const;

    /*!
     * Adds the given probability value to the given state in the underlying distribution.
     */
    void addProbability(StateType const& state, ValueType const& value);

    /*!
     * Adds the given value to the reward associated with this choice.
     */
    void addReward(ValueType const& value);

    /*!
     * Adds the given choices rewards to this choice.
     */
    void addRewards(std::vector<ValueType>&& values);

    /*!
     * Retrieves the rewards for this choice under selected reward models.
     */
    std::vector<ValueType> const& getRewards() const;

    /*!
     * Retrieves whether the choice is Markovian.
     */
    bool isMarkovian() const;

    /*!
     * Retrieves the size of the distribution associated with this choice.
     */
    std::size_t size() const;

    /*!
     * If the size is already known, reserves space in the underlying distribution.
     */
    void reserve(std::size_t const& size);

   private:
    // A flag indicating whether this choice is Markovian or not.
    bool markovian;

    // The action index associated with this choice.
    uint_fast64_t actionIndex;

    // The distribution that is associated with the choice.
    storm::storage::Distribution<ValueType, StateType> distribution;

    // The total probability mass (or rates) of this choice.
    ValueType totalMass;

    // The reward values associated with this choice.
    std::vector<ValueType> rewards;

    // The data that stores what part of the model specification induced this choice
    boost::optional<boost::any> originData;

    // The labels of this choice
    boost::optional<std::set<std::string>> labels;

    // The playerIndex of this choice
    boost::optional<storm::storage::PlayerIndex> playerIndex;
};

template<typename ValueType, typename StateType>
std::ostream& operator<<(std::ostream& out, Choice<ValueType, StateType> const& choice);

}  // namespace generator
}  // namespace storm
