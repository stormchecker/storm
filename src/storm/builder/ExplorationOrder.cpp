#include "ExplorationOrder.h"

namespace storm {
namespace builder {

std::ostream& operator<<(std::ostream& out, ExplorationOrder const& order) {
    switch (order) {
        case ExplorationOrder::Dfs:
            out << "depth-first";
            break;
        case ExplorationOrder::Bfs:
            out << "breadth-first";
            break;
        default:
            out << "undefined";
            break;
    }
    return out;
}

}  // namespace builder
}  // namespace storm
