#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for glpk.
 */
class GlpkSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of glpk settings.
     */
    GlpkSettings();

    /*!
     * Retrieves whether the output option was set.
     *
     * @return True iff the output option was set.
     */
    bool isOutputSet() const;

    /*!
     * Retrieves whether the MILP Presolver should be used.
     */
    bool isMILPPresolverEnabled() const;

    /*!
     * Retrieves whether the integer tolerance has been set.
     *
     * @return True iff the integer tolerance has been set.
     */
    bool isIntegerToleranceSet() const;

    /*!
     * Retrieves the integer tolerance to be used.
     *
     * @return The integer tolerance to be used.
     */
    double getIntegerTolerance() const;

    bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string integerToleranceOption;
    static const std::string outputOptionName;
    static const std::string noMilpPresolverOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
