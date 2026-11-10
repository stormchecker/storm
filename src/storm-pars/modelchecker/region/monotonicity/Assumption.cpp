#include "storm-pars/modelchecker/region/monotonicity/Assumption.h"

namespace storm {
namespace analysis {

std::ostream& operator<<(std::ostream& out, Assumption const& assumption) {
    out << "s" << assumption.state1 << (assumption.relation == AssumptionRelation::Greater ? " > s" : " = s") << assumption.state2;
    return out;
}

}  // namespace analysis
}  // namespace storm
