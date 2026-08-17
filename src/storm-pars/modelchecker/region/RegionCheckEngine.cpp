#include "storm-pars/modelchecker/region/RegionCheckEngine.h"

#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
std::ostream& operator<<(std::ostream& os, RegionCheckEngine const& e) {
    switch (e) {
        case RegionCheckEngine::ParameterLifting:
            os << "Parameter Lifting";
            break;
        case RegionCheckEngine::ExactParameterLifting:
            os << "Exact Parameter Lifting";
            break;
        case RegionCheckEngine::ValidatingParameterLifting:
            os << "Validating Parameter Lifting";
            break;
        case RegionCheckEngine::RobustParameterLifting:
            os << "Robust Parameter Lifting";
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                            "Could not get a string from the region check engine. The case has not been implemented.");
    }
    return os;
}
}  // namespace modelchecker
}  // namespace storm
