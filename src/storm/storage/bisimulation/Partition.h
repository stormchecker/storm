#pragma once

#include <cstddef>
#include <memory>

#include "storm/storage/BitVector.h"
#include "storm/storage/bisimulation/Block.h"

namespace storm {
namespace storage {
namespace bisimulation {

template<typename DataType>
class Partition {
   public:
    /*!
     * Creates a partition with one block consisting of all the states.
     *
     * @param numberOfStates The number of states the partition holds.
     */
    Partition(std::size_t numberOfStates);

    /*!
     * Creates a partition with three blocks: one with all phi states, one with all psi states and one with
     * all other states. The former two blocks are marked as being absorbing, because their outgoing
     * transitions shall not be taken into account for future refinement.
     *
     * @param numberOfStates The number of states the partition holds.
     * @param prob0States The states which have probability 0 of satisfying phi until psi.
     * @param prob1States The states which have probability 1 of satisfying phi until psi.
     * @param representativeProb1State If the set of prob1States is non-empty, this needs to be a state
     * that is representative for this block in the sense that the state representing this block in the quotient
     * model will receive exactly the atomic propositions of the representative state.
     */
    Partition(std::size_t numberOfStates, storm::storage::BitVector const& prob0States, storm::storage::BitVector const& prob1States,
              std::optional<storm::storage::sparse::state_type> representativeProb1State);

    Partition() = default;
    Partition(Partition const& other) = default;
    Partition& operator=(Partition const& other) = default;
    Partition(Partition&& other) = default;
    Partition& operator=(Partition&& other) = default;

    // Retrieves the size of the partition, i.e. the number of blocks.
    std::size_t size() const;

    // Prints the partition to the standard output.
    void print() const;

    // Splits the block at the given position and inserts a new block after the current one holding the rest
    // of the states.
    std::pair<typename std::vector<std::unique_ptr<Block<DataType>>>::iterator, bool> splitBlock(Block<DataType>& block,
                                                                                                 storm::storage::sparse::state_type position);

    // Sorts the given range of the partition according to the given order.
    void sortRange(storm::storage::sparse::state_type beginIndex, storm::storage::sparse::state_type endIndex,
                   std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less, bool updatePositions = true);

    // Sorts the block according to the given order.
    void sortBlock(Block<DataType>& block, std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less,
                   bool updatePositions = true);

    // Computes the start indices of equal ranges within the given range wrt. to the given less function.
    std::vector<uint_fast64_t> computeRangesOfEqualValue(
        uint_fast64_t startIndex, uint_fast64_t endIndex,
        std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less);

    // Splits the block by sorting the states according to the given function and then identifying the split
    // points. The callback function is called for every newly created block.
    bool splitBlock(Block<DataType>& block, std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less,
                    std::function<void(Block<DataType>&)> const& newBlockCallback);

    // Splits the block by sorting the states according to the given function and then identifying the split
    // points.
    bool splitBlock(Block<DataType>& block, std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less);

    // Splits all blocks by using the sorting-based splitting. The callback is called for all newly created
    // blocks.
    bool split(std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less,
               std::function<void(Block<DataType>&)> const& newBlockCallback);

    // Splits all blocks by using the sorting-based splitting.
    bool split(std::function<bool(storm::storage::sparse::state_type, storm::storage::sparse::state_type)> const& less);

    // Splits the block such that the resulting blocks contain only states in the given set or none at all.
    // If the block is split, the given block will contain the states *not* in the given set and the newly
    // created block will contain the states *in* the given set.
    void splitStates(Block<DataType>& block, storm::storage::BitVector const& states);

    /*!
     * Splits all blocks of the partition such that afterwards all blocks contain only states within the given
     * set of states or no such state at all.
     */
    void splitStates(storm::storage::BitVector const& states);

    // Sorts the block based on the state indices.
    void sortBlock(Block<DataType> const& block);

    // Retrieves the blocks of the partition.
    std::vector<std::unique_ptr<Block<DataType>>> const& getBlocks() const;

    // Retrieves the blocks of the partition.
    std::vector<std::unique_ptr<Block<DataType>>>& getBlocks();

    // Checks the partition for internal consistency.
    bool check() const;

    // Returns an iterator to the beginning of the states of the given block.
    std::vector<storm::storage::sparse::state_type>::iterator begin(Block<DataType> const& block);

    // Returns an iterator to the beginning of the states of the given block.
    std::vector<storm::storage::sparse::state_type>::const_iterator begin(Block<DataType> const& block) const;

    // Returns an iterator to the beginning of the states of the given block.
    std::vector<storm::storage::sparse::state_type>::iterator end(Block<DataType> const& block);

    // Returns an iterator to the beginning of the states of the given block.
    std::vector<storm::storage::sparse::state_type>::const_iterator end(Block<DataType> const& block) const;

    // Returns an iterator to the beginning of the states in the partition.
    std::vector<storm::storage::sparse::state_type>::iterator begin();

    // Returns an iterator to the beginning of the states in the partition.
    std::vector<storm::storage::sparse::state_type>::const_iterator begin() const;

    // Returns an iterator to the end of the states in the partition.
    std::vector<storm::storage::sparse::state_type>::iterator end();

    // Returns an iterator to the end of the states in the partition.
    std::vector<storm::storage::sparse::state_type>::const_iterator end() const;

    // Swaps the positions of the two given states.
    void swapStates(storm::storage::sparse::state_type state1, storm::storage::sparse::state_type state2);

    // Retrieves the block of the given state.
    Block<DataType>& getBlock(storm::storage::sparse::state_type state);

    // Retrieves the block of the given state.
    Block<DataType> const& getBlock(storm::storage::sparse::state_type state) const;

    // Retrieves the position of the given state.
    storm::storage::sparse::state_type const& getPosition(storm::storage::sparse::state_type state) const;

    // Sets the position of the state to the given position.
    storm::storage::sparse::state_type const& getState(storm::storage::sparse::state_type position) const;

    // Updates the block mapping for the given range of states to the specified block.
    void mapStatesToBlock(Block<DataType>& block, std::vector<storm::storage::sparse::state_type>::iterator first,
                          std::vector<storm::storage::sparse::state_type>::iterator last);

    // Update the state to position for the states in the given block.
    void mapStatesToPositions(Block<DataType> const& block);

    // Update the state to position for the states in the given range.
    void mapStatesToPositions(std::vector<storm::storage::sparse::state_type>::const_iterator first,
                              std::vector<storm::storage::sparse::state_type>::const_iterator last);

    // Swaps the positions of the two states given by their positions.
    void swapStatesAtPositions(storm::storage::sparse::state_type position1, storm::storage::sparse::state_type position2);

   private:
    // The of blocks in the partition.
    std::vector<std::unique_ptr<Block<DataType>>> blocks;

    // A mapping of states to their blocks.
    std::vector<Block<DataType>*> stateToBlockMapping;

    // A vector containing all the states. It is ordered in a way such that the blocks only need to define
    // their start/end indices.
    std::vector<storm::storage::sparse::state_type> states;

    // This vector keeps track of the position of each state in the state vector.
    std::vector<storm::storage::sparse::state_type> positions;
};
}  // namespace bisimulation
}  // namespace storage
}  // namespace storm
