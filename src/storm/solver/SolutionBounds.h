#pragma once

#include <optional>
#include <utility>
#include <vector>

namespace storm::solver {

/*!
 * Sound lower and upper bounds on a solution vector, as computed by e.g. interval iteration.
 * The first component bounds the solution from below, the second one from above. Both have the same size as
 * the solution. An unset value means that the algorithm in question did not provide any bounds.
 */
template<typename ValueType>
using SolutionBounds = std::optional<std::pair<std::vector<ValueType>, std::vector<ValueType>>>;

}  // namespace storm::solver
