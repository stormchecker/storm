#pragma once

#include <cstdint>
#include <vector>

namespace storm::storage {
template<typename ValueType>
class SparseMatrix;
}

namespace storm::modelchecker::detail {

/*!
 * Computes the (possibly infinite) value `offset + sum_{j} P(rowIndex, j) * vector[j]` for the given row.
 * The result is infinite if there is a successor with positive probability whose value is infinite.
 * This works no matter whether the value type has an infinity of its own (e.g. double) or whether it is extended by one (e.g. rational numbers, see
 * storm/utility/ExtendedNumber.h).
 * @tparam VectorValueType Either ValueType or the type that extends ValueType with infinity
 */
template<typename ValueType, typename VectorValueType>
VectorValueType multiplyRowWithVectorAddNumber(storm::storage::SparseMatrix<ValueType> const& matrix, uint64_t rowIndex,
                                               std::vector<VectorValueType> const& vector, ValueType const& offset);

}  // namespace storm::modelchecker::detail
