#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "storm/storage/BoostTypes.h"
#include "storm/storage/prism/BooleanVariable.h"
#include "storm/storage/prism/ClockVariable.h"
#include "storm/storage/prism/Command.h"
#include "storm/storage/prism/IntegerVariable.h"
#include "storm/storage/prism/ModuleRenaming.h"

namespace storm {
namespace prism {
class Module : public LocatedInformation {
   public:
    /*!
     * Creates a module with the given name, variables and commands.
     *
     * @param moduleName The name of the module.
     * @param booleanVariables The boolean variables defined by the module.
     * @param integerVariables The integer variables defined by the module.
     * @param clockVariables The clock variables defined by the module (only for PTA models).
     * @param invariant An invariant that is used to restrict the values of the clock variables (only for PTA models).
     * @param commands The commands of the module.
     * @param filename The filename in which the module is defined.
     * @param lineNumber The line number in which the module is defined.
     */
    Module(std::string const& moduleName, std::vector<storm::prism::BooleanVariable> const& booleanVariables,
           std::vector<storm::prism::IntegerVariable> const& integerVariables, std::vector<storm::prism::ClockVariable> const& clockVariables,
           storm::expressions::Expression const& invariant, std::vector<storm::prism::Command> const& commands, std::string const& filename = "",
           uint_fast64_t lineNumber = 0);

