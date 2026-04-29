#pragma once

#include "storm-config.h"
#include "storm/modelchecker/helper/conditional/ConditionalAlgorithmSetting.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the LRA solver settings.
 */
class ConditionalSettings : public ModuleSettings {
   public:
    ConditionalSettings();

    /*!
     * Retrieves whether an algorithm for conditional properties has been set.
     */
    bool isConditionalAlgorithmSet() const;

    /*!
     * Retrieves the specified algorithm for conditional probabilities.
     */
    ConditionalAlgorithmSetting getConditionalAlgorithmSetting() const;

    /*!
     * Retrieves the specified precision for computing conditional probabilities.
     */
    double getConditionalPrecision() const;

    /*!
     * Retrieves whether the precision for computing conditional probabilities was set from a default value.
     */
    bool isConditionalPrecisionSetFromDefaultValue() const;

    /*!
     * Retrieves whether the precision for computing conditional probabilities is considered absolute.
     */
    bool isConditionalPrecisionAbsolute() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    static const std::string conditionalAlgorithmOptionName;
    static const std::string conditionalPrecisionOptionName;
    static const std::string conditionalPrecisionAbsoluteOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
