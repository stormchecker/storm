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
template<typename ValueType, typename ValuesType = ValueType>
struct MDPSparseModelCheckingHelperReturnType {
    MDPSparseModelCheckingHelperReturnType(MDPSparseModelCheckingHelperReturnType const&) = delete;
    MDPSparseModelCheckingHelperReturnType(MDPSparseModelCheckingHelperReturnType&&) = default;

    MDPSparseModelCheckingHelperReturnType(std::vector<ValuesType>&& values, std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler = nullptr)
        : values(std::move(values)), scheduler(std::move(scheduler)) {
        // Intentionally left empty.
    }

    virtual ~MDPSparseModelCheckingHelperReturnType() {
        // Intentionally left empty.
    }

    // The values computed for the states.
    std::vector<ValuesType> values;

    // A scheduler, if it was computed.
    std::unique_ptr<storm::storage::Scheduler<ValueType>> scheduler;

    storm::solver::SolutionBounds<ValuesType> solutionBounds;
};
}  // namespace helper

}  // namespace modelchecker
}  // namespace storm
