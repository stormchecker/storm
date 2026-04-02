#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

class GenerateMonitorVerifierSettings : public ModuleSettings {
   public:
    GenerateMonitorVerifierSettings();

    virtual ~GenerateMonitorVerifierSettings() = default;

    // The name of the module.
    static const std::string moduleName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
