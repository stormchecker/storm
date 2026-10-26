#pragma once

#include "storm/environment/solver/SolverEnvironment.h"

#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class TopologicalSolverEnvironment {
   public:
    TopologicalSolverEnvironment();
    ~TopologicalSolverEnvironment();

    storm::solver::EquationSolverType const& getUnderlyingEquationSolverType() const;
    bool const& isUnderlyingEquationSolverTypeSetFromDefault() const;
    void setUnderlyingEquationSolverType(storm::solver::EquationSolverType value, bool isSetFromDefault = false);

    storm::solver::MinMaxMethod const& getUnderlyingMinMaxMethod() const;
    bool const& isUnderlyingMinMaxMethodSetFromDefault() const;
    void setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod value, bool isSetFromDefault = false);

    bool isExtendRelevantValues() const;
    void setExtendRelevantValues(bool value);

   private:
    storm::solver::EquationSolverType underlyingEquationSolverType;
    bool underlyingEquationSolverTypeSetFromDefault;

    storm::solver::MinMaxMethod underlyingMinMaxMethod;
    bool underlyingMinMaxMethodSetFromDefault;
    bool extendRelevantValues;
};
}  // namespace storm
