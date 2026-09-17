#pragma once

#include <cstdint>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"

namespace storm::test::zeroWeight {

template<typename ValueType>
struct ZeroWeightTestModel {
    storm::storage::SparseMatrix<ValueType> matrix;
    std::vector<ValueType> weights;
    storm::storage::BitVector targets;
    storm::storage::BitVector initials;
};

template<typename ValueType>
ZeroWeightTestModel<ValueType> buildMixedModel() {
    auto const one = storm::utility::one<ValueType>();
    ValueType const half = one / (one + one);
    storm::storage::SparseMatrixBuilder<ValueType> builder(9, 4, 12, true, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, half);
    builder.addNextValue(0, 3, half);
    builder.addNextValue(1, 2, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, half);
    builder.addNextValue(2, 3, half);
    builder.addNextValue(3, 2, half);
    builder.addNextValue(3, 3, half);
    builder.addNextValue(4, 3, one);
    builder.newRowGroup(5);
    builder.addNextValue(5, 1, one);
    builder.addNextValue(6, 3, one);
    builder.newRowGroup(7);
    builder.addNextValue(7, 3, one);
    builder.addNextValue(8, 3, one);

    ZeroWeightTestModel<ValueType> model;
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

template<typename ValueType>
ZeroWeightTestModel<ValueType> buildWeakComponentModel() {
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    storm::storage::SparseMatrixBuilder<ValueType> builder(7, 7, 7, true, true, 7);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.newRowGroup(1);
    builder.addNextValue(1, 6, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 0, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 4, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 6, one);
    builder.newRowGroup(5);
    builder.addNextValue(5, 3, one);
    builder.newRowGroup(6);
    builder.addNextValue(6, 6, one);

    ZeroWeightTestModel<ValueType> model;
    model.matrix = builder.build();
    model.weights = {zero, zero, one, zero, zero, one, zero};
    model.targets = storm::storage::BitVector(7, false);
    model.targets.set(6);
    model.initials = storm::storage::BitVector(7, false);
    model.initials.set(2);
    model.initials.set(5);
    return model;
}

template<typename ValueType>
ZeroWeightTestModel<ValueType> buildComponentInterfaceModel() {
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    ValueType const half = one / (one + one);
    ValueType const quarter = half / (one + one);
    storm::storage::SparseMatrixBuilder<ValueType> builder(9, 7, 18, true, true, 7);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, quarter);
    builder.addNextValue(0, 2, quarter);
    builder.addNextValue(0, 3, quarter);
    builder.addNextValue(0, 4, quarter);
    builder.addNextValue(1, 1, half);
    builder.addNextValue(1, 4, zero);
    builder.addNextValue(1, 5, half);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, half);
    builder.addNextValue(2, 5, half);
    builder.addNextValue(3, 5, one);
    builder.addNextValue(3, 6, zero);
    builder.newRowGroup(4);
    builder.addNextValue(4, 5, one);
    builder.newRowGroup(5);
    builder.addNextValue(5, 4, half);
    builder.addNextValue(5, 6, half);
    builder.newRowGroup(6);
    builder.addNextValue(6, 5, one);
    builder.newRowGroup(7);
    builder.addNextValue(7, 1, half);
    builder.addNextValue(7, 3, half);
    builder.newRowGroup(8);
    builder.addNextValue(8, 1, one);

    ZeroWeightTestModel<ValueType> model;
    model.matrix = builder.build();
    model.weights = {one, one, zero, zero, zero, zero, zero, one, one};
    model.targets = storm::storage::BitVector(7, false);
    model.targets.set(6);
    model.initials = storm::storage::BitVector(7, false);
    model.initials.set(0);
    return model;
}

}  // namespace storm::test::zeroWeight
