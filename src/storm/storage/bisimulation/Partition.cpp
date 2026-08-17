#include "storm/storage/bisimulation/Partition.h"

#include <iostream>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/bisimulation/DeterministicBlockData.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace bisimulation {
template<typename DataType>
Partition<DataType>::Partition(std::size_t numberOfStates) : stateToBlockMapping(numberOfStates), states(numberOfStates), positions(numberOfStates) {
    blocks.emplace_back(new Block<DataType>(0, numberOfStates, nullptr, nullptr, blocks.size()));

    // Set up the different parts of the internal structure.
    for (storm::storage::sparse::state_type state = 0; state < numberOfStates; ++state) {
        states[state] = state;
        positions[state] = state;
        stateToBlockMapping[state] = blocks.back().get();
    }
}

template<typename DataType>
Partition<DataType>::Partition(std::size_t numberOfStates, storm::storage::BitVector const& prob0States, storm::storage::BitVector const& prob1States,
                               std::optional<storm::storage::sparse::state_type> representativeProb1State)
    : stateToBlockMapping(numberOfStates), states(numberOfStates), positions(numberOfStates) {
    storm::storage::sparse::state_type position = 0;
    Block<DataType>* firstBlock = nullptr;
    Block<DataType>* secondBlock = nullptr;
    Block<DataType>* thirdBlock = nullptr;
    if (!prob0States.empty()) {
        blocks.emplace_back(new Block<DataType>(0, prob0States.getNumberOfSetBits(), nullptr, nullptr, blocks.size()));
        firstBlock = blocks.front().get();

        for (auto state : prob0States) {
            states[position] = state;
            positions[state] = position;
            stateToBlockMapping[state] = firstBlock;
            ++position;
        }
        firstBlock->data().setAbsorbing(true);
    }

    if (!prob1States.empty()) {
        blocks.emplace_back(new Block<DataType>(position, position + prob1States.getNumberOfSetBits(), firstBlock, nullptr, blocks.size()));
        secondBlock = blocks.back().get();

        for (auto state : prob1States) {
            states[position] = state;
            positions[state] = position;
            stateToBlockMapping[state] = secondBlock;
            ++position;
        }
        secondBlock->data().setAbsorbing(true);
        secondBlock->data().setRepresentativeState(representativeProb1State.value());
    }

    storm::storage::BitVector otherStates = ~(prob0States | prob1States);
    if (!otherStates.empty()) {
        blocks.emplace_back(new Block<DataType>(position, numberOfStates, secondBlock, nullptr, blocks.size()));
        thirdBlock = blocks.back().get();

        for (auto state : otherStates) {
            states[position] = state;
            positions[state] = position;
            stateToBlockMapping[state] = thirdBlock;
            ++position;
        }
    }
}

template<typename DataType>
void Partition<DataType>::swapStates(storm::storage::sparse::state_type state1, storm::storage::sparse::state_type state2) {
    std::swap(this->states[this->positions[state1]], this->states[this->positions[state2]]);
    std::swap(this->positions[state1], this->positions[state2]);
}

template<typename DataType>
void Partition<DataType>::swapStatesAtPositions(storm::storage::sparse::state_type position1, storm::storage::sparse::state_type position2) {
    storm::storage::sparse::state_type state1 = this->states[position1];
    storm::storage::sparse::state_type state2 = this->states[position2];
    std::swap(this->states[position1], this->states[position2]);
    this->positions[state1] = position2;
    this->positions[state2] = position1;
}

template<typename DataType>
storm::storage::sparse::state_type const& Partition<DataType>::getPosition(storm::storage::sparse::state_type state) const {
    return this->positions[state];
}

template<typename DataType>
storm::storage::sparse::state_type const& Partition<DataType>::getState(storm::storage::sparse::state_type position) const {
    return this->states[position];
}

template<typename DataType>
void Partition<DataType>::mapStatesToBlock(Block<DataType>& block, std::vector<storm::storage::sparse::state_type>::iterator first,
                                           std::vector<storm::storage::sparse::state_type>::iterator last) {
    for (; first != last; ++first) {
        this->stateToBlockMapping[*first] = &block;
    }
}

template<typename DataType>
void Partition<DataType>::mapStatesToPositions(std::vector<storm::storage::sparse::state_type>::const_iterator first,
                                               std::vector<storm::storage::sparse::state_type>::const_iterator last) {
    storm::storage::sparse::state_type position = std::distance(this->states.cbegin(), first);
    for (; first != last; ++first, ++position) {
        this->positions[*first] = position;
    }
}

