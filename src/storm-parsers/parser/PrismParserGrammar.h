#pragma once

// Include files for file input.
#include <fstream>
#include <iomanip>
#include <memory>

#include "storm-parsers/parser/SpiritErrorHandler.h"
#include "storm-parsers/parser/SpiritParserDefinitions.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/prism/Program.h"

namespace storm {
namespace expressions {
class ExpressionManager;
}
}  // namespace storm

namespace storm {
namespace parser {
class ExpressionParser;

// A class that stores information about the parsed program.
class GlobalProgramInformation {
   public:
    // Default construct the header information.
    GlobalProgramInformation()
        : modelType(storm::prism::Program::ModelType::UNDEFINED),
          constants(),
          formulas(),
          globalBooleanVariables(),
          globalIntegerVariables(),
          moduleToIndexMap(),
          actionIndices(),
          modules(),
          rewardModels(),
          labels(),
          hasInitialConstruct(false),
          initialConstruct(),
          systemCompositionConstruct(boost::none),
          currentCommandIndex(0),
          currentUpdateIndex(0) {
        // Map the empty action to index 0.
        actionIndices.emplace("", 0);
    }

    void moveToSecondRun() {
        // Clear all data except the action to indices mapping.
        modelType = storm::prism::Program::ModelType::UNDEFINED;
        constants.clear();
        formulas.clear();
        globalBooleanVariables.clear();
        globalIntegerVariables.clear();
        players.clear();
        playerControlledModules.clear();
        playerControlledActions.clear();
        modules.clear();
        rewardModels.clear();
        labels.clear();
        observationLabels.clear();
        hasInitialConstruct = false;
        initialConstruct = storm::prism::InitialConstruct();
        systemCompositionConstruct = boost::none;

        currentCommandIndex = 0;
        currentUpdateIndex = 0;
    }

    // Members for all essential information that needs to be collected.
    storm::prism::Program::ModelType modelType;
    std::vector<storm::prism::Constant> constants;
    std::vector<storm::prism::Formula> formulas;
    std::vector<storm::prism::BooleanVariable> globalBooleanVariables;
    std::vector<storm::prism::IntegerVariable> globalIntegerVariables;
    std::map<std::string, uint_fast64_t> moduleToIndexMap;
    std::map<std::string, uint_fast64_t> actionIndices;
    std::vector<storm::prism::Module> modules;
    std::vector<storm::prism::RewardModel> rewardModels;
    std::vector<storm::prism::Label> labels;
    std::vector<storm::prism::ObservationLabel> observationLabels;
    std::vector<storm::prism::Player> players;
    std::set<uint_fast64_t> playerControlledModules;
    std::set<uint_fast64_t> playerControlledActions;

    bool hasInitialConstruct;
    storm::prism::InitialConstruct initialConstruct;
    boost::optional<storm::prism::SystemCompositionConstruct> systemCompositionConstruct;

    // Counters to provide unique indexing for commands and updates.
    uint_fast64_t currentCommandIndex;
    uint_fast64_t currentUpdateIndex;
};

class PrismParserGrammar : public qi::grammar<Iterator, storm::prism::Program(), Skipper> {
   public:
    /*!
     * Parses the given file into the PRISM storage classes assuming it complies with the PRISM syntax.
     *
     * @param filename the name of the file to parse.
     * @return The resulting PRISM program.
     */
    static storm::prism::Program parse(std::string const& filename, bool prismCompatability = false);

    /*!
     * Parses the given input stream into the PRISM storage classes assuming it complies with the PRISM syntax.
     *
     * @param input The input string to parse.
     * @param filename The name of the file from which the input was read.
     * @return The resulting PRISM program.
     */
    static storm::prism::Program parseFromString(std::string const& input, std::string const& filename, bool prismCompatability = false);

   private:
    struct modelTypeStruct : qi::symbols<char, storm::prism::Program::ModelType> {
        modelTypeStruct() {
            add("dtmc", storm::prism::Program::ModelType::DTMC)("ctmc", storm::prism::Program::ModelType::CTMC)("mdp", storm::prism::Program::ModelType::MDP)(
                "ctmdp", storm::prism::Program::ModelType::CTMDP)("ma", storm::prism::Program::ModelType::MA)("pomdp", storm::prism::Program::ModelType::POMDP)(
                "pta", storm::prism::Program::ModelType::PTA)("smg", storm::prism::Program::ModelType::SMG);
        }
    };

