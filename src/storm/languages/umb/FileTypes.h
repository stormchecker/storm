#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

namespace storm {

namespace storage {
class BitVector;
}

namespace umb {

class GenericVector;

template<typename T>
using VectorType = std::conditional_t<std::is_same_v<T, bool>, storm::storage::BitVector, std::vector<T>>;

template<typename T>
using OptionalVectorType = std::optional<VectorType<T>>;

struct AnyValueType {};

template<typename T>
using TO1 = std::conditional_t<std::is_same_v<T, AnyValueType>, GenericVector, OptionalVectorType<T>>;

template<typename T>
using SEQ = TO1<T>;

using CSR = OptionalVectorType<uint64_t>;

}  // namespace umb
}  // namespace storm