#pragma once

#include <utility>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/utility/OptionalRef.h"

namespace storm::modelchecker {

template<typename ValueType>
class TerminalStateValue {
   public:
    explicit TerminalStateValue(ValueType value);

    static TerminalStateValue getPlusInfinity();

    static TerminalStateValue getMinusInfinity();

    static TerminalStateValue getOne();

    bool isFinite() const;

    bool isOne() const;

    bool isPositiveInfinity() const;

    bool isNegativeInfinity() const;

    ValueType getFiniteValue() const;

   private:
    TerminalStateValue(ValueType value, bool infinite);

    ValueType const value;
    bool const infinite;  /// if true, this represents +inf (if value is positive) and -inf (otherwise)
};

template<typename ValueType>
struct TotalRewardSpecification {
    /// Those states that are target w.r.t. the total reachability reward query
    storm::storage::BitVector terminalStates;
    /// Rewards collected upon entering terminal states (unmentioned states have reward 0)
    /// The occurring state sets must be subsets of terminalStates and pairwise disjoint.
    std::vector<std::pair<TerminalStateValue<ValueType>, storm::storage::BitVector>> terminalStateValues;
    bool terminalStatesUniversallyAlmostSurelyReached;  /// If true, the target states are reached almost surely from every state under every scheduler
    storm::OptionalRef<std::vector<ValueType> const>
        actionRewards;  /// Rewards collected upon executing actions (if empty, rewards are only collected in terminal states)
};
}  // namespace storm::modelchecker
