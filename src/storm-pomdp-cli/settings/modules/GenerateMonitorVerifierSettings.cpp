#include "storm-pomdp-cli/settings/modules/GenerateMonitorVerifierSettings.h"

namespace storm {
namespace settings {
namespace modules {

const std::string GenerateMonitorVerifierSettings::moduleName = "generateMonitorVerifier";

GenerateMonitorVerifierSettings::GenerateMonitorVerifierSettings() : ModuleSettings(moduleName) {}

}  // namespace modules
}  // namespace settings
}  // namespace storm
