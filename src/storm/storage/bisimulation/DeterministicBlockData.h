#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>

#include "storm/storage/bisimulation/Block.h"

namespace storm {
namespace storage {
namespace bisimulation {
class DeterministicBlockData {
   public:
    DeterministicBlockData();
    DeterministicBlockData(uint_fast64_t marker1, uint_fast64_t marker2);

    uint_fast64_t marker1() const;
    void setMarker1(uint_fast64_t newMarker1);
    void incrementMarker1();
    void decrementMarker1();

    uint_fast64_t marker2() const;
    void setMarker2(uint_fast64_t newMarker2);
    void incrementMarker2();
    void decrementMarker2();

    /*!
     * This method needs to be called whenever the block was modified to reset the data of the change.
     *
     * @param block The block that this data belongs to.
     * @return True iff the data changed as a consequence of notifying it.
     */
    bool resetMarkers(Block<DeterministicBlockData> const& block);

    // Checks whether the block is marked as a splitter.
    bool splitter() const;

    // Marks the block as being a splitter.
    void setSplitter(bool value = true);

    // Retrieves whether the block is marked as a predecessor.
    bool needsRefinement() const;

    // Marks the block as needing refinement (or not).
    void setNeedsRefinement(bool value = true);

    // Sets whether the block is to be interpreted as absorbing.
    void setAbsorbing(bool absorbing);

    // Retrieves whether the block is to be interpreted as absorbing.
    bool absorbing() const;

    // Sets whether the states in the block have rewards.
    void setHasRewards(bool value = true);

    // Retrieves whether the states in the block have rewards.
    bool hasRewards() const;

    // Sets the representative state of this block
    void setRepresentativeState(storm::storage::sparse::state_type representativeState);

    // Retrieves whether this block has a representative state.
    bool hasRepresentativeState() const;

    // Retrieves the representative state for this block.
    storm::storage::sparse::state_type representativeState() const;

    friend std::ostream& operator<<(std::ostream& out, DeterministicBlockData const& data);

   public:
    // Helpers to set/retrieve flags.
    bool getFlag(uint64_t flag) const;
    void setFlag(uint64_t flag, bool value);

    // Two markers that can be used for various purposes. Whenever the block is split, both the markers are
    // set to the beginning index of the block.
    uint_fast64_t valMarker1;
    uint_fast64_t valMarker2;

    // Some bits to store flags: splitter flag, refinement flag, absorbing flag.
    static constexpr uint64_t SPLITTER_FLAG = 1ull;
    static constexpr uint64_t REFINEMENT_FLAG = 1ull << 1;
    static constexpr uint64_t ABSORBING_FLAG = 1ull << 2;
    static constexpr uint64_t REWARD_FLAG = 1ull << 3;
    uint8_t flags;

    // An optional representative state for the block. If this is set, this state is used to derive the
    // atomic propositions of the meta state in the quotient model.
    std::optional<storm::storage::sparse::state_type> valRepresentativeState;
};

std::ostream& operator<<(std::ostream& out, DeterministicBlockData const& data);
}  // namespace bisimulation
}  // namespace storage
}  // namespace storm
