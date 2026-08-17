#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "storm-config.h"
#include "storm/adapters/sylvan.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/InternalAdd.h"
#include "storm/storage/dd/InternalBdd.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace storage {
class BitVector;
}

namespace dd {
template<DdType LibraryType>
class InternalDdManager;

class Odd;

template<>
class InternalBdd<DdType::Sylvan> {
   public:
    template<DdType LibraryType, typename ValueType>
    friend class InternalAdd;

#ifdef STORM_HAVE_SYLVAN
    InternalBdd(InternalDdManager<DdType::Sylvan> const* ddManager, sylvan::Bdd const& sylvanBdd);
#endif

    // Instantiate all copy/move constructors/assignments with the default implementation.
    InternalBdd();
    InternalBdd(InternalBdd<DdType::Sylvan> const& other) = default;
    InternalBdd& operator=(InternalBdd<DdType::Sylvan> const& other) = default;
    InternalBdd(InternalBdd<DdType::Sylvan>&& other) = default;
    InternalBdd& operator=(InternalBdd<DdType::Sylvan>&& other) = default;

    /*!
     * Builds a BDD representing the values that make the given filter function evaluate to true.
     *
     * @param ddManager The manager responsible for the BDD.
     * @param odd The ODD used for the translation.
     * @param metaVariables The meta variables used for the translation.
     * @param filter The filter that evaluates whether an encoding is to be mapped to 0 or 1.
     * @return The resulting BDD.
     */
    static InternalBdd<storm::dd::DdType::Sylvan> fromVector(InternalDdManager<DdType::Sylvan> const* ddManager, Odd const& odd,
                                                             std::vector<uint_fast64_t> const& sortedDdVariableIndices,
                                                             std::function<bool(uint64_t)> const& filter);

    /*!
     * Retrieves whether the two BDDs represent the same function.
     *
     * @param other The BDD that is to be compared with the current one.
     * @return True if the BDDs represent the same function.
     */
    bool operator==(InternalBdd<DdType::Sylvan> const& other) const;

    /*!
     * Retrieves whether the two BDDs represent different functions.
     *
     * @param other The BDD that is to be compared with the current one.
     * @return True if the BDDs represent the different functions.
     */
    bool operator!=(InternalBdd<DdType::Sylvan> const& other) const;

    /*!
     * Computes the relational product of the current BDD and the given BDD representing a relation.
     *
     * @param relation The relation to use.
     * @param rowVariables The row variables of the relation represented as individual BDDs.
     * @param columnVariables The row variables of the relation represented as individual BDDs.
     * @return The ralational product.
     */
    InternalBdd<DdType::Sylvan> relationalProduct(InternalBdd<DdType::Sylvan> const& relation, std::vector<InternalBdd<DdType::Sylvan>> const& rowVariables,
                                                  std::vector<InternalBdd<DdType::Sylvan>> const& columnVariables) const;

    /*!
     * Computes the inverse relational product of the current BDD and the given BDD representing a relation.
     *
     * @param relation The relation to use.
     * @param rowVariables The row variables of the relation represented as individual BDDs.
     * @param columnVariables The row variables of the relation represented as individual BDDs.
     * @return The ralational product.
     */
    InternalBdd<DdType::Sylvan> inverseRelationalProduct(InternalBdd<DdType::Sylvan> const& relation,
                                                         std::vector<InternalBdd<DdType::Sylvan>> const& rowVariables,
                                                         std::vector<InternalBdd<DdType::Sylvan>> const& columnVariables) const;

    /*!
     * Computes the inverse relational product of the current BDD and the given BDD representing a relation that
     * contains more than just the row and column variables.
     *
     * @param relation The relation to use.
     * @param rowVariables The row variables of the relation represented as individual BDDs.
     * @param columnVariables The row variables of the relation represented as individual BDDs.
     * @return The ralational product.
     */
    InternalBdd<DdType::Sylvan> inverseRelationalProductWithExtendedRelation(InternalBdd<DdType::Sylvan> const& relation,
                                                                             std::vector<InternalBdd<DdType::Sylvan>> const& rowVariables,
                                                                             std::vector<InternalBdd<DdType::Sylvan>> const& columnVariables) const;

    /*!
     * Performs an if-then-else with the given operands, i.e. maps all valuations that are mapped to a non-zero
     * function value to the function values specified by the first DD and all others to the function values
     * specified by the second DD.
     *
     * @param thenBdd The BDD defining the 'then' part.
     * @param elseBdd The BDD defining the 'else' part.
     * @return The resulting BDD.
     */
    InternalBdd<DdType::Sylvan> ite(InternalBdd<DdType::Sylvan> const& thenBdd, InternalBdd<DdType::Sylvan> const& elseBdd) const;