    struct keywordsStruct : qi::symbols<char, uint_fast64_t> {
        keywordsStruct() {
            add("dtmc", 1)("ctmc", 2)("mdp", 3)("ctmdp", 4)("ma", 5)("pomdp", 6)("pta", 7)("smg", 8)("const", 9)("int", 10)("bool", 11)("module", 12)(
                "endmodule", 13)("rewards", 14)("endrewards", 15)("true", 16)("false", 17)("min", 18)("max", 19)("floor", 20)("ceil", 21)("init", 22)(
                "atLeastOneOf", 23)("atMostOneOf", 24)("exactlyOneOf", 25)("endinit", 26)("invariant", 27)("endinvariant", 28)("player", 29)("endplayer", 30);
        }
    };

    // Same as keywordsStruct, without the model-type-specific keywords (see isValidIdentifier).
    struct expressionKeywordsStruct : qi::symbols<char, uint_fast64_t> {
        expressionKeywordsStruct() {
            add("dtmc", 1)("ctmc", 2)("mdp", 3)("ctmdp", 4)("ma", 5)("pomdp", 6)("pta", 7)("smg", 8)("const", 9)("int", 10)("bool", 11)("module", 12)(
                "endmodule", 13)("rewards", 14)("endrewards", 15)("true", 16)("false", 17)("min", 18)("max", 19)("floor", 20)("ceil", 21)("init", 22)(
                "atLeastOneOf", 23)("atMostOneOf", 24)("exactlyOneOf", 25)("endinit", 26);
        }
    };

    // Functor used for annotating entities with line number information.
    class PositionAnnotation {
       public:
        typedef void result_type;

        PositionAnnotation(Iterator first) : first(first) {
            // Intentionally left empty.
        }

        template<typename Entity, typename First, typename Last>
        result_type operator()(Entity& entity, First f, Last) const {
            entity.setLineNumber(get_line(f));
        }

       private:
        std::string filename;
        Iterator const first;
    };

    /*!
     * Creates a grammar for the given filename and the iterator to the first input to parse.
     *
     * @param filename The filename that is to be read. This is used for proper error reporting.
     * @param first The iterator to the beginning of the input.
     */
    PrismParserGrammar(std::string const& filename, Iterator first, bool prismCompatibility);

    /*!
     * Sets an internal flag that indicates the second run is now taking place.
     */
    void moveToSecondRun();

    /*!
     * Parses the stored formula Expressions.
     */
    void createFormulaIdentifiers(std::vector<storm::prism::Formula> const& formulas);

    // A flag that stores whether the grammar is currently doing the second run.
    bool secondRun;

    bool prismCompatibility;

    /*!
     * Sets whether doubles literals are allowed in the parsed expression.
     *
     * @param flag Indicates whether to allow or forbid double literals in the parsed expression.
     */
    void allowDoubleLiterals(bool flag);

    // The name of the file being parsed.
    std::string filename;

    /*!
     * Retrieves the name of the file currently being parsed.
     *
     * @return The name of the file currently being parsed.
     */
    std::string const& getFilename() const;

    // Name of the last identifier that was rejected for coinciding with a reserved keyword.
    // Only meaningful for diagnosing an actual parse failure, since valid parses may speculatively
    // (and harmlessly) try identifiers that collide with keywords while exploring grammar alternatives.
    std::string lastRejectedKeywordIdentifier;

    mutable std::set<std::string> observables;

    // Store the expressions of formulas. They have to be parsed after the first and before the second run
    std::vector<std::string> formulaExpressions;
    // Stores a proper order in which formulas can be evaluated. This is necessary since formulas might depend on each other.
    // E.g. for "formula x = y; formula y = z;" we have to swap the order of the two formulas.
    std::vector<uint64_t> formulaOrder;

    // A function used for annotating the entities with their position.
    phoenix::function<PositionAnnotation> annotate;

