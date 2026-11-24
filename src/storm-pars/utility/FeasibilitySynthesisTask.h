#pragma once

#include <optional>

#include "storm-pars/storage/ParameterRegion.h"
#include "storm/logic/Bound.h"
#include "storm/logic/Formula.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm::pars {
enum class FeasibilityMethod { PLA, SCP, GD };

class FeasibilitySynthesisTask {
   public:
    /*!
     * @param formula
     */
    FeasibilitySynthesisTask(std::shared_ptr<storm::logic::Formula const> const& formula) : formula(formula) {}

    void setBound(storm::logic::Bound const& bound) {
        this->bound = bound;
    }

    /*!
     * Retrieves the bound (if set).
     */
    storm::logic::Bound const& getBound() const {
        return bound.value();
    }

    /*!
     * Retrieves whether there is a bound with which the values for the states will be compared.
     */
    bool isBoundSet() const {
        return static_cast<bool>(bound);
    }

    storm::logic::Formula const& getFormula() const {
        return *formula;
    }

    void setMaximalAllowedGap(storm::RationalNumber const& maxGap) {
        this->maxGap = maxGap;
    }
    /*!
     *
     */
    std::optional<storm::RationalNumber> getMaximalAllowedGap() const {
        return maxGap;
    }

    void setMaximalAllowedGapIsRelative(bool newValue) {
        gapIsRelative = newValue;
    }

    bool isMaxGapRelative() const {
        return gapIsRelative;
    }

    void setRegion(storm::storage::ParameterRegion<storm::RationalFunction> const& region) {
        this->region = region;
    }

    bool isRegionSet() const {
        return region != std::nullopt;
    }

    storm::storage::ParameterRegion<storm::RationalFunction> const& getRegion() const {
        return region.value();
    }

    void setOptimizationDirection(storm::solver::OptimizationDirection const& dir) {
        this->optimizationDirection = dir;
    }

    storm::solver::OptimizationDirection getOptimizationDirection() const {
        STORM_LOG_ASSERT(isBoundSet() || optimizationDirection != std::nullopt, "Bound or direction should be set.");
        STORM_LOG_ASSERT(!isBoundSet() || optimizationDirection == std::nullopt, "Bound or direction should not be both  set.");

        if (optimizationDirection) {
            return optimizationDirection.value();
        } else {
            return getBound().isLowerBound() ? storm::solver::OptimizationDirection::Maximize : storm::solver::OptimizationDirection::Minimize;
        }
    }

   private:
    /// The formula that is to be checked.
    std::shared_ptr<storm::logic::Formula const> formula;
    /// The bound with which the states will be compared.
    std::optional<storm::logic::Bound> bound = std::nullopt;
    /// Region in which one should search.
    std::optional<storm::storage::ParameterRegion<storm::RationalFunction>> region = std::nullopt;
    /// The maximum allowable gap between an acceptable solution and the optimal solution,
    /// None if any solution is fine. Should also be None if a bound is explicitly given.
    std::optional<storm::RationalNumber> maxGap = std::nullopt;
    /// Should the gap be interpreted relatively or absolutely. Meaningless if gap is none.
    bool gapIsRelative = true;
    /// Optimizationdirection; if a bound is set then this should be null
    std::optional<storm::solver::OptimizationDirection> optimizationDirection = std::nullopt;
};

}  // namespace storm::pars