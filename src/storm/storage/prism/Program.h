#pragma once

#include <boost/optional.hpp>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/BoostTypes.h"
#include "storm/storage/PlayerIndex.h"
#include "storm/storage/prism/Composition.h"
#include "storm/storage/prism/Constant.h"
#include "storm/storage/prism/Formula.h"
#include "storm/storage/prism/InitialConstruct.h"
#include "storm/storage/prism/Label.h"
#include "storm/storage/prism/Module.h"
#include "storm/storage/prism/Player.h"
#include "storm/storage/prism/RewardModel.h"
#include "storm/storage/prism/SystemCompositionConstruct.h"
#include "storm/utility/solver.h"

namespace storm {
namespace jani {
class Model;
class Property;
}  // namespace jani

namespace prism {
class Program : public LocatedInformation {
   public:
    /*!
     * An enum for the different model types.
     */
    enum class ModelType { UNDEFINED, DTMC, CTMC, MDP, CTMDP, MA, POMDP, PTA, SMG };

    enum class ValidityCheckLevel : unsigned { VALIDINPUT = 0, READYFORPROCESSING = 1 };

    /*!
     * Creates a program with the given model type, undefined constants, global variables, modules, reward
     * models, labels and initial states.
     *
     * @param manager The manager responsible for the variables and expressions of the program.
     * @param modelType The type of the program.
     * @param constants The constants of the program.
     * @param globalBooleanVariables The global boolean variables of the program.
     * @param globalIntegerVariables The global integer variables of the program.
     * @param formulas The formulas defined in the program.
     * @param modules The modules of the program.
     * @param actionToIndexMap A mapping of action names to their indices.
     * @param rewardModels The reward models of the program.
     * @param labels The labels defined for this program.
     * @param initialConstruct The initial construct of the program. If none, then an initial construct is built
     * using the initial values of the variables.
     * @param compositionConstruct If not none, specifies how the modules are composed for the full system.
     * If none, the regular parallel composition is assumed.
     * @param filename The filename in which the program is defined.
     * @param lineNumber The line number in which the program is defined.
     * @param finalModel If set to true, the program is checked for input-validity, as well as some post-processing.
     */
    Program(std::shared_ptr<storm::expressions::ExpressionManager> manager, ModelType modelType, std::vector<Constant> const& constants,
            std::vector<BooleanVariable> const& globalBooleanVariables, std::vector<IntegerVariable> const& globalIntegerVariables,
            std::vector<Formula> const& formulas, std::vector<Player> const& players, std::vector<Module> const& modules,
            std::map<std::string, uint_fast64_t> const& actionToIndexMap, std::vector<RewardModel> const& rewardModels, std::vector<Label> const& labels,
            std::vector<ObservationLabel> const& observationLabels, boost::optional<InitialConstruct> const& initialConstruct,
            boost::optional<SystemCompositionConstruct> const& compositionConstruct, bool prismCompatibility, std::string const& filename = "",
            uint_fast64_t lineNumber = 0, bool finalModel = true);

    // Provide default implementations for constructors and assignments.
    Program() = default;
    Program(Program const& other) = default;
    Program& operator=(Program const& other) = default;
    Program(Program&& other) = default;
    Program& operator=(Program&& other) = default;

    /*!
     * Retrieves the model type of the model.
     *
     * @return The type of the model.
     */
    ModelType getModelType() const;

    /*!
     * Retrieves whether the model is a discrete-time model, i.e. a DTMC or an MDP.
     *
     * @return True iff the model is a discrete-time model.
     */
    bool isDiscreteTimeModel() const;

    /*!
     * Retrieves whether the model is one without nondeterministic choices, i.e. a DTMC or a CTMC.
     */
    bool isDeterministicModel() const;

    /*!
     * Retrieves whether the model has restricted observability
     */
    bool isPartiallyObservable() const;

    /*!
     * @return True iff the program contains at least one variable with infinite domain
     */
    bool hasUnboundedVariables() const;

    /*!
     * Retrieves whether there are undefined constants of any type in the program.
     *
     * @return True iff there are undefined constants of any type in the program.
     */
    bool hasUndefinedConstants() const;

