#pragma once

#include <memory>
#include <vector>
#include "storm/solver/SolutionBounds.h"
#include "storm/storage/Scheduler.h"

namespace storm {
namespace storage {
class BitVector;
}

namespace modelchecker {
namespace helper {
template<typename ValueType>
struct MDPSparseModelCheckingHelperReturnType {
    MDPSparseModelCheckingHelperReturnType(MDPSparseModelCheckingHelperReturnType const&) = delete;
    MDPSparseModelCheckingHelperReturnType(MDPSparseModelCheckingHelperReturnType&&) = default;

    MDPSparseModelCheckingHelperReturnType(std::vector<ValueType>&& values, std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler = nullptr)
        : values(std::move(values)), scheduler(std::move(scheduler)) {
        // Intentionally left empty.
    }

    virtual ~MDPSparseModelCheckingHelperReturnType() {
        // Intentionally left empty.
    }

    // The values computed for the states.
    std::vector<ValueType> values;

    // A scheduler, if it was computed.
    std::unique_ptr<storm::storage::Scheduler<ValueType>> scheduler;

    // Sound bounds on the values, if the algorithm that computed them provided any.
    storm::solver::SolutionBounds<ValueType> solutionBounds;
};
}  // namespace helper

}  // namespace modelchecker
}  // namespace storm
