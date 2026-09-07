#include "MultiObjectiveModelCheckingMethod.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

std::string toString(MultiObjectiveMethod m) {
    switch (m) {
        case MultiObjectiveMethod::Pcaa:
            return "Pareto Curve Approximation Algorithm";
        case MultiObjectiveMethod::ConstraintBased:
            return "Constraint Based Algorithm";
    }
    return "invalid";
}
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
