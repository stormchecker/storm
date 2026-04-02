#pragma once

#include <memory>
#include <vector>

#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/UncertaintyResolutionMode.h"

namespace storm {

class Environment;

namespace storage {
template<typename ValueType>
class SparseMatrix;
}

namespace solver {

template<typename ValueType, typename SolutionType = ValueType>
class Multiplier {
   public:
    Multiplier(storm::storage::SparseMatrix<ValueType> const& matrix);

    virtual ~Multiplier() = default;

    /*
     * Clears the currently cached data of this multiplier in order to free some memory.
     */
    virtual void clearCache() const;

    /*!
     * Performs a matrix-vector multiplication x' = A*x + b.
     *
     * @param x The input vector with which to multiply the matrix. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after the multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param result The target vector into which to write the multiplication result. Its length must be equal
     * to the number of rows of A. Can be the same as the x vector.
     */
    virtual void multiply(Environment const& env, std::vector<SolutionType> const& x, std::vector<ValueType> const* b,
                          std::vector<SolutionType>& result) const = 0;

    /*!
     * Performs a matrix-vector multiplication in gauss-seidel style.
     *
     * @param x The input/output vector with which to multiply the matrix. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after the multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param backwards if true, the iterations will be performed beginning from the last row and ending at the first row.
     */
    virtual void multiplyGaussSeidel(Environment const& env, std::vector<SolutionType>& x, std::vector<ValueType> const* b, bool backwards = true) const = 0;

    /*!
     * Performs a matrix-vector multiplication x' = A*x + b and then minimizes/maximizes over the row groups
     * so that the resulting vector has the size of number of row groups of A.
     *
     * @param dir The direction for the reduction step.
     * @param rowGroupIndices A vector storing the row groups over which to reduce.
     * @param x The input vector with which to multiply the matrix. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after the multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param result The target vector into which to write the multiplication result. Its length must be equal
     * to the number of rows of A. Can be the same as the x vector.
     * @param uncertaintyResolutionMode The mode according to which to resolve uncertainty in the reduction step.
     * @param choices If given, the choices made in the reduction process are written to this vector.
     */
    void multiplyAndReduce(Environment const& env, OptimizationDirection const& dir, std::vector<SolutionType> const& x, std::vector<ValueType> const* b,
                           std::vector<SolutionType>& result, UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset,
                           std::vector<uint_fast64_t>* choices = nullptr) const;
    virtual void multiplyAndReduce(Environment const& env, OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                   std::vector<SolutionType> const& x, std::vector<ValueType> const* b, std::vector<SolutionType>& result,
                                   UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset,
                                   std::vector<uint_fast64_t>* choices = nullptr) const = 0;

    /*!
     * Performs a matrix-vector multiplication in gauss-seidel style and then minimizes/maximizes over the row groups
     * so that the resulting vector has the size of number of row groups of A.
     *
     * @param dir The direction for the reduction step.
     * @param rowGroupIndices A vector storing the row groups over which to reduce.
     * @param x The input/output vector with which to multiply the matrix. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after the multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param result The target vector into which to write the multiplication result. Its length must be equal
     * to the number of rows of A. Can be the same as the x vector.
     * @param choices If given, the choices made in the reduction process are written to this vector.
     * @param backwards if true, the iterations will be performed beginning from the last rowgroup and ending at the first rowgroup.
     */
    void multiplyAndReduceGaussSeidel(Environment const& env, OptimizationDirection const& dir, std::vector<SolutionType>& x, std::vector<ValueType> const* b,
                                      std::vector<uint_fast64_t>* choices = nullptr, bool backwards = true) const;
    virtual void multiplyAndReduceGaussSeidel(Environment const& env, OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                              std::vector<SolutionType>& x, std::vector<ValueType> const* b, std::vector<uint_fast64_t>* choices = nullptr,
                                              bool backwards = true) const = 0;

    /*!
     * Performs repeated matrix-vector multiplication, using x[0] = x and x[i + 1] = A*x[i] + b. After
     * performing the necessary multiplications, the result is written to the input vector x. Note that the
     * matrix A has to be given upon construction time of the solver object.
     *
     * @param x The initial vector with which to perform matrix-vector multiplication. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after each multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param n The number of times to perform the multiplication.
     */
    void repeatedMultiply(Environment const& env, std::vector<SolutionType>& x, std::vector<ValueType> const* b, uint64_t n) const;

    /*!
     * Performs repeated matrix-vector multiplication x' = A*x + b and then minimizes/maximizes over the row groups
     * so that the resulting vector has the size of number of row groups of A.
     *
     * @param dir The direction for the reduction step.
     * @param x The input vector with which to multiply the matrix. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after the multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param result The target vector into which to write the multiplication result. Its length must be equal
     * to the number of rows of A.
     * @param n The number of times to perform the multiplication.
     * @param uncertaintyResolutionMode The mode according to which to resolve uncertainty in the reduction step.
     */
    void repeatedMultiplyAndReduce(Environment const& env, OptimizationDirection const& dir, std::vector<SolutionType>& x, std::vector<ValueType> const* b,
                                   uint64_t n, UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset) const;
    /*!
     * Performs repeated matrix-vector multiplication x' = A*(factor * x) + b. Vector x is scaled by factor in each iteration.
     *
     * @param x The input vector with which to multiply the matrix. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after the multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param result The target vector into which to write the multiplication result. Its length must be equal
     * to the number of rows of A.
     * @param n The number of times to perform the multiplication.
     * @param factor The scalar to multiply with in each iteration.
     */
    void repeatedMultiplyWithFactor(Environment const& env, std::vector<SolutionType>& x, std::vector<ValueType> const* b, uint64_t n,
                                    SolutionType factor) const;

    /*!
     * Performs repeated matrix-vector multiplication x' = A*(factor * x) + b, minimizes/maximizes over the row groups
     * so that the resulting vector has the size of number of row groups of A. Vector x is scaled by factor in each iteration.
     *
     * @param dir The direction for the reduction step.
     * @param x The input vector with which to multiply the matrix. Its length must be equal
     * to the number of columns of A.
     * @param b If non-null, this vector is added after the multiplication. If given, its length must be equal
     * to the number of rows of A.
     * @param result The target vector into which to write the multiplication result. Its length must be equal
     * to the number of rows of A.
     * @param n The number of times to perform the multiplication.
     * @param uncertaintyResolutionMode The mode according to which to resolve uncertainty in the reduction step.
     * @param factor The scalar to multiply with in each iteration.
     */
    void repeatedMultiplyAndReduceWithFactor(Environment const& env, OptimizationDirection const& dir, std::vector<SolutionType>& x,
                                             std::vector<ValueType> const* b, uint64_t n, SolutionType factor,
                                             UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset) const;

   protected:
    std::vector<SolutionType>& provideCachedVector(uint64_t size) const;

    mutable std::unique_ptr<std::vector<SolutionType>> cachedVector;
    storm::storage::SparseMatrix<ValueType> const& matrix;
};

template<typename ValueType, typename SolutionType = ValueType>
class MultiplierFactory {
   public:
    MultiplierFactory() = default;
    ~MultiplierFactory() = default;

    std::unique_ptr<Multiplier<ValueType, SolutionType>> create(Environment const& env, storm::storage::SparseMatrix<ValueType> const& matrix);
};

}  // namespace solver
}  // namespace storm
