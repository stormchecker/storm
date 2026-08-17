#include "storm/storage/umb/model/GenericVector.h"

#include "storm/storage/umb/model/ValueEncoding.h"

namespace storm::umb {

void GenericVector::unset() {
    data = std::monostate();
}

bool GenericVector::hasValue() const {
    return !std::holds_alternative<std::monostate>(data);
}

uint64_t GenericVector::size() const {
    return std::visit(
        [](auto const& v) -> uint64_t {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(v)>, std::monostate>) {
                return 0;
            } else {
                return v.size();
            }
        },
        data);
}

}  // namespace storm::umb