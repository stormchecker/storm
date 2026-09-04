#pragma once

#include "storm/numbers/constants.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/solver.h"

namespace storm {
namespace storage {
namespace geometry {
template<typename ValueType>
class ReduceVertexCloud {
   public:
    /*!
     *
     * @param smtSolverFactory
     * @param wiggle
     * @param timeout: Maximal time in milliseconds, 0 is no timeout
     */
    ReduceVertexCloud(std::shared_ptr<storm::utility::solver::SmtSolverFactory>& smtSolverFactory, ValueType wiggle = storm::numbers::zero<ValueType>(),
                      uint64_t timeout = 0)
        : smtSolverFactory(smtSolverFactory), wiggle(wiggle), timeOut(timeout) {}

    std::pair<storm::storage::BitVector, bool> eliminate(std::vector<std::map<uint64_t, ValueType>> const& input, uint64_t maxdimension);

   private:
    std::shared_ptr<storm::utility::solver::SmtSolverFactory>& smtSolverFactory;
    ValueType wiggle;
    uint64_t timeOut;
};

}  // namespace geometry
}  // namespace storage
}  // namespace storm