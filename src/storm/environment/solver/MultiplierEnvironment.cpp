#include "storm/environment/solver/MultiplierEnvironment.h"

namespace storm {

MultiplierEnvironment::MultiplierEnvironment() : type(storm::solver::MultiplierType::ViOperator), typeSetFromDefault(true) {
    // Intentionally left empty.
}

MultiplierEnvironment::~MultiplierEnvironment() {
    // Intentionally left empty
}

storm::solver::MultiplierType const& MultiplierEnvironment::getType() const {
    return type;
}

bool const& MultiplierEnvironment::isTypeSetFromDefault() const {
    return typeSetFromDefault;
}

void MultiplierEnvironment::setType(storm::solver::MultiplierType value, bool isSetFromDefault) {
    type = value;
    typeSetFromDefault = isSetFromDefault;
}

}  // namespace storm
