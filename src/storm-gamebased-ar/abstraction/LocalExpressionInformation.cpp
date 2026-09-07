#include "LocalExpressionInformation.h"

#include <boost/algorithm/string/join.hpp>

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"
#include "storm/utility/macros.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
LocalExpressionInformation<DdType>::LocalExpressionInformation(AbstractionInformation<DdType> const& abstractionInformation)
    : relevantVariables(abstractionInformation.getAbstractedVariables()),
      expressionBlocks(relevantVariables.size()),
      abstractionInformation(abstractionInformation) {
    // Assign each variable to a new block.
    uint_fast64_t currentBlock = 0;
    variableBlocks.resize(relevantVariables.size());
    for (auto const& variable : relevantVariables) {
        this->variableToBlockMapping[variable] = currentBlock;
        this->variableToExpressionsMapping[variable] = std::set<uint_fast64_t>();
        variableBlocks[currentBlock].insert(variable);
        ++currentBlock;
    }
}

template<storm::dd::DdType DdType>
std::map<uint64_t, uint64_t> LocalExpressionInformation<DdType>::addExpression(uint_fast64_t globalExpressionIndex) {
    storm::expressions::Expression const& expression = abstractionInformation.get().getPredicateByIndex(globalExpressionIndex);

    // Register the expression for all variables that appear in it.
    std::set<storm::expressions::Variable> expressionVariables = expression.getVariables();
    for (auto const& variable : expressionVariables) {
        variableToExpressionsMapping[variable].insert(globalExpressionIndex);
    }

    // Add the expression to the block of the first variable. When relating the variables, the blocks will
    // get merged (if necessary).
    STORM_LOG_ASSERT(!expressionVariables.empty(), "Found no variables in expression.");
    expressionBlocks[getBlockIndexOfVariable(*expressionVariables.begin())].insert(globalExpressionIndex);

    // Add expression and relate all the appearing variables.
    return this->relate(expressionVariables);
}

template<storm::dd::DdType DdType>
bool LocalExpressionInformation<DdType>::areRelated(storm::expressions::Variable const& firstVariable, storm::expressions::Variable const& secondVariable) {
    return getBlockIndexOfVariable(firstVariable) == getBlockIndexOfVariable(secondVariable);
}

template<storm::dd::DdType DdType>
std::map<uint64_t, uint64_t> LocalExpressionInformation<DdType>::relate(storm::expressions::Variable const& firstVariable,
                                                                        storm::expressions::Variable const& secondVariable) {
    return this->relate({firstVariable, secondVariable});
}

template<storm::dd::DdType DdType>
std::map<uint64_t, uint64_t> LocalExpressionInformation<DdType>::relate(std::set<storm::expressions::Variable> const& variables) {
    // Determine all blocks that need to be merged.
    std::set<uint_fast64_t> blocksToMerge;
    for (auto const& variable : variables) {
        blocksToMerge.insert(getBlockIndexOfVariable(variable));
    }

    STORM_LOG_ASSERT(variables.empty() || !blocksToMerge.empty(), "Found no blocks to merge.");

    // If we found a single block only, there is nothing to do.
    if (blocksToMerge.size() <= 1) {
        std::map<uint64_t, uint64_t> identity;
        for (uint64_t i = 0; i < getNumberOfBlocks(); ++i) {
            identity.emplace_hint(identity.end(), i, i);
        }
        return identity;
    }

    return this->mergeBlocks(blocksToMerge);
}

