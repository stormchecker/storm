#pragma once

#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/bisimulation/BisimulationOptions.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
class InternalSignatureRefiner;

struct InternalSignatureRefinerOptions {
    InternalSignatureRefinerOptions(bool shiftStateVariables, BisimulationOptions const& bisimulationOptions);

    bool shiftStateVariables;
    bool reuseBlockNumbers;
    bool createChangedStates;
};

class ReuseWrapper {
   public:
    ReuseWrapper();
    ReuseWrapper(bool value);

    bool isReused() const;
    void setReused();

   private:
    bool value;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
