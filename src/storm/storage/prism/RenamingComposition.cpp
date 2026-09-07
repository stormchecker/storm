#include "RenamingComposition.h"

#include <boost/algorithm/string/join.hpp>
#include <sstream>
#include <vector>

namespace storm {
namespace prism {

RenamingComposition::RenamingComposition(std::shared_ptr<Composition> const& sub, std::map<std::string, std::string> const& actionRenaming)
    : sub(sub), actionRenaming(actionRenaming) {
    // Intentionally left empty.
}

boost::any RenamingComposition::accept(CompositionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

Composition const& RenamingComposition::getSubcomposition() const {
    return *sub;
}

std::map<std::string, std::string> const& RenamingComposition::getActionRenaming() const {
    return actionRenaming;
}

void RenamingComposition::writeToStream(std::ostream& stream) const {
    std::vector<std::string> renamings;
    for (auto const& renaming : actionRenaming) {
        std::stringstream s;
        s << renaming.second << " <- " << renaming.first;
        renamings.push_back(s.str());
    }
    stream << *sub << "{" << boost::join(renamings, ", ") << "}";
}

}  // namespace prism
}  // namespace storm
