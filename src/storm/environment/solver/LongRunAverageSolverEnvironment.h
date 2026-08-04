#pragma once

#include <cstdint>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class LongRunAverageSolverEnvironment {
   public:
    LongRunAverageSolverEnvironment();
    ~LongRunAverageSolverEnvironment();

    storm::solver::LraMethod const& getDetLraMethod() const;
    bool const& isDetLraMethodSetFromDefault() const;
    void setDetLraMethod(storm::solver::LraMethod value, bool isSetFromDefault = false);

    storm::solver::LraMethod const& getNondetLraMethod() const;
    bool const& isNondetLraMethodSetFromDefault() const;
    void setNondetLraMethod(storm::solver::LraMethod value, bool isSetFromDefault = false);

    storm::RationalNumber const& getPrecision() const;
    void setPrecision(storm::RationalNumber value);
    bool const& getRelativeTerminationCriterion() const;
    void setRelativeTerminationCriterion(bool value);

    bool isMaximalIterationCountSet() const;
    uint64_t getMaximalIterationCount() const;
    void setMaximalIterationCount(uint64_t value);
    void unsetMaximalIterationCount();

    storm::RationalNumber const& getAperiodicFactor() const;
    void setAperiodicFactor(storm::RationalNumber value);

   private:
    storm::solver::LraMethod detMethod;
    bool detMethodSetFromDefault;

    storm::solver::LraMethod nondetMethod;
    bool nondetMethodSetFromDefault;

    storm::RationalNumber precision;
    bool relative;
    boost::optional<uint64_t> maxIters;

    storm::RationalNumber aperiodicFactor;
};
}  // namespace storm
