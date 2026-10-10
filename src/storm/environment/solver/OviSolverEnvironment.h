#pragma once

#include <optional>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"

namespace storm {

class OviSolverEnvironment {
   public:
    OviSolverEnvironment();
    ~OviSolverEnvironment() = default;

    std::optional<storm::RationalNumber> const& getUpperBoundGuessingFactor() const;
    void setUpperBoundGuessingFactor(storm::RationalNumber value);
    void unsetUpperBoundGuessingFactor();

   private:
    std::optional<storm::RationalNumber> upperBoundGuessingFactor;
};
}  // namespace storm
