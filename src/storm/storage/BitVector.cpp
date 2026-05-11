#include <algorithm>
#include <bit>
#include <bitset>
#include <iostream>

#include "storm/storage/BitVector.h"

#include "storm/storage/BoostTypes.h"
#include "storm/utility/Hash.h"
#include "storm/utility/macros.h"

// Uncomment the following line to enable additional assertions for debugging bitvector operations.
// #define ASSERT_BITVECTOR

namespace storm {
namespace storage {

BitVector::const_iterator::const_iterator() : dataPtr(nullptr), currentIndex(0), endIndex(0) {};

BitVector::const_iterator::const_iterator(uint64_t const* dataPtr, uint64_t startIndex, uint64_t endIndex, bool setOnFirstBit)
    : dataPtr(dataPtr), endIndex(endIndex) {
    if (setOnFirstBit) {
        // Set the index of the first set bit in the vector.
        currentIndex = getNextIndexWithValue<true>(dataPtr, startIndex, endIndex);
    } else {
        currentIndex = startIndex;
    }
}

BitVector::const_iterator::const_iterator(const_iterator const& other) : dataPtr(other.dataPtr), currentIndex(other.currentIndex), endIndex(other.endIndex) {
    // Intentionally left empty.
}

BitVector::const_iterator& BitVector::const_iterator::operator=(const_iterator const& other) {
    // Only assign contents if the source and target are not the same.
    if (this != &other) {
        dataPtr = other.dataPtr;
        currentIndex = other.currentIndex;
        endIndex = other.endIndex;
    }
    return *this;
}

BitVector::const_iterator& BitVector::const_iterator::operator++() {
    currentIndex = getNextIndexWithValue<true>(dataPtr, ++currentIndex, endIndex);
    return *this;
}

BitVector::const_iterator BitVector::const_iterator::operator++(int) {
    BitVector::const_iterator copy{*this};
    ++(*this);
    return copy;
}

BitVector::const_iterator& BitVector::const_iterator::operator+=(size_t n) {
    for (size_t i = 0; i < n; ++i) {
        currentIndex = getNextIndexWithValue<true>(dataPtr, ++currentIndex, endIndex);
    }
    return *this;
}

uint64_t BitVector::const_iterator::operator*() const {
    return currentIndex;
}

bool BitVector::const_iterator::operator!=(const_iterator const& other) const {
    return currentIndex != other.currentIndex;
}

bool BitVector::const_iterator::operator==(const_iterator const& other) const {
    return currentIndex == other.currentIndex;
}

BitVector::const_reverse_iterator::const_reverse_iterator() : dataPtr(nullptr), currentIndex(0), lowerBound(0) {};

BitVector::const_reverse_iterator::const_reverse_iterator(uint64_t const* dataPtr, uint64_t upperBound, uint64_t lowerBound, bool setOnFirstBit)
    : dataPtr(dataPtr), lowerBound(lowerBound) {
    if (setOnFirstBit) {
        // Set the index of the first set bit in the vector.
        currentIndex = getNextIndexWithValue<true, true>(dataPtr, lowerBound, upperBound);
    } else {
        currentIndex = upperBound;
    }
}

BitVector::const_reverse_iterator::const_reverse_iterator(const_reverse_iterator const& other)
    : dataPtr(other.dataPtr), currentIndex(other.currentIndex), lowerBound(other.lowerBound) {
    // Intentionally left empty.
}

BitVector::const_reverse_iterator& BitVector::const_reverse_iterator::operator=(const_reverse_iterator const& other) {
    // Only assign contents if the source and target are not the same.
    if (this != &other) {
        dataPtr = other.dataPtr;
        currentIndex = other.currentIndex;
        lowerBound = other.lowerBound;
    }
    return *this;
}

BitVector::const_reverse_iterator& BitVector::const_reverse_iterator::operator++() {
    currentIndex = getNextIndexWithValue<true, true>(dataPtr, lowerBound, --currentIndex);
    return *this;
}
BitVector::const_reverse_iterator BitVector::const_reverse_iterator::operator++(int) {
    BitVector::const_reverse_iterator copy{*this};
    ++(*this);
    return copy;
}

BitVector::const_reverse_iterator& BitVector::const_reverse_iterator::operator+=(size_t n) {
    for (size_t i = 0; i < n; ++i) {
        currentIndex = getNextIndexWithValue<true, true>(dataPtr, lowerBound, --currentIndex);
    }
    return *this;
}

uint64_t BitVector::const_reverse_iterator::operator*() const {
    return currentIndex - 1;  // the stored index is off-by-one!
}

bool BitVector::const_reverse_iterator::operator!=(const_reverse_iterator const& other) const {
    return currentIndex != other.currentIndex;
}

bool BitVector::const_reverse_iterator::operator==(const_reverse_iterator const& other) const {
    return currentIndex == other.currentIndex;
}

BitVector::BitVector() : bitCount(0), buckets(nullptr) {
    // Intentionally left empty.
}

BitVector::BitVector(uint64_t length, bool init) : bitCount(length), buckets(nullptr) {
    // Compute the correct number of buckets needed to store the given number of bits.
    uint64_t bucketCount = length >> 6;
    if ((length & mod64mask) != 0) {
        ++bucketCount;
    }

    // Initialize the storage with the required values.
    if (init) {
        buckets = new uint64_t[bucketCount];
        std::fill_n(buckets, bucketCount, -1ull);
        truncateLastBucket();
    } else {
        buckets = new uint64_t[bucketCount]();
    }
}

BitVector::~BitVector() {
    delete[] buckets;
}

template<typename InputIterator>
BitVector::BitVector(uint64_t length, InputIterator begin, InputIterator end) : BitVector(length) {
    set(begin, end);
}

BitVector::BitVector(uint64_t length, std::vector<uint64_t> setEntries) : BitVector(length, setEntries.begin(), setEntries.end()) {
    // Intentionally left empty.
}

BitVector::BitVector(uint64_t bucketCount, uint64_t bitCount) : bitCount(bitCount), buckets(nullptr) {
    STORM_LOG_ASSERT((bucketCount << 6) == bitCount, "Bit count does not match number of buckets.");
    buckets = new uint64_t[bucketCount]();
}

BitVector::BitVector(BitVector const& other) : bitCount(other.bitCount), buckets(nullptr) {
    buckets = new uint64_t[other.bucketCount()];
    std::copy_n(other.buckets, other.bucketCount(), buckets);
}

BitVector& BitVector::operator=(BitVector const& other) {
    // Only perform the assignment if the source and target are not identical.
    if (this != &other) {
        if (buckets && bucketCount() != other.bucketCount()) {
            delete[] buckets;
            buckets = nullptr;
        }
        bitCount = other.bitCount;
        if (!buckets) {
            buckets = new uint64_t[other.bucketCount()];
        }
        std::copy_n(other.buckets, other.bucketCount(), buckets);
    }
    return *this;
}

bool BitVector::operator<(BitVector const& other) const {
    if (this->size() < other.size()) {
        return true;
    } else if (this->size() > other.size()) {
        return false;
    }

    uint64_t* first1 = this->buckets;
    uint64_t* last1 = this->buckets + this->bucketCount();
    uint64_t* first2 = other.buckets;

    for (; first1 != last1; ++first1, ++first2) {
        if (*first1 < *first2) {
            return true;
        } else if (*first1 > *first2) {
            return false;
        }
    }
    return false;
}

BitVector::BitVector(BitVector&& other) : bitCount(other.bitCount), buckets(other.buckets) {
    other.bitCount = 0;
    other.buckets = nullptr;
}

BitVector& BitVector::operator=(BitVector&& other) {
    // Only perform the assignment if the source and target are not identical.
    if (this != &other) {
        bitCount = other.bitCount;
        other.bitCount = 0;
        delete[] this->buckets;
        this->buckets = other.buckets;
        other.buckets = nullptr;
    }

    return *this;
}

bool BitVector::operator==(BitVector const& other) const {
    // If the lengths of the vectors do not match, they are considered unequal.
    if (this->bitCount != other.bitCount)
        return false;

    // If the lengths match, we compare the buckets one by one.
    return std::equal(this->buckets, this->buckets + this->bucketCount(), other.buckets);
}

bool BitVector::operator!=(BitVector const& other) const {
    return !(*this == other);
}

void BitVector::set(uint64_t index, bool value) {
    STORM_LOG_ASSERT(index < bitCount, "Invalid call to BitVector::set: written index " << index << " out of bounds.");
    uint64_t bucket = index >> 6;

    uint64_t mask = 1ull << (63 - (index & mod64mask));
    if (value) {
        buckets[bucket] |= mask;
    } else {
        buckets[bucket] &= ~mask;
    }
}

template<typename InputIterator>
void BitVector::set(InputIterator begin, InputIterator end, bool value) {
    for (InputIterator it = begin; it != end; ++it) {
        this->set(*it, value);
    }
}

bool BitVector::operator[](uint64_t index) const {
    uint64_t bucket = index >> 6;
    uint64_t mask = 1ull << (63 - (index & mod64mask));
    return (this->buckets[bucket] & mask) == mask;
}

bool BitVector::get(uint64_t index) const {
    STORM_LOG_ASSERT(index < bitCount, "Invalid call to BitVector::get: read index " << index << " out of bounds.");
    return (*this)[index];
}

void BitVector::resize(uint64_t newLength, bool init) {
    if (newLength > bitCount) {
        uint64_t newBucketCount = newLength >> 6;
        if ((newLength & mod64mask) != 0) {
            ++newBucketCount;
        }

        if (newBucketCount > this->bucketCount()) {
            uint64_t* newBuckets = new uint64_t[newBucketCount];
            std::copy_n(buckets, this->bucketCount(), newBuckets);
            if (init) {
                if (this->bucketCount() > 0) {
                    newBuckets[this->bucketCount() - 1] |= ((1ull << (64 - (bitCount & mod64mask))) - 1ull);
                }
                std::fill_n(newBuckets + this->bucketCount(), newBucketCount - this->bucketCount(), -1ull);
            } else {
                std::fill_n(newBuckets + this->bucketCount(), newBucketCount - this->bucketCount(), 0);
            }
            delete[] buckets;
            buckets = newBuckets;
            bitCount = newLength;
        } else {
            // If the underlying storage does not need to grow, we have to insert the missing bits.
            if (init) {
                buckets[this->bucketCount() - 1] |= ((1ull << (64 - (bitCount & mod64mask))) - 1ull);
            }
            bitCount = newLength;
        }
        truncateLastBucket();
    } else {
        uint64_t newBucketCount = newLength >> 6;
        if ((newLength & mod64mask) != 0) {
            ++newBucketCount;
        }

        // If the number of buckets needs to be reduced, we resize it now. Otherwise, we can just truncate the
        // last bucket.
        if (newBucketCount < this->bucketCount()) {
            uint64_t* newBuckets = new uint64_t[newBucketCount];
            std::copy_n(buckets, newBucketCount, newBuckets);
            delete[] buckets;
            buckets = newBuckets;
            bitCount = newLength;
        }
        bitCount = newLength;
        truncateLastBucket();
    }
}

void BitVector::concat(BitVector const& other) {
    STORM_LOG_ASSERT(size() % 64 == 0, "We expect the length of the left bitvector to be a multiple of 64.");
    // TODO this assumption is due to the implementation of BitVector::set().
    BitVector tmp(size() + other.size());
    tmp.set(size(), other);
    resize(size() + other.size(), false);
    *this |= tmp;
}

void BitVector::expandSize(bool init) {
    // size_t oldBitCount = bitCount;
    bitCount = bucketCount() * 64;
    if (init) {
        STORM_LOG_ASSERT(false, "Not implemented as we do not foresee any need");
    }
}

void BitVector::grow(uint64_t minimumLength, bool init) {
    if (minimumLength > bitCount) {
        // We double the bitcount as long as it is less then the minimum length.
        uint64_t newLength = std::max(static_cast<uint64_t>(64), bitCount);
        // Note that newLength has to be initialized with a non-zero number.
        while (newLength < minimumLength) {
            newLength = newLength << 1;
        }
        resize(newLength, init);
    }
}

BitVector BitVector::operator&(BitVector const& other) const {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
    BitVector result(bitCount);
    std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets,
                   [](uint64_t const& a, uint64_t const& b) { return a & b; });
    return result;
}

BitVector& BitVector::operator&=(BitVector const& other) {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
    std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, this->buckets,
                   [](uint64_t const& a, uint64_t const& b) { return a & b; });
    return *this;
}

