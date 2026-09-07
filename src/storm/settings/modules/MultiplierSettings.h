#pragma once

#include "storm-config.h"
#include "storm/settings/modules/ModuleSettings.h"
#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the multiplier settings.
 */
class MultiplierSettings : public ModuleSettings {
   public:
    MultiplierSettings();

    storm::solver::MultiplierType getMultiplierType() const;

    bool isMultiplierTypeSetFromDefaultValue() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    static const std::string multiplierTypeOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
