#include "storm-pars/modelchecker/region/RegionResultHypothesis.h"

#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
std::ostream& operator<<(std::ostream& os, RegionResultHypothesis const& regionResultHypothesis) {
    switch (regionResultHypothesis) {
        case RegionResultHypothesis::Unknown:
            os << "Unknown";
            break;
        case RegionResultHypothesis::AllSat:
            os << "AllSat?";
            break;
        case RegionResultHypothesis::AllViolated:
            os << "AllViolated?";
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                            "Could not get a string from the region result hypothesis. The case has not been implemented.");
    }
    return os;
}
}  // namespace modelchecker
}  // namespace storm
