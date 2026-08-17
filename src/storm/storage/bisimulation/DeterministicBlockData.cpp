#include "storm/storage/bisimulation/DeterministicBlockData.h"

#include <iostream>

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace bisimulation {

DeterministicBlockData::DeterministicBlockData() : DeterministicBlockData(0, 0) {
    // Intentionally left empty.
}

DeterministicBlockData::DeterministicBlockData(uint_fast64_t marker1, uint_fast64_t marker2)
    : valMarker1(marker1), valMarker2(marker2), flags(0), valRepresentativeState() {
    // Intentionally left empty.
}

uint_fast64_t DeterministicBlockData::marker1() const {
    return valMarker1;
}

void DeterministicBlockData::setMarker1(uint_fast64_t newMarker1) {
    valMarker1 = newMarker1;
}

void DeterministicBlockData::incrementMarker1() {
    ++valMarker1;
}

void DeterministicBlockData::decrementMarker1() {
    --valMarker1;
}

uint_fast64_t DeterministicBlockData::marker2() const {
    return valMarker2;
}

void DeterministicBlockData::setMarker2(uint_fast64_t newMarker2) {
    valMarker2 = newMarker2;
}

void DeterministicBlockData::incrementMarker2() {
    ++valMarker2;
}

void DeterministicBlockData::decrementMarker2() {
    --valMarker2;
}

bool DeterministicBlockData::resetMarkers(Block<DeterministicBlockData> const& block) {
    bool result = block.getBeginIndex() != this->valMarker1 || block.getBeginIndex() != this->valMarker2;
    this->valMarker1 = this->valMarker2 = block.getBeginIndex();
    return result;
}

bool DeterministicBlockData::splitter() const {
    return getFlag(SPLITTER_FLAG);
}

void DeterministicBlockData::setSplitter(bool value) {
    setFlag(SPLITTER_FLAG, value);
}

void DeterministicBlockData::setAbsorbing(bool absorbing) {
    setFlag(ABSORBING_FLAG, absorbing);
}

bool DeterministicBlockData::absorbing() const {
    return getFlag(ABSORBING_FLAG);
}

void DeterministicBlockData::setHasRewards(bool value) {
    setFlag(REWARD_FLAG, value);
}

bool DeterministicBlockData::hasRewards() const {
    return getFlag(REWARD_FLAG);
}

void DeterministicBlockData::setRepresentativeState(storm::storage::sparse::state_type representativeState) {
    this->valRepresentativeState = representativeState;
}

bool DeterministicBlockData::hasRepresentativeState() const {
    return static_cast<bool>(valRepresentativeState);
}

storm::storage::sparse::state_type DeterministicBlockData::representativeState() const {
    STORM_LOG_THROW(hasRepresentativeState(), storm::exceptions::InvalidOperationException, "Unable to retrieve representative state for block.");
    return valRepresentativeState.value();
}

bool DeterministicBlockData::needsRefinement() const {
    return getFlag(REFINEMENT_FLAG);
}

void DeterministicBlockData::setNeedsRefinement(bool value) {
    setFlag(REFINEMENT_FLAG, value);
}

bool DeterministicBlockData::getFlag(uint64_t flag) const {
    return (this->flags & flag) != 0ull;
}

void DeterministicBlockData::setFlag(uint64_t flag, bool value) {
    if (value) {
        this->flags |= flag;
    } else {
        this->flags &= ~flag;
    }
}

std::ostream& operator<<(std::ostream& out, DeterministicBlockData const& data) {
    out << "m1: " << data.marker1() << ", m2: " << data.marker2() << ", r: " << data.hasRewards();
    return out;
}
}  // namespace bisimulation
}  // namespace storage
}  // namespace storm