    /*!
     * Checks that undefined constants (parameters) of the model preserve the graph of the underlying model.
     * That is, undefined constants may only appear in the probability expressions of updates as well as in the
     * values in reward models.
     *
     * @return True iff the undefined constants are graph-preserving.
     */
    bool undefinedConstantsAreGraphPreserving() const;

    /*!
     * Retrieves the undefined constants in the program.
     *
     * @return The undefined constants in the program.
     */
    std::vector<std::reference_wrapper<Constant const>> getUndefinedConstants() const;

    /*!
     * Retrieves the undefined constants in the program as a comma-separated string.
     *
     * @return A string with the undefined constants in the program, separated by a comma
     */
    std::string getUndefinedConstantsAsString() const;

    /*!
     * Retrieves whether the given constant exists in the program.
     *
     * @param constantName The name of the constant to search.
     * @return True iff the constant exists in the program.
     */
    bool hasConstant(std::string const& constantName) const;

    /*!
     * Retrieves the constant with the given name if it exists.
     *
     * @param constantName The name of the constant to retrieve.
     * @return The constant with the given name if it exists.
     */
    Constant const& getConstant(std::string const& constantName) const;

    /*!
     * Retrieves a mapping of all defined constants to their defining expressions.
     *
     * @return A mapping from constants to their 'values'.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getConstantsSubstitution() const;

    /*!
     * Retrieves a mapping of all formula variables to their defining expressions.
     *
     * @return A mapping from constants to their 'values'.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getFormulasSubstitution() const;

    /*!
     * Retrieves a mapping of all defined constants and formula variables to their defining expressions
     *
     * @return A mapping from constants and formulas to their expressions.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getConstantsFormulasSubstitution(bool getConstantsSubstitution = true,
                                                                                                            bool getFormulasSubstitution = true) const;

    /*!
     * Applies the renaming of a renamed module to the given substitution.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getSubstitutionForRenamedModule(
        Module const& renamedModule, std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    /*!
     * Gets the renaming of a module after flattening all renamings.
     * Note that the base of a renamed module might again be a renamed module.
     */
    std::map<std::string, std::string> getFinalRenamingOfModule(Module const& renamedModule) const;

    /*!
     * Retrieves all constants defined in the program.
     *
     * @return The constants defined in the program.
     */
    std::vector<Constant> const& getConstants() const;

    /*!
     * Retrieves the number of all constants defined in the program.
     *
     * @return The number of constants defined in the program.
     */
    std::size_t getNumberOfConstants() const;

    /*!
     * Retrieves the constants that are actually used in the program.
     * @return
     */
    std::vector<Constant> usedConstants() const;

    /*!
     * The total number of commands in the prism file.
     */
    size_t getNumberOfCommands() const;

    /*!
     * Retrieves whether a global Boolean variable with the given name exists
     *
     * @param variableName The name of the variable
     * @return true iff a global variable of type Boolean with the given name exists.
     */
    bool globalBooleanVariableExists(std::string const& variableName) const;

    /**
     * Retrieves whether a global Integer variable with the given name exists
     *
     * @param variableName The name of the variable
     * @return true iff a global variable of type Integer with the given name exists.
     */
    bool globalIntegerVariableExists(std::string const& variableName) const;

    /*!
     * Retrieves the global boolean variables of the program.
     *
     * @return The global boolean variables of the program.
     */
    std::vector<BooleanVariable> const& getGlobalBooleanVariables() const;

    /*!
     * Retrieves a the global boolean variable with the given name.
     *
     * @param variableName The name of the global boolean variable to retrieve.
     * @return The global boolean variable with the given name.
     */
    BooleanVariable const& getGlobalBooleanVariable(std::string const& variableName) const;

    /*!
     * Retrieves the global integer variables of the program.
     *
     * @return The global integer variables of the program.
     */
    std::vector<IntegerVariable> const& getGlobalIntegerVariables() const;

    /*!
     * Retrieves a the global integer variable with the given name.
     *
     * @param variableName The name of the global integer variable to retrieve.
     * @return The global integer variable with the given name.
     */
    IntegerVariable const& getGlobalIntegerVariable(std::string const& variableName) const;

