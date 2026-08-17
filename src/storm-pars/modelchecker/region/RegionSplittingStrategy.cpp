#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"

#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
std::ostream& operator<<(std::ostream& os, RegionSplittingStrategy::Heuristic const& e) {
    switch (e) {
        case RegionSplittingStrategy::Heuristic::EstimateBased:
            os << "Estimate-Based";
            break;
        case RegionSplittingStrategy::Heuristic::RoundRobin:
            os << "Round Robin";
            break;
        case RegionSplittingStrategy::Heuristic::Default:
            os << "Default";
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                            "Could not get a string from the region check engine. The case has not been implemented.");
    }
    return os;
}
}  // namespace modelchecker
}  // namespace storm
