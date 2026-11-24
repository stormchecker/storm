#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm::settings::modules {

/*!
 * This class represents the settings for the SCP feasibility synthesis method.
 */
class ScpSettings : public ModuleSettings {
   public:
    ScpSettings();

    /*!
     * Retrieves the initial trust region radius (delta_0).
     */
    double getTrustRegionInitial() const;

    /*!
     * Retrieves the trust region growth/shrink factor (gamma).
     */
    double getTrustRegionFactor() const;

    /*!
     * Retrieves the upper cap on the trust region radius.
     */
    double getTrustRegionMax() const;

    /*!
     * Retrieves the termination threshold on (radius - 1).
     */
    double getTrustRegionMinExcess() const;

    /*!
     * Retrieves the maximum number of SCP iterations.
     */
    uint64_t getMaxIterations() const;

    /*!
     * Retrieves the weight of the initial state's value in the LP objective.
     */
    double getInitialStateBias() const;

    /*!
     * Retrieves whether candidate valuations are model-checked with exact (rational) arithmetic
     * instead of floating-point arithmetic.
     */
    bool isExactSet() const;

    /*!
     * Retrieves whether the LP objective should minimize the sum of per-state violations instead
     * of the default maximum violation.
     */
    bool isSumViolationObjectiveSet() const;

    const static std::string moduleName;
};

}  // namespace storm::settings::modules
