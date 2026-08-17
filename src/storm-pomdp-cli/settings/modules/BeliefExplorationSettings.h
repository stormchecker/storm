#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm::settings::modules {

/*!
 * This class represents the settings for POMDP model checking.
 */
class BeliefExplorationSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of POMDP settings.
     */
    BeliefExplorationSettings();

    virtual ~BeliefExplorationSettings() = default;

    bool isCutZeroGapSet() const;

    uint64_t getExplorationTimeLimit() const;

    /// Discretization Resolution
    uint64_t getResolutionInit() const;

    /// Clipping Grid Resolution
    uint64_t getClippingGridResolution() const;

    /// The maximal number of newly expanded MDP states in a refinement step
    uint64_t getSizeThresholdInit() const;

    bool isDynamicTriangulationModeSet() const;
    bool isStaticTriangulationModeSet() const;

    /// Controls if grid clipping is to be used
    bool isUseClippingSet() const;

    bool isBeliefMDPNumberTypeDouble() const;
    bool isBeliefMDPNumberTypeRational() const;
    bool isBeliefMDPNumberTypeMatch() const;

    bool isInexactPreprocessingSet() const;

    // The name of the module.
    static const std::string moduleName;

   private:
};

}  // namespace storm::settings::modules
