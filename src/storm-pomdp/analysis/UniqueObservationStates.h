#pragma once

#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace analysis {
template<typename ValueType>
class UniqueObservationStates {
   public:
    UniqueObservationStates(storm::models::sparse::Pomdp<ValueType> const& pomdp);
    storm::storage::BitVector analyse() const;

   private:
    storm::models::sparse::Pomdp<ValueType> const& pomdp;
};
}  // namespace analysis
}  // namespace storm
