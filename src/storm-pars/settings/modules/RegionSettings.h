#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for parametric model checking.
 */
class RegionSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of parametric model checking settings.
     */
    RegionSettings();

    /*!
     * Retrieves whether region(s) were declared
     */
    bool isRegionSet() const;

    /*!
     * Retrieves the region definition string
     */
    std::string getRegionString() const;

    /*!
     * Retrieves whether region bound is declared
     */
    bool isRegionBoundSet() const;

    /*!
     * Retrieves the region definition string
     */
    std::string getRegionBoundString() const;

    /*!
     * Retrieves whether non-graph-preserving mode is enabled
     */
    bool isAssumeGraphPreservingSet() const;

    /*!
     * Retrieves the discrete variables string
     */
    std::string getDiscreteVariablesString() const;

    const static std::string moduleName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
