#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <unordered_map>

#include "ArgumentBase.h"

namespace storm {
namespace settings {

// Forward-declare settings manager and module settings classes.
class SettingsManager;
namespace modules {
class ModuleSettings;
}
class ArgumentBase;

/*!
 * This class represents one command-line option.
 */
class Option {
   public:
    // Declare settings manager and module settings classes as friends.
    friend class SettingsManager;
    friend class modules::ModuleSettings;

    /*!
     * Creates an option with the given parameters.
     *
     * @param moduleName The module to which this option belongs.
     * @param longOptionName The long option name.
     * @param optionDescription The description of the option.
     * @param isOptionRequired Sets whether the option is required to appear.
     * @param requireModulePrefix A flag that indicates whether this option requires to be prefixed with the
     * module name.
     * @param isAdvanced A flag that indicates whether this option is only displayed in the advanced help
     * @param optionArguments The arguments of the option.
     */
    Option(std::string const& moduleName, std::string const& longOptionName, std::string const& optionDescription, bool isOptionRequired,
           bool requireModulePrefix, bool isAdvanced,
           std::vector<std::shared_ptr<ArgumentBase>> const& optionArguments = std::vector<std::shared_ptr<ArgumentBase>>());

    /*!
     * Creates an option with the given parameters.
     *
     * @param moduleName The module to which this option belongs.
     * @param longOptionName The long option name.
     * @param shortOptionName The short option name.
     * @param optionDescription The description of the option.
     * @param isOptionRequired Sets whether the option is required to appear.
     * @param requireModulePrefix A flag that indicates whether this option requires to be prefixed with the
     * @param isAdvanced A flag that indicates whether this option is only displayed in the advanced help
     * @param optionArguments The arguments of the option.
     */
    Option(std::string const& moduleName, std::string const& longOptionName, std::string const& shortOptionName, std::string const& optionDescription,
           bool isOptionRequired, bool requireModulePrefix, bool isAdvanced,
           std::vector<std::shared_ptr<ArgumentBase>> const& optionArguments = std::vector<std::shared_ptr<ArgumentBase>>());

    /*!
     * Checks whether the given option is compatible with the current one. If not, an exception is thrown.
     *
     * @param other The other option with which to check compatibility.
     * @return True iff the given argument is compatible with the current one.
     */
    bool isCompatibleWith(Option const& other);

    /*!
     * Retrieves the argument count this option expects.
     *
     * @return The argument count of this option.
     */
    uint_fast64_t getArgumentCount() const;

    /*!
     * Retrieves the i-th argument of this option.
     *
     * @param argumentIndex The index of the argument to retrieve.
     * @return The i-th argument of this option.
     */
    ArgumentBase const& getArgument(uint_fast64_t argumentIndex) const;

    /*!
     * Retrieves the i-th argument of this option.
     *
     * @param argumentIndex The index of the argument to retrieve.
     * @return The i-th argument of this option.
     */
    ArgumentBase& getArgument(uint_fast64_t argumentIndex);

    /*!
     * Returns a reference to the argument with the specified long name.
     *
     * @param argumentName The name of the argument to retrieve.
     * @return The argument with the given name.
     */
    ArgumentBase const& getArgumentByName(std::string const& argumentName) const;

    /*!
     * Returns a reference to the argument with the specified long name.
     *
     * @param argumentName The name of the argument to retrieve.
     * @return The argument with the given name.
     */
    ArgumentBase& getArgumentByName(std::string const& argumentName);

    /*!
     * Retrieves the long name of this option.
     *
     * @return The long name of this option.
     */
    std::string const& getLongName() const;

    /*!
     * Retrieves whether this option has a short name.
     *
     * @return True iff the option has a short name.
     */
    bool getHasShortName() const;

    /*!
     * Retrieves the short name of this option.
     *
     * @return The short name of this option.
     */
    std::string const& getShortName() const;