    // An object gathering information about the program while parsing.
    GlobalProgramInformation globalProgramInformation;

    // The starting point of the grammar.
    qi::rule<Iterator, storm::prism::Program(), Skipper> start;

    // Rules for model type.
    qi::rule<Iterator, storm::prism::Program::ModelType(), Skipper> modelTypeDefinition;

    // Rules for parsing expressions of specific type
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> boolExpression;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> intExpression;
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> numericalExpression;

    // Rules for parsing the program header.
    qi::rule<Iterator, storm::prism::Constant(), Skipper> undefinedConstantDefinition;
    qi::rule<Iterator, storm::prism::Constant(), Skipper> undefinedBooleanConstantDefinition;
    qi::rule<Iterator, storm::prism::Constant(), Skipper> undefinedIntegerConstantDefinition;
    qi::rule<Iterator, storm::prism::Constant(), Skipper> undefinedDoubleConstantDefinition;
    qi::rule<Iterator, storm::prism::Constant(), Skipper> definedConstantDefinition;
    qi::rule<Iterator, storm::prism::Constant(), Skipper> definedBooleanConstantDefinition;
    qi::rule<Iterator, storm::prism::Constant(), Skipper> definedIntegerConstantDefinition;
    qi::rule<Iterator, storm::prism::Constant(), Skipper> definedDoubleConstantDefinition;

    // Rules for global variable definitions.
    qi::rule<Iterator, qi::unused_type(GlobalProgramInformation&), Skipper> globalVariableDefinition;

    // Rules for modules definition.
    qi::rule<Iterator, std::string(), Skipper> knownModuleName;
    qi::rule<Iterator, std::string(), Skipper> freshModuleName;
    qi::rule<Iterator, storm::prism::Module(GlobalProgramInformation&),
             qi::locals<std::vector<storm::prism::BooleanVariable>, std::vector<storm::prism::IntegerVariable>, std::vector<storm::prism::ClockVariable>>,
             Skipper>
        moduleDefinition;
    qi::rule<Iterator, storm::prism::ModuleRenaming, qi::locals<std::map<std::string, std::string>>, Skipper> moduleRenaming;
    qi::rule<Iterator, storm::prism::Module(GlobalProgramInformation&), qi::locals<std::string, storm::prism::ModuleRenaming>, Skipper> renamedModule;

    // Rules for variable definitions.
    qi::rule<Iterator,
             qi::unused_type(std::vector<storm::prism::BooleanVariable>&, std::vector<storm::prism::IntegerVariable>&,
                             std::vector<storm::prism::ClockVariable>&),
             Skipper>
        variableDefinition;
    qi::rule<Iterator, storm::prism::BooleanVariable(), qi::locals<storm::expressions::Expression>, Skipper> booleanVariableDefinition;
    qi::rule<Iterator, storm::prism::IntegerVariable(), qi::locals<storm::expressions::Expression>, Skipper> integerVariableDefinition;
    qi::rule<Iterator, storm::prism::IntegerVariable(), qi::locals<storm::expressions::Expression>, Skipper> boundedIntegerVariableDefinition;
    qi::rule<Iterator, storm::prism::IntegerVariable(), qi::locals<storm::expressions::Expression>, Skipper> unboundedIntegerVariableDefinition;
    qi::rule<Iterator, storm::prism::ClockVariable(), qi::locals<storm::expressions::Expression>, Skipper> clockVariableDefinition;

    // Rules for command definitions.
    qi::rule<Iterator, storm::prism::Command(GlobalProgramInformation&), qi::locals<bool>, Skipper> commandDefinition;
    qi::rule<Iterator, std::vector<storm::prism::Update>(GlobalProgramInformation&), Skipper> updateListDefinition;
    qi::rule<Iterator, storm::prism::Update(GlobalProgramInformation&), Skipper> updateDefinition;
    qi::rule<Iterator, typename storm::prism::Update::ExpressionPair, Skipper> likelihoodDefinition;
    qi::rule<Iterator, std::vector<storm::prism::Assignment>(), Skipper> assignmentDefinitionList;
    qi::rule<Iterator, storm::prism::Assignment(), Skipper> assignmentDefinition;
    qi::rule<Iterator, std::string(), Skipper> knownActionName;

