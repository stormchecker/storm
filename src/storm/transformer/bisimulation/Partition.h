#pragma once

#include <cstddef>
#include <map>
#include <ranges>
#include <set>
#include <span>

#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

/*!
 * Represents a partition of a set of consecutive indices.
 * Allows efficient access to the blocks of the partition and the elements in each block.
 * Memory is cache-friendly and linear in the number of elements (independent of block count).
 * The downside is that merging blocks is not supported.
 */
class Partition {
   public:
    /// The type used to index the elements of the partition.
    using ElementIndex = uint64_t;

    /// A contiguous range of elements that form a (possibly proper super-) block of the partition.
    /// Note: Splitting a block changes the order of the contained elements.
    using Block = std::span<ElementIndex const>;

    /*!
     * Orders blocks by their size (ascending) and, for equally sized blocks, by their location in memory.
     * Used to keep containers of blocks (e.g. OrderedBlockSet) ordered by size.
     */
    struct BlockCompare {
        bool operator()(Block const& lhs, Block const& rhs) const {
            if (lhs.size() < rhs.size()) {
                return true;
            }
            if (lhs.size() > rhs.size()) {
                return false;
            }
            return lhs.data() < rhs.data();
        }
    };

    /*!
     * A set of blocks, ordered by their size (smallest first).
     */
    using OrderedBlockSet = std::set<Block, BlockCompare>;

    /*!
     * A map of blocks, ordered by their size (smallest first).
     */
    template<typename T>
    using OrderedBlockMap = std::map<Block, T, BlockCompare>;

    /*!
     * Set of blocks that are no proper superblock.
     * It is illegal to insert a block that currently is a proper superblock.
     * Insertion and popping an arbitrary element has (amortized) constant time.
     * However, the set requires Theta(numElements) space.
     */
    class NonSuperBlockSet {
       public:
        /*!
         * Creates an empty set associated with the given partition.
         */
        explicit NonSuperBlockSet(Partition const& partition);

        /*!
         * @return the number of blocks currently contained in the set.
         */
        std::size_t size() const;

        /*!
         * @return true iff the set does not contain any block.
         */
        bool empty() const;

        /*!
         * @return true iff the given block is currently contained in the set.
         */
        bool contains(Block const& block) const;

        /*!
         * Inserts the given block into the set. Does nothing if the block is already contained.
         * @note the given block must not be a proper superblock.
         */
        void insert(Block const& block);

        /*!
         * Removes and returns an arbitrary block from the set.
         * @note the set must not be empty.
         */
        Block pop();

       private:
        Partition const& partition;
        std::vector<uint64_t> containedBlockIndices;
        storm::storage::BitVector blockIndices;
    };

    Partition() = default;
    Partition(Partition const& other) = default;
    Partition& operator=(Partition const& other) = default;
    Partition(Partition&& other) noexcept = default;
    Partition& operator=(Partition&& other) = default;

    /*!
     * Creates the partition { {0,1,...,numElements-1} } with a single block that contains all elements
     *
     * @param numElements the number of elements.
     */
    explicit Partition(ElementIndex numElements);

    /*!
     * Retrieves the number of blocks. Only counts those blocks that do not contain other (smaller) blocks
     */
    std::size_t getNumberOfBlocks() const;

    /*!
     * Retrieves the number of elements.
     */
    std::size_t getNumberOfElements() const;

    /*!
     * @return the block that contains all elements, i.e., the universal block.
     */
    Block getUniversalBlock() const;

    /*!
     * @return the block that contains the given element.
     * @note the lifetime of the returned object is limited to the lifetime of this partition.
     * @note subsequent operations on this partition (e.g. split) may change the order of the elements in the block, but do not affect the contents.
     * This means that changing the partition while iterating over block contents, i.e., `for (auto e : block) { partition.split(...) ... }` is not safe.
     */
    Block getBlockOfElement(ElementIndex element) const;

    /*!
     * @return true iff the given block contains the given element.
     * @note this has constant runtime, i.e., is preferable over a linear search over the block
     */
    bool contains(Block const& block, ElementIndex element) const;

