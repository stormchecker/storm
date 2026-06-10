#pragma once

#include <cstdint>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class NativeSolverEnvironment {
   public:
    NativeSolverEnvironment();
    ~NativeSolverEnvironment();

    storm::solver::NativeLinearEquationSolverMethod const& getMethod() const;
    bool const& isMethodSetFromDefault() const;
    void setMethod(storm::solver::NativeLinearEquationSolverMethod value);
    uint64_t const& getMaximalNumberOfIterations() const;
    void setMaximalNumberOfIterations(uint64_t value);
    storm::RationalNumber const& getPrecision() const;
    void setPrecision(storm::RationalNumber value);
    bool const& getRelativeTerminationCriterion() const;
    void setRelativeTerminationCriterion(bool value);
    storm::solver::MultiplicationStyle const& getPowerMethodMultiplicationStyle() const;
    void setPowerMethodMultiplicationStyle(storm::solver::MultiplicationStyle value);
    storm::RationalNumber const& getSorOmega() const;
    void setSorOmega(storm::RationalNumber const& value);
    bool isSymmetricUpdatesSet() const;
    void setSymmetricUpdates(bool value);

   private:
    storm::solver::NativeLinearEquationSolverMethod method;
    bool methodSetFromDefault;
    uint64_t maxIterationCount;
    storm::RationalNumber precision;
    bool considerRelativeTerminationCriterion;
    storm::solver::MultiplicationStyle powerMethodMultiplicationStyle;
    storm::RationalNumber sorOmega;
    bool symmetricUpdates;
};
}  // namespace storm