    /*!
     * Performs an if-then-else with the given operands, i.e. maps all valuations that are mapped to true to the
     * function values specified by the first DD and all others to the function values specified by the second DD.
     *
     * @param thenAdd The ADD defining the 'then' part.
     * @param elseAdd The ADD defining the 'else' part.
     * @return The resulting ADD.
     */
    template<typename ValueType>
    InternalAdd<DdType::Sylvan, ValueType> ite(InternalAdd<DdType::Sylvan, ValueType> const& thenAdd,
                                               InternalAdd<DdType::Sylvan, ValueType> const& elseAdd) const;

    /*!
     * Performs a logical or of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical or of the operands.
     */
    InternalBdd<DdType::Sylvan> operator||(InternalBdd<DdType::Sylvan> const& other) const;

    /*!
     * Performs a logical or of the current and the given BDD and assigns it to the current BDD.
     *
     * @param other The second BDD used for the operation.
     * @return A reference to the current BDD after the operation
     */
    InternalBdd<DdType::Sylvan>& operator|=(InternalBdd<DdType::Sylvan> const& other);

    /*!
     * Performs a logical and of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical and of the operands.
     */
    InternalBdd<DdType::Sylvan> operator&&(InternalBdd<DdType::Sylvan> const& other) const;

    /*!
     * Performs a logical and of the current and the given BDD and assigns it to the current BDD.
     *
     * @param other The second BDD used for the operation.
     * @return A reference to the current BDD after the operation
     */
    InternalBdd<DdType::Sylvan>& operator&=(InternalBdd<DdType::Sylvan> const& other);

    /*!
     * Performs a logical iff of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical iff of the operands.
     */
    InternalBdd<DdType::Sylvan> iff(InternalBdd<DdType::Sylvan> const& other) const;

    /*!
     * Performs a logical exclusive-or of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical exclusive-or of the operands.
     */
    InternalBdd<DdType::Sylvan> exclusiveOr(InternalBdd<DdType::Sylvan> const& other) const;

    /*!
     * Performs a logical implication of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical implication of the operands.
     */
    InternalBdd<DdType::Sylvan> implies(InternalBdd<DdType::Sylvan> const& other) const;

    /*!
     * Logically inverts the current BDD.
     *
     * @return The resulting BDD.
     */
    InternalBdd<DdType::Sylvan> operator!() const;

    /*!
     * Logically complements the current BDD.
     *
     * @return A reference to the current BDD after the operation.
     */
    InternalBdd<DdType::Sylvan>& complement();

    /*!
     * Existentially abstracts from the given cube.
     *
     * @param cube The cube from which to abstract.
     */
    InternalBdd<DdType::Sylvan> existsAbstract(InternalBdd<DdType::Sylvan> const& cube) const;

    /*!
     * Similar to <code>existsAbstract</code>, but does not abstract but rather picks a valuation for the
     * variables of the given cube such that for this valuation, there exists a valuation (of the other
     * variables) that that make the function evaluate to true.
     *
     * @param cube The cube from which to 'abstract'.
     */
    InternalBdd<DdType::Sylvan> existsAbstractRepresentative(InternalBdd<DdType::Sylvan> const& cube) const;

    /*!
     * Universally abstracts from the given cube.
     *
     * @param cube The cube from which to abstract.
     */
    InternalBdd<DdType::Sylvan> universalAbstract(InternalBdd<DdType::Sylvan> const& cube) const;

    /*!
     * Swaps the given pairs of DD variables in the BDD. The pairs of meta variables have to be represented by
     * BDDs must have equal length.
     *
     * @param from The vector that specifies the 'from' part of the variable renaming.
     * @param to The vector that specifies the 'to' part of the variable renaming.
     * @return The resulting BDD.
     */
    InternalBdd<DdType::Sylvan> swapVariables(std::vector<InternalBdd<DdType::Sylvan>> const& from, std::vector<InternalBdd<DdType::Sylvan>> const& to) const;

    /*!
     * Computes the logical and of the current and the given BDD and existentially abstracts from the given cube.
     *
     * @param other The second BDD for the logical and.
     * @param cube The cube to existentially abstract.
     * @return A BDD representing the result.
     */
    InternalBdd<DdType::Sylvan> andExists(InternalBdd<DdType::Sylvan> const& other, InternalBdd<storm::dd::DdType::Sylvan> const& cube) const;

