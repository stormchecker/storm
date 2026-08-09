#include "storm-pars/modelchecker/region/monotonicity/Assumption.h"

#include "storm/utility/macros.h"

namespace storm {
namespace analysis {

std::ostream& operator<<(std::ostream& out, Assumption const& assumption) {
    STORM_LOG_ASSERT(assumption.relation == storm::expressions::RelationType::Greater || assumption.relation == storm::expressions::RelationType::Equal,
                      "Only Greater or Equal assumptions are supported.");
    out << "s" << assumption.state1 << (assumption.relation == storm::expressions::RelationType::Greater ? " > s" : " = s") << assumption.state2;
    return out;
}

}  // namespace analysis
}  // namespace storm
