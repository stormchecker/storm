#include "storm/environment/solver/TopologicalSolverEnvironment.h"

#include "storm-config.h"

#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidArgumentException.h"

namespace storm {

TopologicalSolverEnvironment::TopologicalSolverEnvironment()
    : underlyingEquationSolverType(
#if defined STORM_HAVE_GMM
          storm::solver::EquationSolverType::Gmmxx
#else
          storm::solver::EquationSolverType::Eigen
#endif
          ),
      underlyingEquationSolverTypeSetFromDefault(true),
      underlyingMinMaxMethod(storm::solver::MinMaxMethod::ValueIteration),
      underlyingMinMaxMethodSetFromDefault(true),
      extendRelevantValues(false) {
    // Intentionally left empty.
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

void TopologicalSolverEnvironment::setUnderlyingEquationSolverType(storm::solver::EquationSolverType value, bool isSetFromDefault) {
    STORM_LOG_THROW(value != storm::solver::EquationSolverType::Topological, storm::exceptions::InvalidArgumentException,
                    "Can not use the topological solver as underlying solver of the topological solver.");
    underlyingEquationSolverTypeSetFromDefault = isSetFromDefault;
    underlyingEquationSolverType = value;
}

storm::solver::MinMaxMethod const& TopologicalSolverEnvironment::getUnderlyingMinMaxMethod() const {
    return underlyingMinMaxMethod;
}

bool const& TopologicalSolverEnvironment::isUnderlyingMinMaxMethodSetFromDefault() const {
    return underlyingMinMaxMethodSetFromDefault;
}

void TopologicalSolverEnvironment::setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod value, bool isSetFromDefault) {
    STORM_LOG_THROW(value != storm::solver::MinMaxMethod::Topological, storm::exceptions::InvalidArgumentException,
                    "Can not use the topological solver as underlying solver of the topological solver.");
    underlyingMinMaxMethodSetFromDefault = isSetFromDefault;
    underlyingMinMaxMethod = value;
}

bool TopologicalSolverEnvironment::isExtendRelevantValues() const {
    return extendRelevantValues;
}

void TopologicalSolverEnvironment::setExtendRelevantValues(bool value) {
    extendRelevantValues = value;
}

}  // namespace storm