BitVector BitVector::operator|(BitVector const& other) const {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
    BitVector result(bitCount);
    std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets,
                   [](uint64_t const& a, uint64_t const& b) { return a | b; });
    return result;
}

BitVector& BitVector::operator|=(BitVector const& other) {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
    std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, this->buckets,
                   [](uint64_t const& a, uint64_t const& b) { return a | b; });
    return *this;
}

BitVector BitVector::operator^(BitVector const& other) const {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");
    BitVector result(bitCount);
    std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets,
                   [](uint64_t const& a, uint64_t const& b) { return a ^ b; });
    result.truncateLastBucket();
    return result;
}

BitVector BitVector::operator%(BitVector const& filter) const {
    STORM_LOG_ASSERT(bitCount == filter.bitCount, "Length of the bit vectors does not match.");

    BitVector result(filter.getNumberOfSetBits());

    // If the current bit vector has not too many elements compared to the given bit vector we prefer iterating
    // over its elements.
    if (filter.getNumberOfSetBits() / 10 < this->getNumberOfSetBits()) {
        uint64_t position = 0;
        for (auto bit : filter) {
            if ((*this)[bit]) {
                result.set(position);
            }
            ++position;
        }
    } else {
        // If the given bit vector had much fewer elements, we iterate over its elements and accept calling the
        // more costly operation getNumberOfSetBitsBeforeIndex on the current bit vector.
        for (auto bit : (*this)) {
            if (filter[bit]) {
                result.set(filter.getNumberOfSetBitsBeforeIndex(bit));
            }
        }
    }

    return result;
}

