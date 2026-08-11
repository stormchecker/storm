#include "storm/storage/dd/bisimulation/InternalSignatureRefiner.h"

namespace storm {
namespace dd {
namespace bisimulation {

InternalSignatureRefinerOptions::InternalSignatureRefinerOptions(bool shiftStateVariables, BisimulationOptions const& bisimulationOptions)
    : shiftStateVariables(shiftStateVariables),
      reuseBlockNumbers(bisimulationOptions.reuseMode == ReuseMode::BlockNumbers),
      createChangedStates(bisimulationOptions.refinementMode == RefinementMode::ChangedStates) {
    // Intentionally left empty.
}

ReuseWrapper::ReuseWrapper() : ReuseWrapper(false) {
    // Intentionally left empty.
}

ReuseWrapper::ReuseWrapper(bool value) : value(value) {
    // Intentionally left empty.
}

bool ReuseWrapper::isReused() const {
    return value;
}

void ReuseWrapper::setReused() {
    value = true;
}

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
