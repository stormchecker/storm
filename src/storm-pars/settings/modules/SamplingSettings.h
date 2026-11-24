#pragma once
#include "storm/settings/modules/ModuleSettings.h"

namespace storm::settings::modules {

class SamplingSettings : public ModuleSettings {
   public:
    SamplingSettings();
    /*!
     * Retrieves the samples as a comma-separated list of samples for each (relevant) variable, where the
     * samples are colon-separated values. For example, 'N=1:2:3,K=2:4' means that N takes the values 1 to
     * 3 and K either 2 or 4.
     */
    std::string getSamples() const;

    /*!
     * Retrieves whether the samples are graph preserving.
     */
    bool isSamplesAreGraphPreservingSet() const;

    /*!
     * Retrieves whether samples are to be computed exactly.
     */
    bool isSampleExactSet() const;

    /*!
     * Retrieves whether samples are to be from the derivative.
     */
    bool isSampleDerivativeSet() const;

    static const std::string moduleName;
};

}  // namespace storm::settings::modules