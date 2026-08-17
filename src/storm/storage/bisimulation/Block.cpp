#include "storm/storage/bisimulation/Block.h"

#include <iostream>

#include "storm/storage/bisimulation/DeterministicBlockData.h"
#include "storm/storage/bisimulation/Partition.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace bisimulation {

template<typename DataType>
Block<DataType>::Block(storm::storage::sparse::state_type beginIndex, storm::storage::sparse::state_type endIndex, Block* previousBlock, Block* nextBlock,
                       std::size_t id)
    : nextBlock(nextBlock), previousBlock(previousBlock), beginIndex(beginIndex), endIndex(endIndex), id(id), mData() {
    if (nextBlock != nullptr) {
        nextBlock->previousBlock = this;
    }
    if (previousBlock != nullptr) {
        previousBlock->nextBlock = this;
    }
    data().resetMarkers(*this);
    STORM_LOG_ASSERT(this->beginIndex < this->endIndex, "Unable to create block of illegal size.");
}

template<typename DataType>
bool Block<DataType>::operator==(Block const& other) const {
    return this == &other;
}

template<typename DataType>
bool Block<DataType>::operator!=(Block const& other) const {
    return this != &other;
}

template<typename DataType>
void Block<DataType>::print(Partition<DataType> const& partition) const {
    std::cout << "block [" << this << "] " << this->id << " from " << this->beginIndex << " to " << this->endIndex << " with data [" << this->data() << "]\n";
    std::cout << "states ";
    for (auto stateIt = partition.begin(*this), stateIte = partition.end(*this); stateIt != stateIte; ++stateIt) {
        std::cout << *stateIt << ", ";
    }
    std::cout << '\n';
}

template<typename DataType>
void Block<DataType>::setBeginIndex(storm::storage::sparse::state_type beginIndex) {
    this->beginIndex = beginIndex;
    data().resetMarkers(*this);
    STORM_LOG_ASSERT(beginIndex < endIndex, "Unable to resize block to illegal size.");
}

template<typename DataType>
void Block<DataType>::setEndIndex(storm::storage::sparse::state_type endIndex) {
    this->endIndex = endIndex;
    data().resetMarkers(*this);
    STORM_LOG_ASSERT(beginIndex < endIndex, "Unable to resize block to illegal size.");
}

template<typename DataType>
std::size_t Block<DataType>::getId() const {
    return this->id;
}

template<typename DataType>
storm::storage::sparse::state_type Block<DataType>::getBeginIndex() const {
    return this->beginIndex;
}

template<typename DataType>
storm::storage::sparse::state_type Block<DataType>::getEndIndex() const {
    return this->endIndex;
}

template<typename DataType>
Block<DataType> const& Block<DataType>::getNextBlock() const {
    return *this->nextBlock;
}

template<typename DataType>
bool Block<DataType>::hasNextBlock() const {
    return this->nextBlock != nullptr;
}

template<typename DataType>
Block<DataType>* Block<DataType>::getNextBlockPointer() {
    return this->nextBlock;
}

template<typename DataType>
Block<DataType> const* Block<DataType>::getNextBlockPointer() const {
    return this->nextBlock;
}

template<typename DataType>
Block<DataType> const& Block<DataType>::getPreviousBlock() const {
    return *this->previousBlock;
}

template<typename DataType>
Block<DataType>* Block<DataType>::getPreviousBlockPointer() {
    return this->previousBlock;
}

template<typename DataType>
Block<DataType> const* Block<DataType>::getPreviousBlockPointer() const {
    return this->previousBlock;
}

template<typename DataType>
bool Block<DataType>::hasPreviousBlock() const {
    return this->previousBlock != nullptr;
}

template<typename DataType>
bool Block<DataType>::check() const {
    STORM_LOG_ASSERT(this->beginIndex < this->endIndex, "Block has negative size.");
    STORM_LOG_ASSERT(!this->hasPreviousBlock() || this->getPreviousBlock().getNextBlockPointer() == this, "Illegal previous block.");
    STORM_LOG_ASSERT(!this->hasNextBlock() || this->getNextBlock().getPreviousBlockPointer() == this, "Illegal next block.");
    return true;
}

template<typename DataType>
std::size_t Block<DataType>::getNumberOfStates() const {
    return (this->endIndex - this->beginIndex);
}

template<typename DataType>
DataType& Block<DataType>::data() {
    return mData;
}

template<typename DataType>
DataType const& Block<DataType>::data() const {
    return mData;
}

template<typename DataType>
void Block<DataType>::resetMarkers() {
    mData.resetMarkers(*this);
}

template class Block<DeterministicBlockData>;

}  // namespace bisimulation
}  // namespace storage
}  // namespace storm
