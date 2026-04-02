#include "storm/generator/Distribution.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/BitVector.h"

namespace storm::generator {

template<typename IndexType, typename ValueType>
Distribution<IndexType, ValueType>::Distribution() : compressed(true) {
    // Intentionally left empty.
}

template<typename IndexType, typename ValueType>
Distribution<IndexType, ValueType>::Distribution(Distribution<IndexType, ValueType> const& other) {
    this->storage = other.storage;
    this->compressed = other.compressed;
}

template<typename IndexType, typename ValueType>
Distribution<IndexType, ValueType>::Distribution(Distribution<IndexType, ValueType>&& other) {
    this->storage = std::move(other.storage);
    this->compressed = other.compressed;
    other.compressed = true;
}

template<typename IndexType, typename ValueType>
Distribution<IndexType, ValueType>& Distribution<IndexType, ValueType>::operator=(Distribution<IndexType, ValueType> const& other) {
    if (this != &other) {
        this->storage = other.storage;
        this->compressed = other.compressed;
    }
    return *this;
}

template<typename IndexType, typename ValueType>
Distribution<IndexType, ValueType>& Distribution<IndexType, ValueType>::operator=(Distribution<IndexType, ValueType>&& other) {
    if (this != &other) {
        this->storage = std::move(other.storage);
        this->compressed = other.compressed;
        other.compressed = true;
    }
    return *this;
}

template<typename IndexType, typename ValueType>
void Distribution<IndexType, ValueType>::add(DistributionEntry<IndexType, ValueType> const& entry) {
    storage.push_back(entry);
    compressed &= storage.back().getState() < entry.getState();
}

template<typename IndexType, typename ValueType>
void Distribution<IndexType, ValueType>::add(IndexType const& index, ValueType const& value) {
    storage.emplace_back(index, value);
    compressed &= storage.back().getState() < index;
}

template<typename IndexType, typename ValueType>
void Distribution<IndexType, ValueType>::add(Distribution&& distribution) {
    storage.insert(storage.end(), std::make_move_iterator(distribution.begin()), std::make_move_iterator(distribution.end()));
    compressed = false;
}

template<typename IndexType, typename ValueType>
void Distribution<IndexType, ValueType>::compress() {
    if (!compressed) {
        std::sort(storage.begin(), storage.end(), [](DistributionEntry<IndexType, ValueType> const& a, DistributionEntry<IndexType, ValueType> const& b) {
            return a.getState() < b.getState();
        });

        // Code taken from std::unique and modified to fit needs.
        auto first = storage.begin();
        auto last = storage.end();

        if (first != last) {
            auto result = first;
            while (++first != last) {
                if (!(result->getState() == first->getState())) {
                    if (++result != first) {
                        *result = std::move(*first);
                    }
                } else {
                    result->addToValue(first->getValue());
                }
            }
            ++result;

            storage.resize(std::distance(storage.begin(), result));
        }
        compressed = true;
    }
}

template<typename IndexType, typename ValueType>
void Distribution<IndexType, ValueType>::divide(ValueType const& value) {
    for (auto& entry : storage) {
        entry.divide(value);
    }
}

template<typename IndexType, typename ValueType>
void Distribution<IndexType, ValueType>::clear() {
    this->storage.clear();
    this->compressed = true;
}

template<typename IndexType, typename ValueType>
typename Distribution<IndexType, ValueType>::ContainerType::iterator Distribution<IndexType, ValueType>::begin() {
    return storage.begin();
}

template<typename IndexType, typename ValueType>
typename Distribution<IndexType, ValueType>::ContainerType::const_iterator Distribution<IndexType, ValueType>::begin() const {
    return storage.begin();
}

template<typename IndexType, typename ValueType>
typename Distribution<IndexType, ValueType>::ContainerType::iterator Distribution<IndexType, ValueType>::end() {
    return storage.end();
}

template<typename IndexType, typename ValueType>
typename Distribution<IndexType, ValueType>::ContainerType::const_iterator Distribution<IndexType, ValueType>::end() const {
    return storage.end();
}

template class Distribution<uint32_t, double>;
template class Distribution<uint32_t, storm::RationalNumber>;
template class Distribution<uint32_t, storm::RationalFunction>;
template class Distribution<uint32_t, storm::Interval>;
template class Distribution<uint32_t, storm::RationalInterval>;

template class Distribution<storm::storage::BitVector, double>;
template class Distribution<storm::storage::BitVector, storm::RationalNumber>;
template class Distribution<storm::storage::BitVector, storm::RationalFunction>;
template class Distribution<storm::storage::BitVector, storm::Interval>;
template class Distribution<storm::storage::BitVector, storm::RationalInterval>;

}  // namespace storm::generator
