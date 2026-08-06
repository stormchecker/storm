#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <unordered_set>
#include <vector>

namespace storm {
namespace storage {
class BitVector;
}

namespace dd {
class Odd {
   public:
    /*!
     * Constructs an offset-labeled DD with the given topmost DD node, else- and then-successor.
     *
     * @param elseNode The else-successor of this ODD node.
     * @param elseOffset The offset of the else-successor.
     * @param thenNode The then-successor of this ODD node.
     * @param thenOffset The offset of the then-successor.
     */
    Odd(std::shared_ptr<Odd> elseNode, uint_fast64_t elseOffset, std::shared_ptr<Odd> thenNode, uint_fast64_t thenOffset);

    // Instantiate all copy/move constructors/assignments with the default implementation.
    Odd() = default;
    Odd(Odd const& other) = default;
    Odd& operator=(Odd const& other) = default;
    Odd(Odd&& other) = default;
    Odd& operator=(Odd&& other) = default;

    /*!
     * Retrieves the then-successor of this ODD node.
     *
     * @return The then-successor of this ODD node.
     */
    Odd const& getThenSuccessor() const;

    /*!
     * Retrieves the else-successor of this ODD node.
     *
     * @return The else-successor of this ODD node.
     */
    Odd const& getElseSuccessor() const;

    /*!
     * Retrieves the else-offset of this ODD node.
     *
     * @return The else-offset of this ODD node.
     */
    uint_fast64_t getElseOffset() const;

    /*!
     * Sets the else-offset of this ODD node.
     *
     * @param newOffset The new else-offset of this ODD node.
     */
    void setElseOffset(uint_fast64_t newOffset);

    /*!
     * Retrieves the then-offset of this ODD node.
     *
     * @return The then-offset of this ODD node.
     */
    uint_fast64_t getThenOffset() const;

    /*!
     * Sets the then-offset of this ODD node.
     *
     * @param newOffset The new then-offset of this ODD node.
     */
    void setThenOffset(uint_fast64_t newOffset);

    /*!
     * Retrieves the total offset, i.e., the sum of the then- and else-offset.
     *
     * @return The total offset of this ODD.
     */
    uint_fast64_t getTotalOffset() const;

    /*!
     * Retrieves the size of the ODD. Note: the size is computed by a traversal, so this may be costlier than
     * expected.
     *
     * @return The size (in nodes) of this ODD.
     */
    uint_fast64_t getNodeCount() const;

    /*!
     * Retrieves the height of the ODD.
     *
     * @return The height of the ODD.
     */
    uint_fast64_t getHeight() const;

    /*!
     * Checks whether the given ODD node is a terminal node, i.e. has no successors.
     *
     * @return True iff the node is terminal.
     */
    bool isTerminalNode() const;

    /*!
     * Adds the old values to the new values. It does so by writing the old values at their correct positions
     * wrt. to the new ODD.
     *
     * @param newOdd The new ODD to use.
     * @param oldValues The old vector of values (which is being read).
     * @param newValues The new vector of values (which is being written).
     */
    template<typename ValueType>
    void expandExplicitVector(storm::dd::Odd const& newOdd, std::vector<ValueType> const& oldValues, std::vector<ValueType>& newValues) const;

    /*!
     * Translates the indices of the old ODD to that of the new ODD by calling the callback for each old-new
     * offset pair. Note that for each old offset, there may be multiple new offsets. The new ODD is expected
     * to extend the old ODD by adding layers *at the bottom*.
     *
     * @param newOdd The new ODD to use.
     * @param callback The callback function.
     */
    void oldToNewIndex(storm::dd::Odd const& newOdd, std::function<void(uint64_t oldOffset, uint64_t newOffset)> const& callback) const;

    /*!
     * Exports the ODD in the dot format to the given file.
     *
     * @param filename The name of the file to which to write the dot output.
     */
    void exportToDot(std::string const& filename) const;

    /*!
     * Exports the ODD in the text format to the given file.
     *
     * @param filename The name of the file to which to write the dot output.
     */
    void exportToText(std::string const& filename) const;

    /*!
     * Retrieves the encoding for the given offset.
     *
     * @param offset The target offset.
     * @param variableCount If not null, this indicates how many variables are contained in the encoding. If 0,
     * this number is automatically determined.
     */
    storm::storage::BitVector getEncoding(uint64_t offset, uint64_t variableCount = 0) const;

   private:
    /*!
     * Adds all nodes below the current one to the given mapping.
     *
     * @param levelToOddNodesMap A mapping of the level to the ODD node.
     * @param level The level of the current node.
     */
    void addToLevelToOddNodesMap(std::map<uint_fast64_t, std::unordered_set<storm::dd::Odd const*>>& levelToOddNodesMap, uint_fast64_t level = 0) const;

    /*!
     * Adds the values of the old explicit values to the new explicit values where the positions in the old vector
     * are given by the current old ODD and the positions in the new vector are given by the new ODD.
     *
     * @param oldOffset The offset in the old explicit values.
     * @param oldOdd The ODD to use for the old explicit values.
     * @param oldValues The vector of old values.
     * @param newOffset The offset in the new explicit values.
     * @param newOdd The ODD to use for the new explicit values.
     * @param newValues The vector of new values.
     */
    template<typename ValueType>
    static void expandValuesToVectorRec(uint_fast64_t oldOffset, storm::dd::Odd const& oldOdd, std::vector<ValueType> const& oldValues, uint_fast64_t newOffset,
                                        storm::dd::Odd const& newOdd, std::vector<ValueType>& newValues);

    static void oldToNewIndexRec(uint_fast64_t oldOffset, storm::dd::Odd const& oldOdd, uint_fast64_t newOffset, storm::dd::Odd const& newOdd,
                                 std::function<void(uint64_t oldOffset, uint64_t newOffset)> const& callback);

    // The then- and else-nodes.
    std::shared_ptr<Odd> elseNode;
    std::shared_ptr<Odd> thenNode;

    // The offsets that need to be added if the then- or else-successor is taken, respectively.
    uint_fast64_t elseOffset;
    uint_fast64_t thenOffset;
};
}  // namespace dd
}  // namespace storm
