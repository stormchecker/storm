#pragma once

#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/stateelimination/EliminationMethod.h"
#include "storm/solver/stateelimination/EliminationOrder.h"

namespace storm {

class EliminationSolverEnvironment {
   public:
    EliminationSolverEnvironment();
    ~EliminationSolverEnvironment();

    storm::solver::stateelimination::EliminationOrder const& getOrder() const;
    void setOrder(storm::solver::stateelimination::EliminationOrder value);
    storm::solver::stateelimination::EliminationMethod const& getMethod() const;
    void setMethod(storm::solver::stateelimination::EliminationMethod value);
    uint64_t const& getMaximalSccSize() const;
    void setMaximalSccSize(uint64_t value);
    bool const& isEliminateEntryStatesLastSet() const;
    void setEliminateEntryStatesLast(bool value);

   private:
    storm::solver::stateelimination::EliminationOrder order;
    storm::solver::stateelimination::EliminationMethod method;
    uint64_t maximalSccSize;
    bool eliminateEntryStatesLast;
};
}  // namespace storm
