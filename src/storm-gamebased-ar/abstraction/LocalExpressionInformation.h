#pragma once

#include <ostream>
#include <set>
#include <unordered_map>
#include <vector>

#include "storm/storage/dd/DdType.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
class AbstractionInformation;

template<storm::dd::DdType DdType>
class LocalExpressionInformation {
   public:
    /*!
     * Constructs a new variable partition.
     *
     * @param abstractionInformation The object storing global information about the abstraction.
     */
    LocalExpressionInformation(AbstractionInformation<DdType> const& abstractionInformation);

    /*!
     * Adds the expression and therefore indirectly may cause blocks of variables to be merged.
     *
     * @param globalExpressionIndex The global index of the expression.
     * @return A mapping from old block indices to the new ones (after possible merges).
     */
    std::map<uint64_t, uint64_t> addExpression(uint_fast64_t globalExpressionIndex);

    /*!
     * Retrieves whether the two given variables are in the same block of the partition.
     *
     * @param firstVariable The first variable.
     * @param secondVariable The second variable.
     * @return True iff the two variables are in the same block.
     */
    bool areRelated(storm::expressions::Variable const& firstVariable, storm::expressions::Variable const& secondVariable);

    /*!
     * Places the given variables in the same block of the partition and performs the implied merges.
     *
     * @param firstVariable The first variable.
     * @param secondVariable The second variable.
     * @return A mapping from old block indices to the new ones (after possible merges).
     */
    std::map<uint64_t, uint64_t> relate(storm::expressions::Variable const& firstVariable, storm::expressions::Variable const& secondVariable);

    /*!
     * Places the given variables in the same block of the partition and performs the implied merges.
     *
     * @param variables The variables to relate.
     * @return A mapping from old block indices to the new ones (after possible merges).
     */
    std::map<uint64_t, uint64_t> relate(std::set<storm::expressions::Variable> const& variables);

    /*!
     * Retrieves the block of related variables of the given variable.
     *
     * @param variable The variable whose block to retrieve.
     * @return The block of the variable.
     */
    std::set<storm::expressions::Variable> const& getBlockOfVariable(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves the block index of the given variable.
     *
     * @param variable The variable for which to retrieve the block.
     * @return The block index of the given variable.
     */
    uint_fast64_t getBlockIndexOfVariable(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves the block indices of the given variables.
     *
     * @param variables The variables for which to retrieve the blocks.
     * @return The block indices of the given variables.
     */
    std::set<uint_fast64_t> getBlockIndicesOfVariables(std::set<storm::expressions::Variable> const& variables) const;

    /*!
     * Retrieves the number of blocks of the variable partition.
     *
     * @return The number of blocks in this partition.
     */
    uint_fast64_t getNumberOfBlocks() const;

    /*!
     * Retrieves the block with the given index.
     *
     * @param blockIndex The index of the block to retrieve.
     * @return The block with the given index.
     */
    std::set<storm::expressions::Variable> const& getVariableBlockWithIndex(uint_fast64_t blockIndex) const;

    /*!
     * Retrieves the indices of the expressions related to the given variable.
     *
     * @param variable The variable for which to retrieve the related expressions.
     * @return The related expressions.
     */
    std::set<uint_fast64_t> const& getRelatedExpressions(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves the indices of the expressions related to any of the given variables.
     *
     * @param variables The variables for which to retrieve the related expressions.
     * @return The related expressions.
     */
    std::set<uint_fast64_t> getRelatedExpressions(std::set<storm::expressions::Variable> const& variables) const;

    /*!
     * Retrieves the indices of the expressions in which the given variable appears.
     *
     * @param variable The variable for which to retrieve the expressions.
     * @return The indices of all expressions using the given variable.
     */
    std::set<uint_fast64_t> const& getExpressionsUsingVariable(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves the indices of the expressions in which the given variables appear.
     *
     * @param variables The variables for which to retrieve the expressions.
     * @return The indices of all expressions using the given variables.
     */
    std::set<uint_fast64_t> getExpressionsUsingVariables(std::set<storm::expressions::Variable> const& variables) const;

    /*!
     * Retrieves the expression block with the given index.
     *
     * @return The requested expression block.
     */
    std::set<uint_fast64_t> const& getExpressionBlock(uint64_t index) const;

    template<storm::dd::DdType DdTypePrime>
    friend std::ostream& operator<<(std::ostream& out, LocalExpressionInformation<DdTypePrime> const& partition);

   private:
    /*!
     * Merges the blocks with the given indices.
     *
     * @param blocksToMerge The indices of the blocks to merge.
     * @return A mapping from old block indices to the new ones (after possible merges).
     */
    std::map<uint64_t, uint64_t> mergeBlocks(std::set<uint_fast64_t> const& blocksToMerge);

    // The set of variables relevant for this partition.
    std::set<storm::expressions::Variable> relevantVariables;

    // A mapping from variables to their blocks.
    std::unordered_map<storm::expressions::Variable, uint_fast64_t> variableToBlockMapping;

    // The variable blocks of the partition.
    std::vector<std::set<storm::expressions::Variable>> variableBlocks;

    // The expression blocks of the partition.
    std::vector<std::set<uint_fast64_t>> expressionBlocks;

    // A mapping from variables to the indices of all expressions they appear in.
    std::unordered_map<storm::expressions::Variable, std::set<uint_fast64_t>> variableToExpressionsMapping;

    // The object storing the abstraction information.
    std::reference_wrapper<AbstractionInformation<DdType> const> abstractionInformation;
};

template<storm::dd::DdType DdType>
std::ostream& operator<<(std::ostream& out, LocalExpressionInformation<DdType> const& partition);

}  // namespace abstraction
}  // namespace storm::gbar
