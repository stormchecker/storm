#pragma once

#include "storm/storage/FlexibleSparseMatrix.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace solver {
namespace stateelimination {

enum class ScalingMode { Divide, DivideOneMinus };

template<typename ValueType, ScalingMode Mode>
class EliminatorBase {
   public:
    typedef typename storm::storage::FlexibleSparseMatrix<ValueType>::row_type FlexibleRowType;
    typedef typename FlexibleRowType::iterator FlexibleRowIterator;

    EliminatorBase(storm::storage::FlexibleSparseMatrix<ValueType>& matrix, storm::storage::FlexibleSparseMatrix<ValueType>& transposedMatrix);
    virtual ~EliminatorBase() = default;

    void eliminate(uint64_t row, uint64_t column, bool clearRow);

    void eliminateLoop(uint64_t row);

    // Provide virtual methods that can be customized by subclasses to govern side-effect of the elimination.
    virtual void updateValue(storm::storage::sparse::state_type const& state, ValueType const& loopProbability);
    virtual void updatePredecessor(storm::storage::sparse::state_type const& predecessor, ValueType const& probability,
                                   storm::storage::sparse::state_type const& state);
    virtual void updatePriority(storm::storage::sparse::state_type const& state);
    virtual bool filterPredecessor(storm::storage::sparse::state_type const& state);
    virtual bool isFilterPredecessor() const;

   protected:
    storm::storage::FlexibleSparseMatrix<ValueType>& matrix;
    storm::storage::FlexibleSparseMatrix<ValueType>& transposedMatrix;
};

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
