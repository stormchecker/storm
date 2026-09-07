#include "Statistics.h"

#include "storm/modelchecker/exploration/ExplorationInformation.h"

namespace storm {
namespace modelchecker {
namespace exploration_detail {

template<typename StateType, typename ValueType>
Statistics<StateType, ValueType>::Statistics()
    : pathsSampled(0),
      pathsSampledSinceLastPrecomputation(0),
      explorationSteps(0),
      explorationStepsSinceLastPrecomputation(0),
      maxPathLength(0),
      numberOfTargetStates(0),
      numberOfExploredStates(0),
      numberOfPrecomputations(0),
      ecDetections(0),
      failedEcDetections(0),
      totalNumberOfEcDetected(0) {
    // Intentionally left empty.
}

template<typename StateType, typename ValueType>
void Statistics<StateType, ValueType>::explorationStep() {
    ++explorationSteps;
    ++explorationStepsSinceLastPrecomputation;
}

template<typename StateType, typename ValueType>
void Statistics<StateType, ValueType>::sampledPath() {
    ++pathsSampled;
    ++pathsSampledSinceLastPrecomputation;
}

template<typename StateType, typename ValueType>
void Statistics<StateType, ValueType>::updateMaxPathLength(std::size_t const& currentPathLength) {
    maxPathLength = std::max(maxPathLength, currentPathLength);
}

template<typename StateType, typename ValueType>
void Statistics<StateType, ValueType>::printToStream(std::ostream& out, ExplorationInformation<StateType, ValueType> const& explorationInformation) const {
    out << "\nExploration statistics:\n";
    out << "Discovered states: " << explorationInformation.getNumberOfDiscoveredStates() << " (" << numberOfExploredStates << " explored, "
        << explorationInformation.getNumberOfUnexploredStates() << " unexplored, " << numberOfTargetStates << " target)\n";
    out << "Exploration steps: " << explorationSteps << '\n';
    out << "Sampled paths: " << pathsSampled << '\n';
    out << "Maximal path length: " << maxPathLength << '\n';
    out << "Precomputations: " << numberOfPrecomputations << '\n';
    out << "EC detections: " << ecDetections << " (" << failedEcDetections << " failed, " << totalNumberOfEcDetected << " EC(s) detected)\n";
}

template struct Statistics<uint32_t, double>;

}  // namespace exploration_detail
}  // namespace modelchecker
}  // namespace storm
