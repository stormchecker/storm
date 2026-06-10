#include "storm/modelchecker/cvar/CvarMethod.h"

namespace storm {
namespace modelchecker {
namespace cvar {

std::string toString(CvarMethod method) {
    switch (method) {
        case CvarMethod::Auto:
            return "auto";
        case CvarMethod::WeightedReachability:
            return "weighted-reachability";
        case CvarMethod::SspParetoVi:
            return "ssp-vi";
    }
    return "unknown";
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
