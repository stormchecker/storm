#include "SolverStatus.h"

namespace storm {
namespace solver {

std::ostream& operator<<(std::ostream& out, SolverStatus const& status) {
    switch (status) {
        case SolverStatus::Converged:
            out << "converged";
            break;
        case SolverStatus::TerminatedEarly:
            out << "terminated";
            break;
        case SolverStatus::MaximalIterationsExceeded:
            out << "maximal iterations exceeded";
            break;
        case SolverStatus::InProgress:
            out << "in progress";
            break;
        case SolverStatus::Aborted:
            out << "aborted";
            break;
    }
    return out;
}

}  // namespace solver
}  // namespace storm
