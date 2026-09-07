#include "HyperplaneCollector.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename ValueType>
bool HyperplaneCollector<ValueType>::insert(EigenVector const& normal, ValueType const& offset, std::vector<uint_fast64_t> const* indexList) {
    EigenVector copyNormal(normal);
    ValueType copyOffset(offset);
    return this->insert(std::move(copyNormal), std::move(copyOffset), indexList);
}

template<typename ValueType>
bool HyperplaneCollector<ValueType>::insert(EigenVector&& normal, ValueType&& offset, std::vector<uint_fast64_t> const* indexList) {
    // Normalize
    ValueType infinityNorm = normal.template lpNorm<Eigen::Infinity>();
    if (infinityNorm != (ValueType)0) {
        normal /= infinityNorm;
        offset /= infinityNorm;
    }

    if (indexList == nullptr) {
        // insert with empty list
        return map.insert(MapValueType(MapKeyType(normal, offset), std::vector<uint_fast64_t>())).second;
    } else {
        auto inserted = map.insert(MapValueType(MapKeyType(normal, offset), *indexList));
        if (!inserted.second) {
            // Append vertex list
            inserted.first->second.insert(inserted.first->second.end(), indexList->begin(), indexList->end());
        }
        return inserted.second;
    }
}

template<typename ValueType>
std::pair<typename HyperplaneCollector<ValueType>::EigenMatrix, typename HyperplaneCollector<ValueType>::EigenVector>
HyperplaneCollector<ValueType>::getCollectedHyperplanesAsMatrixVector() const {
    if (map.empty()) {
        return std::pair<EigenMatrix, EigenVector>();
    }

    EigenMatrix A(map.size(), map.begin()->first.first.rows());
    EigenVector b(map.size());

    uint_fast64_t row = 0;
    for (auto const& mapEntry : map) {
        A.row(row) = mapEntry.first.first;
        b(row) = mapEntry.first.second;
        ++row;
    }
    return std::pair<EigenMatrix, EigenVector>(std::move(A), std::move(b));
}

template<typename ValueType>
std::vector<std::vector<uint_fast64_t>> HyperplaneCollector<ValueType>::getIndexLists() const {
    std::vector<std::vector<uint_fast64_t>> result(map.size());

    auto resultIt = result.begin();
    for (auto const& mapEntry : map) {
        *resultIt = mapEntry.second;
        ++resultIt;
    }
    return result;
}

template<typename ValueType>
uint_fast64_t HyperplaneCollector<ValueType>::numOfCollectedHyperplanes() const {
    return map.size();
}

template class HyperplaneCollector<double>;
template class HyperplaneCollector<storm::RationalNumber>;

}  // namespace geometry
}  // namespace storage
}  // namespace storm
