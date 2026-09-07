#include "RestrictedParallelComposition.h"

#include <boost/algorithm/string/join.hpp>

namespace storm {
namespace prism {

RestrictedParallelComposition::RestrictedParallelComposition(std::shared_ptr<Composition> const& left, std::set<std::string> const& synchronizingActions,
                                                             std::shared_ptr<Composition> const& right)
    : storm::prism::ParallelComposition(left, right), synchronizingActions(synchronizingActions) {
    // Intentionally left empty.
}

boost::any RestrictedParallelComposition::accept(CompositionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::set<std::string> const& RestrictedParallelComposition::getSynchronizingActions() const {
    return synchronizingActions;
}

void RestrictedParallelComposition::writeToStream(std::ostream& stream) const {
    stream << "(" << this->getLeftSubcomposition() << " |[" << boost::algorithm::join(synchronizingActions, ", ") << "]| " << this->getRightSubcomposition()
           << ")";
}

}  // namespace prism
}  // namespace storm