BitVector BitVector::operator~() const {
    BitVector result(this->bitCount);
    std::transform(this->buckets, this->buckets + this->bucketCount(), result.buckets, [](uint64_t const& a) { return ~a; });
    result.truncateLastBucket();
    return result;
}

void BitVector::complement() {
    std::transform(this->buckets, this->buckets + this->bucketCount(), this->buckets, [](uint64_t const& a) { return ~a; });
    truncateLastBucket();
}

void BitVector::increment() {
    uint64_t firstUnsetIndex = getNextUnsetIndex(0);

    // If there is no unset index, we clear the whole vector
    if (firstUnsetIndex == this->bitCount) {
        this->clear();
    } else {
        // All previous buckets have to be set to zero
        uint64_t bucketIndex = firstUnsetIndex >> 6;
        std::fill_n(buckets, bucketIndex, 0);

        // modify the bucket in which the unset entry lies in
        uint64_t& bucket = this->buckets[bucketIndex];
        uint64_t indexInBucket = firstUnsetIndex & mod64mask;
        if (indexInBucket > 0) {
            // Clear all bits before the index
            uint64_t mask = ~(-1ull << (64 - indexInBucket));
            bucket &= mask;
        }

        // Set the bit at the index
        uint64_t mask = 1ull << (63 - indexInBucket);
        bucket |= mask;
    }
}

BitVector BitVector::implies(BitVector const& other) const {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");

    BitVector result(bitCount);
    std::transform(this->buckets, this->buckets + this->bucketCount(), other.buckets, result.buckets,
                   [](uint64_t const& a, uint64_t const& b) { return (~a | b); });
    result.truncateLastBucket();
    return result;
}

bool BitVector::isSubsetOf(BitVector const& other) const {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");

    uint64_t const* it1 = buckets;
    uint64_t const* ite1 = buckets + bucketCount();
    uint64_t const* it2 = other.buckets;

    for (; it1 != ite1; ++it1, ++it2) {
        if ((*it1 & *it2) != *it1) {
            return false;
        }
    }
    return true;
}

bool BitVector::isDisjointFrom(BitVector const& other) const {
    STORM_LOG_ASSERT(bitCount == other.bitCount, "Length of the bit vectors does not match.");

    uint64_t const* it1 = buckets;
    uint64_t const* ite1 = buckets + bucketCount();
    uint64_t const* it2 = other.buckets;

    for (; it1 != ite1; ++it1, ++it2) {
        if ((*it1 & *it2) != 0) {
            return false;
        }
    }
    return true;
}

bool BitVector::matches(uint64_t bitIndex, BitVector const& other) const {
    STORM_LOG_ASSERT((bitIndex & mod64mask) == 0, "Bit index must be a multiple of 64.");
    STORM_LOG_ASSERT(other.size() <= this->size() - bitIndex, "Bit vector argument is too long.");

    // Compute the first bucket that needs to be checked and the number of buckets.
    uint64_t index = bitIndex >> 6;

    uint64_t const* first1 = buckets + index;
    uint64_t const* first2 = other.buckets;
    uint64_t const* last2 = other.buckets + other.bucketCount();

    for (; first2 != last2; ++first1, ++first2) {
        if (*first1 != *first2) {
            return false;
        }
    }
    return true;
}

BitVector BitVector::permute(std::vector<uint64_t> const& inversePermutation) const {
    BitVector result(this->size());
    for (uint64_t i = 0; i < this->size(); ++i) {
        if (this->get(inversePermutation[i])) {
            result.set(i, true);
        }
    }
    return result;
}

BitVector BitVector::permuteGroupedVector(const std::vector<uint64_t>& inversePermutation, const std::vector<uint64_t>& rowGroupIndices) const {
    STORM_LOG_ASSERT(inversePermutation.size() == rowGroupIndices.size() - 1, "Inverse permutation and row group indices do not match.");
    BitVector result(this->size(), false);
    uint64_t targetIndex = 0u;
    for (auto const sourceGroupIndex : inversePermutation) {
        for (uint64_t sourceIndex = rowGroupIndices[sourceGroupIndex]; sourceIndex < rowGroupIndices[sourceGroupIndex + 1]; ++sourceIndex, ++targetIndex) {
            if (this->get(sourceIndex)) {
                result.set(targetIndex, true);
            }
        }
    }
    STORM_LOG_ASSERT(targetIndex == result.size(), "Target index does not match the size of the result.");
    return result;
}

void BitVector::set(uint64_t bitIndex, BitVector const& other) {
    STORM_LOG_ASSERT((bitIndex & mod64mask) == 0, "Bit index must be a multiple of 64.");
    STORM_LOG_ASSERT(other.size() <= this->size() - bitIndex, "Bit vector argument is too long.");

    // Compute the first bucket that needs to be checked and the number of buckets.
    uint64_t index = bitIndex >> 6;

    uint64_t* first1 = buckets + index;
    uint64_t const* first2 = other.buckets;
    uint64_t const* last2 = other.buckets + other.bucketCount();

    for (; first2 != last2; ++first1, ++first2) {
        *first1 = *first2;
    }
}

void BitVector::setMultiple(uint64_t bitIndex, uint64_t nrOfBits, bool newValue) {
    // TODO we may want to optimize this code for large nrs of bits.
    uint64_t endPos = std::min(bitIndex + nrOfBits, bitCount);
    for (uint64_t tmpIndex = bitIndex; tmpIndex < endPos; ++tmpIndex) {
        set(tmpIndex, newValue);
    }
}

