#pragma once

#include <boost/container/flat_map.hpp>
#include <iosfwd>
#include <vector>

#include "storm/storage/sparse/StateType.h"
#include "storm/utility/ConstantsComparator.h"

namespace storm {

namespace storage {

template<typename ValueType, typename StateType = uint32_t>
class Distribution {
   public:
    typedef boost::container::flat_map<StateType, ValueType> container_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;

    /*!
     * Creates an empty distribution.
     */
    Distribution();

    Distribution(Distribution const& other) = default;
    Distribution& operator=(Distribution const& other) = default;
    Distribution(Distribution&& other) = default;
    Distribution& operator=(Distribution&& other) = default;

    /*
     * If the size of this distribution is known before adding probabilities,, this method can be used to reserve enough space.
     */
    void reserve(uint64_t size);

    /*!
     * Adds the given distribution to the current one.
     */
    void add(Distribution const& other);

    /*!
     * Checks whether the two distributions specify the same probabilities to go to the same states.
     *
     * @param other The distribution with which the current distribution is to be compared.
     * @return True iff the two distributions are equal.
     */
    bool equals(Distribution<ValueType, StateType> const& other, storm::utility::ConstantsComparator<ValueType> const& comparator) const;

    /*!
     * Assigns the given state the given probability under this distribution.
     *
     * @param state The state to which to assign the probability.
     * @param probability The probability to assign.
     */
    void addProbability(StateType const& state, ValueType const& probability);

    /*!
     * Removes the given probability mass of going to the given state.
     *
     * @param state The state for which to remove the probability.
     * @param probability The probability to remove.
     * @param comparator A comparator that is used to determine if the remaining probability is zero. If so, the
     * entry is removed.
     */
    void removeProbability(StateType const& state, ValueType const& probability, storm::utility::ConstantsComparator<ValueType> const& comparator);

    /*!
     * Removes the probability mass from one state and adds it to another.
     *
     * @param fromState The state from which to take the probability mass.
     * @param toState The state from which to which to add the probability mass.
     * @param probability The probability mass to shift.
     * @param comparator A comparator that is used to determine if the remaining probability is zero. If so, the
     * entry is removed.
     */
    void shiftProbability(StateType const& fromState, StateType const& toState, ValueType const& probability,
                          storm::utility::ConstantsComparator<ValueType> const& comparator);

    /*!
     * Retrieves an iterator to the elements in this distribution.
     *
     * @return The iterator to the elements in this distribution.
     */
    iterator begin();

    /*!
     * Retrieves an iterator to the elements in this distribution.
     *
     * @return The iterator to the elements in this distribution.
     */
    const_iterator begin() const;

    /*!
     * Retrieves an iterator to the elements in this distribution.
     *
     * @return The iterator to the elements in this distribution.
     */
    const_iterator cbegin() const;

    /*!
     * Retrieves an iterator past the elements in this distribution.
     *
     * @return The iterator past the elements in this distribution.
     */
    iterator end();

    /*!
     * Retrieves an iterator past the elements in this distribution.
     *
     * @return The iterator past the elements in this distribution.
     */
    const_iterator end() const;

    /*!
     * Retrieves an iterator past the elements in this distribution.
     *
     * @return The iterator past the elements in this distribution.
     */
    const_iterator cend() const;

    /*!
     * Scales the distribution by multiplying all the probabilities with 1/p where p is the probability of moving
     * to the given state and sets the probability of moving to the given state to zero. If the probability is
     * already zero, this operation has no effect.
     *
     * @param state The state whose associated probability is used to scale the distribution.
     */
    void scale(StateType const& state);

    /*!
     * Retrieves the size of the distribution, i.e. the size of the support set.
     */
    std::size_t size() const;

    bool less(Distribution<ValueType, StateType> const& other, storm::utility::ConstantsComparator<ValueType> const& comparator) const;

    /*!
     * Returns the probability of the given state
     * @param state The state for which the probability is returned.
     * @return The probability of the given state.
     */
    ValueType getProbability(StateType const& state) const;

    /*!
     * Normalizes the distribution such that the values sum up to one.
     */
    void normalize();

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

   private:
    // A list of states and the probabilities that are assigned to them.
    container_type distribution;
};

template<typename ValueType, typename StateType = uint32_t>
std::ostream& operator<<(std::ostream& out, Distribution<ValueType, StateType> const& distribution);
}  // namespace storage
}  // namespace storm

namespace std {

template<typename ValueType>
struct hash<storm::storage::Distribution<ValueType>> {
    std::size_t operator()(storm::storage::Distribution<ValueType> const& distribution) const {
        return (distribution.getHash());
    }
};

}  // namespace std