    /*!
     * @return true iff all elements of the given subblock are contained in the given superblock.
     * @note this has constant runtime, i.e., is preferable over a linear search over the block
     */
    bool isSubBlockOf(Block const& subblock, Block const& superblock) const;

    /*!
     * @return true iff the two given blocks are the same block of this partition.
     * @note this has constant runtime, i.e., is preferable over comparing the contents of the blocks
     */
    bool isEqualBlock(Block const& lhs, Block const& rhs) const;

    /*!
     * @return true iff the two given elements are in the same block.
     * @note this has constant runtime and does not have to look up the blocks themselves
     */
    bool isSameBlock(ElementIndex const& element1, ElementIndex const& element2) const;

    /*!
     * @return true iff the (smallest known) block of the given element coincides with the given block.
     * @note isBlockOfElement(b,e) implies contains(b,e), but the converse is not true as the former only holds if b is the smallest known block containing e.
     */
    bool isBlockOfElement(Block const& block, ElementIndex const& element) const;

    /*!
     * Checks if the given block is a valid block in the partition. Useful for sanity checks (e.g. via assertions). Does not assert itself.
     */
    bool checkBlockValidity(Block const& block) const;

    /*!
     * @return true iff the given block contains multiple sub-block. For example, {1,2,3} is a (proper) super block of {1,2} and {3}.
     */
    bool isProperSuperBlock(Block const& block) const;

    /*!
     * Applies the given function on each block in the partition.
     * @note: Splitting the currently processed block in the function *is* valid. However, the resulting sub-blocks are not processed recursively.
     */
    template<typename Func>
        requires std::invocable<Func, Block>
    void forEachBlock(Func const& f) const {
        forEachSubBlock(getUniversalBlock(), f);
    }

    /*!
     * Applies the given function on each block in the given super block.
     * For example, if superBlock = {0,1,2,3,4} and the partition currently has blocks {{0}, {1,2,3}, {4}, ...},
     * the function will be called with the blocks {0}, {1,2,3}, and {4}.
     * @note: Splitting the currently processed block in the function *is* valid. However, the resulting sub-blocks are not processed recursively.
     */
    template<typename Func>
        requires std::invocable<Func, Block>
    void forEachSubBlock(Block const& superBlock, Func const& f) const {
        STORM_LOG_ASSERT(!superBlock.empty(), "Superblock is empty.");
        auto superBlockEnd = getBlockIndex(superBlock) + superBlock.size();
        for (BlockIndex blockStart = getBlockIndex(superBlock); blockStart < superBlockEnd;) {
            auto const& subBlock = getBlockOfElement(blockContents[blockStart]);
            blockStart += subBlock.size();
            f(subBlock);
        };
    }

    /*!
     * Splits the given block according to the given order.
     * Specifically, the elements in the block are sorted according to the given order and then the block is split into
     * multiple blocks, divided at every position where the order changes.
     * @param less must define a transitive order (required for well-defined sorting)
     * @return true iff the block was split, i.e. if the input block is now a proper super block.
     */
    template<typename SplittingOrder>
        requires std::invocable<SplittingOrder, ElementIndex, ElementIndex>
    bool splitBlockByOrder(Block const& block, SplittingOrder const& less) {
        return splitBlockByOrder(block, less, less);
    }