    /*!
     * Retrieves all expression variables used by this program.
     *
     * @param includeConstants Whether to include constants in the set of expression variables.
     * @return The set of expression variables used by this program.
     */
    std::set<storm::expressions::Variable> getAllExpressionVariables(bool includeConstants = true) const;

    /*!
     * Retrieves a list of expressions that characterize the legal ranges of all variables.
     *
     * @return A list of expressions that characterize the legal ranges of all variables.
     */
    std::vector<storm::expressions::Expression> getAllRangeExpressions() const;

    /*!
     * Retrieves the number of global boolean variables of the program.
     *
     * @return The number of global boolean variables of the program.
     */
    std::size_t getNumberOfGlobalBooleanVariables() const;

    /*!
     * Retrieves the number of global integer variables of the program.
     *
     * @return The number of global integer variables of the program.
     */
    std::size_t getNumberOfGlobalIntegerVariables() const;

    /*!
     * Retrieves the formulas defined in the program.
     *
     * @return The formulas defined in the program.
     */
    std::vector<Formula> const& getFormulas() const;

    /*!
     * Retrieves the number of formulas in the program.
     *
     * @return The number of formulas in the program.
     */
    std::size_t getNumberOfFormulas() const;

    /*!
     * Retrieves the number of modules in the program.
     *
     * @return The number of modules in the program.
     */
    std::size_t getNumberOfModules() const;

    /*!
     * Retrieves the module with the given index.
     *
     * @param index The index of the module to retrieve.
     * @return The module with the given index.
     */
    Module const& getModule(uint_fast64_t index) const;

    /*!
     * Retrieves whether the program has a module with the given name.
     *
     * @return True iff a module with the given name exists.
     */
    bool hasModule(std::string const& moduleName) const;

    /*!
     * Retrieves the module with the given name.
     *
     * @param moduleName The name of the module to retrieve.
     * @return The module with the given name.
     */
    Module const& getModule(std::string const& moduleName) const;

    /*!
     * Retrieves all modules of the program.
     *
     * @return All modules of the program.
     */
    std::vector<Module> const& getModules() const;

    /*!
     * Retrieves the players of the program.
     *
     * @return The players of the program.
     */
    std::vector<Player> const& getPlayers() const;

    /*!
     * Retrieves the number of players in the program.
     *
     * @return The number of players in the program.
     */
    std::size_t getNumberOfPlayers() const;

    /*!
     * Retrieves the index of the player in the program.
     *
     * @return The index of the player in the program.
     */
    storm::storage::PlayerIndex const& getIndexOfPlayer(std::string const& playerName) const;

    /*!
     * @return Retrieves the mapping of player names to their indices.
     */
    std::map<std::string, storm::storage::PlayerIndex> const& getPlayerNameToIndexMapping() const;

    /*!
     * Retrieves a vector whose i'th entry corresponds to the player controlling module i.
     * Modules that are not controlled by any player will get assigned INVALID_PLAYER_INDEX
     */
    std::vector<storm::storage::PlayerIndex> buildModuleIndexToPlayerIndexMap() const;

    /*!
     * Retrieves a vector whose i'th entry corresponds to the player controlling action with index i.
     * Actions that are not controlled by any player (in particular the silent action) will get assigned INVALID_PLAYER_INDEX.
     */
    std::map<uint_fast64_t, storm::storage::PlayerIndex> buildActionIndexToPlayerIndexMap() const;

    /*!
     * Retrieves the mapping of action names to their indices.
     *
     * @return The mapping of action names to their indices.
     */
    std::map<std::string, uint_fast64_t> const& getActionNameToIndexMapping() const;

    /*!
     * Sets a new initial states expression.
     * May only be called if the program already has an initial states expression.
     */
    void updateInitialStatesExpression(expressions::Expression const& newExpression);

    /*!
     * Retrieves whether the program specifies an initial construct.
     */
    bool hasInitialConstruct() const;

    /*!
     * Retrieves an expression characterizing the initial states.
     *
     * @return an expression characterizing the initial states.
     */
    storm::expressions::Expression getInitialStatesExpression() const;

