#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the debug settings.
 */
class DebugSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of debug settings.
     */
    DebugSettings();

    /*!
     * Retrieves whether the debug option was set.
     *
     * @return True iff the debug option was set.
     */
    bool isDebugSet() const;

    /*!
     * Retrieves whether the trace option was set.
     *
     * @return True iff the trace option was set.
     */
    bool isTraceSet() const;

    /*!
     * Retrieves whether additional checks on the input should be performed.
     *
     * @return True iff additoinal checks on the input should be performed.
     */
    bool isAdditionalChecksSet() const;

    /*!
     * Retrieves whether the logfile option was set.
     *
     * @return True iff the logfile option was set.
     */
    bool isLogfileSet() const;

    /*!
     * Retrieves the name of the log file if the logfile option was set.
     *
     * @return The name of the log file.
     */
    std::string getLogfilename() const;

    /*!
     * Retrieves whether the test option was set. This is a general option which can be
     * used for quick testing purposes to activate/decactivate a certain setting.
     *
     * @return True iff the test option was set.
     */
    bool isTestSet() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string debugOptionName;
    static const std::string traceOptionName;
    static const std::string additionalChecksOptionName;
    static const std::string logfileOptionName;
    static const std::string logfileOptionShortName;
    static const std::string testOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
