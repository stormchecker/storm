#include "SystemCompositionConstruct.h"

namespace storm {
namespace prism {

SystemCompositionConstruct::SystemCompositionConstruct(std::shared_ptr<Composition> const& composition, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), composition(composition) {
    // Intentionlly left empty.
}

Composition const& SystemCompositionConstruct::getSystemComposition() const {
    return *composition;
}

std::ostream& operator<<(std::ostream& stream, SystemCompositionConstruct const& systemCompositionConstruct) {
    stream << "system\n";
    stream << "\t" << systemCompositionConstruct.getSystemComposition() << '\n';
    stream << "endsystem\n";
    return stream;
}

}  // namespace prism
}  // namespace storm
