#pragma once

#include "storm-config.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the general settings.
 */
class ModelCheckerSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of general settings.
     */
    ModelCheckerSettings();

    bool isFilterRewZeroSet() const;

    /*!
     * Retrieves whether the external ltl2da tool has been set.
     *
     * @return True iff the external ltl2da has been set.
     */
    bool isLtl2daToolSet() const;

    /*!
     * Retrieves the external ltl2da tool that is used for converting LTL formulas to deterministic automata.
     *
     * @return The executable to use for converting LTL formulas to deterministic automata.
     */
    std::string getLtl2daTool() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string filterRewZeroOptionName;
    static const std::string ltl2daToolOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
