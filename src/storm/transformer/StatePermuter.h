#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "storm/models/sparse/Model.h"

namespace storm::transformer {

/*!
 * Applies the given permutation to the states of the given model.
 * The permutation must be a permutation of the numbers 0, 1, ..., n-1 for n = model.getNumberOfStates().
 * The state at position i of the input model will be moved to position permutation[i] in the output model.
 * @tparam ValueType
 * @param originalModel
 * @param permutation
 * @return
 */
template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> permuteStates(storm::models::sparse::Model<ValueType> const& originalModel,
                                                                       std::vector<uint64_t> const& permutation);

}  // namespace storm::transformer