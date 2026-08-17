#include "storm/storage/dd/Odd.h"

#include <boost/algorithm/string/join.hpp>
#include <fstream>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/io/file.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
Odd::Odd(std::shared_ptr<Odd> elseNode, uint_fast64_t elseOffset, std::shared_ptr<Odd> thenNode, uint_fast64_t thenOffset)
    : elseNode(elseNode), thenNode(thenNode), elseOffset(elseOffset), thenOffset(thenOffset) {
    STORM_LOG_ASSERT(this != elseNode.get() && this != thenNode.get(), "Cyclic ODD.");
}

Odd const& Odd::getThenSuccessor() const {
    return *this->thenNode;
}

Odd const& Odd::getElseSuccessor() const {
    return *this->elseNode;
}

uint_fast64_t Odd::getElseOffset() const {
    return this->elseOffset;
}

void Odd::setElseOffset(uint_fast64_t newOffset) {
    this->elseOffset = newOffset;
}

uint_fast64_t Odd::getThenOffset() const {
    return this->thenOffset;
}

void Odd::setThenOffset(uint_fast64_t newOffset) {
    this->thenOffset = newOffset;
}

uint_fast64_t Odd::getTotalOffset() const {
    return this->elseOffset + this->thenOffset;
}

uint_fast64_t Odd::getNodeCount() const {
    // If the ODD contains a constant (and thus has no children), the size is 1.
    if (this->elseNode == nullptr && this->thenNode == nullptr) {
        return 1;
    }

    // If the two successors are actually the same, we need to count the subnodes only once.
    if (this->elseNode == this->thenNode) {
        return this->elseNode->getNodeCount();
    } else {
        return this->elseNode->getNodeCount() + this->thenNode->getNodeCount();
    }
}

uint_fast64_t Odd::getHeight() const {
    if (this->isTerminalNode()) {
        return 1;
    } else {
        // Since both subtrees have the same height, we only count the height of the else-tree.
        return 1 + this->getElseSuccessor().getHeight();
    }
}

bool Odd::isTerminalNode() const {
    return this->elseNode == nullptr && this->thenNode == nullptr;
}

template<typename ValueType>
void Odd::expandExplicitVector(storm::dd::Odd const& newOdd, std::vector<ValueType> const& oldValues, std::vector<ValueType>& newValues) const {
    expandValuesToVectorRec(0, *this, oldValues, 0, newOdd, newValues);
}

template<typename ValueType>
void Odd::expandValuesToVectorRec(uint_fast64_t oldOffset, storm::dd::Odd const& oldOdd, std::vector<ValueType> const& oldValues, uint_fast64_t newOffset,
                                  storm::dd::Odd const& newOdd, std::vector<ValueType>& newValues) {
    if (oldOdd.isTerminalNode()) {
        STORM_LOG_THROW(newOdd.isTerminalNode(), storm::exceptions::InvalidArgumentException, "The ODDs for the translation must have the same height.");
        if (oldOdd.getThenOffset() != 0) {
            newValues[newOffset] += oldValues[oldOffset];
        }
    } else {
        expandValuesToVectorRec(oldOffset, oldOdd.getElseSuccessor(), oldValues, newOffset, newOdd.getElseSuccessor(), newValues);
        expandValuesToVectorRec(oldOffset + oldOdd.getElseOffset(), oldOdd.getThenSuccessor(), oldValues, newOffset + newOdd.getElseOffset(),
                                newOdd.getThenSuccessor(), newValues);
    }
}

void Odd::oldToNewIndex(storm::dd::Odd const& newOdd, std::function<void(uint64_t oldOffset, uint64_t newOffset)> const& callback) const {
    STORM_LOG_ASSERT(this->getHeight() < newOdd.getHeight(), "Expected increase in height.");
    oldToNewIndexRec(0, *this, 0, newOdd, callback);
}

