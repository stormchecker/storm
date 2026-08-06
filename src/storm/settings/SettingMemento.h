#pragma once

#include <memory>
#include <string>

namespace storm {
namespace settings {

// Forward-declare the module settings.
namespace modules {
class ModuleSettings;
}

/*!
 * This class is used to reset the state of an option that was temporarily set to a different status.
 */
class SettingMemento {
   public:
    /*!
     * Constructs a new memento for the specified option.
     *
     * @param settings The settings object in which to restore the state of the option.
     * @param longOptionName The long name of the option.
     * @param resetToState A flag that indicates the status to which the option is to be reset upon
     * deconstruction of this object.
     */
    SettingMemento(modules::ModuleSettings& settings, std::string const& longOptionName, bool resetToState);

    /*!
     * Destructs the memento object and resets the value of the option to its original state.
     */
    virtual ~SettingMemento();

   private:
    // The settings object in which the setting is to be restored.
    modules::ModuleSettings& settings;

    // The long name of the option that was temporarily set.
    std::string const optionName;

    // The state of the option before it was set.
    bool resetToState;
};

}  // namespace settings
}  // namespace storm
