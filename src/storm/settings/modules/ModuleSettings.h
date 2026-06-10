#ifndef STORM_SETTINGS_MODULES_MODULESETTINGS_H_
#define STORM_SETTINGS_MODULES_MODULESETTINGS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace storm {
namespace settings {
// Forward-declare some classes.
class SettingsManager;
class SettingMemento;
class Option;

namespace modules {

/*!
 * This is the base class of the settings for a particular module.
 */
class ModuleSettings {
   public:
    // Declare the memento class as a friend so it can manipulate the internal state.
    friend class storm::settings::SettingMemento;

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
     * Sets the option with the given name to the required status. This requires the option to take no
     * arguments. As a result, a pointer to an object is returned such that when the object is destroyed
     * (i.e. the smart pointer goes out of scope), the option is reset to its original status.
     *
     * @param name The name of the option to (unset).
     * @param requiredStatus The status that is to be set for the option.
     * @return A pointer to an object that resets the change upon destruction.
     */
    std::unique_ptr<storm::settings::SettingMemento> overrideOption(std::string const& name, bool requiredStatus);

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
     * Sets the option with the specified name. This requires the option to not have any arguments. This
     * should be used with care and is primarily meant to be used by the SettingMemento.
     *
     * @param name The name of the option to set.
     */
    void set(std::string const& name);

    /*!
     * Unsets the option with the specified name. This requires the option to not have any arguments. This
     * should be used with care and is primarily meant to be used by the SettingMemento.
     *
     * @param name The name of the option to unset.
     */
    void unset(std::string const& name);

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

#endif /* STORM_SETTINGS_MODULES_MODULESETTINGS_H_ */
