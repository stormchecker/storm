#pragma once
#include <boost/container/flat_map.hpp>

#include <cstdint>
#include <limits>

namespace storm::pomdp::beliefs {

/** Shared identifiers and sparse-container types used by the belief subsystem. */
using BeliefStateType = uint64_t;
using BeliefObservationType = uint32_t;
using BeliefId = uint64_t;
using BeliefActionObservationType = uint32_t;
constexpr BeliefActionObservationType DefaultActionObservation = 0;

template<typename BeliefValueType>
using BeliefFlatMap = boost::container::flat_map<BeliefStateType, BeliefValueType>;
constexpr bool BeliefFlatMapIsOrdered = true;  /// Required e.g. for comparing two beliefs

constexpr BeliefObservationType InvalidObservation = std::numeric_limits<BeliefObservationType>::max();
constexpr BeliefId InvalidBeliefId = std::numeric_limits<BeliefId>::max();

}  // namespace storm::pomdp::beliefs
