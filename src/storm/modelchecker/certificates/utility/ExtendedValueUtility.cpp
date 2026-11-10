#include "storm/modelchecker/certificates/utility/ExtendedValueUtility.h"

#include <type_traits>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/constants.h"

namespace storm::modelchecker::detail {

template<typename ValueType, typename VectorValueType>
VectorValueType multiplyRowWithVectorAddNumber(storm::storage::SparseMatrix<ValueType> const& matrix, uint64_t rowIndex,
                                               std::vector<VectorValueType> const& vector, ValueType const& offset) {
    ValueType result{offset};
    for (auto const& entry : matrix.getRow(rowIndex)) {
        if (storm::utility::isZero(entry.getValue())) {
            continue;
        }
        auto const& successorValue = vector[entry.getColumn()];
        if constexpr (std::is_same_v<VectorValueType, storm::utility::ExtendedValueType<ValueType>>) {
            if (!storm::utility::isFinite(successorValue)) {
                return storm::utility::positiveInfinity<ValueType>();
            }
            result += storm::utility::getFinite(successorValue) * entry.getValue();
        } else {
            result += successorValue * entry.getValue();
        }
    }
    return result;
}

template double multiplyRowWithVectorAddNumber(storm::storage::SparseMatrix<double> const&, uint64_t, std::vector<double> const&, double const&);
template storm::RationalNumber multiplyRowWithVectorAddNumber(storm::storage::SparseMatrix<storm::RationalNumber> const&, uint64_t,
                                                              std::vector<storm::RationalNumber> const&, storm::RationalNumber const&);
template storm::ExtendedRationalNumber multiplyRowWithVectorAddNumber(storm::storage::SparseMatrix<storm::RationalNumber> const&, uint64_t,
                                                                      std::vector<storm::ExtendedRationalNumber> const&, storm::RationalNumber const&);

}  // namespace storm::modelchecker::detail
