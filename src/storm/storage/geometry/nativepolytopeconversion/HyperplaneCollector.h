#pragma once

#include <unordered_map>

#include "storm/adapters/EigenAdapter.h"

namespace storm {
namespace storage {
namespace geometry {
/*!
 * This class can be used to collect a set of hyperplanes (without duplicates).
 * The inserted hyperplanes are normalized, i.e. devided by the infinity norm of the normal vector
 */
template<typename ValueType>
class HyperplaneCollector {
   public:
    typedef Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic> EigenMatrix;
    typedef Eigen::Matrix<ValueType, Eigen::Dynamic, 1> EigenVector;

    HyperplaneCollector() = default;
    HyperplaneCollector(const HyperplaneCollector& orig) = default;
    virtual ~HyperplaneCollector() = default;

    /*
     * inserts the given hyperplane.
     * For every (unique) hyperplane, there is a list of indices which can be used e.g. to obtain the set of vertices that lie on each hyperplane.
     * If indexList is given (i.e. not nullptr), the given indices are appended to that list.
     * Returns true iff the hyperplane was inserted (i.e. the hyperplane was not already contained in this)
     */
    bool insert(EigenVector const& normal, ValueType const& offset, std::vector<uint_fast64_t> const* indexList = nullptr);
    bool insert(EigenVector&& normal, ValueType&& offset, std::vector<uint_fast64_t> const* indexList = nullptr);

    std::pair<EigenMatrix, EigenVector> getCollectedHyperplanesAsMatrixVector() const;
    // Note that the returned lists might contain dublicates.
    std::vector<std::vector<uint_fast64_t>> getIndexLists() const;

    uint_fast64_t numOfCollectedHyperplanes() const;

   private:
    typedef std::pair<EigenVector, ValueType> NormalOffset;
    class NormalOffsetHash {
       public:
        std::size_t operator()(NormalOffset const& ns) const {
            std::size_t seed = std::hash<EigenVector>()(ns.first);
            carl::hash_add(seed, std::hash<ValueType>()(ns.second));
            return seed;
        }
    };
    typedef typename std::unordered_map<NormalOffset, std::vector<uint_fast64_t>, NormalOffsetHash>::key_type MapKeyType;
    typedef typename std::unordered_map<NormalOffset, std::vector<uint_fast64_t>, NormalOffsetHash>::value_type MapValueType;

    std::unordered_map<NormalOffset, std::vector<uint_fast64_t>, NormalOffsetHash> map;
};
}  // namespace geometry
}  // namespace storage
}  // namespace storm
