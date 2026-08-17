#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "storm/storage/BoostTypes.h"
#include "storm/storage/prism/BooleanVariable.h"
#include "storm/storage/prism/ClockVariable.h"
#include "storm/storage/prism/Command.h"
#include "storm/storage/prism/IntegerVariable.h"

namespace storm {
namespace prism {
class ModuleRenaming : public LocatedInformation {
   public:
    /*!
     * Creates a module renaming.
     *
     * @param renaming The renaming of identifiers.
     */
    ModuleRenaming(std::map<std::string, std::string> const& renaming);

    /*!
     * Creates a module renaming.
     *
     * @param renaming The renaming of identifiers.
     */
    ModuleRenaming(std::map<std::string, std::string>&& renaming);

    // Create default implementations of constructors/assignment.
    ModuleRenaming() = default;
    ModuleRenaming(ModuleRenaming const& other) = default;
    ModuleRenaming& operator=(ModuleRenaming const& other) = default;
    ModuleRenaming(ModuleRenaming&& other) = default;
    ModuleRenaming& operator=(ModuleRenaming&& other) = default;

    /*!
     * If the module was created via renaming, this method returns the applied renaming of identifiers used for
     * the renaming process.
     *
     * @return A mapping of identifiers to new identifiers that was used in the renaming process.
     */
    std::map<std::string, std::string> const& getRenaming() const;

    friend std::ostream& operator<<(std::ostream& stream, ModuleRenaming const& module);

   private:
    // contains the provided renaming of identifiers.
    std::map<std::string, std::string> renaming;
};

}  // namespace prism
}  // namespace storm
