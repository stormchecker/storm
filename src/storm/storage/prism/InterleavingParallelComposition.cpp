#include "InterleavingParallelComposition.h"

namespace storm {
namespace prism {

InterleavingParallelComposition::InterleavingParallelComposition(std::shared_ptr<Composition> const& left, std::shared_ptr<Composition> const& right)
    : ParallelComposition(left, right) {
    // Intentionally left empty.
}

boost::any InterleavingParallelComposition::accept(CompositionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void InterleavingParallelComposition::writeToStream(std::ostream& stream) const {
    stream << "(" << this->getLeftSubcomposition() << " ||| " << this->getRightSubcomposition() << ")";
}

}  // namespace prism
}  // namespace storm