    /*!
     * Retrieves whether the program considers at least one update with an interval probability/rate
     */
    bool hasIntervalUpdates() const;

    /*!
     * Retrieves whether the program specifies a system composition in terms of process algebra operations over
     * the modules.
     *
     * @return True iff the program specifies a system composition.
     */
    bool specifiesSystemComposition() const;

    /*!
     * If the program specifies a system composition construct, this method retrieves it.
     *
     * @return The system composition construct as specified by the program.
     */
    SystemCompositionConstruct const& getSystemCompositionConstruct() const;

    /*!
     * Retrieves the system composition construct (if any) and none otherwise.
     *
     * @return The system composition construct specified by the program or none.
     */
    boost::optional<SystemCompositionConstruct> getOptionalSystemCompositionConstruct() const;

    /*!
     * Retrieves the default system composition for this program.
     *
     * @return The default system composition.
     */
    std::shared_ptr<Composition> getDefaultSystemComposition() const;

    /*!
     * Retrieves the set of actions present in the program.
     *
     * @return The set of actions present in the program.
     */
    std::set<std::string> const& getActions() const;

    /*!
     * Retrieves the set of synchronizing action indices present in the program.
     *
     * @return The set of synchronizing action indices present in the program.
     */
    std::set<uint_fast64_t> const& getSynchronizingActionIndices() const;

    /*!
     * Retrieves the action name of the given action index.
     *
     * @param actionIndex The index of the action whose name to retrieve.
     * @return The name of the action.
     */
    std::string const& getActionName(uint_fast64_t actionIndex) const;

    /*!
     * Retrieves the index of the action with the given name.
     *
     * @param actionName The name of the action.
     * @return The index of the action.
     */
    uint_fast64_t getActionIndex(std::string const& actionName) const;

    /*!
     * Retrieves whether the program has an action with the given name.
     *
     * @return True iff the program has an action with the given name.
     */
    bool hasAction(std::string const& actionName) const;

    /*!
     * Retrieves whether the program has an action with the given index.
     *
     * @return True iff the program has an action with the given index.
     */
    bool hasAction(uint_fast64_t const& actionIndex) const;

    /*!
     * Retrieves the indices of all modules within this program that contain commands that are labelled with the
     * given action.
     *
     * @param action The name of the action the modules are supposed to possess.
     * @return A set of indices of all matching modules.
     */
    std::set<uint_fast64_t> const& getModuleIndicesByAction(std::string const& action) const;

    /*!
     * Retrieves the indices of all modules within this program that contain commands that are labelled with the
     * given action index.
     *
     * @param actionIndex The index of the action the modules are supposed to possess.
     * @return A set of indices of all matching modules.
     */
    std::set<uint_fast64_t> const& getModuleIndicesByActionIndex(uint_fast64_t actionIndex) const;

    /*!
     * Retrieves the index of the module in which the given variable name was declared.
     *
     * @param variableName The name of the variable to search.
     * @return The index of the module in which the given variable name was declared.
     */
    uint_fast64_t getModuleIndexByVariable(std::string const& variableName) const;

    /*!
     * Retrieves the index of the module and the (local) index of the command with the given global command index.
     *
     * An exception is thrown if the command index is invalid.
     *
     * @note if (x,y) is the result of this method, we have
     * <code> getModule(x).getCommand(y).getGlobalIndex() == globalCommandIndex </code>
     *
     * @param globalCommandIndex the global command index specifying the command that is to be found
     * @return the index of the module and the (local) index of the found command
     */
    std::pair<uint_fast64_t, uint_fast64_t> getModuleCommandIndexByGlobalCommandIndex(uint_fast64_t globalCommandIndex) const;

    /*
     * Get total number of unlabeled commands
     */
    uint64_t getNumberOfUnlabeledCommands() const;

    /*!
     * Retrieves whether the program has reward models.
     *
     * @return True iff the program has at least one reward model.
     */
    bool hasRewardModel() const;

