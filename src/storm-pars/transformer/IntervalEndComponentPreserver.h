#pragma once

#include <optional>
#include <vector>

#include "storm/adapters/IntervalForward.h"

namespace storm {

namespace storage {
template<typename ValueType>
class SparseMatrix;
}  // namespace storage

namespace transformer {

class IntervalEndComponentPreserver {
   public:
    IntervalEndComponentPreserver();
    std::optional<storage::SparseMatrix<Interval>> eliminateMECs(storm::storage::SparseMatrix<Interval> const& matrix, std::vector<Interval> const& vector);
};
}  // namespace transformer
}  // namespace storm
