#pragma once

#include <map>
#include <string>

#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm {
namespace transformer {

enum class PomdpFscApplicationMode { STANDARD, SIMPLE_LINEAR, SIMPLE_LINEAR_INVERSE, SIMPLE_LOG, FULL };

PomdpFscApplicationMode parsePomdpFscApplicationMode(std::string const& mode);

template<typename ValueType>
class ApplyFiniteSchedulerToPomdp {
   public:
    ApplyFiniteSchedulerToPomdp(storm::models::sparse::Pomdp<ValueType> const& pomdp) : pomdp(pomdp) {}

    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> transform(
        PomdpFscApplicationMode applicationMode = PomdpFscApplicationMode::SIMPLE_LINEAR) const;

   private:
    std::unordered_map<uint32_t, std::vector<storm::RationalFunction>> getObservationChoiceWeights(PomdpFscApplicationMode applicationMode) const;

    storm::models::sparse::Pomdp<ValueType> const& pomdp;
};
}  // namespace transformer
}  // namespace storm