    /*!
     * Retrieves whether the program has a reward model with the given name.
     *
     * @param name The name of the reward model to look for.
     * @return True iff the program has a reward model with the given name.
     */
    bool hasRewardModel(std::string const& name) const;

    /*!
     * Retrieves the reward models of the program.
     *
     * @return The reward models of the program.
     */
    std::vector<RewardModel> const& getRewardModels() const;

    /*!
     * Retrieves the number of reward models in the program.
     *
     * @return The number of reward models in the program.
     */
    std::size_t getNumberOfRewardModels() const;

    /*!
     * Retrieves the reward model with the given name.
     *
     * @param rewardModelName The name of the reward model to return.
     * @return The reward model with the given name.
     */
    RewardModel const& getRewardModel(std::string const& rewardModelName) const;

    /*!
     * Retrieves the reward model with the given index.
     *
     * @param index The index of the reward model to return.
     * @return The reward model with the given index.
     */
    RewardModel const& getRewardModel(uint_fast64_t index) const;

    /*!
     * Checks whether the program has a label with the given name.
     *
     * @param labelName The label of the program.
     * @return True iff the label of the program.
     */
    bool hasLabel(std::string const& labelName) const;

    /*!
     * Retrieves all labels that are defined by the probabilitic program.
     *
     * @return A set of labels that are defined in the program.
     */
    std::vector<Label> const& getLabels() const;

    /*!
     * Retrieves all guards appearing in the program.
     *
     * @param negated A flag indicating whether the guards should be negated.
     * @return All guards appearing in the program.
     */
    std::vector<storm::expressions::Expression> getAllGuards(bool negated = false) const;

    /*!
     * Retrieves the expression associated with the given label, if it exists.
     *
     * @param labelName The name of the label to retrieve.
     */
    storm::expressions::Expression const& getLabelExpression(std::string const& label) const;

    /*!
     * Retrieves a mapping from all labels in the program to their defining expressions.
     *
     * @return A mapping from label names to their expressions.
     */
    std::map<std::string, storm::expressions::Expression> getLabelToExpressionMapping() const;

    /*!
     * Retrieves the number of labels in the program.
     *
     * @return The number of labels in the program.
     */
    std::size_t getNumberOfLabels() const;

    /*!
     * Adds a label with the given name and defining expression to the program.
     *
     * @param name The name of the label. This name must not yet exist as a label name in the program.
     * @param statePredicateExpression The predicate that is described by the label.
     */
    void addLabel(std::string const& name, storm::expressions::Expression const& statePredicateExpression);

    /*!
     * Removes the label with the given name from the program.
     *
     * @param name The name of a label that exists within the program.
     */
    void removeLabel(std::string const& name);

    /*!
     * Removes all labels that are not contained in the given set from the program. Note: no check is performed
     * as to whether or not the given label names actually exist.
     *
     * @param labelSet The label set that is to be kept.
     */
    void filterLabels(std::set<std::string> const& labelSet);

    void removeRewardModels();

    /*!
     * Retrieves all observation labels that are defined by this program
     *
     * @return A set of observation labels
     */
    std::vector<ObservationLabel> const& getObservationLabels() const;

    /*!
     * Retrieves the number of observation labels in the program.
     *
     * @return The number of labels in the program.
     */
    std::size_t getNumberOfObservationLabels() const;

    /*!
     * Creates a new program that drops all commands whose indices are not in the given set.
     *
     * @param indexSet The set of indices for which to keep the commands.
     */
    Program restrictCommands(storm::storage::FlatSet<uint_fast64_t> const& indexSet) const;

    /*!
     * Defines the undefined constants according to the given map and returns the resulting program.
     *
     * @param constantDefinitions A mapping from undefined constant to the expressions they are supposed
     * to be replaced with.
     * @return The program after all undefined constants in the given map have been replaced with their
     * definitions.
     */
    Program defineUndefinedConstants(std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) const;

    /*!
     * Substitutes all constants appearing in the expressions of the program by their defining expressions. For
     * this to work, all constants need to be defined prior to calling this.
     *
     * @return The resulting program that only contains expressions over variables of the program (and maybe formulas).
     */
    Program substituteConstants() const;