template<storm::dd::DdType DdType>
std::map<uint64_t, uint64_t> LocalExpressionInformation<DdType>::mergeBlocks(std::set<uint_fast64_t> const& blocksToMerge) {
    std::map<uint64_t, uint64_t> oldToNewIndices;

    // Merge all blocks into the block to keep.
    std::vector<std::set<storm::expressions::Variable>> newVariableBlocks;
    std::vector<std::set<uint_fast64_t>> newExpressionBlocks;

    std::set<uint_fast64_t>::const_iterator blocksToMergeIt = blocksToMerge.begin();
    std::set<uint_fast64_t>::const_iterator blocksToMergeIte = blocksToMerge.end();

    // Determine which block to keep (to merge the other blocks into).
    uint_fast64_t blockToKeep = *blocksToMergeIt;
    ++blocksToMergeIt;

    for (uint_fast64_t blockIndex = 0; blockIndex < variableBlocks.size(); ++blockIndex) {
        // If the block is the next one to merge into the block to keep, do so now.
        if (blocksToMergeIt != blocksToMergeIte && *blocksToMergeIt == blockIndex && blockIndex != blockToKeep) {
            // Adjust the mapping for all variables of the old block.
            for (auto const& variable : variableBlocks[blockIndex]) {
                variableToBlockMapping[variable] = blockToKeep;
            }
            oldToNewIndices[blockIndex] = blockToKeep;

            newVariableBlocks[blockToKeep].insert(variableBlocks[blockIndex].begin(), variableBlocks[blockIndex].end());
            newExpressionBlocks[blockToKeep].insert(expressionBlocks[blockIndex].begin(), expressionBlocks[blockIndex].end());
            ++blocksToMergeIt;
        } else {
            // Otherwise just move the current block to the new partition.
            oldToNewIndices[blockIndex] = newVariableBlocks.size();

            // Adjust the mapping for all variables of the old block.
            for (auto const& variable : variableBlocks[blockIndex]) {
                variableToBlockMapping[variable] = newVariableBlocks.size();
            }

            newVariableBlocks.emplace_back(std::move(variableBlocks[blockIndex]));
            newExpressionBlocks.emplace_back(std::move(expressionBlocks[blockIndex]));
        }
    }

    variableBlocks = std::move(newVariableBlocks);
    expressionBlocks = std::move(newExpressionBlocks);

    return oldToNewIndices;
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& LocalExpressionInformation<DdType>::getBlockOfVariable(storm::expressions::Variable const& variable) const {
    return variableBlocks[getBlockIndexOfVariable(variable)];
}

template<storm::dd::DdType DdType>
uint_fast64_t LocalExpressionInformation<DdType>::getNumberOfBlocks() const {
    return this->variableBlocks.size();
}

template<storm::dd::DdType DdType>
std::set<storm::expressions::Variable> const& LocalExpressionInformation<DdType>::getVariableBlockWithIndex(uint_fast64_t blockIndex) const {
    return this->variableBlocks[blockIndex];
}

template<storm::dd::DdType DdType>
uint_fast64_t LocalExpressionInformation<DdType>::getBlockIndexOfVariable(storm::expressions::Variable const& variable) const {
    STORM_LOG_ASSERT(this->relevantVariables.find(variable) != this->relevantVariables.end(), "Illegal variable '" << variable.getName() << "' for partition.");
    return this->variableToBlockMapping.find(variable)->second;
}

template<storm::dd::DdType DdType>
std::set<uint_fast64_t> LocalExpressionInformation<DdType>::getBlockIndicesOfVariables(std::set<storm::expressions::Variable> const& variables) const {
    std::set<uint_fast64_t> result;
    for (auto const& variable : variables) {
        result.insert(getBlockIndexOfVariable(variable));
    }
    return result;
}

template<storm::dd::DdType DdType>
std::set<uint_fast64_t> const& LocalExpressionInformation<DdType>::getRelatedExpressions(storm::expressions::Variable const& variable) const {
    return this->expressionBlocks[getBlockIndexOfVariable(variable)];
}

template<storm::dd::DdType DdType>
std::set<uint_fast64_t> LocalExpressionInformation<DdType>::getRelatedExpressions(std::set<storm::expressions::Variable> const& variables) const {
    // Start by determining the indices of all expression blocks that are related to any of the variables.
    std::set<uint_fast64_t> relatedExpressionBlockIndices;
    for (auto const& variable : variables) {
        relatedExpressionBlockIndices.insert(getBlockIndexOfVariable(variable));
    }

    // Then join the expressions of these blocks and return the result.
    std::set<uint_fast64_t> result;
    for (auto const& blockIndex : relatedExpressionBlockIndices) {
        result.insert(expressionBlocks[blockIndex].begin(), expressionBlocks[blockIndex].end());
    }
    return result;
}

template<storm::dd::DdType DdType>
std::set<uint_fast64_t> const& LocalExpressionInformation<DdType>::getExpressionsUsingVariable(storm::expressions::Variable const& variable) const {
    STORM_LOG_ASSERT(this->relevantVariables.find(variable) != this->relevantVariables.end(), "Illegal variable '" << variable.getName() << "' for partition.");
    return this->variableToExpressionsMapping.find(variable)->second;
}

template<storm::dd::DdType DdType>
std::set<uint_fast64_t> LocalExpressionInformation<DdType>::getExpressionsUsingVariables(std::set<storm::expressions::Variable> const& variables) const {
    std::set<uint_fast64_t> result;

    for (auto const& variable : variables) {
        STORM_LOG_ASSERT(this->relevantVariables.find(variable) != this->relevantVariables.end(),
                         "Illegal variable '" << variable.getName() << "' for partition.");
        auto it = this->variableToExpressionsMapping.find(variable);
        result.insert(it->second.begin(), it->second.end());
    }

    return result;
}

template<storm::dd::DdType DdType>
std::set<uint_fast64_t> const& LocalExpressionInformation<DdType>::getExpressionBlock(uint64_t index) const {
    return expressionBlocks[index];
}

template<storm::dd::DdType DdType>
std::ostream& operator<<(std::ostream& out, LocalExpressionInformation<DdType> const& partition) {
    std::vector<std::string> blocks;
    for (uint_fast64_t index = 0; index < partition.variableBlocks.size(); ++index) {
        auto const& variableBlock = partition.variableBlocks[index];
        auto const& expressionBlock = partition.expressionBlocks[index];

        std::vector<std::string> variablesInBlock;
        for (auto const& variable : variableBlock) {
            variablesInBlock.push_back(variable.getName());
        }

        std::vector<std::string> expressionsInBlock;
        for (auto const& expressionIndex : expressionBlock) {
            std::stringstream stream;
            stream << partition.abstractionInformation.get().getPredicateByIndex(expressionIndex);
            expressionsInBlock.push_back(stream.str());
        }

        blocks.push_back("<[" + boost::algorithm::join(variablesInBlock, ", ") + "], [" + boost::algorithm::join(expressionsInBlock, ", ") + "]>");
    }

    out << "{";
    out << boost::join(blocks, ", ");
    out << "}";
    return out;
}

template class LocalExpressionInformation<storm::dd::DdType::CUDD>;
template class LocalExpressionInformation<storm::dd::DdType::Sylvan>;

template std::ostream& operator<<(std::ostream& out, LocalExpressionInformation<storm::dd::DdType::CUDD> const& partition);
template std::ostream& operator<<(std::ostream& out, LocalExpressionInformation<storm::dd::DdType::Sylvan> const& partition);
}  // namespace abstraction
}  // namespace storm::gbar
