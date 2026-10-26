#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "storm/models/sparse/Model.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
namespace storage {

/*!
 * This class represents the decomposition of a model into blocks which are of the template type.
 */
template<typename BlockType>
class Decomposition {
   public:
    typedef BlockType block_type;
    typedef typename std::vector<block_type>::iterator iterator;
    typedef typename std::vector<block_type>::const_iterator const_iterator;

    /*!
     * Creates an empty decomposition.
     */
    Decomposition();

    /*!
     * Default (virtual) deconstructor
     */
    virtual ~Decomposition() = default;

    /*!
     * Creates a decomposition by copying the given decomposition.
     *
     * @param other The decomposition to copy.
     */
    Decomposition(Decomposition const& other);

    /*!
     * Assigns the contents of the given decomposition to the current one by copying the contents.
     *
     * @param other The decomposition whose values to copy.
     * @return The current decomposition.
     */
    Decomposition& operator=(Decomposition const& other);

    /*!
     * Creates a decomposition by moving the given decomposition.
     *
     * @param other The decomposition to move.
     */
    Decomposition(Decomposition&& other);

    /*!
     * Assigns the contents of the given decomposition to the current one by moving the contents.
     *
     * @param other The decomposition whose values to move.
     * @return The current decomposition.
     */
    Decomposition& operator=(Decomposition&& other);

    /*!
     * Retrieves the number of blocks of this decomposition.
     *
     * @return The number of blocks of this decomposition.
     */
    std::size_t size() const;

    /*!
     * Checks if the decomposition is empty.
     *
     * @return True, if the decomposition is empty.
     */
    bool empty() const;

    /*!
     * Retrieves an iterator that points to the first block of this decomposition.
     *
     * @return An iterator that points to the first block of this decomposition.
     */
    iterator begin();

    /*!
     * Retrieves an iterator that points past the last block of this decomposition.
     *
     * @return An iterator that points past the last block of this decomposition.
     */
    iterator end();

    /*!
     * Retrieves a const iterator that points to the first block of this decomposition.
     *
     * @return A const iterator that points to the first block of this decomposition.
     */
    const_iterator begin() const;

    /*!
     * Retrieves a const iterator that points past the last block of this decomposition.
     *
     * @return A const iterator that points past the last block of this decomposition.
     */
    const_iterator end() const;

    /*!
     * Retrieves the block with the given index. If the index is out-of-bounds, an exception is thrown.
     *
     * @param index The index of the block to retrieve.
     * @return The block with the given index.
     */
    block_type const& getBlock(uint_fast64_t index) const;

    /*!
     * Retrieves the block with the given index. If the index is out-of-bounds, an exception is thrown.
     *
     * @param index The index of the block to retrieve.
     * @return The block with the given index.
     */
    block_type& getBlock(uint_fast64_t index);

    /*!
     * Retrieves the block with the given index. If the index is out-of-bounds, an behaviour is undefined.
     *
     * @param index The index of the block to retrieve.
     * @return The block with the given index.
     */
    block_type const& operator[](uint_fast64_t index) const;

    /*!
     * Retrieves the block with the given index. If the index is out-of-bounds, an behaviour is undefined.
     *
     * @param index The index of the block to retrieve.
     * @return The block with the given index.
     */
    block_type& operator[](uint_fast64_t index);

    // Declare the streaming operator as a friend function to enable output of decompositions.
    template<typename BlockTypePrime>
    friend std::ostream& operator<<(std::ostream& out, Decomposition<BlockTypePrime> const& decomposition);

    template<typename ValueType>
    storm::storage::SparseMatrix<ValueType> extractPartitionDependencyGraph(storm::storage::SparseMatrix<ValueType> const& matrix) const;

   protected:
    // The blocks of the decomposition.
    std::vector<block_type> blocks;
};
}  // namespace storage
}  // namespace storm
