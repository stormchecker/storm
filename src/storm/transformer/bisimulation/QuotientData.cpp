#include "storm/transformer/bisimulation/QuotientData.h"

#include <algorithm>
#include <limits>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Model.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

template<typename ValueType>
QuotientData<ValueType>::QuotientData(storm::models::sparse::Model<ValueType> const& model, storm::bisimulation::Partition const& partition,
                                      storm::OptionalRef<storm::storage::BitVector const> preferredRepresentatives) {
    uint64_t const undef = std::numeric_limits<uint64_t>::max();
    toQuotientState.assign(model.getNumberOfStates(), undef);
    toRepresentativeState.reserve(partition.getNumberOfBlocks());
    uint64_t quotientState = 0;
    partition.forEachBlock([&](auto const& block) {
        for (auto const s : block) {
            toQuotientState[s] = quotientState;
        }
        uint64_t representativeState = block.front();
        if (preferredRepresentatives) {
            if (auto const it =
                    std::find_if(block.begin(), block.end(), [&preferredRepresentatives](auto const s) { return preferredRepresentatives->get(s); });
                it != block.end()) {
                representativeState = *it;
            }
        }
        toRepresentativeState.push_back(representativeState);
        ++quotientState;
    });
    STORM_LOG_ASSERT(std::none_of(toQuotientState.begin(), toQuotientState.end(), [&undef](auto const& s) { return s == undef; }),
                     "Not all states appear in a block of the partition.");
}

template struct QuotientData<double>;
template struct QuotientData<storm::RationalNumber>;
template struct QuotientData<storm::RationalFunction>;
template struct QuotientData<storm::Interval>;
template struct QuotientData<storm::RationalInterval>;

}  // namespace storm::bisimulation
