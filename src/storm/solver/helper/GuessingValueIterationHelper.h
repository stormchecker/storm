#pragma once

#include <vector>

#include "storm/solver/helper/ValueIterationOperator.h"

#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverStatus.h"
#include "storm/storage/BitVector.h"

namespace storm::solver::helper {

template<typename ValueType>
struct GVIData {
    std::vector<ValueType> const& x;
    std::vector<ValueType> const& y;
    SolverStatus const status;
};

namespace gviinternal {

typedef uint64_t IndexType;
enum class VerifyResult { Verified, Converged, Unverified };

template<typename ValueType>
class IterationHelper {
   public:
    IterationHelper(storm::storage::SparseMatrix<ValueType> const& matrix_);

    IndexType selectRowGroupToGuess(const std::vector<ValueType>& lowerX, const std::vector<ValueType>& upperX);

    ValueType getMaxLength(std::vector<ValueType>& lower, std::vector<ValueType>& upper) const;
    ValueType getSumLength(std::vector<ValueType>& lower, std::vector<ValueType>& upper) const;

   private:
    storm::storage::SparseMatrix<ValueType> const& matrix;
    // this is only used to avoid reallocations
    std::vector<ValueType> weights;

    void swipeWeights();
};

}  // namespace gviinternal

template<typename ValueType, OptimizationDirection Dir>
class GVIBackend;

/*!
 * Implements guessing value iteration
 * @see https://doi.org/10.1007/978-3-031-90653-4_11
 */
template<typename ValueType, bool TrivialRowGrouping>
class GuessingValueIterationHelper {
   public:
    explicit GuessingValueIterationHelper(std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator,
                                          const storage::SparseMatrix<ValueType>& matrix);

    /*!
     * @param lowerX Needs to be some arbitrary lower bound on the actual values initially
     * @param upperX Needs to be some arbitrary upper bound on the actual values initially
     * @param b the values added to each matrix row (the b in A*x+b)
     * @param dir The optimization direction
     * @return The status upon termination as well as the number of iterations Also, the maximum (relative/absolute) difference between lowerX and upperX will
     * be 2*epsilon with the provided precision parameters.
     */
    SolverStatus solveEquations(std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, uint64_t& numIterations,
                                ValueType precision, std::optional<storm::solver::OptimizationDirection> dir,
                                std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback);

   private:
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator;
    gviinternal::IterationHelper<ValueType> iterationHelper;
    std::vector<ValueType> guessLower, guessUpper;
    gviinternal::IndexType selectRowGroupToGuess(std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX);

    template<OptimizationDirection Dir>
    SolverStatus solveEquations(std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, uint64_t& numIterations,
                                ValueType precision, std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback);

    template<OptimizationDirection Dir>
    void applyInPlace(std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, GVIBackend<ValueType, Dir>& backend);

    template<OptimizationDirection Dir>
    std::pair<gviinternal::VerifyResult, SolverStatus> tryVerify(std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX,
                                                                 const std::vector<ValueType>& b, uint64_t& numIterations,
                                                                 gviinternal::IndexType rowGroupToGuess, ValueType guessValue, ValueType precision,
                                                                 std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback);

    template<OptimizationDirection Dir>
    SolverStatus doIterations(std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, uint64_t& numIterations,
                              std::optional<uint64_t> maxIterations, const ValueType& precision,
                              std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback);
};
}  // namespace storm::solver::helper