#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm::dft {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for operations concerning the DFT to GSPN transformation.
 */
class DftGspnSettings : public storm::settings::modules::ModuleSettings {
   public:
    /*!
     * Creates a new set of DFT-GSPN settings.
     */
    DftGspnSettings();

    /*!
     * Retrieves whether the DFT should be transformed into a GSPN.
     *
     * @return True iff the option was set.
     */
    bool isTransformToGspn() const;

    /*!
     * Retrieves whether the smart transformation should be disabled.
     *
     * @return True if the smart transformation should be disabled.
     */
    bool isDisableSmartTransformation() const;

    /*!
     * Retrieves whether the DC and failed place should be merged.
     *
     * @return True if the merge of DC and failed place is enabled.
     */
    bool isMergeDCFailed() const;

    /*!
     * Retrieves whether the experimental setting of priorities should be used
     *
     * @return True if the setting is enabled.
     */
    bool isExtendPriorities() const;

    bool check() const override;

    void finalize() override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string transformToGspnOptionName;
    static const std::string disableSmartTransformationOptionName;
    static const std::string mergeDCFailedOptionName;
    static const std::string extendPrioritiesOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm::dft
