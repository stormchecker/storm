#include "storm/environment/solver/MinMaxLpSolverEnvironment.h"

namespace storm {

MinMaxLpSolverEnvironment::MinMaxLpSolverEnvironment() : useEqualityForSingleActions(false), optimizeOnlyForInitialState(false), useNonTrivialBounds(false) {
    // Intentionally left empty.
}

void MinMaxLpSolverEnvironment::setUseEqualityForSingleActions(bool newValue) {
    useEqualityForSingleActions = newValue;
}
void MinMaxLpSolverEnvironment::setOptimizeOnlyForInitialState(bool newValue) {
    optimizeOnlyForInitialState = newValue;
}
void MinMaxLpSolverEnvironment::setUseNonTrivialBounds(bool newValue) {
    useNonTrivialBounds = newValue;
}

bool MinMaxLpSolverEnvironment::getUseEqualityForSingleActions() const {
    return useEqualityForSingleActions;
}
bool MinMaxLpSolverEnvironment::getOptimizeOnlyForInitialState() const {
    return optimizeOnlyForInitialState;
}
bool MinMaxLpSolverEnvironment::getUseNonTrivialBounds() const {
    return useNonTrivialBounds;
}
}  // namespace storm