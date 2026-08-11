#pragma once

#include <cstdint>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class GmmxxSolverEnvironment {
   public:
    GmmxxSolverEnvironment();
    ~GmmxxSolverEnvironment();

    storm::solver::GmmxxLinearEquationSolverMethod const& getMethod() const;
    void setMethod(storm::solver::GmmxxLinearEquationSolverMethod value);
    storm::solver::GmmxxLinearEquationSolverPreconditioner const& getPreconditioner() const;
    void setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner value);
    uint64_t const& getRestartThreshold() const;
    void setRestartThreshold(uint64_t value);
    uint64_t const& getMaximalNumberOfIterations() const;
    void setMaximalNumberOfIterations(uint64_t value);
    storm::RationalNumber const& getPrecision() const;
    void setPrecision(storm::RationalNumber value);

   private:
    storm::solver::GmmxxLinearEquationSolverMethod method;
    storm::solver::GmmxxLinearEquationSolverPreconditioner preconditioner;
    uint64_t restartThreshold;
    uint64_t maxIterationCount;
    storm::RationalNumber precision;
};
}  // namespace storm
