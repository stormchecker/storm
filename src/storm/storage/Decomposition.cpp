#include "Decomposition.h"

#include <vector>

#include "storm/storage/MaximalEndComponent.h"
#include "storm/storage/StronglyConnectedComponent.h"
#include "storm/utility/constants.h"

namespace storm {
namespace storage {

template<typename BlockType>
Decomposition<BlockType>::Decomposition() : blocks() {
    // Intentionally left empty.
}

template<typename BlockType>
Decomposition<BlockType>::Decomposition(Decomposition const& other) : blocks(other.blocks) {
    // Intentionally left empty.
}

template<typename BlockType>
Decomposition<BlockType>& Decomposition<BlockType>::operator=(Decomposition const& other) {
    this->blocks = other.blocks;
    return *this;
}

template<typename BlockType>
Decomposition<BlockType>::Decomposition(Decomposition&& other) : blocks(std::move(other.blocks)) {
    // Intentionally left empty.
}

template<typename BlockType>
Decomposition<BlockType>& Decomposition<BlockType>::operator=(Decomposition&& other) {
    this->blocks = std::move(other.blocks);
    return *this;
}

template<typename BlockType>
std::size_t Decomposition<BlockType>::size() const {
    return blocks.size();
}

template<typename BlockType>
bool Decomposition<BlockType>::empty() const {
    return blocks.empty();
}

template<typename BlockType>
typename Decomposition<BlockType>::iterator Decomposition<BlockType>::begin() {
    return blocks.begin();
}

template<typename BlockType>
typename Decomposition<BlockType>::iterator Decomposition<BlockType>::end() {
    return blocks.end();
}

template<typename BlockType>
typename Decomposition<BlockType>::const_iterator Decomposition<BlockType>::begin() const {
    return blocks.begin();
}

template<typename BlockType>
typename Decomposition<BlockType>::const_iterator Decomposition<BlockType>::end() const {
    return blocks.end();
}

template<typename BlockType>
BlockType const& Decomposition<BlockType>::getBlock(uint_fast64_t index) const {
    return blocks.at(index);
}

template<typename BlockType>
BlockType& Decomposition<BlockType>::getBlock(uint_fast64_t index) {
    return blocks.at(index);
}

template<typename BlockType>
BlockType const& Decomposition<BlockType>::operator[](uint_fast64_t index) const {
    return blocks[index];
}

template<typename BlockType>
BlockType& Decomposition<BlockType>::operator[](uint_fast64_t index) {
    return blocks[index];
}

template<typename BlockType>
template<typename ValueType>
storm::storage::SparseMatrix<ValueType> Decomposition<BlockType>::extractPartitionDependencyGraph(storm::storage::SparseMatrix<ValueType> const& matrix) const {
    // First, we need to create a mapping of states to their block index, to ease the computation of dependency
    // transitions later.
    std::vector<uint_fast64_t> stateToBlockMap(matrix.getRowGroupCount());
    for (uint_fast64_t i = 0; i < this->size(); ++i) {
        for (auto state : this->getBlock(i)) {
            stateToBlockMap[state] = i;
        }
    }

    // The resulting sparse matrix will have as many rows/columns as there are blocks in the partition.
    storm::storage::SparseMatrixBuilder<ValueType> dependencyGraphBuilder(this->size(), this->size());

    for (uint_fast64_t currentBlockIndex = 0; currentBlockIndex < this->size(); ++currentBlockIndex) {
        // Get the next block.
        block_type const& block = this->getBlock(currentBlockIndex);

        // Now, we determine the blocks which are reachable (in one step) from the current block.
        storm::storage::FlatSet<uint_fast64_t> allTargetBlocks;
        for (auto state : block) {
            for (auto const& transitionEntry : matrix.getRowGroup(state)) {
                uint_fast64_t targetBlock = stateToBlockMap[transitionEntry.getColumn()];

                // We only need to consider transitions that are actually leaving the SCC.
                if (targetBlock != currentBlockIndex) {
                    allTargetBlocks.insert(targetBlock);
                }
            }
        }

        // Now we can just enumerate all the target blocks and insert the corresponding transitions.
        for (auto const& targetBlock : allTargetBlocks) {
            dependencyGraphBuilder.addNextValue(currentBlockIndex, targetBlock, storm::utility::one<ValueType>());
        }
    }

    return dependencyGraphBuilder.build();
}

template<typename BlockType>
std::ostream& operator<<(std::ostream& out, Decomposition<BlockType> const& decomposition) {
    out << "[";
    if (decomposition.size() > 0) {
        for (uint_fast64_t blockIndex = 0; blockIndex < decomposition.size() - 1; ++blockIndex) {
            out << decomposition.blocks[blockIndex] << ", ";
        }
        out << decomposition.blocks.back();
    }
    out << "]";
    return out;
}

template storm::storage::SparseMatrix<double> Decomposition<StateBlock>::extractPartitionDependencyGraph(
    storm::storage::SparseMatrix<double> const& matrix) const;
template class Decomposition<StateBlock>;
template std::ostream& operator<<(std::ostream& out, Decomposition<StateBlock> const& decomposition);

template storm::storage::SparseMatrix<double> Decomposition<StronglyConnectedComponent>::extractPartitionDependencyGraph(
    storm::storage::SparseMatrix<double> const& matrix) const;
template class Decomposition<StronglyConnectedComponent>;
template std::ostream& operator<<(std::ostream& out, Decomposition<StronglyConnectedComponent> const& decomposition);

template class Decomposition<MaximalEndComponent>;
template std::ostream& operator<<(std::ostream& out, Decomposition<MaximalEndComponent> const& decomposition);
}  // namespace storage
}  // namespace storm
