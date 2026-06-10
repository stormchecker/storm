#pragma once

#include "storm/solver/multiplier/Multiplier.h"

#include "storm/solver/OptimizationDirection.h"

namespace storm {
namespace storage {
template<typename ValueType>
class SparseMatrix;
}

namespace solver {

template<typename ValueType>
class NativeMultiplier : public Multiplier<ValueType> {
   public:
    NativeMultiplier(storm::storage::SparseMatrix<ValueType> const& matrix);
    virtual ~NativeMultiplier() = default;

    virtual void multiply(Environment const& env, std::vector<ValueType> const& x, std::vector<ValueType> const* b,
                          std::vector<ValueType>& result) const override;
    virtual void multiplyGaussSeidel(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const* b, bool backwards = true) const override;
    virtual void multiplyAndReduce(Environment const& env, OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                   std::vector<ValueType> const& x, std::vector<ValueType> const* b, std::vector<ValueType>& result,
                                   UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset,
                                   std::vector<uint_fast64_t>* choices = nullptr) const override;
    virtual void multiplyAndReduceGaussSeidel(Environment const& env, OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                              std::vector<ValueType>& x, std::vector<ValueType> const* b, std::vector<uint_fast64_t>* choices = nullptr,
                                              bool backwards = true) const override;
    /*!
     * Multiplies the row with the given index with x and adds the result to the provided value.
     * @param rowIndex The index of the considered row
     * @param x The input vector with which the row is multiplied
     * @param value The multiplication result is added to this value. It shall not refer to a value in x or in the matrix.
     */
    void multiplyRow(uint64_t const& rowIndex, std::vector<ValueType> const& x, ValueType& value) const;

    /*!
     * Multiplies the row with the given index with x1 and x2 and adds the given offset o1 and o2, respectively.
     * @param rowIndex The index of the considered row
     * @param x1 The first input vector with which the row is multiplied.
     * @param val1 The first multiplication result is added to this value. It shall not refer to a value in x or in the matrix.
     * @param x2 The second input vector with which the row is multiplied.
     * @param val2 The second multiplication result is added to this value. It shall not refer to a value in x or in the matrix.
     */
    void multiplyRow2(uint64_t const& rowIndex, std::vector<ValueType> const& x1, ValueType& val1, std::vector<ValueType> const& x2, ValueType& val2) const;

   private:
    void multAdd(std::vector<ValueType> const& x, std::vector<ValueType> const* b, std::vector<ValueType>& result) const;

    void multAddReduce(storm::solver::OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices, std::vector<ValueType> const& x,
                       std::vector<ValueType> const* b, std::vector<ValueType>& result, std::vector<uint64_t>* choices = nullptr) const;
};

}  // namespace solver
}  // namespace storm