    // Rules for reward definitions.
    qi::rule<Iterator, std::string(), Skipper> freshRewardModelName;
    qi::rule<Iterator, storm::prism::RewardModel(GlobalProgramInformation&),
             qi::locals<std::string, std::vector<storm::prism::StateReward>, std::vector<storm::prism::StateActionReward>,
                        std::vector<storm::prism::TransitionReward>>,
             Skipper>
        rewardModelDefinition;
    qi::rule<Iterator, storm::prism::StateReward(), Skipper> stateRewardDefinition;
    qi::rule<Iterator, storm::prism::StateActionReward(GlobalProgramInformation&), Skipper> stateActionRewardDefinition;
    qi::rule<Iterator, storm::prism::TransitionReward(GlobalProgramInformation&),
             qi::locals<std::string, storm::expressions::Expression, storm::expressions::Expression, storm::expressions::Expression>, Skipper>
        transitionRewardDefinition;

    // Rules for player definitions
    qi::rule<Iterator, std::string(), Skipper> freshPlayerName;
    qi::rule<Iterator, std::string(), qi::locals<std::string>, Skipper> playerControlledActionName;
    qi::rule<Iterator, std::string(), qi::locals<std::string>, Skipper> playerControlledModuleName;
    qi::rule<Iterator, storm::prism::Player(GlobalProgramInformation&), qi::locals<std::string, std::vector<std::string>, std::vector<std::string>>, Skipper>
        playerConstruct;

    // Rules for initial states expression.
    qi::rule<Iterator, qi::unused_type(GlobalProgramInformation&), Skipper> initialStatesConstruct;

    // Rules for POMDP observables (standard prism)
    qi::rule<Iterator, qi::unused_type(), Skipper> observablesConstruct;

    // Rules for invariant constructs
    qi::rule<Iterator, storm::expressions::Expression(), Skipper> invariantConstruct;

    // Rules for the system composition.
    qi::rule<Iterator, qi::unused_type(GlobalProgramInformation&), Skipper> systemCompositionConstruct;
    qi::rule<Iterator, std::shared_ptr<storm::prism::Composition>(), Skipper> parallelComposition;
    qi::rule<Iterator, qi::unused_type(), Skipper> synchronizingParallelComposition;
    qi::rule<Iterator, qi::unused_type(), Skipper> interleavingParallelComposition;
    qi::rule<Iterator, std::set<std::string>(), Skipper> restrictedParallelComposition;
    qi::rule<Iterator, std::shared_ptr<storm::prism::Composition>(), Skipper> hidingOrRenamingComposition;
    qi::rule<Iterator, std::shared_ptr<storm::prism::Composition>(), Skipper> hidingComposition;
    qi::rule<Iterator, std::shared_ptr<storm::prism::Composition>(), Skipper> renamingComposition;
    qi::rule<Iterator, std::shared_ptr<storm::prism::Composition>(), Skipper> atomicComposition;
    qi::rule<Iterator, std::shared_ptr<storm::prism::Composition>(), Skipper> moduleComposition;
    qi::rule<Iterator, std::set<std::string>(), Skipper> actionNameList;
    qi::rule<Iterator, std::map<std::string, std::string>(), Skipper> actionRenamingList;

    // Rules for label definitions.
    qi::rule<Iterator, storm::prism::Label(), Skipper> labelDefinition;
    qi::rule<Iterator, std::string(), Skipper> freshLabelName;

    // Rules for observable (observation-label) definitions.
    qi::rule<Iterator, storm::prism::ObservationLabel(), Skipper> observableDefinition;
    qi::rule<Iterator, std::string(), Skipper> freshObservationLabelName;

    // Rules for formula definitions.
    qi::rule<Iterator, std::string(), Skipper> formulaDefinitionRhs;
    qi::rule<Iterator, storm::prism::Formula(), Skipper> formulaDefinition;

    // Rules for identifier parsing.
    qi::rule<Iterator, std::string(), Skipper> identifier;
    qi::rule<Iterator, std::string(), Skipper> freshIdentifier;