storm::storage::BitVector BitVector::get(uint64_t bitIndex, uint64_t numberOfBits) const {
    uint64_t numberOfBuckets = numberOfBits >> 6;
    uint64_t index = bitIndex >> 6;
    STORM_LOG_ASSERT(index + numberOfBuckets <= this->bucketCount(), "Argument is out-of-range.");

    storm::storage::BitVector result(numberOfBuckets, numberOfBits);
    std::copy(this->buckets + index, this->buckets + index + numberOfBuckets, result.buckets);
    result.truncateLastBucket();
    return result;
}

uint64_t BitVector::getAsInt(uint64_t bitIndex, uint64_t numberOfBits) const {
    if (numberOfBits == 0) {  // It is necessary to catch this case as we might have an empty bitvector (i.e. uninitialized buckets).
        return 0;
    }
    STORM_LOG_ASSERT(numberOfBits <= 64, "Number of bits must be <= 64.");
    uint64_t const firstBucket = bitIndex >> 6;                                                   // the bucket where the value starts
    uint8_t const bitIndexInFirstBucket = bitIndex & mod64mask;                                   // the index within that bucket
    uint8_t const availableBitsInFirstBucket = static_cast<uint8_t>(64 - bitIndexInFirstBucket);  // number of available bits in that bucket

    // First get the result in the form rr...rrxx...xx (r = result, x = garbage)
    uint64_t result = buckets[firstBucket] << bitIndexInFirstBucket;
    // We might have to look at the next bucket, too
    if (availableBitsInFirstBucket < numberOfBits) {
        result |= buckets[firstBucket + 1] >> availableBitsInFirstBucket;
    }
    // Get rid of the garbage bits and return the result.
    return result >> (64 - numberOfBits);
}

uint64_t BitVector::getTwoBitsAligned(uint64_t bitIndex) const {
    // Check whether it is aligned.
    STORM_LOG_ASSERT(bitIndex % 64 != 63, "Bits not aligned.");
    uint64_t bucket = bitIndex >> 6;
    uint64_t bitIndexInBucket = bitIndex & mod64mask;

    uint64_t mask;
    if (bitIndexInBucket == 0) {
        mask = -1ull;
    } else {
        mask = (1ull << (64 - bitIndexInBucket)) - 1ull;
    }

    if (bitIndexInBucket < 62) {  // bitIndexInBucket + 2 < 64
        // If the value stops before the end of the bucket, we need to erase some lower bits.
        mask &= ~((1ull << (62 - (bitIndexInBucket))) - 1ull);
        return (buckets[bucket] & mask) >> (62 - bitIndexInBucket);
    } else {
        // In this case, it suffices to take the current mask.
        return buckets[bucket] & mask;
    }
}

void BitVector::setFromInt(uint64_t bitIndex, uint64_t numberOfBits, uint64_t value) {
    STORM_LOG_ASSERT(numberOfBits <= 64, "Number of bits must be <= 64.");
    STORM_LOG_ASSERT(numberOfBits == 64 || (value >> numberOfBits) == 0,
                     "Integer value (" << value << ") too large to fit in the given number of bits (" << numberOfBits << ").");

    uint64_t bucket = bitIndex >> 6;
    uint64_t bitIndexInBucket = bitIndex & mod64mask;

    uint64_t mask;
    if (bitIndexInBucket == 0) {
        mask = -1ull;
    } else {
        mask = (1ull << (64 - bitIndexInBucket)) - 1ull;
    }

    if (bitIndexInBucket + numberOfBits < 64) {
        // If the value stops before the end of the bucket, we need to erase some lower bits.
        mask &= ~((1ull << (64 - (bitIndexInBucket + numberOfBits))) - 1ull);
        buckets[bucket] = (buckets[bucket] & ~mask) | (value << (64 - (bitIndexInBucket + numberOfBits)));
    } else if (bitIndexInBucket + numberOfBits > 64) {
        // Write the part of the value that falls into the first bucket.
        buckets[bucket] = (buckets[bucket] & ~mask) | (value >> (numberOfBits + (bitIndexInBucket - 64)));
        ++bucket;

        // Compute the remaining number of bits.
        numberOfBits -= (64 - bitIndexInBucket);

        // Shift the bits of the value such that the already set bits disappear.
        value <<= (64 - numberOfBits);

        // Put the remaining bits in their place.
        mask = ((1ull << (64 - numberOfBits)) - 1ull);
        buckets[bucket] = (buckets[bucket] & mask) | value;
    } else {
        buckets[bucket] = (buckets[bucket] & ~mask) | value;
    }
}

bool BitVector::empty() const {
    uint64_t* last = buckets + bucketCount();
    uint64_t* it = std::find_if(buckets, last, [](uint64_t const& a) { return a != 0; });
    return it == last;
}

bool BitVector::full() const {
    if (bitCount == 0) {
        return true;
    }
    // Check that all buckets except the last one have all bits set.
    uint64_t* last = buckets + bucketCount() - 1;
    for (uint64_t const* it = buckets; it < last; ++it) {
        if (*it != -1ull) {
            return false;
        }
    }

    // Now check whether the relevant bits are set in the last bucket.
    uint64_t mask = ~((1ull << (64 - (bitCount & mod64mask))) - 1ull);
    if ((*last & mask) != mask) {
        return false;
    }
    return true;
}

void BitVector::clear() {
    std::fill_n(buckets, this->bucketCount(), 0);
}

void BitVector::fill() {
    std::fill_n(buckets, this->bucketCount(), -1ull);
    truncateLastBucket();
}

uint64_t BitVector::getNumberOfSetBits() const {
    return getNumberOfSetBitsBeforeIndex(bitCount);
}

uint64_t BitVector::getNumberOfSetBitsBeforeIndex(uint64_t index) const {
    STORM_LOG_ASSERT(index <= bitCount, "Invalid call to BitVector::getNumberOfSetBitsBeforeIndex: read index " << index << " out of bounds.");
    uint64_t const lastBucketIndex = index >> 6;
    uint64_t result = 0;

    // First, count all full buckets.
    for (uint64_t i = 0; i < lastBucketIndex; ++i) {
        result += std::popcount(buckets[i]);
    }

    // Now check if we have to count part of a bucket.
    uint8_t const endIndexInLastBucket = index & mod64mask;
    if (endIndexInLastBucket != 0) {
        result += std::popcount(buckets[lastBucketIndex] >> (64 - endIndexInLastBucket));
    }

    return result;
}

