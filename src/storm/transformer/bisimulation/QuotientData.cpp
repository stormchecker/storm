#include "storm/transformer/bisimulation/QuotientData.h"

#include <algorithm>
#include <limits>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"

namespace storm::bisimulation {

template<typename ValueType>
QuotientData<ValueType>::QuotientData(storm::bisimulation::Partition const& partition,
                                      storm::OptionalRef<storm::storage::BitVector const> preferredRepresentatives) {
    uint64_t constexpr Undef = std::numeric_limits<uint64_t>::max();
    toQuotientState.assign(partition.getNumberOfElements(), Undef);
    toRepresentativeState.reserve(partition.getNumberOfBlocks());
    // Number the quotient states in the order of the smallest state of their block, so that the quotient resembles the order of the original states.
    for (uint64_t state = 0; state < partition.getNumberOfElements(); ++state) {
        if (toQuotientState[state] != Undef) {
            continue;
        }
        auto const block = partition.getBlockOfElement(state);
        uint64_t const quotientState = toRepresentativeState.size();
        for (auto const s : block) {
            toQuotientState[s] = quotientState;
        }
        // Unless a preferred representative is available, the first state of the block is the representative. Approximative signature-based refinement
        // relies on this: that state is the anchor that all other states of the block were compared with, cf. Signatures::extendQuotientData.
        uint64_t representativeState = block.front();
        if (preferredRepresentatives) {
            if (auto const it =
                    std::find_if(block.begin(), block.end(), [&preferredRepresentatives](auto const s) { return preferredRepresentatives->get(s); });
                it != block.end()) {
                representativeState = *it;
            }
        }
        toRepresentativeState.push_back(representativeState);
    }
}

template struct QuotientData<double>;
template struct QuotientData<storm::RationalNumber>;
template struct QuotientData<storm::RationalFunction>;
template struct QuotientData<storm::Interval>;
template struct QuotientData<storm::RationalInterval>;

}  // namespace storm::bisimulation
