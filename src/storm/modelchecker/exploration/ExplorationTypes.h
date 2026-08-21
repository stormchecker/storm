#pragma once

#include <cstdint>

namespace storm {
namespace modelchecker {
namespace exploration_detail {

// An enumeration of all available precomputation types.
enum class PrecomputationType { Local, Global };

// The available heuristics to choose the next state.
enum class NextStateHeuristic { DifferenceProbabilitySum, Probability, Uniform };

}  // namespace exploration_detail
}  // namespace modelchecker
}  // namespace storm