std::vector<uint64_t> BitVector::getNumberOfSetBitsBeforeIndices() const {
    std::vector<uint64_t> bitsSetBeforeIndices;
    bitsSetBeforeIndices.reserve(this->size());
    uint64_t lastIndex = 0;
    uint64_t currentNumberOfSetBits = 0;
    for (auto index : *this) {
        while (lastIndex <= index) {
            bitsSetBeforeIndices.push_back(currentNumberOfSetBits);
            ++lastIndex;
        }
        ++currentNumberOfSetBits;
    }
    while (lastIndex < this->size()) {
        bitsSetBeforeIndices.push_back(currentNumberOfSetBits);
        ++lastIndex;
    }
    return bitsSetBeforeIndices;
}

bool BitVector::hasUniqueSetBit() const {
    return getNumberOfSetBits() == 1;
}

size_t BitVector::size() const {
    return static_cast<size_t>(bitCount);
}

std::size_t BitVector::getSizeInBytes() const {
    return sizeof(*this) + sizeof(uint64_t) * bucketCount();
}

size_t BitVector::bucketCount() const {
    size_t result = (bitCount >> 6);
    if ((bitCount & mod64mask) != 0) {
        ++result;
    }
    return result;
}

void BitVector::setBucket(uint64_t bucketIndex, uint64_t value) {
    STORM_LOG_ASSERT(bucketIndex < bucketCount(), "Invalid call to BitVector::setBucket: bucket index " << bucketIndex << " out of bounds.");
    buckets[bucketIndex] = value;
    if (bucketIndex == bucketCount() - 1) {
        truncateLastBucket();
    }
}

uint64_t BitVector::getBucket(uint64_t bucketIndex) const {
    STORM_LOG_ASSERT(bucketIndex < bucketCount(), "Invalid call to BitVector::getBucket: bucket index " << bucketIndex << " out of bounds.");
    STORM_LOG_ASSERT(bucketIndex < bucketCount() - 1 || (bitCount & mod64mask) == 0ull || (buckets[bucketIndex] << (bitCount & mod64mask)) == 0ull,
                     "Bitvector in invalid state: last bucket contains bits beyond bitCount.");
    if (bucketIndex == bucketCount() - 1) {
        return buckets[bucketIndex] & ~((1ll << (64 - (bitCount & mod64mask))) - 1ll);
    }
    return buckets[bucketIndex];
}

BitVector::const_iterator BitVector::begin() const {
    return const_iterator(buckets, 0, bitCount);
}

BitVector::const_iterator BitVector::begin(uint64_t lowerBound) const {
    return const_iterator(buckets, lowerBound, bitCount);
}

BitVector::const_iterator BitVector::end() const {
    return const_iterator(buckets, bitCount, bitCount, false);
}

BitVector::const_reverse_iterator BitVector::rbegin() const {
    return const_reverse_iterator(buckets, bitCount);
}
BitVector::const_reverse_iterator BitVector::rbegin(uint64_t upperBound) const {
    return const_reverse_iterator(buckets, upperBound);
}

BitVector::const_reverse_iterator BitVector::rend() const {
    return const_reverse_iterator(buckets, 0ull, 0ull, false);
}

uint64_t BitVector::getNextSetIndex(uint64_t startingIndex) const {
    return getNextIndexWithValue<true>(buckets, startingIndex, bitCount);
}

uint64_t BitVector::getNextUnsetIndex(uint64_t startingIndex) const {
#ifdef ASSERT_BITVECTOR
    STORM_LOG_ASSERT(getNextIndexWithValue<false>(buckets, startingIndex, bitCount) == (~(*this)).getNextSetIndex(startingIndex),
                     "The result is inconsistent with the next set index of the complement of this bitvector");
#endif
    return getNextIndexWithValue<false>(buckets, startingIndex, bitCount);
}

uint64_t BitVector::getStartOfZeroSequenceBefore(uint64_t endIndex) const {
    return getNextIndexWithValue<true, true>(buckets, 0, endIndex);
}

uint64_t BitVector::getStartOfOneSequenceBefore(uint64_t endIndex) const {
#ifdef ASSERT_BITVECTOR
    STORM_LOG_ASSERT((getNextIndexWithValue<false, true>(buckets, 0, endIndex) == (~(*this)).getStartOfZeroSequenceBefore(endIndex)),
                     "The result is inconsistent with the next set index of the complement of this bitvector");
#endif
    return getNextIndexWithValue<false, true>(buckets, 0, endIndex);
}

