#pragma once

#include <memory>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/adapters/eigen.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
namespace adapters {

class EigenAdapter {
   public:
    /*!
     * Converts a sparse matrix into a sparse matrix in the gmm++ format.
     * @return A pointer to a row-major sparse matrix in gmm++ format.
     */
    template<class ValueType>
    static std::unique_ptr<Eigen::SparseMatrix<ValueType>> toEigenSparseMatrix(storm::storage::SparseMatrix<ValueType> const& matrix);

    template<typename ValueType>
    static std::vector<ValueType> toStdVector(Eigen::Matrix<ValueType, Eigen::Dynamic, 1> const& v);

    template<typename ValueType>
    static Eigen::Matrix<ValueType, Eigen::Dynamic, 1> toEigenVector(std::vector<ValueType> const& v);
};

}  // namespace adapters
}  // namespace storm

namespace std {
template<class ValueType>
struct hash<Eigen::Matrix<ValueType, Eigen::Dynamic, 1>> {
    std::size_t operator()(Eigen::Matrix<ValueType, Eigen::Dynamic, 1> const& vector) const {
        size_t seed = 0;
        for (uint_fast64_t i = 0; i < static_cast<uint_fast64_t>(vector.rows()); ++i) {
            carl::hash_add(seed, std::hash<ValueType>()(vector(i)));
        }
        return seed;
    }
};
}  // namespace std

namespace Eigen {
template<>
struct NumTraits<storm::RationalNumber> : GenericNumTraits<storm::RationalNumber> {
    typedef storm::RationalNumber Real;
    typedef storm::RationalNumber NonInteger;
    typedef storm::RationalNumber Nested;
    static inline Real epsilon() {
        return 0;
    }
    static inline Real dummy_precision() {
        return 0;
    }
    static inline int digits10() {
        return 0;
    }
    enum { IsInteger = 0, IsSigned = 1, IsComplex = 0, RequireInitialization = 1, ReadCost = 6, AddCost = 150, MulCost = 100 };
};
}  // namespace Eigen
