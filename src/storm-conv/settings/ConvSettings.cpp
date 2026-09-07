#include "ConvSettings.h"

#include "storm-conv/settings/modules/ConversionGeneralSettings.h"
#include "storm-conv/settings/modules/ConversionInputSettings.h"
#include "storm-conv/settings/modules/ConversionOutputSettings.h"
#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-conv/settings/modules/PrismExportSettings.h"
#include "storm/settings/SettingsManager.h"

namespace storm {
namespace settings {
void initializeConvSettings(std::string const& name, std::string const& executableName) {
    storm::settings::mutableManager().setName(name, executableName);

    // Register relevant settings modules.
    storm::settings::addModule<storm::settings::modules::ConversionGeneralSettings>();
    storm::settings::addModule<storm::settings::modules::ConversionInputSettings>();
    storm::settings::addModule<storm::settings::modules::ConversionOutputSettings>();
    storm::settings::addModule<storm::settings::modules::JaniExportSettings>();
    storm::settings::addModule<storm::settings::modules::PrismExportSettings>();
}

}  // namespace settings
}  // namespace storm