    /*!
     * Splits the given block according to the given order and splitting condition.
     * Specifically, the elements in the block are first sorted according to the given order resulting in a (sorted) block [e_0, e_1, e_2, ..., e_m].
     * Then, indices i_0, i_1, ..., i_k are determined such that
     * - i_0 = 0 and
     * - i_j > i_{j-1} is the smallest number such that condition(e_i_{j-1}, e_{i_j}) is true (or i_j = m+1 if no such number exists).
     * We then split the block into subblocks [e_i_{j-1}, ..., e_(i_j-1)]
     * @param less must define a strict weak order (required for well-defined sorting)
     * @param splitCondition returns true for elements that should not be in the same block as explained above
     * @return true iff the block was split, i.e. if the input block is now a proper super block.
     */
    template<typename Order, typename Condition>
        requires std::invocable<Order, ElementIndex, ElementIndex> && std::invocable<Condition, ElementIndex, ElementIndex>
    bool splitBlockByOrder(Block const& block, Order const& less, Condition const& splitCondition) {
        if (block.size() <= 1) {
            return false;  // nothing to do
        }

        // Sort the contents of the block first
        STORM_LOG_ASSERT(!isProperSuperBlock(block), "Tried to split a block that consists of multiple sub-blocks.");
        auto const blockStart = getBlockIndex(block);

        auto const blockEnd = blockStart + block.size();
        std::sort(blockContents.begin() + blockStart, blockContents.begin() + blockEnd, less);

        // update the inverse after sorting
        for (ElementIndex i = blockStart; i < blockEnd; ++i) {
            blockContentsInverse[blockContents[i]] = i;
        }

        // Catch the special case where there is no split
        if (!splitCondition(blockContents[blockStart], blockContents[blockEnd - 1])) {
            return false;  // nothing to do
        }

        // helper function to find the end index of a current block
        auto getEndOfBlock = [this, splitCondition, blockEnd](BlockIndex const currIndex) {
            for (auto i = currIndex + 1; i < blockEnd; ++i) {
                if (splitCondition(blockContents[currIndex], blockContents[i])) {
                    return i;
                }
            }
            return blockEnd;
        };

        // Now create new blocks whenever the order changes
        for (auto newBlockIndex = blockStart; newBlockIndex < blockEnd;) {
            auto const newBlockEnd = getEndOfBlock(newBlockIndex);
            registerNewBlock(newBlockIndex, newBlockEnd);
            newBlockIndex = newBlockEnd;
            ++numBlocks;
        }
        --numBlocks;  // the old superblock is no longer a block as counted by this partition
        STORM_LOG_ASSERT(isProperSuperBlock(block), "Partition in inconsistent state: Block was not split into multiple sub-blocks.");
        STORM_LOG_ASSERT(checkBlockValidity(block), "Partition in inconsistent state: Block is not valid.");
        return true;  // there must have been a split because the case without a split is already caught above
    }

    /*!
     * Splits the given block by anchor-based clustering: repeatedly picks the first not-yet-grouped element as the anchor of a new group, moves every
     * remaining not-yet-grouped element e with !condition(anchor, e) into that group (wherever it currently sits in the block), and finalizes the group
     * once no more such elements remain; then continues with the next not-yet-grouped element as the next anchor.
     * Unlike splitBlockByOrder, this does not require condition to be consistent with any (total or weak) order over the block's elements.
     * @param condition returns true for pairs of elements that should not be in the same block; only ever invoked with the (still not-yet-grouped) anchor
     * of the group currently being built as the first argument.
     * @return true iff the block was split, i.e. if the input block is now a proper super block.
     */
    template<typename Condition>
        requires std::invocable<Condition, ElementIndex, ElementIndex>
    bool splitBlockByClustering(Block const& block, Condition const& condition) {
        STORM_LOG_ASSERT(!isProperSuperBlock(block), "Tried to split a block that consists of multiple sub-blocks.");
        if (block.size() <= 1) {
            return false;  // nothing to do
        }

        auto const blockStart = getBlockIndex(block);
        auto const blockEnd = blockStart + block.size();

        // Swaps the elements in [start, blockEnd) so that those compatible (!condition) with the anchor at start directly follow it.
        // @return mid such that all elements in [start, mid) are compatible with the anchor (located at start) and those at [mid, blockEnd) are not.
        auto const partitionBlock = [this, &condition, &blockEnd](BlockIndex const start) {
            auto const anchor = blockContents[start];
            auto l = start + 1;
            auto r = blockEnd - 1;
            // Loop invariant: all elements at position in (start, l) are compatible with anchor, all elements at position > r are not.
            while (l <= r) {
                while (l <= r && !condition(anchor, std::as_const(blockContents[l]))) {
                    ++l;
                }
                if (l > r) {
                    break;
                }
                while (l < r && condition(anchor, std::as_const(blockContents[r]))) {
                    --r;
                }
                if (l == r) {
                    --r;
                    break;
                }
                std::swap(blockContents[l], blockContents[r]);
                blockContentsInverse[blockContents[l]] = l;
                blockContentsInverse[blockContents[r]] = r;
                ++l;
                --r;
            }
            return l;
        };

        for (uint64_t start = blockStart; start < blockEnd;) {
            auto const currEnd = partitionBlock(start);
            // Catch the special case where there is no split
            if (start == blockStart && currEnd == blockEnd) {
                return false;  // nothing to do
            }
            registerNewBlock(start, currEnd);
            start = currEnd;
            ++numBlocks;
        }
        --numBlocks;  // the old superblock is no longer a block as counted by this partition
        STORM_LOG_ASSERT(isProperSuperBlock(block), "Partition in inconsistent state: Block was not split into multiple sub-blocks.");
        STORM_LOG_ASSERT(checkBlockValidity(block), "Partition in inconsistent state: Block is not valid.");
        return true;  // there must have been a split because the case without a split is already caught above
    }

