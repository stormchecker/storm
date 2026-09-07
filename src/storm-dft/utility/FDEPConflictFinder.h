#pragma once

#include <vector>

#include "storm-dft/storage/DFT.h"

namespace storm::dft {
namespace utility {

template<typename ValueType>
class FDEPConflictFinder {
   public:
    /*!
     * Get a vector of index pairs of FDEPs in the DFT which are conflicting. Two FDEPs are conflicting if
     * their simultaneous triggering may cause unresolvable non-deterministic behavior.
     *
     * @param dft The DFT.
     * @param useSMT If set, an SMT solver is used to refine the conflict set.
     * @param timeout Timeout for each SMT query in seconds, defaults to 10 seconds.
     * @return A vector of pairs of indices. The indices in a pair refer to FDEPs which are conflicting.
     */
    static std::vector<std::pair<uint64_t, uint64_t>> getDependencyConflicts(storm::dft::storage::DFT<ValueType> const& dft, bool useSMT = false,
                                                                             uint_fast64_t timeout = 10);

    /*!
     *
     * @param dft
     * @return
     */
    static std::vector<bool> getDynamicBehavior(storm::dft::storage::DFT<ValueType> const& dft);
};

}  // namespace utility
}  // namespace storm::dft
