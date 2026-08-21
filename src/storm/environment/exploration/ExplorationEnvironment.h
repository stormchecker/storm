#pragma once

#include <boost/optional.hpp>
#include <cstdint>

#include "storm/modelchecker/exploration/ExplorationTypes.h"

namespace storm {

class ExplorationEnvironment {
   public:
    ExplorationEnvironment();
    ~ExplorationEnvironment();
    ExplorationEnvironment(ExplorationEnvironment const& other);
    ExplorationEnvironment& operator=(ExplorationEnvironment const& other);

    storm::modelchecker::exploration_detail::PrecomputationType getPrecomputationType() const;
    void setPrecomputationType(storm::modelchecker::exploration_detail::PrecomputationType value);

    uint64_t getStepsUntilPrecomputation() const;
    void setStepsUntilPrecomputation(uint64_t value);

    boost::optional<uint64_t> const& getSampledPathsUntilPrecomputation() const;
    void setSampledPathsUntilPrecomputation(uint64_t value);
    void unsetSampledPathsUntilPrecomputation();

    storm::modelchecker::exploration_detail::NextStateHeuristic getNextStateHeuristic() const;
    void setNextStateHeuristic(storm::modelchecker::exploration_detail::NextStateHeuristic value);

    double getPrecision() const;
    void setPrecision(double value);

   private:
    storm::modelchecker::exploration_detail::PrecomputationType precomputationType;
    uint64_t stepsUntilPrecomputation;
    boost::optional<uint64_t> sampledPathsUntilPrecomputation;
    storm::modelchecker::exploration_detail::NextStateHeuristic nextStateHeuristic;
    double precision;
};
}  // namespace storm
