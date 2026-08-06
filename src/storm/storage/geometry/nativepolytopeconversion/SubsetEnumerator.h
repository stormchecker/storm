#pragma once

#include <cstdint>
#include <vector>

namespace storm {
namespace storage {
namespace geometry {
/*!
 * This class can be used to enumerate all k-sized subsets of {0,...,n-1}.
 * A subset is represented as a vector of ascending numbers.
 * Example: (n=5, k=3)
 * [0,1,2] --> [0,1,3] --> [0,1,4] --> [0,2,3] --> [0,2,4] --> [0,3,4] -->
 * [1,2,3] --> [1,2,4] --> [1,3,4] --> [2,3,4]
 * A filter can be given which should answer true iff it is ok to add a given
 * item to a given subset.
 * Example: (n=5, k=3, filter answers false iff 0 and 2 would be in the
 * resulting subset):
 * [0,1,3] --> [0,1,4] --> [0,3,4] --> [1,2,3] --> [1,2,4] --> [1,3,4] --> [2,3,4]
 */
template<typename DataType = std::nullptr_t>
class SubsetEnumerator {
   public:
    // A typedef for the filter.
    // Note that the function will be called with subset.size() in {0, ..., k-1}.
    typedef bool (*SubsetFilter)(std::vector<uint_fast64_t> const& subset, uint_fast64_t const& item, DataType const& data);

    /*
     * Constructs a subset enumerator that can enumerate all k-sized Subsets of {0,...,n-1}
     * The given filter can be used to skip certain subsets.
     * @note call "setToFirstSubset()" before retrieving the first subset
     */
    SubsetEnumerator(uint_fast64_t n, uint_fast64_t k, DataType const& data = DataType(), SubsetFilter subsetFilter = trueFilter);

    ~SubsetEnumerator();

    // returns the current subset of size k.
    // Arbitrary behavior if setToFirstSubset or incrementSubset returned false or have never been executed
    std::vector<uint_fast64_t> const& getCurrentSubset();

    // Sets the current subset to the very first one.
    // @note Needs to be called initially.
    // Returns true iff there actually is a first subset and false if not (e.g. when n<k or the filter answers false in all cases).
    bool setToFirstSubset();

    // Increments the current subset.
    // Returns true if there is a new subset and false if the current subset is already the last one.
    bool incrementSubset();

    // Default filter that returns always true.
    static bool trueFilter(std::vector<uint_fast64_t> const& subset, uint_fast64_t const& item, DataType const& data);

   private:
    uint_fast64_t n;                             // the size of the source set
    uint_fast64_t k;                             // the size of the desired subsets
    DataType const& data;                        // The data which is given as additional information when invoking the filter
    SubsetFilter filter;                         // returns true iff it is okay to insert a new element
    std::vector<uint_fast64_t> current;          // the current subset
    std::vector<uint_fast64_t> upperBoundaries;  // will always be [n-k, ..., n-1]. Used to easily check whether we can increment the subset at a given position
};
}  // namespace geometry
}  // namespace storage
}  // namespace storm
