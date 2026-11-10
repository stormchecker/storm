#pragma once

#include <cstdint>

#include "storm/transformer/bisimulation/dd/InitialPartitionMode.h"
#include "storm/transformer/bisimulation/dd/RefinementMode.h"
#include "storm/transformer/bisimulation/dd/ReuseMode.h"

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
    bool verbose = false;
    uint64_t showProgressDelay = 5;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