    /*!
     * Substitutes all formulas appearing in the expressions of the program by their defining expressions.
     *
     * The resulting program still contains the function definition, but does not apply them.
     * @return The resulting program that only contains expressions over variables of the program (and maybe constants).
     */
    Program substituteFormulas() const;

    /*!
     * Substitutes all nonstandard predicates in expressions of the program by their defining expressions
     */
    Program substituteNonStandardPredicates() const;

    /*!
     * Substitutes all constants and/or formulas appearing in the expressions of the program by their defining expressions. For
     * this to work, all constants need to be defined prior to calling this.
     *
     * @return The resulting program that only contains expressions over variables of the program.
     */
    Program substituteConstantsFormulas(bool substituteConstants = true, bool substituteFormulas = true) const;

    /*!
     * Replace the initialization in variables by an init-expression. This should not change the semantics of the program and can be a preprocessing step.
     *
     * @return
     */
    Program replaceVariableInitializationByInitExpression() const;

    /**
     * Substitutes the given constant by a fresh global variable that is bound between lowerBound and upperBound.
     * The provided lowerBound and upperBound are not considered if the constant is Boolean.
     * If the constant is defined by an expression, that expression is used as initial value for the created variable.
     * @note If the given constant appears at positions where a variable would be invalid (e.g. variable bounds
         or the definition of another constant), the resulting program is malformed.
     * @return
     */
    Program replaceConstantByVariable(Constant const& c, expressions::Expression const& lowerBound, expressions::Expression const& upperBound,
                                      bool observable = true) const;

    /**
     * Entry point for static analysis for simplify. As we use the same expression manager, we recommend to not use the original program any further.
     * @return A simplified, equivalent program.
     */
    Program simplify();

    /*!
     * Checks the validity of the program. If the program is not valid, an exception is thrown with a message
     * that indicates the source of the problem.
     */
    void checkValidity(Program::ValidityCheckLevel lvl = Program::ValidityCheckLevel::READYFORPROCESSING) const;

    /*!
     * Creates an equivalent program that contains exactly one module.
     *
     * @param smtSolverFactory an SMT solver factory to use. If none is given, the default one is used.
     * @return The resulting program.
     */
    Program flattenModules(std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory =
                               std::shared_ptr<storm::utility::solver::SmtSolverFactory>(new storm::utility::solver::SmtSolverFactory())) const;

    /*!
     * Give commands that do not have an action name an action,
     * which can be helpful for debugging and understanding traces.
     *
     * @param nameSuggestions Optional suggestions that map command indices to names
     * @return
     */
    Program labelUnlabelledCommands(std::map<uint64_t, std::string> const& nameSuggestions = {}) const;

    friend std::ostream& operator<<(std::ostream& stream, Program const& program);

    /*!
     * Retrieves the manager responsible for the expressions of this program.
     *
     * @return The manager responsible for the expressions of this program.
     */
    storm::expressions::ExpressionManager& getManager() const;

    std::unordered_map<uint_fast64_t, std::string> buildCommandIndexToActionNameMap() const;

    std::unordered_map<uint_fast64_t, uint_fast64_t> buildCommandIndexToActionIndex() const;

    std::unordered_map<uint_fast64_t, std::string> buildActionIndexToActionNameMap() const;

    /*!
     * Converts the PRISM model into an equivalent JANI model.
     */
    storm::jani::Model toJani(bool allVariablesGlobal = true, std::string suffix = "") const;

    /*!
     * Converts the PRISM model into an equivalent JANI model and if labels or reward models had
     * to be renamed in the process, the renamings are applied to the given properties
     * @return The jani model of this and either the new set of properties or an empty vector if no renamings were necessary
     */
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> toJani(std::vector<storm::jani::Property> const& properties,
                                                                             bool allVariablesGlobal = true, std::string suffix = "") const;

    /*!
     * Compute the (labelled) commands in the program that may be synchronizing
     * @return A bitvector representing the set of commands by global command index
     */
    storm::storage::BitVector const& getPossiblySynchronizingCommands() const;

   private:
    /*!
     * Retrieves the initial construct of the program.
     *
     * @return The initial construct of the program.
     */
    InitialConstruct const& getInitialConstruct() const;

