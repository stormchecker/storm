#include "SubsetEnumerator.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/eigen.h"

namespace storm {
namespace storage {
namespace geometry {

template<typename DataType>
SubsetEnumerator<DataType>::SubsetEnumerator(uint_fast64_t n, uint_fast64_t k, DataType const& data, SubsetFilter subsetFilter)
    : n(n), k(k), data(data), filter(subsetFilter) {
    // Intentionally left empty
}

template<typename DataType>
SubsetEnumerator<DataType>::~SubsetEnumerator() {
    // Intentionally left empty
}

template<typename DataType>
std::vector<uint_fast64_t> const& SubsetEnumerator<DataType>::getCurrentSubset() {
    return this->current;
}

template<typename DataType>
bool SubsetEnumerator<DataType>::setToFirstSubset() {
    if (n < k) {
        return false;
    }
    // set the upper boundaries first.
    upperBoundaries.clear();
    upperBoundaries.reserve(k);
    for (uint_fast64_t bound = (n - k); bound < n; ++bound) {
        upperBoundaries.push_back(bound);
    }
    // now set the current subset to the very first one
    current.clear();
    current.reserve(k);
    uint_fast64_t newItem = 0;
    while (current.size() != k && newItem <= upperBoundaries[current.size()]) {
        // Check if it is okay to add the new item...
        if (filter(current, newItem, data)) {
            current.push_back(newItem);
        }
        ++newItem;
    }
    // Note that we only insert things into the vector if it is "okay" to do so.
    // Hence, we have failed iff we were not able to insert k elements.
    return current.size() == k;
}

template<typename DataType>
bool SubsetEnumerator<DataType>::incrementSubset() {
    // The currentSelection will be the numbers that are already inside of our new subset.
    std::vector<uint_fast64_t> currentSelection(current);
    currentSelection.pop_back();
    uint_fast64_t pos = k - 1;
    while (true) {
        // check whether we can increment at the current position
        if (current[pos] == upperBoundaries[pos]) {
            if (pos == 0) {
                // we already moved to the very left and still can not increment.
                // Hence, we are already at the last subset
                return false;
            }
            currentSelection.pop_back();
            --pos;
        } else {
            ++current[pos];
            // check if the new subset is inside our filter
            if (filter(currentSelection, current[pos], data)) {
                // it is, so add it and go on with the position on the right
                currentSelection.push_back(current[pos]);
                ++pos;
                if (pos == k) {
                    // we are already at the very right.
                    // Hence, we have found our new subset of size k
                    return true;
                }
                // initialize the value at the new position
                current[pos] = current[pos - 1];
            }
        }
    }
}

template<typename DataType>
bool SubsetEnumerator<DataType>::trueFilter(std::vector<uint_fast64_t> const&, uint_fast64_t const&, DataType const&) {
    return true;
}

template class SubsetEnumerator<std::nullptr_t>;
template class SubsetEnumerator<std::vector<Eigen::Matrix<double, Eigen::Dynamic, 1>>>;
template class SubsetEnumerator<std::vector<Eigen::Matrix<storm::RationalNumber, Eigen::Dynamic, 1>>>;
template class SubsetEnumerator<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>>;
template class SubsetEnumerator<Eigen::Matrix<storm::RationalNumber, Eigen::Dynamic, Eigen::Dynamic>>;
}  // namespace geometry
}  // namespace storage
}  // namespace storm