template<typename DataType>
void Partition<DataType>::mapStatesToPositions(Block<DataType> const& block) {
    mapStatesToPositions(this->begin(block), this->end(block));
}

template<typename DataType>
Block<DataType>& Partition<DataType>::getBlock(storm::storage::sparse::state_type state) {
    return *this->stateToBlockMapping[state];
}

template<typename DataType>
Block<DataType> const& Partition<DataType>::getBlock(storm::storage::sparse::state_type state) const {
    return *this->stateToBlockMapping[state];
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::iterator Partition<DataType>::begin(Block<DataType> const& block) {
    auto it = this->states.begin();
    std::advance(it, block.getBeginIndex());
    return it;
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::const_iterator Partition<DataType>::begin(Block<DataType> const& block) const {
    auto it = this->states.begin();
    std::advance(it, block.getBeginIndex());
    return it;
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::iterator Partition<DataType>::end(Block<DataType> const& block) {
    auto it = this->states.begin();
    std::advance(it, block.getEndIndex());
    return it;
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::const_iterator Partition<DataType>::end(Block<DataType> const& block) const {
    auto it = this->states.begin();
    std::advance(it, block.getEndIndex());
    return it;
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::iterator Partition<DataType>::begin() {
    return this->states.begin();
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::const_iterator Partition<DataType>::begin() const {
    return this->states.begin();
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::iterator Partition<DataType>::end() {
    return this->states.end();
}

template<typename DataType>
std::vector<storm::storage::sparse::state_type>::const_iterator Partition<DataType>::end() const {
    return this->states.end();
}

template<typename DataType>
void Partition<DataType>::sortRange(storm::storage::sparse::state_type beginIndex, storm::storage::sparse::state_type endIndex,
                                    std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less,
                                    bool updatePositions) {
    std::sort(this->states.begin() + beginIndex, this->states.begin() + endIndex, less);

    if (updatePositions) {
        mapStatesToPositions(this->states.begin() + beginIndex, this->states.begin() + endIndex);
    }
}

template<typename DataType>
void Partition<DataType>::sortBlock(Block<DataType>& block,
                                    std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less,
                                    bool updatePositions) {
    sortRange(block.getBeginIndex(), block.getEndIndex(), less, updatePositions);
}

template<typename DataType>
std::vector<uint_fast64_t> Partition<DataType>::computeRangesOfEqualValue(
    uint_fast64_t startIndex, uint_fast64_t endIndex, std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less) {
    auto it = this->states.cbegin() + startIndex;
    auto ite = this->states.cbegin() + endIndex;

    std::vector<storm::storage::sparse::state_type>::const_iterator upperBound;
    std::vector<uint_fast64_t> result;
    result.push_back(startIndex);
    do {
        upperBound = std::upper_bound(it, ite, *it, less);
        result.push_back(std::distance(this->states.cbegin(), upperBound));
        it = upperBound;
    } while (upperBound != ite);

    return result;
}

template<typename DataType>
std::pair<typename std::vector<std::unique_ptr<Block<DataType>>>::iterator, bool> Partition<DataType>::splitBlock(Block<DataType>& block,
                                                                                                                  storm::storage::sparse::state_type position) {
    STORM_LOG_ASSERT(position >= block.getBeginIndex() && position <= block.getEndIndex(), "Cannot split block at illegal position.");
    STORM_LOG_TRACE("Splitting " << block.getId() << " at position " << position << " (begin was " << block.getBeginIndex() << ").");

    // In case one of the resulting blocks would be empty, we simply return the current block and do not create
    // a new one.
    if (position == block.getBeginIndex() || position == block.getEndIndex()) {
        auto it = blocks.begin();
        std::advance(it, block.getId());
        return std::make_pair(it, false);
    }

    // Actually create the new block.
    blocks.emplace_back(new Block<DataType>(block.getBeginIndex(), position, block.getPreviousBlockPointer(), &block, blocks.size()));
    auto newBlockIt = std::prev(blocks.end());

    // Resize the current block appropriately.
    block.setBeginIndex(position);

    // Update the mapping of the states in the newly created block.
    this->mapStatesToBlock(**newBlockIt, this->begin(**newBlockIt), this->end(**newBlockIt));

    return std::make_pair(newBlockIt, true);
}

template<typename DataType>
bool Partition<DataType>::splitBlock(Block<DataType>& block,
                                     std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less,
                                     std::function<void(Block<DataType>&)> const& newBlockCallback) {
    // Sort the block, but leave the positions untouched.
    this->sortBlock(block, less, false);

    auto originalBegin = block.getBeginIndex();
    auto originalEnd = block.getEndIndex();

    auto it = this->states.cbegin() + block.getBeginIndex();
    auto ite = this->states.cbegin() + block.getEndIndex();

    bool wasSplit = false;
    std::vector<storm::storage::sparse::state_type>::const_iterator upperBound;
    do {
        upperBound = std::upper_bound(it, ite, *it, less);

        if (upperBound != ite) {
            wasSplit = true;
            auto result = this->splitBlock(block, std::distance(this->states.cbegin(), upperBound));
            newBlockCallback(**result.first);
        }
        it = upperBound;
    } while (upperBound != ite);

    // Finally, repair the positions mapping.
    mapStatesToPositions(this->states.begin() + originalBegin, this->states.begin() + originalEnd);

    return wasSplit;
}

// Splits the block by sorting the states according to the given function and then identifying the split
// points.
template<typename DataType>
bool Partition<DataType>::splitBlock(Block<DataType>& block,
                                     std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less) {
    return this->splitBlock(block, less, [](Block<DataType>&) {});
}

template<typename DataType>
bool Partition<DataType>::split(std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less,
                                std::function<void(Block<DataType>&)> const& newBlockCallback) {
    bool result = false;
    // Since the underlying storage of the blocks may change during iteration, we remember the current size
    // and iterate over indices. This assumes that new blocks will be added at the end of the blocks vector.
    std::size_t currentSize = this->size();
    for (uint_fast64_t index = 0; index < currentSize; ++index) {
        result |= splitBlock(*blocks[index], less, newBlockCallback);
    }
    return result;
}

template<typename DataType>
bool Partition<DataType>::split(std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less) {
    return this->split(less, [](Block<DataType>&) {});
}

template<typename DataType>
void Partition<DataType>::splitStates(Block<DataType>& block, storm::storage::BitVector const& states) {
    this->splitBlock(
        block, [&states](storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) { return states.get(a) && !states.get(b); });
}

template<typename DataType>
void Partition<DataType>::splitStates(storm::storage::BitVector const& states) {
    this->split(
        [&states](storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) { return states.get(a) && !states.get(b); });
}

template<typename DataType>
void Partition<DataType>::sortBlock(Block<DataType> const& block) {
    std::sort(this->begin(block), this->end(block),
              [](storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) { return a < b; });
    mapStatesToPositions(block);
}

template<typename DataType>
std::vector<std::unique_ptr<Block<DataType>>> const& Partition<DataType>::getBlocks() const {
    return this->blocks;
}

template<typename DataType>
std::vector<std::unique_ptr<Block<DataType>>>& Partition<DataType>::getBlocks() {
    return this->blocks;
}

template<typename DataType>
bool Partition<DataType>::check() const {
    for (uint_fast64_t state = 0; state < this->positions.size(); ++state) {
        STORM_LOG_ASSERT(this->states[this->positions[state]] == state, "Position mapping corrupted.");
    }
    for (auto const& blockPtr : this->blocks) {
        STORM_LOG_ASSERT(blockPtr->check(), "Block corrupted.");
        for (auto stateIt = this->begin(*blockPtr), stateIte = this->end(*blockPtr); stateIt != stateIte; ++stateIt) {
            STORM_LOG_ASSERT(this->stateToBlockMapping[*stateIt] == blockPtr.get(), "Block mapping corrupted.");
        }
    }
    return true;
}

template<typename DataType>
void Partition<DataType>::print() const {
    for (auto const& block : this->blocks) {
        block->print(*this);
    }
    std::cout << "states in partition\n";
    for (auto const& state : states) {
        std::cout << state << " ";
    }
    std::cout << "\npositions: \n";
    for (auto const& index : positions) {
        std::cout << index << " ";
    }
    std::cout << "\nstate to block mapping: \n";
    for (auto const& block : stateToBlockMapping) {
        std::cout << block << "[" << block->getId() << "] ";
    }
    std::cout << "\nsize: " << blocks.size() << '\n';
    STORM_LOG_ASSERT(this->check(), "Partition corrupted.");
}

template<typename DataType>
std::size_t Partition<DataType>::size() const {
    return blocks.size();
}

template class Partition<DeterministicBlockData>;

}  // namespace bisimulation
}  // namespace storage
}  // namespace storm
