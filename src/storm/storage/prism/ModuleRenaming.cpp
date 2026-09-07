#include "ModuleRenaming.h"

namespace storm {
namespace prism {
ModuleRenaming::ModuleRenaming(std::map<std::string, std::string> const& renaming) : renaming(renaming) {
    // Intentionally left empty
}

ModuleRenaming::ModuleRenaming(std::map<std::string, std::string>&& renaming) : renaming(std::move(renaming)) {
    // Intentionally left empty
}

std::map<std::string, std::string> const& ModuleRenaming::getRenaming() const {
    return this->renaming;
}

std::ostream& operator<<(std::ostream& stream, ModuleRenaming const& renaming) {
    stream << "[ ";
    bool first = true;
    for (auto const& renamingPair : renaming.getRenaming()) {
        stream << renamingPair.first << "=" << renamingPair.second;
        if (first) {
            first = false;
        } else {
            stream << ",";
        }
    }
    stream << "]";
    return stream;
}

}  // namespace prism
}  // namespace storm
