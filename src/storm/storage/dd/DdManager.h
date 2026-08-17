#pragma once

#include <boost/optional.hpp>
#include <functional>
#include <set>
#include <unordered_map>

#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdMetaVariable.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/InternalDdManager.h"
#include "storm/storage/dd/cudd/InternalCuddDdManager.h"
#include "storm/storage/dd/sylvan/InternalSylvanDdManager.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
class Environment;
}  // namespace storm

namespace storm {
namespace dd {

// Declare DdManager class so we can then specialize it for the different DD types.
template<DdType LibraryType>
class DdManager : public std::enable_shared_from_this<DdManager<LibraryType>> {
   public:
    friend class Bdd<LibraryType>;

    template<DdType LibraryTypePrime, typename ValueType>
    friend class Add;

    template<DdType LibraryTypePrime, typename ValueType>
    friend class AddIterator;

    /*!
     * Creates an empty manager without any meta variables.
     *
     * @param env The environment providing the settings for the DD library (e.g. Sylvan or CUDD).
     */
    explicit DdManager(storm::Environment const& env);

    /*!
     * Creates a new manager that is configured according to a default environment.
     */
    static std::shared_ptr<DdManager<LibraryType>> createWithDefaultEnvironment();

    // Explicitly forbid copying a DdManager, but allow moving it.
    DdManager(DdManager<LibraryType> const& other) = delete;
    DdManager<LibraryType>& operator=(DdManager<LibraryType> const& other) = delete;
    DdManager(DdManager<LibraryType>&& other) = default;
    DdManager<LibraryType>& operator=(DdManager<LibraryType>&& other) = default;

    std::shared_ptr<DdManager<LibraryType>> asSharedPointer();
    std::shared_ptr<DdManager<LibraryType> const> asSharedPointer() const;

    /*!
     * Retrieves a BDD representing the constant one function.
     *
     * @return A BDD representing the constant one function.
     */
    Bdd<LibraryType> getBddOne() const;

    /*!
     * Retrieves an ADD representing the constant one function.
     *
     * @return An ADD representing the constant one function.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> getAddOne() const;

    /*!
     * Retrieves a BDD representing the constant zero function.
     *
     * @return A BDD representing the constant zero function.
     */
    Bdd<LibraryType> getBddZero() const;

    /*!
     * Retrieves an ADD representing the constant zero function.
     *
     * @return An ADD representing the constant zero function.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> getAddZero() const;

    /*!
     * Retrieves an ADD representing an undefined value.
     *
     * @return An ADD representing an undefined value.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> getAddUndefined() const;

    /*!
     * Retrieves an ADD representing the constant infinity function.
     *
     * @return An ADD representing the constant infinity function.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> getInfinity() const;

    /*!
     * Retrieves an ADD representing the constant function with the given value.
     *
     * @return An ADD representing the constant function with the given value.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> getConstant(ValueType const& value) const;

    /*!
     * Retrieves the BDD representing the function that maps all inputs which have the given meta variable equal
     * to the given value one.
     *
     * @param variable The expression variable associated with the meta variable.
     * @param value The value the meta variable is supposed to have.
     * @param mostSignificantBitAtTop A flag indicating whether the most significant bit of the value is to be
     * encoded with the topmost variable or the bottommost.
     * @return The DD representing the function that maps all inputs which have the given meta variable equal
     * to the given value one.
     */
    Bdd<LibraryType> getEncoding(storm::expressions::Variable const& variable, int_fast64_t value, bool mostSignificantBitAtTop = true) const;

