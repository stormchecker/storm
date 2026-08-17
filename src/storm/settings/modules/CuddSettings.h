#pragma once

#include "storm/settings/modules/ModuleSettings.h"
#include "storm/storage/dd/cudd/CuddReorderingTechnique.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for CUDD.
 */
class CuddSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of CUDD settings.
     */
    CuddSettings();

    /*!
     * Retrieves the precision that CUDD is supposed to use for distinguishing constants.
     *
     * @return The desired precision of CUDD.
     */
    double getConstantPrecision() const;

    /*!
     * Retrieves the maximal amount of memory (in megabytes) that CUDD can occupy.
     *
     * @return The maximal amount of memory to use.
     */
    uint_fast64_t getMaximalMemory() const;

    /*!
     * Retrieves whether dynamic reordering is enabled.
     *
     * @return True iff dynamic reordering is enabled.
     */
    bool isReorderingEnabled() const;

    /*!
     * Retrieves the reordering technique that CUDD is supposed to use.
     *
     * @return The reordering technique to use.
     */
    storm::dd::CuddReorderingTechnique getReorderingTechnique() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string precisionOptionName;
    static const std::string maximalMemoryOptionName;
    static const std::string reorderOptionName;
    static const std::string reorderTechniqueOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
