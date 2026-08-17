#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace storm {
namespace settings {
// Forward-declare some classes.
class SettingsManager;
class Option;

namespace modules {

/*!
 * This is the base class of the settings for a particular module.
 */
class ModuleSettings {
   public:
    /*!
     * Constructs a new settings object.
     *
     * @param moduleName The name of the module for which to build the settings.
     */
    ModuleSettings(std::string const& moduleName);
    virtual ~ModuleSettings() {}

    /*!
     * Checks whether the settings are consistent. If they are inconsistent, an exception is thrown.
     *
     * @return True if the settings are consistent.
     */
    virtual bool check() const;

    /*!
     * Prepares the modules for further usage, should be called at the end of the initialization, before checks are executed.
     */
    virtual void finalize();

    /*!
     * Retrieves the name of the module to which these settings belong.
     *
     * @return The name of the module.
     */
    std::string const& getModuleName() const;

    /*!
     * Retrieves the options of this module.
     *
     * @return A list of options of this module.
     */
    std::vector<std::shared_ptr<Option>> const& getOptions() const;

    /*!
     * Retrieves the (print) length of the longest option.
     *
     * @param includeAdvanced if set, also includes options flagged as advanced.
     * @return The length of the longest option.
     */
    uint_fast64_t getPrintLengthOfLongestOption(bool includeAdvanced) const;

    /*!
     * Restores the default values for all arguments of all options.
     */
    void restoreDefaults();

   protected:
    /*!
     * Retrieves the option with the given long name. If no such option exists, an exception is thrown.
     *
     * @param longName The long name of the option to retrieve.
     * @return The option associated with the given option name.
     */
    Option& getOption(std::string const& longName);

    /*!
     * Retrieves the option with the given long name. If no such option exists, an exception is thrown.
     *
     * @param longName The long name of the option to retrieve.
     * @return The option associated with the given option name.
     */
    Option const& getOption(std::string const& longName) const;

    /*!
     * Retrieves whether the option with the given name was set.
     *
     * @param The name of the option.
     * @return True iff the option was set.
     */
    bool isSet(std::string const& optionName) const;

    /*!
     * Adds and registers the given option.
     *
     * @param option The option to add and register.
     */
    void addOption(std::shared_ptr<Option> const& option);

   private:
    // The name of the module.
    std::string moduleName;

    // A mapping of option names of the module to the actual options.
    std::unordered_map<std::string, std::shared_ptr<Option>> optionMap;

    // The list of known option names in the order they were registered.
    std::vector<std::shared_ptr<Option>> options;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
