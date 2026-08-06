#pragma once

#include <ostream>

namespace storm {
namespace builder {

// An enum that contains all currently supported exploration orders.
enum class ExplorationOrder { Dfs, Bfs };

std::ostream& operator<<(std::ostream& out, ExplorationOrder const& order);

}  // namespace builder
}  // namespace storm