    /*!
     * Creates a module with the given name, variables and commands that is marked as being renamed from the
     * given module with the given renaming.
     *
     * @param moduleName The name of the module.
     * @param booleanVariables The boolean variables defined by the module.
     * @param integerVariables The integer variables defined by the module.
     * @param clockVariables The clock variables defined by the module (only for PTA models).
     * @param invariant An invariant that is used to restrict the values of the clock variables (only for PTA models).
     * @param commands The commands of the module.
     * @param renamedFromModule The name of the module from which this module was renamed.
     * @param renaming The renaming of identifiers used to create this module.
     * @param filename The filename in which the module is defined.
     * @param lineNumber The line number in which the module is defined.
     */
    Module(std::string const& moduleName, std::vector<storm::prism::BooleanVariable> const& booleanVariables,
           std::vector<storm::prism::IntegerVariable> const& integerVariables, std::vector<storm::prism::ClockVariable> const& clockVariables,
           storm::expressions::Expression const& invariant, std::vector<storm::prism::Command> const& commands, std::string const& renamedFromModule,
           storm::prism::ModuleRenaming const& renaming, std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    Module() = default;
    Module(Module const& other) = default;
    Module& operator=(Module const& other) = default;
    Module(Module&& other) = default;
    Module& operator=(Module&& other) = default;

    /*!
     * @return True iff the module contains at least one variable with infinite domain
     */
    bool hasUnboundedVariables() const;

    /*!
     * Retrieves the number of boolean variables in the module.
     *
     * @return the number of boolean variables in the module.
     */
    std::size_t getNumberOfBooleanVariables() const;

    /*!
     * Retrieves the number of integer variables in the module.
     *
     * @return The number of integer variables in the module.
     */
    std::size_t getNumberOfIntegerVariables() const;

    /*!
     * Retrieves a reference to the boolean variable with the given name.
     *
     * @param variableName The name of the boolean variable to retrieve.
     * @return A reference to the boolean variable with the given name.
     */
    storm::prism::BooleanVariable const& getBooleanVariable(std::string const& variableName) const;

    /*!
     * Retrieves the boolean variables of the module.
     *
     * @return The boolean variables of the module.
     */
    std::vector<storm::prism::BooleanVariable> const& getBooleanVariables() const;

    /*!
     * Retrieves a reference to the integer variable with the given name.
     *
     * @param variableName The name of the integer variable to retrieve.
     * @return A reference to the integer variable with the given name.
     */
    storm::prism::IntegerVariable const& getIntegerVariable(std::string const& variableName) const;

    /*!
     * Retrieves the integer variables of the module.
     *
     * @return The integer variables of the module.
     */
    std::vector<storm::prism::IntegerVariable> const& getIntegerVariables() const;

    /*!
     * Retrieves the number of clock variables in the module.
     *
     * @return the number of clock variables in the module.
     */
    std::size_t getNumberOfClockVariables() const;

    /*!
     * Retrieves a reference to the clock variable with the given name.
     *
     * @param variableName The name of the clock variable to retrieve.
     * @return A reference to the clock variable with the given name.
     */
    storm::prism::ClockVariable const& getClockVariable(std::string const& variableName) const;

    /*!
     * Retrieves the clock variables of the module.
     *
     * @return The clock variables of the module.
     */
    std::vector<storm::prism::ClockVariable> const& getClockVariables() const;

    /*!
     * Retrieves all expression variables used by this module.
     *
     * @return The set of expression variables used by this module.
     */
    std::set<storm::expressions::Variable> getAllExpressionVariables() const;

    /*!
     * Retrieves a list of expressions that characterize the legal ranges of all variables declared by this
     * module.
     *
     * @return The list of expressions that characterize the legal ranges.
     */
    std::vector<storm::expressions::Expression> getAllRangeExpressions() const;

    /*!
     * Retrieves the number of commands of this module.
     *
     * @return The number of commands of this module.
     */
    std::size_t getNumberOfCommands() const;

    /*!
     * Retrieves the total number of updates of this module.
     *
     * @return The total number of updates of this module.
     */
    std::size_t getNumberOfUpdates() const;

    /*!
     * Retrieves a reference to the command with the given index.
     *
     * @param index The index of the command to retrieve.
     * @return A reference to the command with the given index.
     */
    storm::prism::Command const& getCommand(uint_fast64_t index) const;

    /*!
     * Retrieves the commands of the module.
     *
     * @return The commands of the module.
     */
    std::vector<storm::prism::Command> const& getCommands() const;

    /*!
     * Retrieves the commands of the module.
     *
     * @return The commands of the module.
     */
    std::vector<storm::prism::Command>& getCommands();

    /*!
     * Retrieves the name of the module.
     *
     * @return The name of the module.
     */
    std::string const& getName() const;

    /*!
     * Retrieves the set of synchronizing action indices present in this module.
     *
     * @return the set of synchronizing action indices present in this module.
     */
    std::set<uint_fast64_t> const& getSynchronizingActionIndices() const;

    /*!
     * Retrieves whether or not this module contains a command labeled with the given action index.
     *
     * @param actionIndex The index of the action to look for in this module.
     * @return True iff the module has at least one command labeled with the given action index.
     */
    bool hasActionIndex(uint_fast64_t actionIndex) const;

    /*!
     * Retrieves whether this module was created from another module via renaming.
     *
     * @return True iff the module was created via renaming.
     */
    bool isRenamedFromModule() const;

    /*!
     * If the module was created via renaming, this method retrieves the name of the module that was used as the
     * in the base in the renaming process.
     *
     * @return The name of the module from which this module was created via renaming.
     */
    std::string const& getBaseModule() const;

    /*!
     * If the module was created via renaming, this method returns the applied renaming of identifiers used for
     * the renaming process.
     *
     * @return A mapping of identifiers to new identifiers that was used in the renaming process.
     */
    std::map<std::string, std::string> const& getRenaming() const;

    /*!
     * Retrieves the indices of all commands within this module that are labelled by the given action.
     *
     * @param actionIndex The index of the action with which the commands have to be labelled.
     * @return A set of indices of commands that are labelled with the given action index.
     */
    std::set<uint_fast64_t> const& getCommandIndicesByActionIndex(uint_fast64_t actionIndex) const;

    /*!
     * Creates a new module that drops all commands whose indices are not in the given set.
     *
     * @param indexSet The set of indices for which to keep the commands.
     * @return The module resulting from erasing all commands whose indices are not in the given set.
     */
    Module restrictCommands(storm::storage::FlatSet<uint_fast64_t> const& indexSet) const;

    /*!
     * Creates a new module that drops all commands whose action indices are not in the given set.
     *
     * @param indexSet The set of action indices for which to keep the commands.
     * @return The module resulting from erasing all commands whose action indices are not in the given set.
     */
    Module restrictActionIndices(storm::storage::FlatSet<uint_fast64_t> const& actionIndices) const;

    /*!
     * Substitutes all variables in the module according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting module.
     */
    Module substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    /**
     * Nonstandard predicates such as ExacltyOneOff etc can be substituted
     * @return
     */
    Module substituteNonStandardPredicates() const;

    /**
     * Label unlabelled commands.
     *
     * @param suggestions Map from global index to names that is used to label unlabelled commands
     * @param newId The new action ids to use are given sequentially from this number onwards (and the argument is updated)
     * @param nameToIdMapping A mapping that is updated giving action indices to used names
     * @return
     */
    Module labelUnlabelledCommands(std::map<uint64_t, std::string> const& suggestions, uint64_t& newId, std::map<std::string, uint64_t>& nameToIdMapping) const;

    /*!
     * Checks whether the given variables only appear in the update probabilities of the module and nowhere else.
     *
     * @param undefinedConstantVariables A set of variables that may only appear in the update probabilities.
     * @return True iff the given variables only appear in the update probabilities of the module and nowhere else.
     */
    bool containsVariablesOnlyInUpdateProbabilities(std::set<storm::expressions::Variable> const& undefinedConstantVariables) const;

    /*!
     * Equips all of the modules' variables without initial values with initial values based on their type.
     */
    void createMissingInitialValues();

    /**
     * Auxiliary function for Program::replaceVariableInitializationByInitExpression
     * Effect: All of the modules' variables are no longer initialized.
     */
    void removeVariableInitialization();

    /*
     * Gets the number of commands without a label
     */
    uint64_t getNumberOfUnlabeledCommands() const;

    /*!
     * Returns true, if an invariant was specified (only relevant for PTA models)
     */
    bool hasInvariant() const;

    /*!
     * Returns the specified invariant (only relevant for PTA models)
     */
    storm::expressions::Expression const& getInvariant() const;

    friend std::ostream& operator<<(std::ostream& stream, Module const& module);

   private:
    /*!
     * Computes the locally maintained mappings for fast data retrieval.
     */
    void createMappings();

    // The name of the module.
    std::string moduleName;

    // A list of boolean variables.
    std::vector<storm::prism::BooleanVariable> booleanVariables;

    // A mapping from boolean variables to the corresponding indices in the vector.
    std::map<std::string, uint_fast64_t> booleanVariableToIndexMap;

    // A list of integer variables.
    std::vector<storm::prism::IntegerVariable> integerVariables;

    // A mapping from integer variables to the corresponding indices in the vector.
    std::map<std::string, uint_fast64_t> integerVariableToIndexMap;

    // A list of clock variables.
    std::vector<storm::prism::ClockVariable> clockVariables;

    // A mapping from clock variables to the corresponding indices in the vector.
    std::map<std::string, uint_fast64_t> clockVariableToIndexMap;

    // An invariant (only for PTA models)
    storm::expressions::Expression invariant;

    // The commands associated with the module.
    std::vector<storm::prism::Command> commands;

    // The set of action indices present in this module.
    std::set<uint_fast64_t> synchronizingActionIndices;

    // A map of actions to the set of commands labeled with this action.
    std::map<uint_fast64_t, std::set<uint_fast64_t>> actionIndicesToCommandIndexMap;

    // This string indicates whether and from what module this module was created via renaming.
    std::string renamedFromModule;

    // If the module was created by renaming, this mapping contains the provided renaming of identifiers.
    storm::prism::ModuleRenaming renaming;
};

}  // namespace prism
}  // namespace storm
