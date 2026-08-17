#pragma once

#include "storm/api/storm.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm {
namespace pomdp {
namespace transformer {
template<class ValueType>
class KnownProbabilityTransformer {
   public:
    KnownProbabilityTransformer();

    std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> transform(storm::models::sparse::Pomdp<ValueType> const &pomdp,
                                                                       storm::storage::BitVector &prob0States, storm::storage::BitVector &prob1States);
};
}  // namespace transformer
}  // namespace pomdp
}  // namespace storm