    /*!
     * Retrieves the BDD representing the range of the meta variable, i.e., a function that maps all legal values
     * of the range of the meta variable to one.
     *
     * @param variable The expression variable associated with the meta variable.
     * @return The range of the meta variable.
     */
    Bdd<LibraryType> getRange(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves the ADD representing the identity of the meta variable, i.e., a function that maps all legal
     * values of the range of the meta variable to the values themselves.
     *
     * @param variable The expression variable associated with the meta variable.
     * @return The identity of the meta variable.
     */
    template<typename ValueType>
    Add<LibraryType, ValueType> getIdentity(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves a BDD in which an encoding is mapped to true iff the meta variables of each pair encode the same value.
     *
     * @param variablePairs The variable pairs for which to compute the identity.
     * @param restrictToFirstRange If set, the identity will be restricted to the legal range of the first variable.
     */
    Bdd<LibraryType> getIdentity(std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& variablePairs,
                                 bool restrictToFirstRange = true) const;

    /*!
     * Retrieves a BDD in which an encoding is mapped to true iff the two meta variables encode the same value.
     *
     * @param first The first meta variable in the identity.
     * @param second The second meta variable in the identity.
     * @param restrictToFirstRange If set, the identity will be restricted to the legal range of the first variable.
     */
    Bdd<LibraryType> getIdentity(storm::expressions::Variable const& first, storm::expressions::Variable const& second, bool restrictToFirstRange = true) const;

    /*!
     * Retrieves a BDD that is the cube of the variables representing the given meta variable.
     *
     * @param variable The expression variable associated with the meta variable.
     * @return The cube of the meta variable.
     */
    Bdd<LibraryType> getCube(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves a BDD that is the cube of the variables representing the given meta variables.
     *
     * @param variables The expression variables associated with the meta variables.
     * @return The cube of the meta variables.
     */
    Bdd<LibraryType> getCube(std::set<storm::expressions::Variable> const& variables) const;

    /*!
     * Clones the given meta variable and optionally changes the number of layers of the variable.
     *
     * @param variable The variable to clone.
     * @param newVariableName The name of the variable to create.
     * @param numberOfLayers The number of layers of the variable to create.
     */
    std::vector<storm::expressions::Variable> cloneVariable(storm::expressions::Variable const& variable, std::string const& newVariableName,
                                                            boost::optional<uint64_t> const& numberOfLayers = boost::none);

    /*!
     * Adds an integer meta variable with the given range with two layers (a 'normal' and a 'primed' one).
     *
     * @param variableName The name of the new variable.
     * @param low The lowest value of the range of the variable.
     * @param high The highest value of the range of the variable.
     */
    std::pair<storm::expressions::Variable, storm::expressions::Variable> addMetaVariable(
        std::string const& variableName, int_fast64_t low, int_fast64_t high,
        boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position = boost::none);

    /*!
     * Creates a meta variable with the given number of layers.
     *
     * @param variableName The name of the variable.
     * @param numberOfLayers The number of layers of this variable (must be greater or equal 1).
     */
    std::vector<storm::expressions::Variable> addMetaVariable(
        std::string const& variableName, int_fast64_t low, int_fast64_t high, uint64_t numberOfLayers,
        boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position = boost::none);

    /*!
     * Creates a meta variable with the given number of layers.
     *
     * @param variableName The name of the variable.
     * @param numberOfLayers The number of layers of this variable (must be greater or equal 1).
     */
    std::vector<storm::expressions::Variable> addBitVectorMetaVariable(
        std::string const& variableName, uint64_t bits, uint64_t numberOfLayers,
        boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position = boost::none);

    /*!
     * Adds a boolean meta variable with two layers (a 'normal' and a 'primed' one).
     *
     * @param variableName The name of the new variable.
     */
    std::pair<storm::expressions::Variable, storm::expressions::Variable> addMetaVariable(
        std::string const& variableName, boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position = boost::none);

    /*!
     * Creates a meta variable with the given number of layers.
     *
     * @param variableName The name of the variable.
     * @param numberOfLayers The number of layers of this variable (must be greater or equal 1).
     */
    std::vector<storm::expressions::Variable> addMetaVariable(
        std::string const& variableName, uint64_t numberOfLayers,
        boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position = boost::none);

    /*!
     * Retrieves the names of all meta variables that have been added to the manager.
     *
     * @return The set of all meta variable names of the manager.
     */
    std::set<std::string> getAllMetaVariableNames() const;

    /*!
     * Retrieves the number of meta variables that are contained in this manager.
     *
     * @return The number of meta variables contained in this manager.
     */
    std::size_t getNumberOfMetaVariables() const;

    /*!
     * Retrieves whether the given meta variable name is already in use.
     *
     * @param variableName The name of the variable.
     * @return True if the given meta variable name is managed by this manager.
     */
    bool hasMetaVariable(std::string const& variableName) const;

    /*!
     * Retrieves the given meta variable by name.
     *
     * @param variableName The name of the variable.
     * @return The meta variable.
     */
    storm::expressions::Variable getMetaVariable(std::string const& variableName) const;

    /*!
     * Checks whether this manager supports the ordered insertion of variables, i.e. inserting variables at
     * positions between already existing variables.
     *
     * @return True iff the manager supports ordered insertion.
     */
    bool supportsOrderedInsertion() const;

    /*!
     * Sets whether dynamic reordering is allowed for the DDs managed by this manager (if supported).
     *
     * @param value If set to true, dynamic reordering is allowed and forbidden otherwise.
     */
    void allowDynamicReordering(bool value);

    /*!
     * Retrieves whether dynamic reordering is currently allowed (if supported).
     *
     * @return True iff dynamic reordering is currently allowed.
     */
    bool isDynamicReorderingAllowed() const;

    /*!
     * Triggers a reordering of the DDs managed by this manager (if supported).
     */
    void triggerReordering();

    /*!
     * Retrieves the meta variable with the given name if it exists.
     *
     * @param variable The expression variable associated with the meta variable.
     * @return The corresponding meta variable.
     */
    DdMetaVariable<LibraryType> const& getMetaVariable(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves the set of meta variables contained in the DD.
     *
     * @return All contained meta variables.
     */
    std::set<storm::expressions::Variable> getAllMetaVariables() const;

    /*!
     * Retrieves the (sorted) list of the variable indices of the DD variables given by the meta variable set.
     *
     * @return The sorted list of variable indices.
     */
    std::vector<uint_fast64_t> getSortedVariableIndices() const;

    /*!
     * Retrieves the (sorted) list of the variable indices of the DD variables given by the meta variable set.
     *
     * @param metaVariables The set of meta variables for which to retrieve the index list.
     * @return The sorted list of variable indices.
     */
    std::vector<uint_fast64_t> getSortedVariableIndices(std::set<storm::expressions::Variable> const& metaVariables) const;

    /*!
     * Retrieves the internal DD manager.
     *
     * @return The internal DD manager.
     */
    InternalDdManager<LibraryType>& getInternalDdManager();

    /*!
     * Retrieves the internal DD manager.
     *
     * @return The internal DD manager.
     */
    InternalDdManager<LibraryType> const& getInternalDdManager() const;

    /*!
     * Performs a debug check if available.
     */
    void debugCheck() const;

    /*!
     * All code that manipulates DDs shall be called through this function.
     * This is generally needed to set up the correct context.
     * Specifically for sylvan, this is required to make sure that DD-manipulating code is executed as a LACE task.
     * Example usage: `manager->execute([&]() { bar = foo(arg1,arg2); }`
     *
     * @param f the function that is executed
     */
    void execute(std::function<void()> const& f) const;

   private:
    /*!
     * Creates a meta variable with the given number of DD variables and layers.
     *
     * @param type The type of the meta variable to create.
     * @param name The name of the variable.
     * @param numberOfLayers The number of layers of this variable (must be greater or equal 1).
     */
    std::vector<storm::expressions::Variable> addMetaVariableHelper(
        MetaVariableType const& type, std::string const& name, uint64_t numberOfDdVariables, uint64_t numberOfLayers,
        boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position = boost::none,
        boost::optional<std::pair<int_fast64_t, int_fast64_t>> const& bounds = boost::none);

    /*!
     * Retrieves a list of names of the DD variables in the order of their index.
     *
     * @return A list of DD variable names.
     */
    std::vector<std::string> getDdVariableNames() const;

    /*!
     * Retrieves a list of expression variables in the order of their index.
     *
     * @return A list of DD variables.
     */
    std::vector<storm::expressions::Variable> getDdVariables() const;

    /*!
     * Retrieves the underlying expression manager.
     *
     * @return The underlying expression manager.
     */
    storm::expressions::ExpressionManager const& getExpressionManager() const;

    /*!
     * Retrieves the underlying expression manager.
     *
     * @return The underlying expression manager.
     */
    storm::expressions::ExpressionManager& getExpressionManager();

    /*!
     * Retrieves a pointer to the internal DD manager.
     *
     * @return A pointer to the internal DD manager.
     */
    InternalDdManager<LibraryType>* getInternalDdManagerPointer();

    /*!
     * Retrieves a pointer to the internal DD manager.
     *
     * @return A pointer to the internal DD manager.
     */
    InternalDdManager<LibraryType> const* getInternalDdManagerPointer() const;

    // ATTENTION: as the DD packages typically perform garbage collection, the order of members is crucial here:
    // First, the references to the DDs of the meta variables need to be disposed of and *then* the manager.

    // The DD manager that is customized according to the selected library type.
    InternalDdManager<LibraryType> internalDdManager;

    // A mapping from variables to the meta variable information.
    std::unordered_map<storm::expressions::Variable, DdMetaVariable<LibraryType>> metaVariableMap;

    // The manager responsible for the variables.
    std::shared_ptr<storm::expressions::ExpressionManager> manager;
};
}  // namespace dd
}  // namespace storm