template<bool Value, bool Backward>
uint64_t BitVector::getNextIndexWithValue(uint64_t const* dataPtr, uint64_t startingIndex, uint64_t endIndex) {
    if (startingIndex >= endIndex) {
        return Backward ? startingIndex : endIndex;
    }

    uint64_t currentBucketIndexOffset = Backward ? endIndex - 1 : startingIndex;
    uint_fast8_t currentBitInBucket = currentBucketIndexOffset & mod64mask;
    uint64_t const* bucketIt = dataPtr + (currentBucketIndexOffset >> 6);
    currentBucketIndexOffset = (currentBucketIndexOffset >> 6 << 6);

    // Get relevant contents of the first bucket (the one that contains the bit with index currentBucketIndexOffset + currentBitInBucket)
    uint64_t relevantBitsInBucket;
    if constexpr (Backward) {
        relevantBitsInBucket = -1ull << (63 - currentBitInBucket);  // 111..111'1'000...000 where the last '1' is at the currentBitInBucket
    } else {
        relevantBitsInBucket = -1ull >> currentBitInBucket;  // 000..000'1'111..111 where the first '1' is at the currentBitInBucket
    }
    uint64_t currentBucket = Value ? (*bucketIt & relevantBitsInBucket) : (*bucketIt | ~relevantBitsInBucket);

    // Find the right bucket
    if (currentBucket == (Value ? 0ull : -1ull)) {
        // The first bucket does not contain a bit with the desired value...
        do {
            // Move to next bucket (if there is some)
            if constexpr (Backward) {
                if (currentBucketIndexOffset <= startingIndex) {
                    // No bucket found!
                    return startingIndex;
                }
                --bucketIt;
                currentBucketIndexOffset -= 64;  // does not underflow:  currentBucketIndexOffset is greater than startIndex and always a multiple of 64
            } else {
                ++bucketIt;
                currentBucketIndexOffset += 64;
                if (currentBucketIndexOffset >= endIndex) {
                    // No bucket found!
                    return endIndex;
                }
            }
            // Check if the bucket contains our bit
        } while ((*bucketIt) == (Value ? 0ull : -1ull));
        // At this point we have found our bucket, but it is not the first one
        currentBucket = *bucketIt;
        currentBitInBucket = Backward ? 63u : 0u;  // search within the bucket starting at the last or at the first bit
    }

    if constexpr (!Value) {
        currentBucket = ~currentBucket;  // invert so that we always search for a '1' from this point
    }
    // At this point, currentBucket definitely contains a 1-bit and all bits (Backward ? after : before) the currentBitInBucket are zero
    STORM_LOG_ASSERT(currentBucket != 0ull, "Bitvector's getNextIndexWithValue method in invalid state.");

#if (defined(__GNUG__) || defined(__clang__))
    // Use fast and easy builtin functions to find the correct bit index
    if constexpr (Backward) {
        // take max since the startIndex might point somewhere into the current bucket so the found bit might come before the startIndex
        return std::max<uint64_t>(startingIndex,
                                  currentBucketIndexOffset + 64ull - __builtin_ctzll(currentBucket));  // make sure to return +1 index after the found 1
    } else {
        // take min since the endIndex might point somewhere into the current bucket so the found bit might come after the endIndex
        return std::min<uint64_t>(endIndex, currentBucketIndexOffset + __builtin_clzll(currentBucket));
    }
#else
    // Find the correct bit index manually
    uint64_t compareMask = 1ull << (63 - currentBitInBucket);  // 000..000'1'000..000 with '1' at currentBitInBucket position
    while (!static_cast<bool>(currentBucket & compareMask)) {
        if constexpr (Backward) {
            compareMask <<= 1ull;
            --currentBitInBucket;
        } else {
            compareMask >>= 1ull;
            ++currentBitInBucket;
        }
    }
    if constexpr (Backward) {
        return std::max(startingIndex, currentBucketIndexOffset + currentBitInBucket + 1ull);  // make sure to return +1 index after the found 1
    } else {
        return std::min(endIndex, currentBucketIndexOffset + currentBitInBucket);
    }
#endif
}

storm::storage::BitVector BitVector::getAsBitVector(uint64_t start, uint64_t length) const {
    STORM_LOG_ASSERT(start + length <= bitCount, "Invalid range.");
#ifdef ASSERT_BITVECTOR
    BitVector original(*this);
#endif
    storm::storage::BitVector result(length, false);

    uint64_t offset = start % 64;
    uint64_t* getBucket = buckets + (start / 64);
    uint64_t* insertBucket = result.buckets;
    uint64_t getValue;
    uint64_t writeValue = 0;
    uint64_t noBits = 0;
    if (offset == 0) {
        // Copy complete buckets
        for (; noBits + 64 <= length; ++getBucket, ++insertBucket, noBits += 64) {
            *insertBucket = *getBucket;
        }
    } else {
        // Get first bits up until next bucket
        getValue = *getBucket;
        writeValue = (getValue << offset);
        noBits += (64 - offset);
        ++getBucket;

        // Get complete buckets
        for (; noBits + 64 <= length; ++getBucket, ++insertBucket, noBits += 64) {
            getValue = *getBucket;
            // Get bits till write bucket is full
            writeValue |= (getValue >> (64 - offset));
            *insertBucket = writeValue;
            // Get bits up until next bucket
            writeValue = (getValue << offset);
        }
    }

    // Write last bits
    uint64_t remainingBits = length - noBits;
    STORM_LOG_ASSERT(getBucket != buckets + bucketCount(), "Bucket index incorrect.");
    // Get remaining bits
    getValue = (*getBucket >> (64 - remainingBits)) << (64 - remainingBits);
    STORM_LOG_ASSERT(remainingBits < 64, "Too many remaining bits.");
    // Write bucket
    STORM_LOG_ASSERT(insertBucket != result.buckets + result.bucketCount(), "Bucket index incorrect.");
    if (offset == 0) {
        *insertBucket = getValue;
    } else {
        writeValue |= getValue >> (64 - offset);
        *insertBucket = writeValue;
        if (remainingBits > offset) {
            // Write last bits in new value
            writeValue = (getValue << offset);
            ++insertBucket;
            STORM_LOG_ASSERT(insertBucket != result.buckets + result.bucketCount(), "Bucket index incorrect.");
            *insertBucket = writeValue;
        }
    }

#ifdef ASSERT_BITVECTOR
    // Check correctness of getter
    for (uint64_t i = 0; i < length; ++i) {
        if (result.get(i) != get(start + i)) {
            STORM_LOG_ERROR("Getting of bits not correct for index " << i);
            STORM_LOG_ERROR("Getting from " << start << " with length " << length);
            std::stringstream stream;
            printBits(stream);
            stream << '\n';
            result.printBits(stream);
            STORM_LOG_ERROR(stream.str());
            STORM_LOG_ASSERT(false, "Getting of bits not correct.");
        }
    }
    for (uint64_t i = 0; i < bitCount; ++i) {
        if (i < start || i >= start + length) {
            if (original.get(i) != get(i)) {
                STORM_LOG_ERROR("Getting did change bitvector at index " << i);
                STORM_LOG_ERROR("Getting from " << start << " with length " << length);
                std::stringstream stream;
                printBits(stream);
                stream << '\n';
                original.printBits(stream);
                STORM_LOG_ERROR(stream.str());
                STORM_LOG_ASSERT(false, "Getting of bits not correct.");
            }
        }
    }

#endif
    return result;
}

