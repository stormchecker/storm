#include "TransformationEnvironment.h"

namespace storm::dft {

TransformationEnvironment::TransformationEnvironment()
    : useBisimulation(false), eliminateChains(false), labelBehavior(storm::transformer::EliminationLabelBehavior::KeepLabels) {
    // Intentionally left empty
}

TransformationEnvironment::~TransformationEnvironment() = default;

bool TransformationEnvironment::isUseBisimulation() const {
    return useBisimulation;
}

void TransformationEnvironment::setUseBisimulation(bool value) {
    useBisimulation = value;
}

bool TransformationEnvironment::isEliminateChains() const {
    return eliminateChains;
}

void TransformationEnvironment::setEliminateChains(bool value) {
    eliminateChains = value;
}

storm::transformer::EliminationLabelBehavior TransformationEnvironment::getLabelBehavior() const {
    return labelBehavior;
}

void TransformationEnvironment::setLabelBehavior(storm::transformer::EliminationLabelBehavior value) {
    labelBehavior = value;
}

}  // namespace storm::dft
