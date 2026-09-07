#include "HidingComposition.h"

#include <boost/algorithm/string/join.hpp>

namespace storm {
namespace prism {

HidingComposition::HidingComposition(std::shared_ptr<Composition> const& sub, std::set<std::string> const& actionsToHide)
    : sub(sub), actionsToHide(actionsToHide) {
    // Intentionally left empty.
}

boost::any HidingComposition::accept(CompositionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

Composition const& HidingComposition::getSubcomposition() const {
    return *sub;
}

std::set<std::string> const& HidingComposition::getActionsToHide() const {
    return actionsToHide;
}

void HidingComposition::writeToStream(std::ostream& stream) const {
    stream << "(" << *sub << ")" << " " << "{" << boost::join(actionsToHide, ", ") << "}";
}

}  // namespace prism
}  // namespace storm
