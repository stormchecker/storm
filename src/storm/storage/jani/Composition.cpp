#include "Composition.h"

#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/ParallelComposition.h"

namespace storm {
namespace jani {

bool Composition::isAutomatonComposition() const {
    return false;
}

bool Composition::isParallelComposition() const {
    return false;
}

AutomatonComposition const& Composition::asAutomatonComposition() const {
    return static_cast<AutomatonComposition const&>(*this);
}

ParallelComposition const& Composition::asParallelComposition() const {
    return static_cast<ParallelComposition const&>(*this);
}

std::ostream& operator<<(std::ostream& stream, Composition const& composition) {
    composition.write(stream);
    return stream;
}

}  // namespace jani
}  // namespace storm
