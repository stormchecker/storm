#pragma once

#include "storm/models/sparse/Pomdp.h"

namespace storm {
namespace transformer {

template<typename ValueType>
class MakePOMDPCanonic {
   public:
    MakePOMDPCanonic(storm::models::sparse::Pomdp<ValueType> const& pomdp) : pomdp(pomdp) {}

    std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> transform() const;

   protected:
    std::vector<uint64_t> computeCanonicalPermutation() const;
    std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> applyPermutationOnPomdp(std::vector<uint64_t> permutation) const;
    std::string getStateInformation(uint64_t state) const;
    std::string getObservationInformation(uint32_t obs) const;

    storm::models::sparse::Pomdp<ValueType> const& pomdp;
};
}  // namespace transformer
}  // namespace storm