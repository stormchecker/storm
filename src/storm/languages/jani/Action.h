#pragma once

#include <string>

namespace storm {
namespace jani {
class Action {
   public:
    /**
     * Creates an action
     * @param name name of the action
     */
    Action(std::string const& name);

    /**
     * Returns the name of the location.
     */
    std::string const& getName() const;

   private:
    /// The name of the action.
    std::string name;
};

}  // namespace jani
}  // namespace storm
