#include "storm/settings/modules/ModuleSettings.h"

#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/settings/Option.h"
#include "storm/settings/SettingMemento.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

ModuleSettings::ModuleSettings(std::string const& moduleName) : moduleName(moduleName) {
    // Intentionally left empty.
}

bool ModuleSettings::check() const {
    return true;
}

void ModuleSettings::finalize() {}

void ModuleSettings::set(std::string const& name) {
    this->getOption(name).setHasOptionBeenSet();
}

void ModuleSettings::unset(std::string const& name) {
    this->getOption(name).setHasOptionBeenSet(false);
    this->getOption(name).setHasOptionBeenSetWithModulePrefix(false);
}

std::vector<std::shared_ptr<Option>> const& ModuleSettings::getOptions() const {
    return this->options;
}

Option const& ModuleSettings::getOption(std::string const& longName) const {
    auto optionIterator = this->optionMap.find(longName);
    STORM_LOG_THROW(optionIterator != this->optionMap.end(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot retrieve unknown option '" << longName << "'.");
    return *optionIterator->second;
}

Option& ModuleSettings::getOption(std::string const& longName) {
    auto optionIterator = this->optionMap.find(longName);
    STORM_LOG_THROW(optionIterator != this->optionMap.end(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot retrieve unknown option '" << longName << "'.");
    return *optionIterator->second;
}

std::string const& ModuleSettings::getModuleName() const {
    return this->moduleName;
}

std::unique_ptr<storm::settings::SettingMemento> ModuleSettings::overrideOption(std::string const& name, bool requiredStatus) {
    bool currentStatus = this->isSet(name);
    if (requiredStatus) {
        this->set(name);
    } else {
        this->unset(name);
    }
    return std::unique_ptr<storm::settings::SettingMemento>(new storm::settings::SettingMemento(*this, name, currentStatus));
}

bool ModuleSettings::isSet(std::string const& optionName) const {
    return this->getOption(optionName).getHasOptionBeenSet();
}

void ModuleSettings::addOption(std::shared_ptr<Option> const& option) {
    auto optionIterator = this->optionMap.find(option->getLongName());
    STORM_LOG_THROW(optionIterator == this->optionMap.end(), storm::exceptions::IllegalFunctionCallException,
                    "Unable to register the option '" << option->getLongName() << "' in module '" << this->getModuleName()
                                                      << "', because an option with this name already exists.");
    this->optionMap.emplace(option->getLongName(), option);
    this->options.push_back(option);
}

uint_fast64_t ModuleSettings::getPrintLengthOfLongestOption(bool includeAdvanced) const {
    uint_fast64_t length = 0;
    for (auto const& option : this->options) {
        if (includeAdvanced || !option->getIsAdvanced()) {
            length = std::max(length, option->getPrintLength());
        }
    }
    return length;
}

void ModuleSettings::restoreDefaults() {
    for (auto& option : options) {
        for (auto& argument : option->getArguments()) {
            if (argument->getHasDefaultValue()) {
                argument->setFromDefaultValue();
            }
        }
    }
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