    // Parsers that recognize special keywords and model types.
    storm::parser::PrismParserGrammar::keywordsStruct keywords_;
    // Same as keywords_, but without the keywords that are only reserved for a specific model type
    // (see isValidIdentifier) -- the model type is not statically known here, so this is unconditional.
    storm::parser::PrismParserGrammar::expressionKeywordsStruct expressionKeywords_;
    storm::parser::PrismParserGrammar::modelTypeStruct modelType_;
    qi::symbols<char, storm::expressions::Expression> identifiers_;

    // Parser and manager used for recognizing expressions.
    std::shared_ptr<storm::expressions::ExpressionManager> manager;
    std::shared_ptr<storm::parser::ExpressionParser> expressionParser;

    // Helper methods used in the grammar.
    bool isValidIdentifier(std::string const& identifier);
    // Logs the last keyword collision recorded by isValidIdentifier, if any. Called from the top-level error handler.
    void reportRejectedKeywordIdentifier();
    bool isFreshIdentifier(std::string const& identifier);
    bool isKnownModuleName(std::string const& moduleName, bool inSecondRun);
    bool isFreshModuleName(std::string const& moduleName);
    bool isKnownActionName(std::string const& actionName, bool inSecondRun);
    bool isFreshLabelName(std::string const& moduleName);
    bool isFreshObservationLabelName(std::string const& labelName);
    bool isFreshRewardModelName(std::string const& moduleName);
    bool isFreshPlayerName(std::string const& playerName);
    bool isOfBoolType(storm::expressions::Expression const& expression);
    bool isOfIntType(storm::expressions::Expression const& expression);
    bool isOfNumericalType(storm::expressions::Expression const& expression);
    bool isValidModuleRenaming(std::string const& oldModuleName, storm::prism::ModuleRenaming const& renaming,
                               GlobalProgramInformation const& globalProgramInformation) const;
    bool addInitialStatesConstruct(storm::expressions::Expression const& initialStatesExpression, GlobalProgramInformation& globalProgramInformation);
    bool addSystemCompositionConstruct(std::shared_ptr<storm::prism::Composition> const& composition, GlobalProgramInformation& globalProgramInformation);
    void setModelType(GlobalProgramInformation& globalProgramInformation, storm::prism::Program::ModelType const& modelType);

    std::shared_ptr<storm::prism::Composition> createModuleComposition(std::string const& moduleName) const;
    std::shared_ptr<storm::prism::Composition> createRenamingComposition(std::shared_ptr<storm::prism::Composition> const& subcomposition,
                                                                         std::map<std::string, std::string> const& renaming) const;
    std::shared_ptr<storm::prism::Composition> createHidingComposition(std::shared_ptr<storm::prism::Composition> const& subcomposition,
                                                                       std::set<std::string> const& actionsToHide) const;
    std::shared_ptr<storm::prism::Composition> createSynchronizingParallelComposition(std::shared_ptr<storm::prism::Composition> const& left,
                                                                                      std::shared_ptr<storm::prism::Composition> const& right) const;
    std::shared_ptr<storm::prism::Composition> createInterleavingParallelComposition(std::shared_ptr<storm::prism::Composition> const& left,
                                                                                     std::shared_ptr<storm::prism::Composition> const& right) const;
    std::shared_ptr<storm::prism::Composition> createRestrictedParallelComposition(std::shared_ptr<storm::prism::Composition> const& left,
                                                                                   std::set<std::string> const& synchronizingActions,
                                                                                   std::shared_ptr<storm::prism::Composition> const& right) const;

