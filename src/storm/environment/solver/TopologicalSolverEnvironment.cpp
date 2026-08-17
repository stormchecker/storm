#include "storm/environment/solver/TopologicalSolverEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/TopologicalEquationSolverSettings.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidArgumentException.h"

namespace storm {

TopologicalSolverEnvironment::TopologicalSolverEnvironment() {
    auto const& topologicalSettings = storm::settings::getModule<storm::settings::modules::TopologicalEquationSolverSettings>();
    underlyingEquationSolverType = topologicalSettings.getUnderlyingEquationSolverType();
    underlyingEquationSolverTypeSetFromDefault = topologicalSettings.isUnderlyingEquationSolverTypeSetFromDefaultValue();

    underlyingMinMaxMethod = topologicalSettings.getUnderlyingMinMaxMethod();
    underlyingMinMaxMethodSetFromDefault = topologicalSettings.isUnderlyingMinMaxMethodSetFromDefaultValue();
    extendRelevantValues = topologicalSettings.isExtendRelevantValues();
}

TopologicalSolverEnvironment::~TopologicalSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::EquationSolverType const& TopologicalSolverEnvironment::getUnderlyingEquationSolverType() const {
    return underlyingEquationSolverType;
}

bool const& TopologicalSolverEnvironment::isUnderlyingEquationSolverTypeSetFromDefault() const {
    return underlyingEquationSolverTypeSetFromDefault;
}

void TopologicalSolverEnvironment::setUnderlyingEquationSolverType(storm::solver::EquationSolverType value) {
    STORM_LOG_THROW(value != storm::solver::EquationSolverType::Topological, storm::exceptions::InvalidArgumentException,
                    "Can not use the topological solver as underlying solver of the topological solver.");
    underlyingEquationSolverTypeSetFromDefault = false;
    underlyingEquationSolverType = value;
}

storm::solver::MinMaxMethod const& TopologicalSolverEnvironment::getUnderlyingMinMaxMethod() const {
    return underlyingMinMaxMethod;
}

bool const& TopologicalSolverEnvironment::isUnderlyingMinMaxMethodSetFromDefault() const {
    return underlyingMinMaxMethodSetFromDefault;
}

void TopologicalSolverEnvironment::setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod value) {
    STORM_LOG_THROW(value != storm::solver::MinMaxMethod::Topological, storm::exceptions::InvalidArgumentException,
                    "Can not use the topological solver as underlying solver of the topological solver.");
    underlyingMinMaxMethodSetFromDefault = false;
    underlyingMinMaxMethod = value;
}

bool TopologicalSolverEnvironment::isExtendRelevantValues() const {
    return extendRelevantValues;
}

void TopologicalSolverEnvironment::setExtendRelevantValues(bool value) {
    extendRelevantValues = value;
}

}  // namespace storm
