#include "storm/solver/SolverGuarantee.h"

namespace storm {
namespace solver {

std::ostream& operator<<(std::ostream& out, SolverGuarantee const& guarantee) {
    switch (guarantee) {
        case SolverGuarantee::GreaterOrEqual:
            out << "greater-or-equal";
            break;
        case SolverGuarantee::LessOrEqual:
            out << "less-or-equal";
            break;
        case SolverGuarantee::None:
            out << "none";
            break;
    }
    return out;
}

}  // namespace solver
}  // namespace storm
