#include "storm-pars/modelchecker/region/monotonicity/MonotonicityKind.h"

#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm::analysis {
std::ostream& operator<<(std::ostream& out, MonotonicityKind kind) {
    switch (kind) {
        case MonotonicityKind::Incr:
            out << "MonIncr";
            break;
        case MonotonicityKind::Decr:
            out << "MonDecr";
            break;
        case MonotonicityKind::Constant:
            out << "Constant";
            break;
        case MonotonicityKind::Not:
            out << "NotMon";
            break;
        case MonotonicityKind::Unknown:
            out << "Unknown";
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                            "Could not get a string from the region monotonicity check result. The case has not been implemented.");
    }
    return out;
}

bool isMonotone(MonotonicityKind kind) {
    return kind == MonotonicityKind::Incr || kind == MonotonicityKind::Decr || kind == MonotonicityKind::Constant;
}
}  // namespace storm::analysis