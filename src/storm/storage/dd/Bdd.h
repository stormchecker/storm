#pragma once

#include <functional>

#include "storm/storage/BitVector.h"
#include "storm/storage/PairHash.h"
#include "storm/storage/dd/Dd.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/cudd/InternalCuddBdd.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

namespace storm {
namespace logic {
enum class ComparisonType;
}

namespace dd {
template<DdType LibraryType, typename ValueType>
class Add;

class Odd;

template<DdType LibraryType>
class Bdd : public Dd<LibraryType> {
   public:
    friend class DdManager<LibraryType>;

    template<DdType LibraryTypePrime, typename ValueTypePrime>
    friend class Add;

    /*!
     * Creates a DD that encapsulates the given internal BDD.
     *
     * @param ddManager The manager responsible for this DD.
     * @param internalBdd The internal BDD to store.
     * @param containedMetaVariables The meta variables that appear in the DD.
     */
    Bdd(DdManager<LibraryType> const& ddManager, InternalBdd<LibraryType> const& internalBdd,
        std::set<storm::expressions::Variable> const& containedMetaVariables = std::set<storm::expressions::Variable>());

    // Instantiate all copy/move constructors/assignments with the default implementation.
    Bdd() = default;
    Bdd(Bdd<LibraryType> const& other) = default;
    Bdd& operator=(Bdd<LibraryType> const& other) = default;
    Bdd(Bdd<LibraryType>&& other) = default;
    Bdd& operator=(Bdd<LibraryType>&& other) = default;

    /*!
     * Constructs the BDD representation of the encoding with the given offset.
     *
     * @param ddManager The DD manager responsible for the resulting BDD.
     * @param targetOffset The offset to encode (interpreted within the ODD).
     * @param odd The ODD used for the translation from the explicit representation to a symbolic one.
     * @param metaVariables The meta variables to use for the symbolic encoding.
     */
    static Bdd<LibraryType> getEncoding(DdManager<LibraryType> const& ddManager, uint64_t targetOffset, storm::dd::Odd const& odd,
                                        std::set<storm::expressions::Variable> const& metaVariables);

    /*!
     * Constructs a BDD representation of all encodings whose value is true in the given list of truth values.
     *
     * @param ddManager The DD manager responsible for the resulting BDD.
     * @param truthValues The truth values.
     * @param odd The ODD used for the translation from the explicit representation to a symbolic one.
     * @param metaVariables The meta variables to use for the symbolic encoding.
     */
    static Bdd<LibraryType> fromVector(DdManager<LibraryType> const& ddManager, storm::storage::BitVector const& truthValues, storm::dd::Odd const& odd,
                                       std::set<storm::expressions::Variable> const& metaVariables);

    /*!
     * Constructs a BDD representation of all encodings that are in the requested relation with the given value.
     *
     * @param ddManager The DD manager responsible for the resulting BDD.
     * @param explicitValues The explicit values to compare to the given value.
     * @param odd The ODD used for the translation from the explicit representation to a symbolic one.
     * @param metaVariables The meta variables to use for the symbolic encoding.
     * @param comparisonType The relation that needs to hold for the values (wrt. to the given value).
     * @param value The value to compare with.
     */
    template<typename ValueType>
    static Bdd<LibraryType> fromVector(DdManager<LibraryType> const& ddManager, std::vector<ValueType> const& explicitValues, storm::dd::Odd const& odd,
                                       std::set<storm::expressions::Variable> const& metaVariables, storm::logic::ComparisonType comparisonType,
                                       ValueType value);

    /*!
     * Retrieves whether the two BDDs represent the same function.
     *
     * @param other The BDD that is to be compared with the current one.
     * @return True if the BDDs represent the same function.
     */
    bool operator==(Bdd<LibraryType> const& other) const;

    /*!
     * Retrieves whether the two BDDs represent different functions.
     *
     * @param other The BDD that is to be compared with the current one.
     * @return True if the BDDs represent the different functions.
     */
    bool operator!=(Bdd<LibraryType> const& other) const;

