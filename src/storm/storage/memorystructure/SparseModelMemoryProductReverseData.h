#pragma once

#include <vector>
#include "storm/storage/Scheduler.h"
#include "storm/storage/memorystructure/MemoryStructure.h"

namespace storm {
namespace storage {
/*!
 * Data to restore memory incorporation applied with SparseModelMemoryProduct
 */
class SparseModelMemoryProductReverseData {
   public:
    SparseModelMemoryProductReverseData(SparseModelMemoryProductReverseData const&) = default;
    SparseModelMemoryProductReverseData(SparseModelMemoryProductReverseData&&) = default;
    SparseModelMemoryProductReverseData& operator=(SparseModelMemoryProductReverseData const&) = default;
    SparseModelMemoryProductReverseData& operator=(SparseModelMemoryProductReverseData&&) = default;

    /*!
     * Initialization using results of SparseModelMemoryProduct
     * @param numberOfModelStates Number of states of the model the product has been taken with
     * @param memory the memory structure that has been used in the product
     * @param toProductStateMapping Maps (modelState * numberOfMemoryStates) + memoryState to the state in the product model that represents
     * (memoryState,modelState)
     */
    SparseModelMemoryProductReverseData(uint64_t numberOfModelStates, storm::storage::MemoryStructure const& memory,
                                        std::vector<uint64_t> const& toProductStateMapping)
        : numberOfModelStates(numberOfModelStates), memory(std::move(memory)), toProductStateMapping(std::move(toProductStateMapping)) {
        // Intentionally left empty
    }

    /*!
     * Creates a finite-memory scheduler from a scheduler of the product model.
     * @tparam ValueType
     * @param productScheduler
     * @return
     */
    template<typename ValueType>
    storm::storage::Scheduler<ValueType> createMemorySchedulerFromProductScheduler(storm::storage::Scheduler<ValueType> const& productScheduler) const {
        auto const numberOfMemoryStates = memory.getNumberOfStates();
        auto const numberOfProductStates = productScheduler.getNumberOfModelStates();
        storm::storage::Scheduler<ValueType> result(numberOfModelStates, memory);
        for (uint64_t state = 0; state < numberOfModelStates; state++) {
            for (uint64_t memState = 0; memState < numberOfMemoryStates; memState++) {
                auto const& productState = toProductStateMapping[state * numberOfMemoryStates + memState];
                if (productState < numberOfProductStates) {
                    result.setChoice(productScheduler.getChoice(productState), state, memState);
                } else {
                    result.setDontCare(state, memState, true);
                }
            }
        }
        return result;
    }

   private:
    uint64_t numberOfModelStates;
    storm::storage::MemoryStructure memory;
    std::vector<uint64_t> toProductStateMapping;
};

}  // namespace storage
}  // namespace storm
