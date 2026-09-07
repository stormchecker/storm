#include "Option.h"

#include <algorithm>
#include <iomanip>
#include <string>

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/OptionUnificationException.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBase.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {

Option::Option(std::string const& moduleName, std::string const& longOptionName, std::string const& optionDescription, bool isOptionRequired,
               bool requireModulePrefix, bool isAdvanced, std::vector<std::shared_ptr<ArgumentBase>> const& optionArguments)
    : Option(moduleName, longOptionName, "", false, optionDescription, isOptionRequired, requireModulePrefix, isAdvanced, optionArguments) {
    // Intentionally left empty.
}

Option::Option(std::string const& moduleName, std::string const& longOptionName, std::string const& shortOptionName, std::string const& optionDescription,
               bool isOptionRequired, bool requireModulePrefix, bool isAdvanced, std::vector<std::shared_ptr<ArgumentBase>> const& optionArguments)
    : Option(moduleName, longOptionName, shortOptionName, true, optionDescription, isOptionRequired, requireModulePrefix, isAdvanced, optionArguments) {
    // Intentionally left empty.
}

bool Option::isCompatibleWith(Option const& other) {
    STORM_LOG_THROW(this->getArgumentCount() == other.getArgumentCount(), storm::exceptions::OptionUnificationException,
                    "Unable to unify two options, because their argument count differs.");

    for (size_t i = 0; i != this->arguments.size(); i++) {
        ArgumentBase const& firstArgument = this->getArgument(i);
        ArgumentBase const& secondArgument = other.getArgument(i);

        STORM_LOG_THROW(firstArgument.getType() == secondArgument.getType(), storm::exceptions::OptionUnificationException,
                        "Unable to unify two options, because their arguments are incompatible.");

        switch (firstArgument.getType()) {
            case ArgumentType::String:
                static_cast<storm::settings::Argument<std::string> const&>(firstArgument)
                    .isCompatibleWith(static_cast<storm::settings::Argument<std::string> const&>(secondArgument));
                break;
            case ArgumentType::Integer:
                static_cast<storm::settings::Argument<int_fast64_t> const&>(firstArgument)
                    .isCompatibleWith(static_cast<storm::settings::Argument<int_fast64_t> const&>(secondArgument));
                break;
            case ArgumentType::UnsignedInteger:
                static_cast<storm::settings::Argument<uint_fast64_t> const&>(firstArgument)
                    .isCompatibleWith(static_cast<storm::settings::Argument<uint_fast64_t> const&>(secondArgument));
                break;
            case ArgumentType::Double:
                static_cast<storm::settings::Argument<double> const&>(firstArgument)
                    .isCompatibleWith(static_cast<storm::settings::Argument<double> const&>(secondArgument));
                break;
            case ArgumentType::Boolean:
                static_cast<storm::settings::Argument<bool> const&>(firstArgument)
                    .isCompatibleWith(static_cast<storm::settings::Argument<bool> const&>(secondArgument));
                break;
        }
    }
    return true;
}

uint_fast64_t Option::getArgumentCount() const {
    return this->arguments.size();
}

ArgumentBase const& Option::getArgument(uint_fast64_t argumentIndex) const {
    STORM_LOG_THROW(argumentIndex < this->getArgumentCount(), storm::exceptions::IllegalArgumentException, "Index of argument is out of bounds.");
    return *this->arguments.at(argumentIndex);
}

ArgumentBase& Option::getArgument(uint_fast64_t argumentIndex) {
    STORM_LOG_THROW(argumentIndex < this->getArgumentCount(), storm::exceptions::IllegalArgumentException, "Index of argument is out of bounds.");
    return *this->arguments.at(argumentIndex);
}

ArgumentBase const& Option::getArgumentByName(std::string const& argumentName) const {
    auto argumentIterator = this->argumentNameMap.find(argumentName);
    STORM_LOG_THROW(argumentIterator != this->argumentNameMap.end(), storm::exceptions::IllegalArgumentException,
                    "Unable to retrieve argument with unknown name '" << argumentName << "'.");
    return *argumentIterator->second;
}

ArgumentBase& Option::getArgumentByName(std::string const& argumentName) {
    auto argumentIterator = this->argumentNameMap.find(argumentName);
    STORM_LOG_THROW(argumentIterator != this->argumentNameMap.end(), storm::exceptions::IllegalArgumentException,
                    "Unable to retrieve argument with unknown name '" << argumentName << "'.");
    return *argumentIterator->second;
}

std::string const& Option::getLongName() const {
    return this->longName;
}

bool Option::getHasShortName() const {
    return this->hasShortName;
}

std::string const& Option::getShortName() const {
    return this->shortName;
}

std::string const& Option::getDescription() const {
    return this->description;
}

std::string const& Option::getModuleName() const {
    return this->moduleName;
}

bool Option::getIsRequired() const {
    return this->isRequired;
}

bool Option::getRequiresModulePrefix() const {
    return this->requireModulePrefix;
}

bool Option::getIsAdvanced() const {
    return this->isAdvanced;
}

bool Option::getHasOptionBeenSet() const {
    return this->hasBeenSet;
}

bool Option::getHasOptionBeenSetWithModulePrefix() const {
    return this->hasBeenSetWithModulePrefix;
}

Option::Option(std::string const& moduleName, std::string const& longOptionName, std::string const& shortOptionName, bool hasShortOptionName,
               std::string const& optionDescription, bool isOptionRequired, bool requireModulePrefix, bool isAdvanced,
               std::vector<std::shared_ptr<ArgumentBase>> const& optionArguments)
    : longName(longOptionName),
      hasShortName(hasShortOptionName),
      shortName(shortOptionName),
      description(optionDescription),
      moduleName(moduleName),
      isRequired(isOptionRequired),
      requireModulePrefix(requireModulePrefix),
      isAdvanced(isAdvanced),
      hasBeenSet(false),
      hasBeenSetWithModulePrefix(false),
      arguments(optionArguments),
      argumentNameMap() {
    // First, do some sanity checks.
    STORM_LOG_THROW(!longName.empty(), storm::exceptions::IllegalArgumentException, "Unable to construct option with empty name.");
    STORM_LOG_THROW(!moduleName.empty(), storm::exceptions::IllegalArgumentException, "Unable to construct option with empty module name.");

    bool longNameContainsIllegalCharacter =
        std::find_if(longName.begin(), longName.end(), [](char c) { return !(std::isalpha(c) || std::isdigit(c) || c == '-' || c == '_'); }) != longName.end();
    STORM_LOG_THROW(!longNameContainsIllegalCharacter, storm::exceptions::IllegalArgumentException,
                    "Unable to construct option with illegal long name '" << longName << "'.");

    bool shortNameContainsIllegalCharacter = std::find_if(shortName.begin(), shortName.end(), [](char c) {
                                                 return !(std::isalpha(c) || std::isdigit(c) || c == 'c' || c == '_');
                                             }) != shortName.end();
    STORM_LOG_THROW(!shortNameContainsIllegalCharacter, storm::exceptions::IllegalArgumentException,
                    "Unable to construct option with illegal short name '" << shortName << "'.");

    // Then index all arguments.
    for (auto const& argument : arguments) {
        argumentNameMap.emplace(argument->getName(), argument);
    }
}

void Option::setHasOptionBeenSet(bool newValue) {
    this->hasBeenSet = newValue;
}

void Option::setHasOptionBeenSetWithModulePrefix(bool newValue) {
    this->hasBeenSetWithModulePrefix = newValue;
}

uint_fast64_t Option::getPrintLength() const {
    uint_fast64_t length = 2;
    if (!this->getRequiresModulePrefix()) {
        length += 2;
    }
    length += this->getModuleName().length() + 1;
    length += this->getLongName().length();
    if (this->getHasShortName()) {
        length += this->getShortName().length() + 3;
    }

    if (this->getArgumentCount() > 0) {
        for (auto const& argument : this->getArguments()) {
            length += argument->getName().size() + 3;
        }
    }
    return length;
}

std::vector<std::shared_ptr<ArgumentBase>> const& Option::getArguments() const {
    return this->arguments;
}

std::ostream& operator<<(std::ostream& out, Option const& option) {
    uint_fast64_t width = static_cast<uint_fast64_t>(out.width());

    uint_fast64_t charactersPrinted = 0;
    out << std::setw(0) << "--";
    charactersPrinted += 2;
    if (!option.getRequiresModulePrefix()) {
        out << "[";
        ++charactersPrinted;
    }
    out << option.getModuleName() << ":";
    charactersPrinted += option.getModuleName().length() + 1;
    if (!option.getRequiresModulePrefix()) {
        out << "]";
        ++charactersPrinted;
    }
    out << option.getLongName();
    charactersPrinted += option.getLongName().length();
    if (option.getHasShortName()) {
        out << " (" << option.getShortName() << ")";
        charactersPrinted += option.getShortName().length() + 3;
    }

    if (option.getArgumentCount() > 0) {
        for (auto const& argument : option.getArguments()) {
            out << " <" << argument->getName() << ">";
            charactersPrinted += argument->getName().size() + 3;
        }
    }

    // Now fill the width.
    for (uint_fast64_t i = charactersPrinted; i < width; ++i) {
        if (i == charactersPrinted) {
            out << " ";
        } else {
            out << ".";
        }
    }

    out << " " << option.getDescription();

    for (auto const& argument : option.getArguments()) {
        out << " " << *argument;
    }

    return out;
}
}  // namespace settings
}  // namespace storm
