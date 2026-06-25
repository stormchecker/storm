#include "storm/storage/valuations/ValuationDescription.h"

namespace storm::storage::sparse {
uint64_t ValuationClassDescription::sizeInBits() const {
    uint64_t totalSize = 0;
    for (auto const& variable : variables) {
        if (std::holds_alternative<Padding>(variable)) {
            totalSize += std::get<Padding>(variable).padding;
        } else if (std::holds_alternative<Variable>(variable)) {
            auto const& var = std::get<Variable>(variable);
            if (var.isOptional.value_or(false)) {
                ++totalSize;
            }
            totalSize += var.type.bitSize();
        }
    }
    return totalSize;
}

bool ValuationClassDescription::hasStringVariable() const {
    for (auto const& variable : variables) {
        if (std::holds_alternative<Variable>(variable)) {
            auto const& var = std::get<Variable>(variable);
            if (isStringType(var.type.type)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace storm::storage::sparse
