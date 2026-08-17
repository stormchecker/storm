#include "storm/adapters/EigenAdapter.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm {
namespace adapters {

template<typename ValueType>
std::unique_ptr<Eigen::SparseMatrix<ValueType>> EigenAdapter::toEigenSparseMatrix(storm::storage::SparseMatrix<ValueType> const& matrix) {
    // Build a list of triplets and let Eigen care about the insertion.
    std::vector<Eigen::Triplet<ValueType>> triplets;
    triplets.reserve(matrix.getNonzeroEntryCount());

    for (uint64_t row = 0; row < matrix.getRowCount(); ++row) {
        for (auto const& element : matrix.getRow(row)) {
            triplets.emplace_back(row, element.getColumn(), element.getValue());
        }
    }

    std::unique_ptr<Eigen::SparseMatrix<ValueType>> result = std::make_unique<Eigen::SparseMatrix<ValueType>>(matrix.getRowCount(), matrix.getColumnCount());
    result->setFromTriplets(triplets.begin(), triplets.end());
    return result;
}

template<typename ValueType>
std::vector<ValueType> EigenAdapter::toStdVector(Eigen::Matrix<ValueType, Eigen::Dynamic, 1> const& v) {
    return std::vector<ValueType>(v.data(), v.data() + v.rows());
}

template<typename ValueType>
Eigen::Matrix<ValueType, Eigen::Dynamic, 1> EigenAdapter::toEigenVector(std::vector<ValueType> const& v) {
    return Eigen::Matrix<ValueType, Eigen::Dynamic, 1>::Map(v.data(), v.size());
}

template std::unique_ptr<Eigen::SparseMatrix<double>> EigenAdapter::toEigenSparseMatrix(storm::storage::SparseMatrix<double> const& matrix);
template std::vector<double> EigenAdapter::toStdVector(Eigen::Matrix<double, Eigen::Dynamic, 1> const& v);
template Eigen::Matrix<double, Eigen::Dynamic, 1> EigenAdapter::toEigenVector(std::vector<double> const& v);

template std::unique_ptr<Eigen::SparseMatrix<storm::RationalNumber>> EigenAdapter::toEigenSparseMatrix(
    storm::storage::SparseMatrix<storm::RationalNumber> const& matrix);
template std::vector<storm::RationalNumber> EigenAdapter::toStdVector(Eigen::Matrix<storm::RationalNumber, Eigen::Dynamic, 1> const& v);
template Eigen::Matrix<storm::RationalNumber, Eigen::Dynamic, 1> EigenAdapter::toEigenVector(std::vector<storm::RationalNumber> const& v);

template std::unique_ptr<Eigen::SparseMatrix<storm::RationalFunction>> EigenAdapter::toEigenSparseMatrix(
    storm::storage::SparseMatrix<storm::RationalFunction> const& matrix);
template std::vector<storm::RationalFunction> EigenAdapter::toStdVector(Eigen::Matrix<storm::RationalFunction, Eigen::Dynamic, 1> const& v);
template Eigen::Matrix<storm::RationalFunction, Eigen::Dynamic, 1> EigenAdapter::toEigenVector(std::vector<storm::RationalFunction> const& v);
}  // namespace adapters
}  // namespace storm
