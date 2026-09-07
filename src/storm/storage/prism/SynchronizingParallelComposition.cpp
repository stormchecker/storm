#include "SynchronizingParallelComposition.h"

namespace storm {
namespace prism {

SynchronizingParallelComposition::SynchronizingParallelComposition(std::shared_ptr<Composition> const& left, std::shared_ptr<Composition> const& right)
    : ParallelComposition(left, right) {
    // Intentionally left empty.
}

boost::any SynchronizingParallelComposition::accept(CompositionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void SynchronizingParallelComposition::writeToStream(std::ostream& stream) const {
    stream << "(" << this->getLeftSubcomposition() << " || " << this->getRightSubcomposition() << ")";
}

}  // namespace prism
}  // namespace storm
