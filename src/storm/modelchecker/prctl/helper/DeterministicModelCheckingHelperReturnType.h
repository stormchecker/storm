#pragma once

#include <utility>
#include <vector>

#include "storm/solver/SolutionBounds.h"

namespace storm {
namespace modelchecker {
namespace helper {

/*!
 * The outcome of a quantitative model checking query on a sparse deterministic model, i.e. a DTMC or a CTMC.
 * Unlike its MDP counterpart it carries no scheduler, as such a model has no nondeterminism to resolve.
 */
template<typename ValueType>
struct DeterministicSparseModelCheckingHelperReturnType {
    DeterministicSparseModelCheckingHelperReturnType(DeterministicSparseModelCheckingHelperReturnType const&) = delete;
    DeterministicSparseModelCheckingHelperReturnType(DeterministicSparseModelCheckingHelperReturnType&&) = default;
    DeterministicSparseModelCheckingHelperReturnType& operator=(DeterministicSparseModelCheckingHelperReturnType&&) = default;

    explicit DeterministicSparseModelCheckingHelperReturnType(std::vector<ValueType>&& values) : values(std::move(values)) {
        // Intentionally left empty.
    }

    virtual ~DeterministicSparseModelCheckingHelperReturnType() {
        // Intentionally left empty.
    }

    // The values computed for the states.
    std::vector<ValueType> values;

    // Sound bounds on the values, if the algorithm that computed them provided any.
    storm::solver::SolutionBounds<ValueType> solutionBounds;
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