    /*!
     * Computes the constraint of the current BDD with the given constraint. That is, the function value of the
     * resulting BDD will be the same as the current ones for all assignments mapping to one in the constraint
     * and may be different otherwise.
     *
     * @param constraint The constraint to use for the operation.
     * @return The resulting BDD.
     */
    InternalBdd<DdType::Sylvan> constrain(InternalBdd<DdType::Sylvan> const& constraint) const;

    /*!
     * Computes the restriction of the current BDD with the given constraint. That is, the function value of the
     * resulting DD will be the same as the current ones for all assignments mapping to one in the constraint
     * and may be different otherwise.
     *
     * @param constraint The constraint to use for the operation.
     * @return The resulting BDD.
     */
    InternalBdd<DdType::Sylvan> restrict(InternalBdd<DdType::Sylvan> const& constraint) const;

    /*!
     * Retrieves the support of the current BDD.
     *
     * @return The support represented as a BDD.
     */
    InternalBdd<DdType::Sylvan> getSupport() const;

    /*!
     * Retrieves the number of encodings that are mapped to a non-zero value.
     *
     * @param numberOfDdVariables The number of DD variables contained in this BDD.
     * @return The number of encodings that are mapped to a non-zero value.
     */
    uint_fast64_t getNonZeroCount(uint_fast64_t numberOfDdVariables) const;

    /*!
     * Retrieves the number of leaves of the DD.
     *
     * @return The number of leaves of the DD.
     */
    uint_fast64_t getLeafCount() const;

    /*!
     * Retrieves the number of nodes necessary to represent the DD.
     *
     * @return The number of nodes in this DD.
     */
    uint_fast64_t getNodeCount() const;

    /*!
     * Retrieves whether this DD represents the constant one function.
     *
     * @return True if this DD represents the constant one function.
     */
    bool isOne() const;

    /*!
     * Retrieves whether this DD represents the constant zero function.
     *
     * @return True if this DD represents the constant zero function.
     */
    bool isZero() const;

    /*!
     * Retrieves the index of the topmost variable in the BDD.
     *
     * @return The index of the topmost variable in BDD.
     */
    uint_fast64_t getIndex() const;

    /*!
     * Retrieves the level of the topmost variable in the BDD.
     *
     * @return The level of the topmost variable in BDD.
     */
    uint_fast64_t getLevel() const;

    /*!
     * Exports the BDD to the given file in the dot format.
     *
     * @param filename The name of the file to which the BDD is to be exported.
     * @param ddVariableNamesAsStrings The names of the variables to display in the dot file.
     */
    void exportToDot(std::string const& filename, std::vector<std::string> const& ddVariableNamesAsStrings, bool showVariablesIfPossible = true) const;

    /*!
     * Exports the DD to the given file in a textual format as specified in Sylvan.
     *
     * @param filename The name of the file to which the DD is to be exported.
     */
    void exportToText(std::string const& filename) const;

    /*!
     * Converts a BDD to an equivalent ADD.
     *
     * @return The corresponding ADD.
     */
    template<typename ValueType>
    InternalAdd<DdType::Sylvan, ValueType> toAdd() const;

    /*!
     * Converts the BDD to a bit vector. The given offset-labeled DD is used to determine the correct row of
     * each entry.
     *
     * @param rowOdd The ODD used for determining the correct row.
     * @param ddVariableIndices The indices of the DD variables contained in this BDD.
     * @return The bit vector that is represented by this BDD.
     */
    storm::storage::BitVector toVector(storm::dd::Odd const& rowOdd, std::vector<uint_fast64_t> const& ddVariableIndices) const;

    /*!
     * Translates the function the BDD is representing to a set of expressions that characterize the function.
     *
     * @param manager The manager that is used to build the expression and, in particular, create new variables in.
     * @return A list of expressions representing the function of the BDD and a mapping of DD variable indices to
     * the variables that represent these variables in the expressions.
     */
    std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> toExpression(
        storm::expressions::ExpressionManager& manager) const;

    /*!
     * Creates an ODD based on the current BDD.
     *
     * @param ddVariableIndices The indices of the DD variables contained in this BDD.
     * @return The corresponding ODD.
     */
    Odd createOdd(std::vector<uint_fast64_t> const& ddVariableIndices) const;