void Odd::oldToNewIndexRec(uint_fast64_t oldOffset, storm::dd::Odd const& oldOdd, uint_fast64_t newOffset, storm::dd::Odd const& newOdd,
                           std::function<void(uint64_t oldOffset, uint64_t newOffset)> const& callback) {
    if (oldOdd.getTotalOffset() == 0 || newOdd.getTotalOffset() == 0) {
        return;
    }

    if (oldOdd.isTerminalNode()) {
        if (oldOdd.getThenOffset() != 0) {
            if (newOdd.isTerminalNode()) {
                if (newOdd.getThenOffset() != 0) {
                    callback(oldOffset, newOffset);
                }
            } else {
                oldToNewIndexRec(oldOffset, oldOdd, newOffset, newOdd.getElseSuccessor(), callback);
                oldToNewIndexRec(oldOffset, oldOdd, newOffset + newOdd.getElseOffset(), newOdd.getThenSuccessor(), callback);
            }
        }
    } else {
        oldToNewIndexRec(oldOffset, oldOdd.getElseSuccessor(), newOffset, newOdd.getElseSuccessor(), callback);
        oldToNewIndexRec(oldOffset + oldOdd.getElseOffset(), oldOdd.getThenSuccessor(), newOffset + newOdd.getElseOffset(), newOdd.getThenSuccessor(),
                         callback);
    }
}

void Odd::exportToDot(std::string const& filename) const {
    std::ofstream dotFile;
    storm::io::openFile(filename, dotFile);

    // Print header.
    dotFile << "digraph \"ODD\" {\n"
            << "center=true;\n"
            << "edge [dir = none];\n";

    // Print levels as ranks.
    dotFile << "{ node [shape = plaintext];\n"
            << "edge [style = invis];\n";
    std::vector<std::string> levelNames;
    for (uint_fast64_t level = 0; level < this->getHeight(); ++level) {
        levelNames.push_back("\"" + std::to_string(level) + "\"");
    }
    dotFile << boost::join(levelNames, " -> ") << ";";
    dotFile << "}\n";

    std::map<uint_fast64_t, std::unordered_set<storm::dd::Odd const*>> levelToOddNodesMap;
    this->addToLevelToOddNodesMap(levelToOddNodesMap);

    for (auto const& levelNodes : levelToOddNodesMap) {
        dotFile << "{ rank = same; \"" << levelNodes.first << "\"\n";
        for (auto const& node : levelNodes.second) {
            dotFile << "\"" << node << "\";\n";
        }
        dotFile << "}\n";
    }

    for (auto const& levelNodes : levelToOddNodesMap) {
        for (auto const& node : levelNodes.second) {
            dotFile << "\"" << node << "\" [label=\"" << node->getTotalOffset() << "\"];\n";
            if (!node->isTerminalNode()) {
                dotFile << "\"" << node << "\" -> \"" << &node->getElseSuccessor() << "\" [style=dashed, label=\"0\"];\n";
                dotFile << "\"" << node << "\" -> \"" << &node->getThenSuccessor() << "\" [style=solid, label=\"" << node->getElseOffset() << "\"];\n";
            }
        }
    }

    dotFile << "}\n";
    storm::io::closeFile(dotFile);
}

void Odd::exportToText(std::string const& filename) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

void getEncodingRec(Odd const& odd, uint64_t index, uint64_t offset, storm::storage::BitVector& result) {
    if (odd.isTerminalNode()) {
        return;
    }

    bool thenPath = false;
    if (odd.getElseOffset() <= offset) {
        thenPath = true;
        offset -= odd.getElseOffset();
        result.set(index);
    }
    getEncodingRec(thenPath ? odd.getThenSuccessor() : odd.getElseSuccessor(), index + 1, offset, result);
}

storm::storage::BitVector Odd::getEncoding(uint64_t offset, uint64_t variableCount) const {
    storm::storage::BitVector result(variableCount > 0 ? variableCount : this->getHeight());
    getEncodingRec(*this, 0, offset, result);
    return result;
}

void Odd::addToLevelToOddNodesMap(std::map<uint_fast64_t, std::unordered_set<storm::dd::Odd const*>>& levelToOddNodesMap, uint_fast64_t level) const {
    levelToOddNodesMap[level].emplace(this);
    if (!this->isTerminalNode()) {
        this->getElseSuccessor().addToLevelToOddNodesMap(levelToOddNodesMap, level + 1);
        if (this->thenNode != this->elseNode) {
            this->getThenSuccessor().addToLevelToOddNodesMap(levelToOddNodesMap, level + 1);
        }
    }
}

template void Odd::expandExplicitVector(storm::dd::Odd const& newOdd, std::vector<double> const& oldValues, std::vector<double>& newValues) const;
template void Odd::expandExplicitVector(storm::dd::Odd const& newOdd, std::vector<storm::RationalNumber> const& oldValues,
                                        std::vector<storm::RationalNumber>& newValues) const;
template void Odd::expandExplicitVector(storm::dd::Odd const& newOdd, std::vector<storm::RationalFunction> const& oldValues,
                                        std::vector<storm::RationalFunction>& newValues) const;
}  // namespace dd
}  // namespace storm
