#ifndef STORM_SETTINGS_MODULES_CVARSETTINGS_H_
#define STORM_SETTINGS_MODULES_CVARSETTINGS_H_

#include "storm/modelchecker/cvar/CvarMethod.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for CVaR model checking.
 */
class CvarSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of CVaR model checking settings.
     */
    CvarSettings();

    /*!
     * Retrieves the selected CVaR method.
     *
     * @return The selected CVaR method.
     */
    storm::modelchecker::cvar::CvarMethod getCvarMethod() const;

    static std::string const moduleName;

   private:
    static std::string const methodOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm

#endif /* STORM_SETTINGS_MODULES_CVARSETTINGS_H_ */