    /*!
     * Retrieves an optional containing the initial construct of the program if there is any and nothing otherwise.
     *
     * @return The initial construct of the program.
     */
    boost::optional<InitialConstruct> const& getOptionalInitialConstruct() const;

    /*!
     * This function builds a command that corresponds to the synchronization of the given list of commands.
     *
     * @param newCommandIndex The index of the command to construct.
     * @param actionIndex The index of the action of the resulting command.
     * @param firstUpdateIndex The index of the first update of the resulting command.
     * @param actionName The name of the action of the resulting command.
     * @param commands The commands to synchronize.
     * @return The resulting command.
     */
    Command synchronizeCommands(uint_fast64_t newCommandIndex, uint_fast64_t actionIndex, uint_fast64_t firstUpdateIndex, std::string const& actionName,
                                std::vector<std::reference_wrapper<Command const>> const& commands) const;

    /*!
     * Equips all global variables without initial values with initial values based on their type.
     */
    void createMissingInitialValues();

    // The manager responsible for the variables/expressions of the program.
    std::shared_ptr<storm::expressions::ExpressionManager> manager;

    // Creates the internal mappings.
    void createMappings();

    uint64_t getHighestCommandIndex() const;

    // The type of the model.
    ModelType modelType;

    // The constants of the program.
    std::vector<Constant> constants;

    // A mapping from constant names to their corresponding indices.
    std::map<std::string, uint_fast64_t> constantToIndexMap;

    // The global boolean variables.
    std::vector<BooleanVariable> globalBooleanVariables;

    // A mapping from global boolean variable names to their corresponding indices.
    std::map<std::string, uint_fast64_t> globalBooleanVariableToIndexMap;

    // The global integer variables.
    std::vector<IntegerVariable> globalIntegerVariables;

    // A mapping from global integer variable names to their corresponding indices.
    std::map<std::string, uint_fast64_t> globalIntegerVariableToIndexMap;

    // The formulas defined in the program.
    std::vector<Formula> formulas;

    // A mapping of formula names to their corresponding indices.
    std::map<std::string, uint_fast64_t> formulaToIndexMap;

    // The players associated with the program.
    std::vector<Player> players;

    // A mapping of player names to their indices.
    std::map<std::string, storm::storage::PlayerIndex> playerToIndexMap;

    // The modules associated with the program.
    std::vector<Module> modules;

    // A mapping of module names to their indices.
    std::map<std::string, uint_fast64_t> moduleToIndexMap;

    // The reward models associated with the program.
    std::vector<RewardModel> rewardModels;

    // A mapping of reward models to their indices.
    std::map<std::string, uint_fast64_t> rewardModelToIndexMap;

    // The initial construct of the program.
    boost::optional<InitialConstruct> initialConstruct;

    // If set, this specifies the way the modules are composed to obtain the full system.
    boost::optional<SystemCompositionConstruct> systemCompositionConstruct;

    // The labels that are defined for this model.
    std::vector<Label> labels;

    // A mapping from labels to their indices.
    std::map<std::string, uint_fast64_t> labelToIndexMap;

    // Observation labels
    std::vector<ObservationLabel> observationLabels;

    // A mapping from action names to their indices.
    std::map<std::string, uint_fast64_t> actionToIndexMap;

    // A mapping from action indices to their names.
    std::map<uint_fast64_t, std::string> indexToActionMap;

    // The set of actions present in this program.
    std::set<std::string> actions;

    // The set of synchronizing actions present in this program.
    std::set<uint_fast64_t> synchronizingActionIndices;

    // A map of actions to the set of modules containing commands labelled with this action.
    std::map<uint_fast64_t, std::set<uint_fast64_t>> actionIndicesToModuleIndexMap;

    // A mapping from variable names to the modules in which they were declared.
    std::map<std::string, uint_fast64_t> variableToModuleIndexMap;

    storage::BitVector possiblySynchronizingCommands;

    bool prismCompatibility;
};

std::ostream& operator<<(std::ostream& out, Program::ModelType const& type);

}  // namespace prism
}  // namespace storm
