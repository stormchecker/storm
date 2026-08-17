#pragma once

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

#include "storm-pomdp/beliefs/utility/types.h"

namespace storm::pomdp::beliefs {

template<typename ValueType>
class BeliefBuilder;

/*!
 * Represents a belief of a Pomdp, i.e. a probability distribution over the states of a POMDP.
 * A belief also knows its observation.
 * @note A belief is immutable. Use the BeliefBuilder class to construct new beliefs.
 * @tparam ValueTypeArg the type of the values (probabilities) of the belief.
 */
template<typename ValueTypeArg>
class Belief {
   public:
    using ValueType = ValueTypeArg;
    friend class BeliefBuilder<Belief<ValueType>>;

    Belief() = delete;
    Belief(Belief const& other) = default;
    Belief(Belief&& other) = default;
    Belief<ValueType>& operator=(Belief const& other) = default;
    Belief<ValueType>& operator=(Belief&& other) = default;

    /*!
     * @return the number of states in the support of this belief.
     */
    std::size_t size() const;

    /*!
     * @return a representative state of this belief.
     * @pre the belief is valid, in particular not empty.
     */
    BeliefStateType representativeState() const;

    /*!
     * @return the observation of this belief.
     */
    BeliefObservationType const& observation() const;

    /*!
     * @return true if this belief is equal to the other belief.
     */
    bool operator==(Belief const& other) const;

    /*!
     * A (human-readable) string representation of this belief
     * @param convertToDouble if true, numbers are converted to double before printing. If this has ValueType=RationalNumber, the output as double is readable
     * but potentially imprecise
     */
    std::string toString(bool convertToDouble = true) const;

    /*!
     * @param summands a vector containing a value for each state of the underlying POMDP.
     * @return the sum of the POMDP state values, each multiplied by the value assigned to that state in this belief
     * @pre for every state in the support of this belief, the corresponding index in summands must be valid
     */
    template<typename SummandsType>
    SummandsType getWeightedSum(std::vector<SummandsType> const& summands) const {
        auto sum = storm::utility::zero<SummandsType>();
        forEach([&sum, &summands](auto const& state, auto const& val) {
            STORM_LOG_ASSERT(state < summands.size(), "State " << state << " is out of range for the given summands.");
            // If SummandsType is the same as ValueType, we can directly multiply the value, otherwise convert
            if constexpr (std::is_same_v<SummandsType, ValueType>) {
                sum += summands[state] * val;
            } else {
                sum += summands[state] * storm::utility::convertNumber<SummandsType>(val);
            }
        });
        return sum;
    };

    /*!
     * @param f a function that is called for each (state, value)-pair in the support of this belief.
     */
    template<typename FunctionType>
    void forEach(FunctionType const& f) const {
        for (auto const& [state, value] : data) {
            f(state, value);
        }
    }

    /*!
     * @return true if f returns true for all (state, value)-pairs in the support of this belief.
     */
    template<typename FunctionType>
    bool allOf(FunctionType const& f) const {
        for (auto const& [state, value] : data) {
            if (!f(state, value)) {
                return false;
            }
        }
        return true;
    }

    /*!
     * @param f a function that is called for each state in the support of this belief.
     */
    template<typename FunctionType>
    void forEachStateInSupport(FunctionType const& f) const {
        for (auto const& [state, value] : data) {
            f(state);
        }
    }

    /*!
     * Calls the function f for each state in X with the arguments (state, value1, value2), where
     * - value1 is the value for state of this belief
     * - value2 is the value for state of the other belief
     * - if considerOnlyThisSupport is true, X is the set of states in the support of this belief
     * - otherwise, X is the set of states in the support of either this or the other belief
     */
    template<typename FunctionType>
    void forEachCombine(Belief<ValueType> const& other, FunctionType const& f, bool considerOnlyThisSupport = false) const {
        static_assert(BeliefFlatMapIsOrdered);
        auto const zero = storm::utility::zero<ValueType>();
        auto it2 = other.data.cbegin();
        auto const it2End = other.data.cend();
        if (considerOnlyThisSupport) {
            for (auto const& [state1, value1] : data) {
                while (it2 != it2End && it2->first < state1) {
                    ++it2;
                }
                if (it2 == it2End || it2->first > state1) {
                    f(state1, value1, zero);
                } else {
                    STORM_LOG_ASSERT(state1 == it2->first, "Unexpected state.");
                    f(state1, value1, it2->second);
                }
            }
        } else {
            auto it1 = data.cbegin();
            auto const it1End = data.cend();
            while (it1 != it1End && it2 != it2End) {
                auto const state1 = it1->first;
                auto const state2 = it2->first;
                if (state1 == state2) {
                    f(state1, it1->second, it2->second);
                    ++it1;
                    ++it2;
                } else if (state1 < state2) {
                    f(state1, it1->second, zero);
                    ++it1;
                } else {
                    STORM_LOG_ASSERT(state2 < state1, "unexpected states.");
                    f(state2, zero, it2->second);
                    ++it2;
                }
            }
            for (; it1 != it1End; ++it1) {
                f(it1->first, it1->second, zero);
            }
            for (; it2 != it2End; ++it2) {
                f(it2->first, zero, it2->second);
            }
        }
    }

    /*!
     * @return provides a hash value for the given belief
     */
    struct BeliefHash {
        std::size_t operator()(Belief const& belief) const;
    };

   private:
    /*!
     * Constructs a belief from the given Data.
     * @note Use the BeliefBuilder class to create a belief.
     */
    Belief(BeliefFlatMap<ValueType>&& data, BeliefObservationType&& obs);

    BeliefFlatMap<ValueType> const data;
    BeliefObservationType const obs;
};

}  // namespace storm::pomdp::beliefs
