#pragma once

#include <cstddef>

#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace storage {
namespace bisimulation {
// Forward-declare partition class.
template<typename DataType>
class Partition;

template<typename DataType>
class Block {
   public:
    friend class Partition<DataType>;

    // Creates a new block with the given begin and end.
    Block(storm::storage::sparse::state_type beginIndex, storm::storage::sparse::state_type endIndex, Block* previous, Block* next, std::size_t id);

    Block() = default;
    Block(Block const& other) = default;
    Block& operator=(Block const& other) = default;
    Block(Block&& other) = default;
    Block& operator=(Block&& other) = default;

    bool operator==(Block const& other) const;
    bool operator!=(Block const& other) const;

    // Prints the block to the standard output.
    void print(Partition<DataType> const& partition) const;

    // Returns the beginning index of the block.
    storm::storage::sparse::state_type getBeginIndex() const;

    // Returns the beginning index of the block.
    storm::storage::sparse::state_type getEndIndex() const;

    // Gets the next block (if there is one).
    Block const& getNextBlock() const;

    // Gets a pointer to the next block (if there is one).
    Block* getNextBlockPointer();

    // Gets a pointer to the next block (if there is one).
    Block const* getNextBlockPointer() const;

    // Retrieves whether the block as a successor block.
    bool hasNextBlock() const;

    // Gets the next block (if there is one).
    Block const& getPreviousBlock() const;

    // Gets a pointer to the previous block (if there is one).
    Block* getPreviousBlockPointer();

    // Gets a pointer to the previous block (if there is one).
    Block const* getPreviousBlockPointer() const;

    // Retrieves whether the block as a successor block.
    bool hasPreviousBlock() const;

    // Checks consistency of the information in the block.
    bool check() const;

    // Retrieves the number of states in this block.
    std::size_t getNumberOfStates() const;

    // Retrieves the additional data associated with this block.
    DataType& data();

    // Retrieves the additional data associated with this block.
    DataType const& data() const;

    // Resets all markers.
    void resetMarkers();

    // Retrieves the ID of the block.
    std::size_t getId() const;

   private:
    // Sets the beginning index of the block.
    void setBeginIndex(storm::storage::sparse::state_type beginIndex);

    // Sets the end index of the block.
    void setEndIndex(storm::storage::sparse::state_type endIndex);

    // Pointers to the next and previous block.
    Block* nextBlock;
    Block* previousBlock;

    // The begin and end indices of the block in terms of the state vector of the partition.
    storm::storage::sparse::state_type beginIndex;
    storm::storage::sparse::state_type endIndex;

    // The ID of the block. This is only used for debugging purposes.
    std::size_t id;

    // A member that stores additional data that depends on the kind of bisimulation.
    DataType mData;
};
}  // namespace bisimulation
}  // namespace storage
}  // namespace storm
