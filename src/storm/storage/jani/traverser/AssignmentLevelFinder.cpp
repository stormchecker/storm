#include "AssignmentLevelFinder.h"

namespace storm {
namespace jani {

int64_t AssignmentLevelFinder::getLowestAssignmentLevel(Model const& model) {
    int64_t res = std::numeric_limits<int64_t>::max();
    ConstJaniTraverser::traverse(model, &res);
    return res;
}

void AssignmentLevelFinder::traverse(Assignment const& assignment, boost::any const& data) {
    auto& res = *boost::any_cast<int64_t*>(data);
    res = std::min<int64_t>(res, assignment.getLevel());
}
}  // namespace jani
}  // namespace storm
