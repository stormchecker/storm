#include "storm/transformer/bisimulation/Partition.h"

namespace storm::bisimulation {

Partition::NonSuperBlockSet::NonSuperBlockSet(Partition const& partition) : partition(partition), blockIndices(partition.getNumberOfElements(), false) {}

std::size_t Partition::NonSuperBlockSet::size() const {
    return containedBlockIndices.size();
}

bool Partition::NonSuperBlockSet::empty() const {
    return containedBlockIndices.empty();
}

bool Partition::NonSuperBlockSet::contains(Block const& block) const {
    return blockIndices.get(partition.getBlockIndex(block));
}

void Partition::NonSuperBlockSet::insert(Block const& block) {
    STORM_LOG_ASSERT(!block.empty(), "Cannot insert empty block.");
    STORM_LOG_ASSERT(!partition.isProperSuperBlock(block), "Cannot insert block that is a proper superblock.");
    if (BlockIndex i = partition.getBlockIndex(block); !blockIndices.get(i)) {
        blockIndices.set(i, true);
        containedBlockIndices.push_back(i);
    }
}

Partition::Block Partition::NonSuperBlockSet::pop() {
    STORM_LOG_ASSERT(!empty(), "Cannot pop from empty blockset.");
    BlockIndex const i = containedBlockIndices.back();
    blockIndices.set(i, false);
    containedBlockIndices.pop_back();
    return partition.getBlockFromIndex(i);
}

Partition::Partition(ElementIndex numElements) : numBlocks(1) {
    auto indexRange = std::ranges::iota_view<ElementIndex, ElementIndex>(0, numElements);
    blockContents.assign(indexRange.begin(), indexRange.end());
    blockContentsInverse = blockContents;
    elementToBlockIndex.assign(numElements, 0);
    blockSizes.assign(numElements, 0);
    blockSizes[0] = blockContents.size();
}

std::size_t Partition::getNumberOfBlocks() const {
    STORM_LOG_ASSERT(
        [this]() {
            std::size_t blocksCount = 0;
            forEachBlock([&blocksCount](const Block& block) { ++blocksCount; });
            return blocksCount;
        }() == numBlocks,
        "The cached number of blocks is inconsistent with the actual number of blocks");
    return numBlocks;
}

std::size_t Partition::getNumberOfElements() const {
    return elementToBlockIndex.size();
}

Partition::Block Partition::getUniversalBlock() const {
    return Block(blockContents.begin(), blockContents.end());
}

Partition::Block Partition::getBlockOfElement(ElementIndex element) const {
    STORM_LOG_ASSERT(element < elementToBlockIndex.size(), "Element index out of bounds");
    return getBlockFromIndex(elementToBlockIndex[element]);
}

bool Partition::contains(ElementIndex element, Block const& block) const {
    // We use the fact that each pair of blocks is either disjoint or one is a subset of the other.
    return isSubBlockOf(getBlockOfElement(element), block);
}

bool Partition::isSubBlockOf(Block const& subblock, Block const& superblock) const {
    STORM_LOG_ASSERT(checkBlockValidity(subblock), "Invalid subblock.");
    STORM_LOG_ASSERT(checkBlockValidity(superblock), "Invalid superblock.");
    // Check if subblock is a subspan of superblock
    return subblock.data() >= superblock.data() && (subblock.data() + subblock.size()) <= (superblock.data() + superblock.size());
}

bool Partition::isEqualBlock(Block const& lhs, Block const& rhs) const {
    STORM_LOG_ASSERT(checkBlockValidity(lhs), "Invalid block.");
    STORM_LOG_ASSERT(checkBlockValidity(rhs), "Invalid block.");
    return lhs.data() == rhs.data() && lhs.size() == rhs.size();
}

bool Partition::isSameBlock(ElementIndex const& element1, ElementIndex const& element2) const {
    STORM_LOG_ASSERT(element1 < elementToBlockIndex.size() && element2 < elementToBlockIndex.size(), "Element index out of bounds");
    // Each block is identified by the index at which it starts, so the elements are in the same block iff their (finest) blocks start at the same index.
    return elementToBlockIndex[element1] == elementToBlockIndex[element2];
}

bool Partition::isBlockOfElement(Block const& block, ElementIndex const& element) const {
    Block const eBlock = getBlockOfElement(element);
    return eBlock.data() == block.data() && eBlock.size() == block.size();
}

Partition::BlockIndex Partition::getBlockIndex(Block const& block) const {
    STORM_LOG_ASSERT(checkBlockValidity(block), "Tried to get the index of an invalid block.");
    return std::distance(blockContents.data(), block.data());
}

Partition::Block Partition::getBlockFromIndex(BlockIndex blockIndex) const {
    STORM_LOG_ASSERT(blockIndex < blockContents.size(), "Block index out of bounds");
    STORM_LOG_ASSERT(blockSizes[blockIndex] > 0, "Block index points to a non-existent block");
    return Block(blockContents.data() + blockIndex, blockSizes[blockIndex]);
}

Partition::Block Partition::registerNewBlock(BlockIndex const start, BlockIndex const end) {
    STORM_LOG_ASSERT(start < end && end <= blockContents.size(), "Invalid block range");
    Block newBlock(blockContents.data() + start, end - start);
    // Shrinking an existing block doesn't require iterating over all block contents
    if (blockSizes[start] == 0) {
        for (ElementIndex const e : newBlock) {
            elementToBlockIndex[e] = start;
        }
    } else {
        STORM_LOG_ASSERT(blockSizes[start] >= newBlock.size(), "Shrinking a block should not increase its size");
    }
    blockSizes[start] = newBlock.size();
    return newBlock;
}

bool Partition::checkBlockValidity(Block const& block) const {
    // block must not be empty
    if (block.empty())
        return false;
    // the block refers to this partition
    if (block.data() < blockContents.data() || block.data() + block.size() > blockContents.data() + blockContents.size())
        return false;

    BlockIndex const blockIndex = std::distance(blockContents.data(), block.data());
    BlockIndex const blockEndIndex = blockIndex + block.size();

    // the block has a stored size and is either the last block or ends where another block starts
    if (!(blockSizes[blockIndex] > 0 && (blockEndIndex == blockContents.size() || blockSizes[blockEndIndex] != 0)))
        return false;

    // The index of the first element must always match this block index (even if the element belongs to a subblock, it must have the same index)
    if (elementToBlockIndex[block[0]] != blockIndex)
        return false;

    // Check if the inverse mapping is consistent for the first and the last element (must actually be true for all elements but checking that is too costly)
    if (&blockContents[blockContentsInverse[block.front()]] != &block.front() || &blockContents[blockContentsInverse[block.back()]] != &block.back())
        return false;

    if (isBlockOfElement(block, block[0])) {
        // this block has no proper sub-block
        // The stored sizes should coincide
        if (blockSizes[blockIndex] != block.size())
            return false;
        // all elements should point to this blockIndex
        // we only check this for the first and last element.
        return (elementToBlockIndex[block.front()] == blockIndex) && (elementToBlockIndex[block.back()] == blockIndex);
    } else {
        // this block has proper sub-blocks.
        // The stored size should be strictly smaller
        if (blockSizes[blockIndex] >= block.size())
            return false;
        // The block index of all elements should be within the range of this block.
        return std::all_of(block.begin(), block.end(), [this, blockIndex, blockEndIndex](ElementIndex const& e) {
            // e's block index should be in the range of this block
            return elementToBlockIndex[e] >= blockIndex && elementToBlockIndex[e] < blockEndIndex;
        });
    }
}

bool Partition::isProperSuperBlock(Block const& block) const {
    STORM_LOG_ASSERT(checkBlockValidity(block), "Tried to check if an invalid block is a proper superblock.");
    // block is a proper super-block iff it has a proper sub-block iff the block of some element is not equal to block
    return !isBlockOfElement(block, block[0]);
}

std::ostream& operator<<(std::ostream& os, const Partition& partition) {
    os << "Partition (" << partition.getNumberOfBlocks() << " block(s), " << partition.getNumberOfElements() << " element(s)): {\n";
    partition.forEachBlock([&os](Partition::Block const& block) {
        os << "\t{";
        for (bool first = true; typename Partition::ElementIndex const e : block) {
            if (!first)
                os << ", ";
            first = false;
            os << e;
        }
        os << "}\n";
    });
    os << "}";
    return os;
}

}  // namespace storm::bisimulation