    /*!
     * Uses the current BDD to filter values from the explicit vector.
     *
     * @param odd The ODD used to determine which entries to select.
     * @param ddVariableIndices The indices of the DD variables contained in this BDD.
     * @param sourceValues The source vector.
     * @param targetValues The vector to which to write the selected values.
     */
    template<typename ValueType>
    void filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices, std::vector<ValueType> const& sourceValues,
                              std::vector<ValueType>& targetValues) const;

    /*!
     * Uses the current BDD to filter values from the explicit vector.
     *
     * @param odd The ODD used to determine which entries to select.
     * @param ddVariableIndices The indices of the DD variables contained in this BDD.
     * @param sourceValues The source vector.
     * @param targetValues The vector to which to write the selected values.
     */
    void filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& ddVariableIndices, storm::storage::BitVector const& sourceValues,
                              storm::storage::BitVector& targetValues) const;

    /*!
     * Splits the BDD into several BDDs that differ in the encoding of the given group variables (given via indices).
     *
     * @param ddGroupVariableIndices The indices of the variables that are used to distinguish the groups.
     * @return A vector of BDDs that are the separate groups (wrt. to the encoding of the given variables).
     */
    std::vector<InternalBdd<DdType::Sylvan>> splitIntoGroups(std::vector<uint_fast64_t> const& ddGroupVariableIndices) const;

    friend struct std::hash<storm::dd::InternalBdd<storm::dd::DdType::Sylvan>>;

#ifdef STORM_HAVE_SYLVAN
    /*!
     * Retrieves the sylvan BDD.
     *
     * @return The sylvan BDD.
     */
    sylvan::Bdd& getSylvanBdd();

    /*!
     * Retrieves the sylvan BDD.
     *
     * @return The sylvan BDD.
     */
    sylvan::Bdd const& getSylvanBdd() const;
#endif

   private:
#ifdef STORM_HAVE_SYLVAN
    /*!
     * Builds a BDD representing the values that make the given filter function evaluate to true.
     *
     * @param currentOffset The current offset in the vector.
     * @param currentLevel The current level in the DD.
     * @param maxLevel The maximal level in the DD.
     * @param odd The ODD used for the translation.
     * @param ddVariableIndices The (sorted) list of DD variable indices to use.
     * @param filter A function that determines which encodings are to be mapped to true.
     * @return The resulting (Sylvan) BDD node.
     */
    static BDD fromVectorRec(uint_fast64_t& currentOffset, uint_fast64_t currentLevel, uint_fast64_t maxLevel, Odd const& odd,
                             std::vector<uint_fast64_t> const& ddVariableIndices, std::function<bool(uint64_t)> const& filter);

    // Declare a hash functor that is used for the unique tables in the construction process of ODDs.
    class HashFunctor {
       public:
        std::size_t operator()(std::pair<BDD, bool> const& key) const;
    };

    /*!
     * Recursively builds the ODD from a BDD (that potentially has complement edges).
     *
     * @param dd The BDD for which to build the ODD.
     * @param complement A flag indicating whether or not the given node is to be considered as complemented.
     * @param currentLevel The currently considered level in the DD.
     * @param maxLevel The number of levels that need to be considered.
     * @param ddVariableIndices The (sorted) indices of all DD variables that need to be considered.
     * @param uniqueTableForLevels A vector of unique tables, one for each level to be considered, that keeps
     * ODD nodes for the same DD and level unique.
     * @return A pointer to the constructed ODD for the given arguments.
     */
    static std::shared_ptr<Odd> createOddRec(BDD dd, bool complement, uint_fast64_t currentLevel, uint_fast64_t maxLevel,
                                             std::vector<uint_fast64_t> const& ddVariableIndices,
                                             std::vector<std::unordered_map<std::pair<BDD, bool>, std::shared_ptr<Odd>, HashFunctor>>& uniqueTableForLevels);

    /*!
     * Helper function to convert the DD into a bit vector.
     *
     * @param dd The DD to convert.
     * @param result The vector that will hold the values upon successful completion.
     * @param rowOdd The ODD used for the row translation.
     * @param complement A flag indicating whether the result is to be interpreted as a complement.
     * @param currentRowLevel The currently considered row level in the DD.
     * @param maxLevel The number of levels that need to be considered.
     * @param currentRowOffset The current row offset.
     * @param ddRowVariableIndices The (sorted) indices of all DD row variables that need to be considered.
     */
    void toVectorRec(BDD dd, storm::storage::BitVector& result, Odd const& rowOdd, bool complement, uint_fast64_t currentRowLevel, uint_fast64_t maxLevel,
                     uint_fast64_t currentRowOffset, std::vector<uint_fast64_t> const& ddRowVariableIndices) const;

    /*!
     * Adds the selected values the target vector.
     *
     * @param dd The current node of the DD representing the selected values.
     * @param currentLevel The currently considered level in the DD.
     * @param maxLevel The number of levels that need to be considered.
     * @param ddVariableIndices The sorted list of variable indices to use.
     * @param currentOffset The offset along the path taken in the DD representing the selected values.
     * @param odd The current ODD node.
     * @param result The target vector to which to write the values.
     * @param currentIndex The index at which the next element is to be written.
     * @param values The value vector from which to select the values.
     */
    template<typename ValueType>
    static void filterExplicitVectorRec(BDD dd, uint_fast64_t currentLevel, bool complement, uint_fast64_t maxLevel,
                                        std::vector<uint_fast64_t> const& ddVariableIndices, uint_fast64_t currentOffset, storm::dd::Odd const& odd,
                                        std::vector<ValueType>& result, uint_fast64_t& currentIndex, std::vector<ValueType> const& values);

    /*!
     * Adds the selected values the target vector.
     *
     * @param dd The current node of the DD representing the selected values.
     * @param currentLevel The currently considered level in the DD.
     * @param maxLevel The number of levels that need to be considered.
     * @param ddVariableIndices The sorted list of variable indices to use.
     * @param currentOffset The offset along the path taken in the DD representing the selected values.
     * @param odd The current ODD node.
     * @param result The target vector to which to write the values.
     * @param currentIndex The index at which the next element is to be written.
     * @param values The value vector from which to select the values.
     */
    static void filterExplicitVectorRec(BDD dd, uint_fast64_t currentLevel, bool complement, uint_fast64_t maxLevel,
                                        std::vector<uint_fast64_t> const& ddVariableIndices, uint_fast64_t currentOffset, storm::dd::Odd const& odd,
                                        storm::storage::BitVector& result, uint_fast64_t& currentIndex, storm::storage::BitVector const& values);

    /*!
     * Creates a vector of expressions that represent the function of the given BDD node.
     *
     *
     * @param dd The current node of the BDD.
     * @param manager The expression manager over which to build the expressions.
     * @param expressions The list of expressions to fill during the translation.
     * @param indexToVariableMap A mapping of variable indices to expression variables that are associated with
     * the respective node level of the BDD.
     * @param countIndexToVariablePair A mapping of (count, variable index) pairs to a pair of expression variables
     * such that entry (i, j) is mapped to a variable that represents the i-th node labeled with variable j (counting
     * from left to right).
     * @param nodeToCounterMap A mapping from DD nodes to a number j such that the DD node was the j-th node
     * visited with the same variable index as the given node.
     * @param nextCounterForIndex A vector storing a mapping from variable indices to a counter that indicates
     * how many nodes with the given variable index have been seen before.
     */
    static storm::expressions::Variable toExpressionRec(
        BDD dd, storm::expressions::ExpressionManager& manager, std::vector<storm::expressions::Expression>& expressions,
        std::unordered_map<uint_fast64_t, storm::expressions::Variable>& indexToVariableMap,
        std::unordered_map<std::pair<uint_fast64_t, uint_fast64_t>, storm::expressions::Variable>& countIndexToVariablePair,
        std::unordered_map<BDD, uint_fast64_t>& nodeToCounterMap, std::vector<uint_fast64_t>& nextCounterForIndex);

    void splitIntoGroupsRec(BDD dd, std::vector<InternalBdd<DdType::Sylvan>>& groups, std::vector<uint_fast64_t> const& ddGroupVariableIndices,
                            uint_fast64_t currentLevel, uint_fast64_t maxLevel) const;

    // The internal manager responsible for this BDD.
    InternalDdManager<DdType::Sylvan> const* ddManager;

    // The sylvan BDD.
    sylvan::Bdd sylvanBdd;
#endif
};
}  // namespace dd
}  // namespace storm

namespace std {
template<>
struct hash<storm::dd::InternalBdd<storm::dd::DdType::Sylvan>> {
#ifdef STORM_HAVE_SYLVAN
    std::size_t operator()(storm::dd::InternalBdd<storm::dd::DdType::Sylvan> const& key) const {
        return static_cast<std::size_t>(key.sylvanBdd.GetBDD());
    }
#else
    std::size_t operator()(storm::dd::InternalBdd<storm::dd::DdType::Sylvan> const&) const {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }
#endif
};
}  // namespace std