    storm::prism::Constant createUndefinedBooleanConstant(std::string const& newConstant) const;
    storm::prism::Constant createUndefinedIntegerConstant(std::string const& newConstant) const;
    storm::prism::Constant createUndefinedDoubleConstant(std::string const& newConstant) const;
    storm::prism::Constant createDefinedBooleanConstant(std::string const& newConstant, storm::expressions::Expression expression) const;
    storm::prism::Constant createDefinedIntegerConstant(std::string const& newConstant, storm::expressions::Expression expression) const;
    storm::prism::Constant createDefinedDoubleConstant(std::string const& newConstant, storm::expressions::Expression expression) const;
    storm::prism::Formula createFormulaFirstRun(std::string const& formulaName, std::string const& expression);
    storm::prism::Formula createFormulaSecondRun(std::string const& formulaName, storm::expressions::Expression const& expression);
    storm::prism::Label createLabel(std::string const& labelName, storm::expressions::Expression expression) const;
    storm::prism::ObservationLabel createObservationLabel(std::string const& labelName, storm::expressions::Expression expression) const;
    storm::prism::RewardModel createRewardModel(std::string const& rewardModelName, std::vector<storm::prism::StateReward> const& stateRewards,
                                                std::vector<storm::prism::StateActionReward> const& stateActionRewards,
                                                std::vector<storm::prism::TransitionReward> const& transitionRewards) const;
    storm::prism::StateReward createStateReward(storm::expressions::Expression statePredicateExpression,
                                                storm::expressions::Expression rewardValueExpression) const;
    storm::prism::StateActionReward createStateActionReward(boost::optional<std::string> const& actionName,
                                                            storm::expressions::Expression statePredicateExpression,
                                                            storm::expressions::Expression rewardValueExpression,
                                                            GlobalProgramInformation& globalProgramInformation) const;
    storm::prism::TransitionReward createTransitionReward(boost::optional<std::string> const& actionName,
                                                          storm::expressions::Expression sourceStatePredicateExpression,
                                                          storm::expressions::Expression targetStatePredicateExpression,
                                                          storm::expressions::Expression rewardValueExpression,
                                                          GlobalProgramInformation& globalProgramInformation) const;
    storm::prism::Assignment createAssignment(std::string const& variableName, storm::expressions::Expression assignedExpression) const;
    storm::prism::Update createUpdate(typename storm::prism::Update::ExpressionPair likelihoodExpressions,
                                      std::vector<storm::prism::Assignment> const& assignments, GlobalProgramInformation& globalProgramInformation) const;

    storm::prism::Command createCommand(bool markovianCommand, boost::optional<std::string> const& actionName, storm::expressions::Expression guardExpression,
                                        std::vector<storm::prism::Update> const& updates, GlobalProgramInformation& globalProgramInformation) const;
    storm::prism::Command createDummyCommand(boost::optional<std::string> const& actionName, GlobalProgramInformation& globalProgramInformation) const;
    storm::prism::BooleanVariable createBooleanVariable(std::string const& variableName, storm::expressions::Expression initialValueExpression) const;
    storm::prism::IntegerVariable createIntegerVariable(std::string const& variableName, storm::expressions::Expression lowerBoundExpression,
                                                        storm::expressions::Expression upperBoundExpression,
                                                        storm::expressions::Expression initialValueExpression) const;
    storm::prism::ClockVariable createClockVariable(std::string const& variableName) const;
    storm::prism::Module createModule(std::string const& moduleName, std::vector<storm::prism::BooleanVariable> const& booleanVariables,
                                      std::vector<storm::prism::IntegerVariable> const& integerVariables,
                                      std::vector<storm::prism::ClockVariable> const& clockVariables,
                                      boost::optional<storm::expressions::Expression> const& invariant, std::vector<storm::prism::Command> const& commands,
                                      GlobalProgramInformation& globalProgramInformation) const;
    storm::prism::ModuleRenaming createModuleRenaming(std::map<std::string, std::string> const& renaming) const;
    storm::prism::Module createRenamedModule(std::string const& newModuleName, std::string const& oldModuleName, storm::prism::ModuleRenaming const& renaming,
                                             GlobalProgramInformation& globalProgramInformation) const;
    storm::prism::Player createPlayer(std::string const& playerName, std::vector<std::string> const& moduleNames, std::vector<std::string> const& commandNames);
    storm::prism::Program createProgram(GlobalProgramInformation const& globalProgramInformation) const;
    void createObservablesList(std::vector<std::string> const& observables);

    void removeInitialConstruct(GlobalProgramInformation& globalProgramInformation) const;

    // An error handler function.
    phoenix::function<SpiritErrorHandler> handler;
};
}  // namespace parser
}  // namespace storm