    /*!
     * Performs an if-then-else with the given operands, i.e. maps all valuations that are mapped to a non-zero
     * function value to the function values specified by the first DD and all others to the function values
     * specified by the second DD.
     *
     * @param thenBdd The BDD defining the 'then' part.
     * @param elseBdd The BDD defining the 'else' part.
     * @return The resulting BDD.
     */
    Bdd<LibraryType> ite(Bdd<LibraryType> const& thenBdd, Bdd<LibraryType> const& elseBdd) const;

    /*!
     * Performs an if-then-else with the given operands, i.e. maps all valuations that are mapped to true to the
     * function values specified by the first DD and all others to the function values specified by the second DD.
     *
     * @param thenAdd The ADD defining the 'then' part.
     * @param elseAdd The ADD defining the 'else' part.
     * @return The resulting ADD.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> ite(Add<LibraryType, ValueType> const& thenAdd, Add<LibraryType, ValueType> const& elseAdd) const;

    /*!
     * Performs a logical or of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical or of the operands.
     */
    Bdd<LibraryType> operator||(Bdd<LibraryType> const& other) const;

    /*!
     * Performs a logical or of the current and the given BDD and assigns it to the current BDD.
     *
     * @param other The second BDD used for the operation.
     * @return A reference to the current BDD after the operation
     */
    Bdd<LibraryType>& operator|=(Bdd<LibraryType> const& other);

    /*!
     * Performs a logical and of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical and of the operands.
     */
    Bdd<LibraryType> operator&&(Bdd<LibraryType> const& other) const;

    /*!
     * Performs a logical and of the current and the given BDD and assigns it to the current BDD.
     *
     * @param other The second BDD used for the operation.
     * @return A reference to the current BDD after the operation
     */
    Bdd<LibraryType>& operator&=(Bdd<LibraryType> const& other);

    /*!
     * Performs a logical iff of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical iff of the operands.
     */
    Bdd<LibraryType> iff(Bdd<LibraryType> const& other) const;

    /*!
     * Performs a logical exclusive-or of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical exclusive-or of the operands.
     */
    Bdd<LibraryType> exclusiveOr(Bdd<LibraryType> const& other) const;

    /*!
     * Performs a logical implication of the current and the given BDD.
     *
     * @param other The second BDD used for the operation.
     * @return The logical implication of the operands.
     */
    Bdd<LibraryType> implies(Bdd<LibraryType> const& other) const;

    /*!
     * Logically inverts the current BDD.
     *
     * @return The resulting BDD.
     */
    Bdd<LibraryType> operator!() const;

    /*!
     * Logically complements the current BDD.
     *
     * @return A reference to the current BDD after the operation.
     */
    Bdd<LibraryType>& complement();

    /*!
     * Existentially abstracts from the given meta variables.
     *
     * @param metaVariables The meta variables from which to abstract.
     */
    Bdd<LibraryType> existsAbstract(std::set<storm::expressions::Variable> const& metaVariables) const;

    /*!
     * Similar to <code>existsAbstract</code>, but does not abstract from the variables but rather picks a
     * valuation of each of the meta variables "to abstract from" such that for this valuation, there exists a
     * valuation (of the other variables) that that make the function evaluate to true.
     *
     * @param metaVariables The meta variables from which to abstract.
     */
    Bdd<LibraryType> existsAbstractRepresentative(std::set<storm::expressions::Variable> const& metaVariables) const;

    /*!
     * Universally abstracts from the given meta variables.
     *
     * @param metaVariables The meta variables from which to abstract.
     */
    Bdd<LibraryType> universalAbstract(std::set<storm::expressions::Variable> const& metaVariables) const;

    /*!
     * Computes the logical and of the current and the given BDD and existentially abstracts from the given set
     * of variables.
     *
     * @param other The second BDD for the logical and.
     * @param existentialVariables The variables from which to existentially abstract.
     * @return A BDD representing the result.
     */
    Bdd<LibraryType> andExists(Bdd<LibraryType> const& other, std::set<storm::expressions::Variable> const& existentialVariables) const;

