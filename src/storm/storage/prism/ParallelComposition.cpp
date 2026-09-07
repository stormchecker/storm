#include "ParallelComposition.h"

#include <boost/algorithm/string/join.hpp>

namespace storm {
namespace prism {

ParallelComposition::ParallelComposition(std::shared_ptr<Composition> const& left, std::shared_ptr<Composition> const& right) : left(left), right(right) {
    // Intentionally left empty.
}

Composition const& ParallelComposition::getLeftSubcomposition() const {
    return *left;
}

Composition const& ParallelComposition::getRightSubcomposition() const {
    return *right;
}

}  // namespace prism
}  // namespace storm
