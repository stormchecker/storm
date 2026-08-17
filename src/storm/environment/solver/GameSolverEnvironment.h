#pragma once

#include <cstdint>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class GameSolverEnvironment {
   public:
    GameSolverEnvironment();
    ~GameSolverEnvironment();

    storm::solver::GameMethod const& getMethod() const;
    bool const& isMethodSetFromDefault() const;
    void setMethod(storm::solver::GameMethod value);
    uint64_t const& getMaximalNumberOfIterations() const;
    void setMaximalNumberOfIterations(uint64_t value);
    storm::RationalNumber const& getPrecision() const;
    void setPrecision(storm::RationalNumber value);
    bool const& getRelativeTerminationCriterion() const;
    void setRelativeTerminationCriterion(bool value);

   private:
    storm::solver::GameMethod gameMethod;
    bool methodSetFromDefault;
    uint64_t maxIterationCount;
    storm::RationalNumber precision;
    bool considerRelativeTerminationCriterion;
};
}  // namespace storm
