#pragma once

#include <cstdint>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class EigenSolverEnvironment {
   public:
    EigenSolverEnvironment();
    ~EigenSolverEnvironment();

    storm::solver::EigenLinearEquationSolverMethod const& getMethod() const;
    void setMethod(storm::solver::EigenLinearEquationSolverMethod value);
    bool isMethodSetFromDefault() const;
    storm::solver::EigenLinearEquationSolverPreconditioner const& getPreconditioner() const;
    void setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner value);
    uint64_t const& getRestartThreshold() const;
    void setRestartThreshold(uint64_t value);
    uint64_t const& getMaximalNumberOfIterations() const;
    void setMaximalNumberOfIterations(uint64_t value);
    storm::RationalNumber const& getPrecision() const;
    void setPrecision(storm::RationalNumber value);

   private:
    storm::solver::EigenLinearEquationSolverMethod method;
    bool methodSetFromDefault;
    storm::solver::EigenLinearEquationSolverPreconditioner preconditioner;
    uint64_t restartThreshold;
    uint64_t maxIterationCount;
    storm::RationalNumber precision;
};
}  // namespace storm
