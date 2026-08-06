#pragma once

namespace storm {
namespace solver {
namespace stateelimination {

/*!
 * An enum that contains all available elimination methods.
 */
enum class EliminationMethod { State, Scc, Hybrid };

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