void BitVector::setFromBitVector(uint64_t start, BitVector const& other) {
#ifdef ASSERT_BITVECTOR
    BitVector original(*this);
#endif
    STORM_LOG_ASSERT(start + other.bitCount <= bitCount, "Range invalid.");

    uint64_t offset = start % 64;
    uint64_t* insertBucket = buckets + (start / 64);
    uint64_t* getBucket = other.buckets;
    uint64_t getValue;
    uint64_t writeValue = 0;
    uint64_t noBits = 0;
    if (offset == 0) {
        // Copy complete buckets
        for (; noBits + 64 <= other.bitCount; ++insertBucket, ++getBucket, noBits += 64) {
            *insertBucket = *getBucket;
        }
    } else {
        // Get first bits up until next bucket
        getValue = *getBucket;
        writeValue = (*insertBucket >> (64 - offset)) << (64 - offset);
        writeValue |= (getValue >> offset);
        *insertBucket = writeValue;
        noBits += (64 - offset);
        ++insertBucket;

        // Get complete buckets
        for (; noBits + 64 <= other.bitCount; ++insertBucket, noBits += 64) {
            // Get all remaining bits from other bucket
            writeValue = getValue << (64 - offset);
            // Get bits from next bucket
            ++getBucket;
            getValue = *getBucket;
            writeValue |= getValue >> offset;
            *insertBucket = writeValue;
        }
    }

    // Write last bits
    uint64_t remainingBits = other.bitCount - noBits;
    STORM_LOG_ASSERT(remainingBits < 64, "Too many remaining bits.");
    STORM_LOG_ASSERT(insertBucket != buckets + bucketCount(), "Bucket index incorrect.");
    STORM_LOG_ASSERT(getBucket != other.buckets + other.bucketCount(), "Bucket index incorrect.");
    // Get remaining bits of bucket
    getValue = *getBucket;
    if (offset > 0) {
        getValue = getValue << (64 - offset);
    }
    // Get unchanged part of bucket
    writeValue = (*insertBucket << remainingBits) >> remainingBits;
    if (remainingBits > offset && offset > 0) {
        // Remaining bits do not come from one bucket -> consider next bucket
        ++getBucket;
        STORM_LOG_ASSERT(getBucket != other.buckets + other.bucketCount(), "Bucket index incorrect.");
        getValue |= *getBucket >> offset;
    }
    // Write completely
    writeValue |= getValue;
    *insertBucket = writeValue;

#ifdef ASSERT_BITVECTOR
    // Check correctness of setter
    for (uint64_t i = 0; i < other.bitCount; ++i) {
        if (other.get(i) != get(start + i)) {
            STORM_LOG_ERROR("Setting of bits not correct for index " << i);
            STORM_LOG_ERROR("Setting from " << start << " with length " << other.bitCount);
            std::stringstream stream;
            printBits(stream);
            stream << '\n';
            other.printBits(stream);
            STORM_LOG_ERROR(stream.str());
            STORM_LOG_ASSERT(false, "Setting of bits not correct.");
        }
    }
    for (uint64_t i = 0; i < bitCount; ++i) {
        if (i < start || i >= start + other.bitCount) {
            if (original.get(i) != get(i)) {
                STORM_LOG_ERROR("Setting did change bitvector at index " << i);
                STORM_LOG_ERROR("Setting from " << start << " with length " << other.bitCount);
                std::stringstream stream;
                printBits(stream);
                stream << '\n';
                original.printBits(stream);
                STORM_LOG_ERROR(stream.str());
                STORM_LOG_ASSERT(false, "Setting of bits not correct.");
            }
        }
    }
#endif
}

bool BitVector::compareAndSwap(uint64_t start1, uint64_t start2, uint64_t length) {
    if (length < 64) {
        // Just use one number
        uint64_t elem1 = getAsInt(start1, length);
        uint64_t elem2 = getAsInt(start2, length);
        if (elem1 < elem2) {
            // Swap elements
            setFromInt(start1, length, elem2);
            setFromInt(start2, length, elem1);
            return true;
        }
        return false;
    } else {
        // Use bit vectors
        BitVector elem1 = getAsBitVector(start1, length);
        BitVector elem2 = getAsBitVector(start2, length);

        if (!(elem1 < elem2)) {
            // Elements already sorted
#ifdef ASSERT_BITVECTOR
            // Check that sorted
            for (uint64_t i = 0; i < length; ++i) {
                if (get(start1 + i) > get(start2 + i)) {
                    break;
                }
                STORM_LOG_ASSERT(get(start1 + i) >= get(start2 + i), "Bit vector not sorted for indices " << start1 + i << " and " << start2 + i);
            }
#endif
            return false;
        }

#ifdef ASSERT_BITVECTOR
        BitVector check(*this);
#endif

        // Swap elements
        setFromBitVector(start1, elem2);
        setFromBitVector(start2, elem1);

#ifdef ASSERT_BITVECTOR
        // Check correctness of swapping
        bool tmp;
        for (uint64_t i = 0; i < length; ++i) {
            tmp = check.get(i + start1);
            check.set(i + start1, check.get(i + start2));
            check.set(i + start2, tmp);
        }
        STORM_LOG_ASSERT(*this == check, "Swapping not correct");

        // Check that sorted
        for (uint64_t i = 0; i < length; ++i) {
            if (get(start1 + i) > get(start2 + i)) {
                break;
            }
            STORM_LOG_ASSERT(get(start1 + i) >= get(start2 + i), "Bit vector not sorted for indices " << start1 + i << " and " << start2 + i);
        }
#endif

        return true;
    }
}

void BitVector::truncateLastBucket() {
    if ((bitCount & mod64mask) != 0) {
        buckets[bucketCount() - 1] &= ~((1ll << (64 - (bitCount & mod64mask))) - 1ll);
    }
}

std::ostream& operator<<(std::ostream& out, BitVector const& bitvector) {
    out << "bit vector(" << bitvector.getNumberOfSetBits() << "/" << bitvector.bitCount << ") [";
    for (auto index : bitvector) {
        out << index << " ";
    }
    out << "]";

    return out;
}

void BitVector::printBits(std::ostream& out) const {
    out << "bit vector(" << getNumberOfSetBits() << "/" << bitCount << ") ";
    uint64_t index = 0;
    for (; index * 64 + 64 <= bitCount; ++index) {
        std::bitset<64> tmp(buckets[index]);
        out << tmp << "|";
    }

    // Print last bits
    if (index * 64 < bitCount) {
        STORM_LOG_ASSERT(index == bucketCount() - 1, "Not last bucket.");
        std::bitset<64> tmp(buckets[index]);
        for (size_t i = 0; i + index * 64 < bitCount; ++i) {
            // Bits are counted from rightmost in bitset
            out << tmp[63 - i];
        }
    }
    out << '\n';
}

std::size_t FNV1aBitVectorHash::operator()(storm::storage::BitVector const& bv) const {
    std::size_t seed = 14695981039346656037ull;

    uint8_t* it = reinterpret_cast<uint8_t*>(bv.buckets);
    uint8_t const* ite = it + 8 * bv.bucketCount();

    while (it < ite) {
        seed ^= *it++;

        // Multiplication with magic prime.
        seed += (seed << 1) + (seed << 4) + (seed << 5) + (seed << 7) + (seed << 8) + (seed << 40);
    }

    return seed;
}

inline __attribute__((always_inline)) uint32_t fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

inline __attribute__((always_inline)) uint64_t fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdull;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ull;
    k ^= k >> 33;

    return k;
}

inline uint32_t rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

