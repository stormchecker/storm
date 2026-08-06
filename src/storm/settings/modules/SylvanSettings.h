#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for Sylvan.
 */
class SylvanSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of Sylvan settings.
     */
    SylvanSettings();

    /*!
     * Retrieves the maximal amount of memory (in megabytes) that Sylvan can occupy.
     *
     * @return The maximal amount of memory to use.
     */
    uint64_t getMaximalMemory() const;

    /*!
     * Retrieves the amount of threads available to Sylvan. Note that a value of zero means that the number
     * of threads is auto-detected to fit the current machine.
     *
     * @rreturn The number of threads.
     */
    uint64_t getNumberOfThreads() const;

    /*!
     * Retrieves whether the number of threads to use was set.
     */
    bool isNumberOfThreadsSet() const;

    bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string maximalMemoryOptionName;
    static const std::string threadCountOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