    /*!
     * Splits the given block according to the given predicate.
     * Specifically, the elements in the block are swapped around, such that all elements where the predicate f evaluates to false come first.
     * Then, two sub-blocks are created accordingly. If all elements are true/false, no splitting is performed.
     * @return a pair containing first the 'false' sub-block and then the 'true' sub-block. One of them can be empty.
     */
    template<typename SplittingPredicate>
        requires std::predicate<SplittingPredicate, ElementIndex>
    std::pair<Block, Block> splitBlockByPredicate(Block const& block, SplittingPredicate const& f) {
        STORM_LOG_ASSERT(!isProperSuperBlock(block), "Tried to split a block that consists of multiple sub-blocks.");
        STORM_LOG_ASSERT(!block.empty(), "Tried to split an empty block");

        // swap the block contents so that all elements e with f(e) == false come first
        auto const blockStart = getBlockIndex(block);
        auto const blockEnd = blockStart + block.size();
        auto l = blockStart;
        auto r = blockEnd - 1;
        // Loop invariant: all elements at position < l are false, all elements at position > r are true
        while (l <= r) {
            while (l <= r && !f(std::as_const(blockContents[l]))) {
                ++l;
            }
            if (l > r) {
                break;  // no more elements to swap
            }
            // At this point we know that f(blockContents[l]) == true
            while (l < r && f(std::as_const(blockContents[r]))) {
                --r;
            }
            if (l == r) {
                // We have f(blockContents[r]) == f(blockContents[l]) == true
                --r;
                break;  // l > r holds now
            } else if (l < r) {
                std::swap(blockContents[l], blockContents[r]);
                blockContentsInverse[blockContents[l]] = l;
                blockContentsInverse[blockContents[r]] = r;
                ++l;
                --r;
            }
        }
        STORM_LOG_ASSERT(l == r + 1, "Unexpected indices");

        // Handle cases where there is no split
        if (l == blockStart) {
            return {Block{}, block};  // all elements are true
        } else if (l == blockEnd) {
            return {block, Block{}};  // all elements are false
        }

        // Perform a split
        Block noBlock = registerNewBlock(blockStart, l);
        Block yesBlock = registerNewBlock(l, blockEnd);
        ++numBlocks;  // We have split a single block into two blocks, so the total number increments by 1
        return {noBlock, yesBlock};
    }

