#pragma once

#include <cstdint>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"

namespace storm::test::zeroWeight {

template<typename ValueType>
struct SplitterModel {
    storm::storage::SparseMatrix<ValueType> matrix;
    std::vector<ValueType> weights;
    storm::storage::BitVector targets;
    storm::storage::BitVector initials;
};

template<typename ValueType>
SplitterModel<ValueType> buildMixedModel() {
    auto const one = storm::utility::one<ValueType>();
    ValueType const half = one / (one + one);
    storm::storage::SparseMatrixBuilder<ValueType> builder(9, 4, 12, true, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, half);
    builder.addNextValue(0, 3, half);
    builder.addNextValue(1, 2, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, half);
    builder.addNextValue(2, 2, half);
    builder.addNextValue(3, 2, half);
    builder.addNextValue(3, 3, half);
    builder.addNextValue(4, 3, one);
    builder.newRowGroup(5);
    builder.addNextValue(5, 1, one);
    builder.addNextValue(6, 3, one);
    builder.newRowGroup(7);
    builder.addNextValue(7, 3, one);
    builder.addNextValue(8, 3, one);

    SplitterModel<ValueType> model;
    model.matrix = builder.build();
    for (uint64_t weight : std::vector<uint64_t>{1, 5, 3, 0, 7, 0, 2, 0, 4}) {
        model.weights.push_back(storm::utility::convertNumber<ValueType>(weight));
    }
    model.targets = storm::storage::BitVector(4, false);
    model.targets.set(3);
    model.initials = storm::storage::BitVector(4, false);
    model.initials.set(0);
    return model;
}

}  // namespace storm::test::zeroWeight
