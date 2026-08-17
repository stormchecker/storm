#pragma once

#include <cstdint>
#include <vector>

namespace storm {
namespace utility {
namespace numerical {

template<typename ValueType>
struct FoxGlynnResult {
    FoxGlynnResult();

    uint64_t left;
    uint64_t right;
    ValueType totalWeight;
    std::vector<ValueType> weights;
};

template<typename ValueType>
FoxGlynnResult<ValueType> foxGlynn(ValueType lambda, ValueType epsilon);

}  // namespace numerical
}  // namespace utility
}  // namespace storm
