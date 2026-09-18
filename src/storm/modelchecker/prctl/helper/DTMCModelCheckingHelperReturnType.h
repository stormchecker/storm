#pragma once

#include <utility>
#include <vector>

#include "storm/solver/SolutionBounds.h"

namespace storm {
namespace modelchecker {
namespace helper {

/*!
 * The outcome of a quantitative model checking query on a sparse DTMC. Unlike its MDP counterpart it carries no
 * scheduler, as a DTMC has no nondeterminism to resolve.
 */
template<typename ValueType>
struct DTMCSparseModelCheckingHelperReturnType {
    DTMCSparseModelCheckingHelperReturnType(DTMCSparseModelCheckingHelperReturnType const&) = delete;
    DTMCSparseModelCheckingHelperReturnType(DTMCSparseModelCheckingHelperReturnType&&) = default;
    DTMCSparseModelCheckingHelperReturnType& operator=(DTMCSparseModelCheckingHelperReturnType&&) = default;

    explicit DTMCSparseModelCheckingHelperReturnType(std::vector<ValueType>&& values) : values(std::move(values)) {
        // Intentionally left empty.
    }

    virtual ~DTMCSparseModelCheckingHelperReturnType() {
        // Intentionally left empty.
    }

    // The values computed for the states.
    std::vector<ValueType> values;

    storm::solver::SolutionBounds<ValueType> solutionBounds;
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
