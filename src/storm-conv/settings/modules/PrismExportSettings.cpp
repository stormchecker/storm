#include "PrismExportSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

#include <boost/algorithm/string.hpp>

namespace storm {
namespace settings {
namespace modules {
const std::string PrismExportSettings::moduleName = "exportPrism";

const std::string PrismExportSettings::exportFlattenOptionName = "flatten";
const std::string PrismExportSettings::exportSimplifyOptionName = "simplify";

PrismExportSettings::PrismExportSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, exportFlattenOptionName, false,
                                                   "Flattens the composition of modules to obtain an equivalent program that contains exactly one module")
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportSimplifyOptionName, false, "Applies static analysis to simplify the program.").build());
}

bool PrismExportSettings::isExportFlattenedSet() const {
    return this->getOption(exportFlattenOptionName).getHasOptionBeenSet();
}

bool PrismExportSettings::isSimplifySet() const {
    return this->getOption(exportSimplifyOptionName).getHasOptionBeenSet();
}

void PrismExportSettings::finalize() {}

bool PrismExportSettings::check() const {
    return true;
}
}  // namespace modules
}  // namespace settings
}  // namespace storm
