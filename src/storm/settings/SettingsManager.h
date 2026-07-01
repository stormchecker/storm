#ifndef STORM_SETTINGS_SETTINGSMANAGER_H_
#define STORM_SETTINGS_SETTINGSMANAGER_H_

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace storm {
namespace settings {
namespace modules {
class BuildSettings;
class ModuleSettings;
class AbstractionSettings;
}  // namespace modules
class Option;

/*!
 * Provides the central API for the registration of command line options and parsing the options from the
 * command line. Since this class is a singleton, the only instance is accessible via a call to the manager()
 * function.
 */
class SettingsManager {
   public:
    // Explicitly delete copy constructor
    SettingsManager(SettingsManager const&) = delete;
    void operator=(SettingsManager const&) = delete;

    /*!
     * This function parses the given command line arguments and sets all registered options accordingly. If the
     * command line cannot be matched using the known options, an exception is thrown.
     *
     * @param argc The number of command line arguments.
     * @param argv The command line arguments.
     */
    void setFromCommandLine(int const argc, char const* const argv[]);

    /*!
     * This function parses the given command line arguments (represented by one big string) and sets all
     * registered options accordingly. If the command line cannot be matched using the known options, an
     * exception is thrown.
     *
     * @param commandLineString The command line arguments as one string.
     */
    void setFromString(std::string const& commandLineString);

    /*!
     * This function parses the given command line arguments (represented by several strings) and sets all
     * registered options accordingly. If the command line cannot be matched using the known options, an
     * exception is thrown.
     *
     * @param commandLineArguments The command line arguments.
     */
    void setFromExplodedString(std::vector<std::string> const& commandLineArguments);

    /*!
     * This function parses the given file and sets all registered options accordingly. If the settings cannot
     * be matched using the known options, an exception is thrown.
     */
    void setFromConfigurationFile(std::string const& configFilename);

    /*!
     * Throws an exception with a nice error message indicating similar valid option names.
     */
    void handleUnknownOption(std::string const& optionName, bool isShort) const;

    /*!
     * This function prints a help message to the standard output. A string can be given as a filter.
     * If it is 'frequent', only the options that are not flagged as advanced will be shown.
     * If it is 'all', the complete help is shown.
     * Otherwise, the string is interpreted as a regular expression
     * and matched against the known modules and options to restrict the help output.
     *
     * @param filter A regular expression to restrict the help output or "all" for the full help text.
     */
    void printHelp(std::string const& filter = "frequent") const;

    /*!
     * This function prints a help message for the specified module to the standard output.
     *
     * @param moduleName The name of the module for which to show the help.
     * @param maxLength The maximal length of an option name (necessary for proper alignment).
     * @param includeAdvanced if set, also includes options flagged as advanced.
     */
    std::string getHelpForModule(std::string const& moduleName, uint_fast64_t maxLength = 30, bool includeAdvanced = true) const;

    /*!
     * Retrieves the only existing instance of a settings manager.
     *
     * @return The only existing instance of a settings manager
     */
    static SettingsManager& manager();

    /*!
     * Sets the name of the tool.
     * @param name Name of the tool.
     * @param executableName Filename of the executable.
     */
    void setName(std::string const& name, std::string const& executableName);

    /*!
     * Adds a new module with the given name. If the module could not be successfully added, an exception is
     * thrown.
     *
     * @param moduleSettings The settings of the module to add.
     */
    void addModule(std::unique_ptr<modules::ModuleSettings>&& moduleSettings, bool doRegister = true);

    /*!
     * Checks whether the module with the given name exists.
     *
     * @param moduleName The name of the module to search.
     * @param checkHidden If true hidden modules are included in the search.
     * @return True iff the module exists.
     */
    bool hasModule(std::string const& moduleName, bool checkHidden = false) const;

    /*!
     * Retrieves the settings of the module with the given name.
     *
     * @param moduleName The name of the module for which to retrieve the settings.
     * @return An object that provides access to the settings of the module.
     */
    modules::ModuleSettings const& getModule(std::string const& moduleName) const;

    /*!
     * Retrieves the settings of the module with the given name.
     *
     * @param moduleName The name of the module for which to retrieve the settings.
     * @return An object that provides access to the settings of the module.
     */
    modules::ModuleSettings& getModule(std::string const& moduleName);

   private:
    /*!
     * Constructs a new manager. This constructor is private to forbid instantiation of this class. The only
     * way to create a new instance is by calling the static manager() method.
     */
    SettingsManager();

    /*!
     * This destructor is private, since we need to forbid explicit destruction of the manager.
     */
    virtual ~SettingsManager();

    /*!
     * This function prints a help message to the standard output.
     *
     * @param moduleFilter only modules where this function returns true are included
     * @param optionFilter only options where this function returns true are included
     * @return true if at least one module or option matched the filter.
     *
     */
    std::string getHelpForSelection(std::vector<std::string> const& selectedModuleNames, std::vector<std::string> const& selectedLongOptionNames,
                                    std::string modulesHeader = "", std::string optionsHeader = "") const;

    // The name of the tool
    std::string name;
    std::string executableName;

    // The registered modules.
    std::vector<std::string> moduleNames;
    std::unordered_map<std::string, std::unique_ptr<modules::ModuleSettings>> modules;

    // Mappings from all known option names to the options that match it. All options for one option name need
    // to be compatible in the sense that calling isCompatible(...) pairwise on all options must always return true.
    std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>> longNameToOptions;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>> shortNameToOptions;

    // A mapping of module names to the corresponding options.
    std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>> moduleOptions;

    // A list of long option names to keep the order in which they were registered. This is, for example, used
    // to match the regular expression given to the help option against the option names.
    std::vector<std::string> longOptionNames;

    /*!
     * Adds the given option to the known options.
     *
     * @param option The option to add.
     */
    void addOption(std::shared_ptr<Option> const& option);

    /*!
     * Sets the arguments of the given option from the provided strings.
     *
     * @param optionName The name of the option. This is only used for error output.
     * @param option The option for which to set the arguments.
     * @param argumentCache The arguments of the option as string values.
     */
    static void setOptionArguments(std::string const& optionName, std::shared_ptr<Option> option, std::vector<std::string> const& argumentCache);

    /*!
     * Sets the arguments of the options matching the given name from the provided strings.
     *
     * @param optionName The name of the options for which to set the arguments.
     * @param optionMap The mapping from option names to options.
     * @param argumentCache The arguments of the option as string values.
     */
    static void setOptionsArguments(std::string const& optionName, std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>> const& optionMap,
                                    std::vector<std::string> const& argumentCache);

    /*!
     * Checks whether the given option is compatible with all options with the given name in the given mapping.
     */
    static bool isCompatible(std::shared_ptr<Option> const& option, std::string const& optionName,
                             std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>> const& optionMap);

    /*!
     * Inserts the given option to the options with the given name in the given map.
     *
     * @param name The name of the option.
     * @param option The option to add.
     * @param optionMap The map into which the option is to be inserted.
     */
    static void addOptionToMap(std::string const& name, std::shared_ptr<Option> const& option,
                               std::unordered_map<std::string, std::vector<std::shared_ptr<Option>>>& optionMap);

    /*!
     * Prepares some modules for further changes.
     * Checks all modules for consistency by calling their respective check method.
     */
    void finalizeAllModules();

    /*!
     * Retrieves the (print) length of the longest option of all modules.
     *
     * @param includeAdvanced if set, also includes options flagged as advanced.
     * @return The length of the longest option.
     */
    uint_fast64_t getPrintLengthOfLongestOption(bool includeAdvanced) const;

    /*!
     * Retrieves the (print) length of the longest option in the given module, so we can align the options.
     *
     * @param moduleName The module name for which to retrieve the length of the longest option.
     * @param includeAdvanced if set, also includes options flagged as advanced.
     * @return The length of the longest option name.
     */
    uint_fast64_t getPrintLengthOfLongestOption(std::string const& moduleName, bool includeAdvanced) const;

    /*!
     * Parses the given file and stores the settings in the returned map.
     *
     * @param filename The name of the file that is to be scanned for settings.
     * @return A mapping of option names to the argument values (represented as strings).
     */
    std::map<std::string, std::vector<std::string>> parseConfigFile(std::string const& filename) const;
};

/*!
 * Retrieves the settings manager.
 *
 * @return The only settings manager.
 */
SettingsManager const& manager();

/*!
 * Retrieves the settings manager.
 *
 * @return The only settings manager.
 */
SettingsManager& mutableManager();

/*!
 * Add new module to use for the settings. The new module is given as a template argument.
 */
template<typename SettingsType>
void addModule(bool doRegister = true) {
    static_assert(std::is_base_of<storm::settings::modules::ModuleSettings, SettingsType>::value, "Template argument must be derived from ModuleSettings");
    mutableManager().addModule(std::unique_ptr<modules::ModuleSettings>(new SettingsType()), doRegister);
}

/*!
 * Initialize the settings manager with all available modules.
 * @param name Name of the tool.
 * @param executableName Filename of the executable.
 */
void initializeAll(std::string const& name, std::string const& executableName);

/*!
 * Get module. The type of the module is given as a template argument.
 *
 * @return The module.
 */
template<typename SettingsType>
SettingsType const& getModule() {
    static_assert(std::is_base_of<storm::settings::modules::ModuleSettings, SettingsType>::value, "Template argument must be derived from ModuleSettings");
    return dynamic_cast<SettingsType const&>(manager().getModule(SettingsType::moduleName));
}

/*!
 * Returns true if the given module is registered.
 *
 */
template<typename SettingsType>
bool hasModule() {
    static_assert(std::is_base_of<storm::settings::modules::ModuleSettings, SettingsType>::value, "Template argument must be derived from ModuleSettings");
    if (manager().hasModule(SettingsType::moduleName)) {
        return dynamic_cast<SettingsType const*>(&(manager().getModule(SettingsType::moduleName))) != nullptr;
    }
    return false;
}

/*!
 * Retrieves the build settings in a mutable form. This is only meant to be used for debug purposes or very
 * rare cases where it is necessary.
 *
 * @return An object that allows accessing and modifying the build settings.
 */
storm::settings::modules::BuildSettings& mutableBuildSettings();

/*!
 * Retrieves the abstraction settings in a mutable form. This is only meant to be used for debug purposes or very
 * rare cases where it is necessary.
 *
 * @return An object that allows accessing and modifying the abstraction settings.
 */
storm::settings::modules::AbstractionSettings& mutableAbstractionSettings();

}  // namespace settings
}  // namespace storm

#endif /* STORM_SETTINGS_SETTINGSMANAGER_H_ */
