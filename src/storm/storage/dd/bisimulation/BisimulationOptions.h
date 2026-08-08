#pragma once

#include "storm/storage/dd/bisimulation/InitialPartitionMode.h"
#include "storm/storage/dd/bisimulation/RefinementMode.h"
#include "storm/storage/dd/bisimulation/ReuseMode.h"

namespace storm {
namespace dd {
namespace bisimulation {

// Configuration for dd-based bisimulation minimization.
struct BisimulationOptions {
    ReuseMode reuseMode = ReuseMode::BlockNumbers;
    RefinementMode refinementMode = RefinementMode::Full;
    InitialPartitionMode initialPartitionMode = InitialPartitionMode::Finer;
    bool useRepresentatives = false;
    bool useOriginalVariables = false;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