    /*!
     * Computes the constraint of the current BDD with the given constraint. That is, the function value of the
     * resulting BDD will be the same as the current ones for all assignments mapping to one in the constraint
     * and may be different otherwise.
     *
     * @param constraint The constraint to use for the operation.
     * @return The resulting BDD.
     */
    Bdd<LibraryType> constrain(Bdd<LibraryType> const& constraint) const;

    /*!
     * Computes the restriction of the current BDD with the given constraint. That is, the function value of the
     * resulting DD will be the same as the current ones for all assignments mapping to one in the constraint
     * and may be different otherwise.
     *
     * @param constraint The constraint to use for the operation.
     * @return The resulting BDD.
     */
    Bdd<LibraryType> restrict(Bdd<LibraryType> const& constraint) const;

    /*!
     * Computes the relational product of the current BDD and the given BDD representing a relation.
     * Note that this operation assumes that the row and column variables are interleaved and that the relation
     * only contains the row and column variables.
     *
     * @param relation The relation to use.
     * @param rowMetaVariables The row meta variables used in the relation.
     * @param columnMetaVariables The row meta variables used in the relation.
     * @return The relational product.
     */
    Bdd<LibraryType> relationalProduct(Bdd<LibraryType> const& relation, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                       std::set<storm::expressions::Variable> const& columnMetaVariables) const;

    /*!
     * Computes the inverse relational product of the current BDD and the given BDD representing a relation.
     * Note that this operation assumes that the row and column variables are interleaved and that the relation
     * only contains the row and column variables.
     *
     * @param relation The relation to use.
     * @param rowMetaVariables The row meta variables used in the relation.
     * @param columnMetaVariables The row meta variables used in the relation.
     * @return The inverse relational product.
     */
    Bdd<LibraryType> inverseRelationalProduct(Bdd<LibraryType> const& relation, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                              std::set<storm::expressions::Variable> const& columnMetaVariables) const;

    /*!
     * Computes the inverse relational product of the current BDD and the given BDD representing a relation that
     * contains more variables than just the row and column variables.
     * Note that this operation assumes that the row and column variables are interleaved.
     *
     * @param relation The relation to use.
     * @param rowMetaVariables The row meta variables used in the relation.
     * @param columnMetaVariables The row meta variables used in the relation.
     * @return The inverse relational product.
     */
    Bdd<LibraryType> inverseRelationalProductWithExtendedRelation(Bdd<LibraryType> const& relation,
                                                                  std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                                  std::set<storm::expressions::Variable> const& columnMetaVariables) const;

    /*!
     * Swaps the given pairs of meta variables in the BDD. The pairs of meta variables must be guaranteed to have
     * the same number of underlying BDD variables.
     *
     * @param metaVariablePairs A vector of meta variable pairs that are to be swapped for one another.
     * @return The resulting BDD.
     */
    Bdd<LibraryType> swapVariables(std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& metaVariablePairs) const;

    /*!
     * Renames the given meta variables in the BDD. The number of the underlying DD variables of the both meta
     * variable sets needs to agree.
     *
     * @param from The meta variables to be renamed. The current BDD needs to contain all these meta variables.
     * @param to The meta variables that are the target of the renaming process. The current BDD must not contain
     * any of these meta variables.
     * @return The resulting BDD.
     */
    Bdd<LibraryType> renameVariables(std::set<storm::expressions::Variable> const& from, std::set<storm::expressions::Variable> const& to) const;

    /*!
     * Renames the given meta variables in the BDD. The number of the underlying DD variables of the meta
     * variable set needs to be at least as large as the meta variable set. If the amount of variables coincides,
     * this operation coincides with renameVariables. Otherwise, it first abstracts from the superfluous variables
     * and then performs the renaming.
     *
     * @param from The meta variables to be renamed. The current ADD needs to contain all these meta variables.
     * @param to The meta variables that are the target of the renaming process. The current ADD must not contain
     * any of these meta variables.
     * @return The resulting ADD.
     */
    Bdd<LibraryType> renameVariablesAbstract(std::set<storm::expressions::Variable> const& from, std::set<storm::expressions::Variable> const& to) const;

