#include "StronglyConnectedComponent.h"

namespace storm {
namespace storage {
StronglyConnectedComponent::StronglyConnectedComponent() : isTrivialScc(false) {
    // Intentionally left empty.
}

void StronglyConnectedComponent::setIsTrivial(bool trivial) {
    this->isTrivialScc = trivial;
}

bool StronglyConnectedComponent::isTrivial() const {
    return isTrivialScc;
}
}  // namespace storage
}  // namespace storm
