#pragma once

#include <boost/algorithm/string.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "storm/settings/ArgumentBase.h"
#include "storm/settings/ArgumentType.h"
#include "storm/settings/Option.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {

/*!
 * This class provides the interface to create an option...
 */
class OptionBuilder {
   public:
    /*!
     * Creates a new option builder for an option with the given module, name and description.
     *
     * @param moduleName The name of the module to which this option belongs.
     * @param longName The long name of the option.
     * @param requireModulePrefix Sets whether this option can only be set by specifying the module name as its prefix.
     * @param description A description that explains the purpose of this option.
     */
    OptionBuilder(std::string const& moduleName, std::string const& longName, bool requireModulePrefix, std::string const& description)
        : longName(longName),
          shortName(""),
          hasShortName(false),
          description(description),
          moduleName(moduleName),
          requireModulePrefix(requireModulePrefix),
          isRequired(false),
          isAdvanced(false),
          isBuild(false),
          arguments(),
          argumentNameSet() {
        // Intentionally left empty.
    }

    /*!
     * Sets a short name for the option.
     *
     * @param shortName A short name for the option.
     * @return A reference to the current builder.
     */
    OptionBuilder& setShortName(std::string const& shortName) {
        this->shortName = shortName;
        this->hasShortName = true;
        return *this;
    }

    /*!
     * Sets whether the option is required.
     *
     * @param isRequired A flag indicating whether the option is required.
     * @return A reference to the current builder.
     */
    OptionBuilder& setIsRequired(bool isRequired) {
        this->isRequired = isRequired;
        return *this;
    }

    /*!
     * Sets whether the option is only displayed in the advanced help.
     */
    OptionBuilder& setIsAdvanced(bool isAdvanced = true) {
        this->isAdvanced = isAdvanced;
        return *this;
    }

    /*!
     * Adds the given argument to the arguments of this option.
     *
     * @param argument The argument to be added.
     * @return A reference to the current builder.
     */
    OptionBuilder& addArgument(std::shared_ptr<ArgumentBase> argument) {
        STORM_LOG_THROW(!this->isBuild, storm::exceptions::IllegalFunctionCallException,
                        "Cannot add an argument to an option builder that was already used to build the option.");
        STORM_LOG_THROW(this->arguments.empty() || argument->getIsOptional() || !this->arguments.back()->getIsOptional(),
                        storm::exceptions::IllegalArgumentException, "Unable to add non-optional argument after an option that is optional.");

        std::string lowerArgumentName = boost::algorithm::to_lower_copy(argument->getName());
        STORM_LOG_THROW(argumentNameSet.find(lowerArgumentName) == argumentNameSet.end(), storm::exceptions::IllegalArgumentException,
                        "Unable to add argument to option, because it already has an argument with the same name.");

        argumentNameSet.insert(lowerArgumentName);
        this->arguments.push_back(argument);

        return *this;
    }

    /*!
     * Builds an option from the data that was added to this builder.
     *
     * @return The resulting option.
     */
    std::shared_ptr<Option> build() {
        STORM_LOG_THROW(!this->isBuild, storm::exceptions::IllegalFunctionCallException, "Cannot rebuild an option with one builder.");
        this->isBuild = true;

        if (this->hasShortName) {
            return std::shared_ptr<Option>(new Option(this->moduleName, this->longName, this->shortName, this->description, this->isRequired,
                                                      this->requireModulePrefix, this->isAdvanced, this->arguments));
        } else {
            return std::shared_ptr<Option>(new Option(this->moduleName, this->longName, this->description, this->isRequired, this->requireModulePrefix,
                                                      this->isAdvanced, this->arguments));
        }
    }

   private:
    // The long name of the option.
    std::string longName;

    // A possible short name of the option or the empty string in case the option does not have a short name.
    std::string shortName;

    // A flag indicating whether the option has a short name.
    bool hasShortName;

    // The description of the option.
    std::string description;

    // The name of the module to which this option belongs.
    std::string moduleName;

    // A flag indicating whether the option has to be prefixed with the module name.
    bool requireModulePrefix;

    // A flag indicating whether the option is required.
    bool isRequired;

    // A flag that indicates whether this option is only displayed in the advanced help.
    bool isAdvanced;

    // A flag indicating whether the builder has already been used to build an option.
    bool isBuild;

    // The arguments of the option that is being built.
    std::vector<std::shared_ptr<ArgumentBase>> arguments;

    // The names of the arguments of the option.
    std::unordered_set<std::string> argumentNameSet;
};
}  // namespace settings
}  // namespace storm