    /*!
     * Renames the given meta variables in the BDD. The number of the underlying DD variables of the meta
     * variable set needs to be at most as large as the meta variable set. If the amount of variables coincides,
     * this operation coincides with renameVariables. Otherwise, it first adds a unique encoding in terms of the
     * superfluous variables and then performs the renaming.
     *
     * @param from The meta variables to be renamed. The current ADD needs to contain all these meta variables.
     * @param to The meta variables that are the target of the renaming process. The current ADD must not contain
     * any of these meta variables.
     * @return The resulting ADD.
     */
    Bdd<LibraryType> renameVariablesConcretize(std::set<storm::expressions::Variable> const& from, std::set<storm::expressions::Variable> const& to) const;

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
     * Converts a BDD to an equivalent ADD.
     *
     * @return The corresponding ADD.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> toAdd() const;

    /*!
     * Splits the BDD along the given variables (must be at the top).
     */
    std::vector<Bdd<LibraryType>> split(std::set<storm::expressions::Variable> const& variables) const;

    /*!
     * Converts the BDD to a bit vector. The given offset-labeled DD is used to determine the correct row of
     * each entry.
     *
     * @param rowOdd The ODD used for determining the correct row.
     * @return The bit vector that is represented by this BDD.
     */
    storm::storage::BitVector toVector(storm::dd::Odd const& rowOdd) const;

    /*!
     * Translates the function the BDD is representing to a set of expressions that characterize the function.
     *
     * @param manager The manager that is used to build the expression and, in particular, create new variables in.
     * @return A list of expressions representing the function of the BDD and a mapping of DD variable indices to
     * the variables that represent these variables in the expressions.
     */
    std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> toExpression(
        storm::expressions::ExpressionManager& manager) const;

    virtual Bdd<LibraryType> getSupport() const override;

    virtual uint_fast64_t getNonZeroCount() const override;

    virtual uint_fast64_t getLeafCount() const override;

    virtual uint_fast64_t getNodeCount() const override;

    virtual uint_fast64_t getIndex() const override;

    virtual uint_fast64_t getLevel() const override;

    virtual void exportToDot(std::string const& filename, bool showVariablesIfPossible = true) const override;

    virtual void exportToText(std::string const& filename) const override;

    /*!
     * Retrieves the cube of all given meta variables.
     *
     * @param manager Dd manager.
     * @param metaVariables The variables for which to create the cube.
     * @return The resulting cube.
     */
    static Bdd<LibraryType> getCube(DdManager<LibraryType> const& manager, std::set<storm::expressions::Variable> const& metaVariables);

    /*!
     * Creates an ODD based on the current BDD.
     *
     * @return The corresponding ODD.
     */
    Odd createOdd() const;

    /*!
     * Filters the given explicit vector using the symbolic representation of which values to select.
     *
     * @param odd The ODD used for the translation from symbolic to explicit.
     * @param values The value vector from which to select the values.
     * @return The resulting vector.
     */
    template<typename ValueType>
    std::vector<ValueType> filterExplicitVector(Odd const& odd, std::vector<ValueType> const& values) const;

    /*!
     * Filters the given explicit vector using the symbolic representation of which values to select.
     *
     * @param odd The ODD used for the translation from symbolic to explicit.
     * @param values The value vector from which to select the values.
     * @return The resulting vector.
     */
    storm::storage::BitVector filterExplicitVector(Odd const& odd, storm::storage::BitVector const& values) const;

    /*!
     * Retrieves the internal BDD.
     */
    InternalBdd<LibraryType> const& getInternalBdd() const;

    friend struct std::hash<storm::dd::Bdd<LibraryType>>;

    template<DdType LibraryTypePrime, typename ValueType>
    friend struct FromVectorHelper;

   private:
    // The internal BDD that depends on the chosen library.
    InternalBdd<LibraryType> internalBdd;
};
}  // namespace dd
}  // namespace storm

namespace std {
template<storm::dd::DdType LibraryType>
struct hash<storm::dd::Bdd<LibraryType>> {
    std::size_t operator()(storm::dd::Bdd<LibraryType> const& key) const {
        return std::hash<storm::dd::InternalBdd<LibraryType>>().operator()(key.internalBdd);
    }
};
}  // namespace std