    /*!
     * Splits the given block according to the given range of elements.
     * Specifically, the elements in the block are swapped around, such that all elements that are not in the given range come first.
     * Then, two sub-blocks are created accordingly. If the range is empty or contains all elements of the block, no splitting is performed.
     * @param r the input range.
     * @return a pair containing first the sub-block that is not in the range and then the sub-block in the range. One of them can be empty.
     */
    template<typename SplitRange>
        requires std::ranges::input_range<std::remove_reference_t<SplitRange>> &&
                 std::same_as<std::ranges::range_value_t<std::remove_reference_t<SplitRange>>, ElementIndex>
    std::pair<Block, Block> splitBlockByRange(Block const& block, SplitRange&& r) {
        STORM_LOG_ASSERT(!isProperSuperBlock(block), "Tried to split a block that consists of multiple sub-blocks.");
        STORM_LOG_ASSERT(!block.empty(), "Tried to split an empty block");

        auto const blockStart = getBlockIndex(block);
        auto const blockEnd = blockStart + block.size();

        // We split the block into a "no"-part and a "yes"-part

        auto yesBlockStart = blockEnd;
        for (ElementIndex const element : r) {
            auto const src = blockContentsInverse[element];
            if (src < blockStart || src >= yesBlockStart) {
                continue;  // element is either not in the block or already occurred before (duplicate in r)
            }
            // swap the element to the end of the block, into the 'yes' part
            --yesBlockStart;
            std::swap(blockContents[src], blockContents[yesBlockStart]);
            blockContentsInverse[blockContents[src]] = src;
            blockContentsInverse[blockContents[yesBlockStart]] = yesBlockStart;
        }

        // Handle cases where there is no split
        if (yesBlockStart == blockStart) {
            return {Block{}, block};  // all elements are in the range
        } else if (yesBlockStart == blockEnd) {
            return {block, Block{}};  // no elements are in the range
        }

        // Perform a split
        Block noBlock = registerNewBlock(blockStart, yesBlockStart);
        Block yesBlock = registerNewBlock(yesBlockStart, blockEnd);
        ++numBlocks;  // We have split a single block into two blocks, so the total number increments by 1
        return {noBlock, yesBlock};
    }

   private:
    /*!
     * The (internal) index of a block in the partition.
     * @note we make this private since we do not want to expose the internal representation of the partition.
     */
    using BlockIndex = uint64_t;

    /*!
     * @return the index at which the given block currently starts within blockContents.
     * @note the meaning of a given index changes as the partition evolves: the block currently starting at that index may later become a proper
     * superblock once it is split further. We therefore do not want to expose block indices.
     */
    BlockIndex getBlockIndex(Block const& block) const;

    /*!
     * @return the (currently smallest known) block that starts at the given index.
     * @note blockIndex must be the start index of a currently valid block, cf. registerNewBlock.
     */
    Block getBlockFromIndex(BlockIndex blockIndex) const;

    /*!
     * Registers a (new) block spanning blockContents[start, end) and updates the internal bookkeeping accordingly.
     * If start has not been used as a block's start index before, elementToBlockIndex is updated for all elements of the new block.
     * Otherwise, the block at start is assumed to merely shrink (i.e. the new block must be a sub-block of the block that was previously registered at
     * start), so elementToBlockIndex does not need to be touched.
     * @note When splitting a block, all sub-blocks need to be registered to avoid putting the partition into an inconsistent state.
     * @return the newly registered block.
     */
    Block registerNewBlock(BlockIndex const start, BlockIndex const end);

    /// The elements of the partition. Each block occupies a contiguous range within this vector; a block is identified by the index at which it starts
    /// and its size (cf. blockSizes).
    std::vector<ElementIndex> blockContents;

    /// The inverse of blockContents, i.e. blockContents[blockContentsInverse[e]] == e for every element e. Used to quickly find the position of an element in
    /// its block.
    std::vector<BlockIndex> blockContentsInverse;

    /// Maps each element to the start index (within blockContents) of the finest currently known block containing it.
    std::vector<BlockIndex> elementToBlockIndex;

    /// For every index i that currently is the start index of a block, blockSizes[i] holds that block's current size.
    /// A value of 0 means that i has never been used as a block's start index.
    std::vector<std::size_t> blockSizes;

    /// The current number of blocks in the partition, cf. getNumberOfBlocks.
    uint64_t numBlocks = 0;
};

std::ostream& operator<<(std::ostream& os, const Partition& partition);

}  // namespace storm::bisimulation