    /*!
     * Retrieves the description of the option.
     *
     * @return The description of the option.
     */
    std::string const& getDescription() const;

    /*!
     * Retrieves the name of the module to which this option belongs.
     *
     * @return The name of the module to which this option belongs.
     */
    std::string const& getModuleName() const;

    /*!
     * Retrieves whether the option is required.
     *
     * @return True iff the option is required.
     */
    bool getIsRequired() const;

    /*!
     * Retrieves whether the option requires the module name as a prefix.
     *
     * @return True iff the option requires the module name as a prefix.
     */
    bool getRequiresModulePrefix() const;

    /*!
     * Retrieves whether the option has been set.
     *
     * @return True iff the option has been set.
     */
    bool getHasOptionBeenSet() const;

    /*!
     * Retrieves whether the option has been set by including the module prefix.
     *
     * @return True iff the option has been set by including the module prefix.
     */
    bool getHasOptionBeenSetWithModulePrefix() const;

    /*!
     * Retrieves whether the option is only displayed in the advanced help.
     */
    bool getIsAdvanced() const;

    /*!
     * Retrieves the arguments of the option.
     *
     * @return The arguments of the option.
     */
    std::vector<std::shared_ptr<ArgumentBase>> const& getArguments() const;

    /*!
     * Retrieves the (print) length of the option.
     *
     * @return The length of the option.
     */
    uint_fast64_t getPrintLength() const;

    friend std::ostream& operator<<(std::ostream& out, Option const& option);

   private:
    // The long name of the option.
    std::string longName;

    // A flag that indicates whether the option has a short name.
    bool hasShortName;

    // The short name of the option if any is set and an empty string otherwise.
    std::string shortName;

    // The description of the option.
    std::string description;

    // The name of the module to which this option belongs.
    std::string moduleName;

    // A flag that indicates whether this option is required to appear.
    bool isRequired;

    // A flag that indicates whether this option is required to be prefixed with the module name.
    bool requireModulePrefix;

    // A flag that indicates whether this option is only displayed in the advanced help.
    bool isAdvanced;

    // A flag that indicates whether this option has been set.
    bool hasBeenSet;

    // A flag that indicates whether this option has been set.
    bool hasBeenSetWithModulePrefix;

    // The arguments of this option (possibly empty).
    std::vector<std::shared_ptr<ArgumentBase>> arguments;

    // A mapping from argument names of this option to the actual arguments.
    std::unordered_map<std::string, std::shared_ptr<ArgumentBase>> argumentNameMap;

    /*!
     * Creates an option with the given parameters.
     *
     * @param moduleName The module to which this option belongs.
     * @param longOptionName The long option name.
     * @param shortOptionName The short option name.
     * @param hasShortOptionName A flag that indicates whether this option has a short name.
     * @param optionDescription The description of the option.
     * @param isOptionRequired Sets whether the option is required to appear.
     * @param requireModulePrefix A flag that indicates whether this option requires to be prefixed with the
     * module name.
     * @param isAdvanced A flag that indicates whether this option is only displayed in the advanced help
     * @param optionArguments The arguments of the option.
     */
    Option(std::string const& moduleName, std::string const& longOptionName, std::string const& shortOptionName, bool hasShortOptionName,
           std::string const& optionDescription, bool isOptionRequired, bool requireModulePrefix, bool isAdvanced,
           std::vector<std::shared_ptr<ArgumentBase>> const& optionArguments = std::vector<std::shared_ptr<ArgumentBase>>());

    /*!
     * Sets the flag that marks the option as being (un)set.
     *
     * @param newValue The new status of the flag.
     */
    void setHasOptionBeenSet(bool newValue = true);

    /*!
     * Sets the flag that marks the option as being (un)set by including the module prefix.
     *
     * @param newValue The new status of the flag.
     */
    void setHasOptionBeenSetWithModulePrefix(bool newValue = true);
};
}  // namespace settings
}  // namespace storm