inline uint64_t rotl64(uint64_t x, int8_t r) {
    return (x << r) | (x >> (64 - r));
}

inline __attribute__((always_inline)) uint32_t getblock32(uint32_t const* p, int i) {
    return p[i];
}

inline __attribute__((always_inline)) uint32_t getblock64(uint64_t const* p, int i) {
    return p[i];
}

template<>
uint32_t Murmur3BitVectorHash<uint32_t>::operator()(storm::storage::BitVector const& bv) const {
    uint8_t const* data = reinterpret_cast<uint8_t const*>(bv.buckets);
    uint32_t len = bv.bucketCount() * 8;
    const int nblocks = bv.bucketCount() * 2;

    // Using 0 as seed.
    uint32_t h1 = 0;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t* blocks = reinterpret_cast<uint32_t const*>(data + nblocks * 4);

    for (int i = -nblocks; i; i++) {
        uint32_t k1 = getblock32(blocks, i);

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    //----------
    // finalization

    h1 ^= len;

    h1 = fmix32(h1);

    return h1;
}

template<>
uint64_t Murmur3BitVectorHash<uint64_t>::operator()(storm::storage::BitVector const& bv) const {
    uint8_t const* data = reinterpret_cast<uint8_t const*>(bv.buckets);
    uint64_t len = bv.bucketCount() * 8;
    const int nblocks = bv.bucketCount() / 2;

    uint64_t h1 = 0;
    uint64_t h2 = 0;

    const uint64_t c1 = 0x87c37b91114253d5ull;
    const uint64_t c2 = 0x4cf5ad432745937full;

    //----------
    // body

    const uint64_t* blocks = (const uint64_t*)(data);

    for (int i = 0; i < nblocks; i++) {
        uint64_t k1 = getblock64(blocks, i * 2 + 0);
        uint64_t k2 = getblock64(blocks, i * 2 + 1);

        k1 *= c1;
        k1 = rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = rotl64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = rotl64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = rotl64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    //----------
    // tail

    uint8_t const* tail = reinterpret_cast<uint8_t const*>(data + nblocks * 16);

    uint64_t k1 = 0;
    uint64_t k2 = 0;
    // Loop unrolling via Duff's device
    // Cases are supposed to fall-through
    switch (len & 15) {
        case 15:
            k2 ^= ((uint64_t)tail[14]) << 48;
            // fallthrough
        case 14:
            k2 ^= ((uint64_t)tail[13]) << 40;
            // fallthrough
        case 13:
            k2 ^= ((uint64_t)tail[12]) << 32;
            // fallthrough
        case 12:
            k2 ^= ((uint64_t)tail[11]) << 24;
            // fallthrough
        case 11:
            k2 ^= ((uint64_t)tail[10]) << 16;
            // fallthrough
        case 10:
            k2 ^= ((uint64_t)tail[9]) << 8;
            // fallthrough
        case 9:
            k2 ^= ((uint64_t)tail[8]) << 0;
            k2 *= c2;
            k2 = rotl64(k2, 33);
            k2 *= c1;
            h2 ^= k2;
            // fallthrough

        case 8:
            k1 ^= ((uint64_t)tail[7]) << 56;
            // fallthrough
        case 7:
            k1 ^= ((uint64_t)tail[6]) << 48;
            // fallthrough
        case 6:
            k1 ^= ((uint64_t)tail[5]) << 40;
            // fallthrough
        case 5:
            k1 ^= ((uint64_t)tail[4]) << 32;
            // fallthrough
        case 4:
            k1 ^= ((uint64_t)tail[3]) << 24;
            // fallthrough
        case 3:
            k1 ^= ((uint64_t)tail[2]) << 16;
            // fallthrough
        case 2:
            k1 ^= ((uint64_t)tail[1]) << 8;
            // fallthrough
        case 1:
            k1 ^= ((uint64_t)tail[0]) << 0;
            // fallthrough
            k1 *= c1;
            k1 = rotl64(k1, 31);
            k1 *= c2;
            h1 ^= k1;
    }

    //----------
    // finalization

    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    return h1 ^ h2;
}

void BitVector::store(std::ostream& os) const {
    os << bitCount;
    for (uint64_t i = 0; i < bucketCount(); ++i) {
        os << " " << buckets[i];
    }
}

BitVector BitVector::load(std::string const& description) {
    std::vector<std::string> splitted;
    std::stringstream ss(description);
    ss >> std::noskipws;
    std::string field;
    char ws_delim;
    while (true) {
        if (ss >> field)
            splitted.push_back(field);
        else if (ss.eof())
            break;
        else
            splitted.push_back(std::string());
        ss.clear();
        ss >> ws_delim;
    }
    BitVector bv(std::stoul(splitted[0]));
    for (uint64_t i = 0; i < splitted.size() - 1; ++i) {
        bv.buckets[i] = std::stoul(splitted[i + 1]);
    }
    return bv;
}

// All necessary explicit template instantiations.
template BitVector::BitVector(uint64_t length, std::vector<uint64_t>::iterator begin, std::vector<uint64_t>::iterator end);
template BitVector::BitVector(uint64_t length, std::vector<uint64_t>::const_iterator begin, std::vector<uint64_t>::const_iterator end);
template BitVector::BitVector(uint64_t length, storm::storage::FlatSet<uint64_t>::iterator begin, storm::storage::FlatSet<uint64_t>::iterator end);
template BitVector::BitVector(uint64_t length, storm::storage::FlatSet<uint64_t>::const_iterator begin, storm::storage::FlatSet<uint64_t>::const_iterator end);
template void BitVector::set(std::vector<uint64_t>::iterator begin, std::vector<uint64_t>::iterator end, bool value);
template void BitVector::set(std::vector<uint64_t>::const_iterator begin, std::vector<uint64_t>::const_iterator end, bool value);
template void BitVector::set(storm::storage::FlatSet<uint64_t>::iterator begin, storm::storage::FlatSet<uint64_t>::iterator end, bool value);
template void BitVector::set(storm::storage::FlatSet<uint64_t>::const_iterator begin, storm::storage::FlatSet<uint64_t>::const_iterator end, bool value);

template struct Murmur3BitVectorHash<uint32_t>;
template struct Murmur3BitVectorHash<uint64_t>;
}  // namespace storage
}  // namespace storm

namespace std {
std::size_t hash<storm::storage::BitVector>::operator()(storm::storage::BitVector const& bitvector) const {
    return boost::hash_range(bitvector.buckets, bitvector.buckets + bitvector.bucketCount());
}
}  // namespace std
