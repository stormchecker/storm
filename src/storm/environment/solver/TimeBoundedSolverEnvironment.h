#pragma once

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class TimeBoundedSolverEnvironment {
   public:
    TimeBoundedSolverEnvironment();
    ~TimeBoundedSolverEnvironment();

    storm::solver::MaBoundedReachabilityMethod const& getMaMethod() const;
    bool const& isMaMethodSetFromDefault() const;
    void setMaMethod(storm::solver::MaBoundedReachabilityMethod value, bool isSetFromDefault = false);

    storm::RationalNumber const& getPrecision() const;
    void setPrecision(storm::RationalNumber value);
    bool const& getRelativeTerminationCriterion() const;
    void setRelativeTerminationCriterion(bool value);

    storm::RationalNumber const& getUnifPlusKappa() const;
    void setUnifPlusKappa(storm::RationalNumber value);

   private:
    storm::solver::MaBoundedReachabilityMethod maMethod;
    bool maMethodSetFromDefault;

    storm::RationalNumber precision;
    bool relative;

    storm::RationalNumber unifPlusKappa;
};
}  // namespace storm
