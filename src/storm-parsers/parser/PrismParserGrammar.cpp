#include "storm-parsers/parser/PrismParserGrammar.h"

#include <unordered_set>
#include "storm/storage/prism/Compositions.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/utility/macros.h"

#include "storm/storage/BitVector.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/VariableExpression.h"

#include "storm-parsers/parser/ExpressionParser.h"

namespace storm {
namespace parser {
storm::prism::Program PrismParserGrammar::parse(std::string const& filename, bool prismCompatibility) {
    // Open file and initialize result.
    std::ifstream inputFileStream;
    storm::io::openFile(filename, inputFileStream);
    storm::prism::Program result;

    // Now try to parse the contents of the file.
    try {
        std::string fileContent((std::istreambuf_iterator<char>(inputFileStream)), (std::istreambuf_iterator<char>()));
        result = parseFromString(fileContent, filename, prismCompatibility);
    } catch (storm::exceptions::WrongFormatException& e) {
        // In case of an exception properly close the file before passing exception.
        storm::io::closeFile(inputFileStream);
        throw e;
    } catch (std::exception& e) {
        // In case of an exception properly close the file before passing exception.
        storm::io::closeFile(inputFileStream);
        throw e;
    }

    // Close the stream in case everything went smoothly and return result.
    storm::io::closeFile(inputFileStream);
    return result;
}

storm::prism::Program PrismParserGrammar::parseFromString(std::string const& input, std::string const& filename, bool prismCompatibility) {
    bool hasByteOrderMark = input.size() >= 3 && input[0] == '\xEF' && input[1] == '\xBB' && input[2] == '\xBF';

    PositionIteratorType first(hasByteOrderMark ? input.begin() + 3 : input.begin());
    PositionIteratorType iter = first;
    PositionIteratorType last(input.end());
    STORM_LOG_ASSERT(first != last, "Illegal input to PRISM parser.");

    // Create empty result;
    storm::prism::Program result;

    // Create grammar.
    storm::parser::PrismParserGrammar grammar(filename, first, prismCompatibility);
    try {
        // Start first run.
        storm::spirit_encoding::space_type space;
        bool succeeded = qi::phrase_parse(iter, last, grammar, space | qi::lit("//") >> *(qi::char_ - (qi::eol | qi::eoi)) >> (qi::eol | qi::eoi), result);
        STORM_LOG_THROW(succeeded, storm::exceptions::WrongFormatException, "Parsing failed in first pass.");
        STORM_LOG_DEBUG("First pass of parsing PRISM input finished.");

        // Start second run.
        first = PositionIteratorType(input.begin());
        iter = first;
        last = PositionIteratorType(input.end());
        grammar.moveToSecondRun();
        succeeded = qi::phrase_parse(iter, last, grammar, space | qi::lit("//") >> *(qi::char_ - (qi::eol | qi::eoi)) >> (qi::eol | qi::eoi), result);
        STORM_LOG_THROW(succeeded, storm::exceptions::WrongFormatException, "Parsing failed in second pass.");
    } catch (qi::expectation_failure<PositionIteratorType> const& e) {
        // If the parser expected content different than the one provided, display information about the location of the error.
        std::size_t lineNumber = boost::spirit::get_line(e.first);

        // Now propagate exception.
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Parsing error in line " << lineNumber << " of file " << filename << ".");
    }

    STORM_LOG_TRACE("Parsed PRISM input: " << result);

    return result;
}

PrismParserGrammar::PrismParserGrammar(std::string const& filename, Iterator first, bool prismCompatibility)
    : PrismParserGrammar::base_type(start),
      secondRun(false),
      prismCompatibility(prismCompatibility),
      filename(filename),
      annotate(first),
      manager(new storm::expressions::ExpressionManager()),
      expressionParser(new ExpressionParser(*manager, expressionKeywords_, false, false)) {
    ExpressionParser& expression_ = *expressionParser;
    boolExpression = (expression_[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isOfBoolType, phoenix::ref(*this), qi::_val)];
    boolExpression.name("boolean expression");

    intExpression = (expression_[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isOfIntType, phoenix::ref(*this), qi::_val)];
    intExpression.name("integer expression");

    numericalExpression = (expression_[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isOfNumericalType, phoenix::ref(*this), qi::_val)];
    numericalExpression.name("numerical expression");

    // Parse simple identifier.
    identifier %= qi::as_string[qi::raw[qi::lexeme[((qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_')))]]]
                               [qi::_pass = phoenix::bind(&PrismParserGrammar::isValidIdentifier, phoenix::ref(*this), qi::_1)];
    identifier.name("identifier");

    // Fail if the identifier has been used before
    freshIdentifier = (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isFreshIdentifier, phoenix::ref(*this), qi::_1)];
    freshIdentifier.name("fresh identifier");

    modelTypeDefinition %= modelType_;
    modelTypeDefinition.name("model type");

    // Defined constants. Will be checked before undefined constants.
    // ">>" before literal '=' because we can still parse an undefined constant afterwards.
    definedBooleanConstantDefinition =
        (((qi::lit("const") >> qi::lit("bool")) > freshIdentifier) >>
         (qi::lit("=") > boolExpression >
          qi::lit(";")))[qi::_val = phoenix::bind(&PrismParserGrammar::createDefinedBooleanConstant, phoenix::ref(*this), qi::_1, qi::_2)];
    definedBooleanConstantDefinition.name("defined boolean constant declaration");

    definedIntegerConstantDefinition =
        (((qi::lit("const") >> -qi::lit("int")) >> freshIdentifier) >>
         (qi::lit("=") > intExpression > qi::lit(";")))[qi::_val = phoenix::bind(&PrismParserGrammar::createDefinedIntegerConstant, phoenix::ref(*this), qi::_1,
                                                                                 qi::_2)];  // '>>' before freshIdentifier because of the optional 'int'.
                                                                                            // Otherwise, undefined constant 'const bool b;' would not parse.
    definedIntegerConstantDefinition.name("defined integer constant declaration");

    definedDoubleConstantDefinition =
        (((qi::lit("const") >> qi::lit("double")) > freshIdentifier) >>
         (qi::lit("=") > numericalExpression >
          qi::lit(";")))[qi::_val = phoenix::bind(&PrismParserGrammar::createDefinedDoubleConstant, phoenix::ref(*this), qi::_1, qi::_2)];
    definedDoubleConstantDefinition.name("defined double constant declaration");

    definedConstantDefinition %= (definedBooleanConstantDefinition | definedDoubleConstantDefinition | definedIntegerConstantDefinition);
    definedConstantDefinition.name("defined constant definition");

    // Undefined constants. At this point we already checked for a defined constant, therefore a ";" is required after the identifier;
    undefinedBooleanConstantDefinition =
        (((qi::lit("const") >> qi::lit("bool")) > freshIdentifier) >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createUndefinedBooleanConstant, phoenix::ref(*this), qi::_1)];
    undefinedBooleanConstantDefinition.name("undefined boolean constant declaration");

    undefinedIntegerConstantDefinition =
        (((qi::lit("const") >> -qi::lit("int")) > freshIdentifier) >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createUndefinedIntegerConstant, phoenix::ref(*this), qi::_1)];
    undefinedIntegerConstantDefinition.name("undefined integer constant declaration");

    undefinedDoubleConstantDefinition =
        (((qi::lit("const") >> qi::lit("double")) > freshIdentifier) >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createUndefinedDoubleConstant, phoenix::ref(*this), qi::_1)];
    undefinedDoubleConstantDefinition.name("undefined double constant definition");

    undefinedConstantDefinition = (undefinedBooleanConstantDefinition | undefinedDoubleConstantDefinition |
                                   undefinedIntegerConstantDefinition);  // Due to the 'const N;' syntax, it is important to have integer constants last

    undefinedConstantDefinition.name("undefined constant definition");

    // formula definitions. This will be changed for the second run.
    formulaDefinitionRhs = (qi::lit("=") > qi::as_string[(+(qi::char_ - (qi::lit(";") | qi::lit("endmodule"))))][qi::_val = qi::_1] > qi::lit(";"));
    formulaDefinitionRhs.name("formula defining expression");

    formulaDefinition = (qi::lit("formula") > freshIdentifier >
                         formulaDefinitionRhs)[qi::_val = phoenix::bind(&PrismParserGrammar::createFormulaFirstRun, phoenix::ref(*this), qi::_1, qi::_2)];
    formulaDefinition.name("formula definition");

    booleanVariableDefinition =
        (((freshIdentifier > qi::lit(":")) >> qi::lit("bool")) > -((qi::lit("init") > boolExpression[qi::_a = qi::_1]) | qi::attr(manager->boolean(false))) >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createBooleanVariable, phoenix::ref(*this), qi::_1, qi::_a)];
    booleanVariableDefinition.name("boolean variable definition");

    boundedIntegerVariableDefinition =
        (((freshIdentifier > qi::lit(":")) >> qi::lit("[")) > intExpression > qi::lit("..") > intExpression > qi::lit("]") >
         -(qi::lit("init") > intExpression[qi::_a = qi::_1]) >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createIntegerVariable, phoenix::ref(*this), qi::_1, qi::_2, qi::_3, qi::_a)];
    boundedIntegerVariableDefinition.name("bounded integer variable definition");

    unboundedIntegerVariableDefinition = (((freshIdentifier > qi::lit(":")) >> qi::lit("int")) > -(qi::lit("init") > intExpression[qi::_a = qi::_1]) >
                                          qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createIntegerVariable, phoenix::ref(*this), qi::_1,
                                                                                 storm::expressions::Expression(), storm::expressions::Expression(), qi::_a)];
    unboundedIntegerVariableDefinition.name("unbounded integer variable definition");

    integerVariableDefinition = boundedIntegerVariableDefinition | unboundedIntegerVariableDefinition;
    integerVariableDefinition.name("integer variable definition");

    clockVariableDefinition = (((freshIdentifier > qi::lit(":")) >> qi::lit("clock")) >
                               qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createClockVariable, phoenix::ref(*this), qi::_1)];
    clockVariableDefinition.name("clock variable definition");

    variableDefinition = (booleanVariableDefinition[phoenix::push_back(qi::_r1, qi::_1)] | integerVariableDefinition[phoenix::push_back(qi::_r2, qi::_1)] |
                          clockVariableDefinition[phoenix::push_back(qi::_r3, qi::_1)]);
    variableDefinition.name("variable declaration");

    globalVariableDefinition =
        (qi::lit("global") >
         (booleanVariableDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::globalBooleanVariables, qi::_r1), qi::_1)] |
          integerVariableDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::globalIntegerVariables, qi::_r1), qi::_1)]));
    globalVariableDefinition.name("global variable declaration list");

    stateRewardDefinition = (boolExpression > qi::lit(":") > numericalExpression >
                             qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createStateReward, phoenix::ref(*this), qi::_1, qi::_2)];
    stateRewardDefinition.name("state reward definition");

    stateActionRewardDefinition =
        (qi::lit("[") > -identifier > qi::lit("]") > boolExpression > qi::lit(":") > numericalExpression >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createStateActionReward, phoenix::ref(*this), qi::_1, qi::_2, qi::_3, qi::_r1)];
    stateActionRewardDefinition.name("state action reward definition");

    transitionRewardDefinition =
        ((qi::lit("[") > -identifier[qi::_a = qi::_1] > qi::lit("]") > boolExpression[qi::_b = qi::_1]) >>
         (qi::lit("->") > boolExpression[qi::_c = qi::_1] > qi::lit(":") > numericalExpression[qi::_d = qi::_1] >
          qi::lit(";")))[qi::_val = phoenix::bind(&PrismParserGrammar::createTransitionReward, phoenix::ref(*this), qi::_a, qi::_b, qi::_c, qi::_d, qi::_r1)];
    transitionRewardDefinition.name("transition reward definition");

    freshRewardModelName = (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isFreshRewardModelName, phoenix::ref(*this), qi::_1)];
    freshRewardModelName.name("fresh reward model name");

    rewardModelDefinition =
        (qi::lit("rewards") > -(qi::lit("\"") > freshRewardModelName[qi::_a = qi::_1] > qi::lit("\"")) >
         +(transitionRewardDefinition(qi::_r1)[phoenix::push_back(qi::_d, qi::_1)] | stateActionRewardDefinition(qi::_r1)[phoenix::push_back(qi::_c, qi::_1)] |
           stateRewardDefinition[phoenix::push_back(qi::_b, qi::_1)]) >
         qi::lit("endrewards"))[qi::_val = phoenix::bind(&PrismParserGrammar::createRewardModel, phoenix::ref(*this), qi::_a, qi::_b, qi::_c, qi::_d)];
    rewardModelDefinition.name("reward model definition");

    initialStatesConstruct =
        (qi::lit("init") > boolExpression >
         qi::lit("endinit"))[qi::_pass = phoenix::bind(&PrismParserGrammar::addInitialStatesConstruct, phoenix::ref(*this), qi::_1, qi::_r1)];
    initialStatesConstruct.name("initial construct");

    observablesConstruct = (qi::lit("observables") > (identifier % qi::lit(",")) >
                            qi::lit("endobservables"))[phoenix::bind(&PrismParserGrammar::createObservablesList, phoenix::ref(*this), qi::_1)];
    observablesConstruct.name("observables construct");

    invariantConstruct = (qi::lit("invariant") > boolExpression > qi::lit("endinvariant"))[qi::_val = qi::_1];
    invariantConstruct.name("invariant construct");

    knownModuleName = (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isKnownModuleName, phoenix::ref(*this), qi::_1, false)];
    knownModuleName.name("existing module name");

    freshModuleName = (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isFreshModuleName, phoenix::ref(*this), qi::_1)];
    freshModuleName.name("fresh module name");

    systemCompositionConstruct =
        (qi::lit("system") > parallelComposition >
         qi::lit("endsystem"))[phoenix::bind(&PrismParserGrammar::addSystemCompositionConstruct, phoenix::ref(*this), qi::_1, qi::_r1)];
    systemCompositionConstruct.name("system composition construct");

    actionNameList %= identifier[phoenix::insert(qi::_val, qi::_1)] >> *("," >> identifier[phoenix::insert(qi::_val, qi::_1)]);
    actionNameList.name("action list");

    parallelComposition =
        hidingOrRenamingComposition[qi::_val = qi::_1] >>
        *((interleavingParallelComposition > hidingOrRenamingComposition)[qi::_val = phoenix::bind(&PrismParserGrammar::createInterleavingParallelComposition,
                                                                                                   phoenix::ref(*this), qi::_val, qi::_1)] |
          (synchronizingParallelComposition > hidingOrRenamingComposition)[qi::_val = phoenix::bind(&PrismParserGrammar::createSynchronizingParallelComposition,
                                                                                                    phoenix::ref(*this), qi::_val, qi::_1)] |
          (restrictedParallelComposition > hidingOrRenamingComposition)[qi::_val = phoenix::bind(&PrismParserGrammar::createRestrictedParallelComposition,
                                                                                                 phoenix::ref(*this), qi::_val, qi::_1, qi::_2)]);
    parallelComposition.name("parallel composition");

    synchronizingParallelComposition = qi::lit("||");
    synchronizingParallelComposition.name("synchronizing parallel composition");

    interleavingParallelComposition = qi::lit("|||");
    interleavingParallelComposition.name("interleaving parallel composition");

    restrictedParallelComposition = qi::lit("|[") > actionNameList > qi::lit("]|");
    restrictedParallelComposition.name("restricted parallel composition");

    hidingOrRenamingComposition = hidingComposition | renamingComposition | atomicComposition;
    hidingOrRenamingComposition.name("hiding/renaming composition");

    hidingComposition =
        (atomicComposition >>
         (qi::lit("/") > (qi::lit("{") > actionNameList >
                          qi::lit("}"))))[qi::_val = phoenix::bind(&PrismParserGrammar::createHidingComposition, phoenix::ref(*this), qi::_1, qi::_2)];
    hidingComposition.name("hiding composition");

    actionRenamingList =
        +(identifier >> (qi::lit("<-") >> identifier))[phoenix::insert(qi::_val, phoenix::construct<std::pair<std::string, std::string>>(qi::_1, qi::_2))];
    actionRenamingList.name("action renaming list");

    renamingComposition =
        (atomicComposition >>
         (qi::lit("{") >
          (actionRenamingList > qi::lit("}"))))[qi::_val = phoenix::bind(&PrismParserGrammar::createRenamingComposition, phoenix::ref(*this), qi::_1, qi::_2)];
    renamingComposition.name("renaming composition");

    atomicComposition = (qi::lit("(") > parallelComposition > qi::lit(")")) | moduleComposition;
    atomicComposition.name("atomic composition");

    moduleComposition = identifier[qi::_val = phoenix::bind(&PrismParserGrammar::createModuleComposition, phoenix::ref(*this), qi::_1)];
    moduleComposition.name("module composition");

    freshLabelName = (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isFreshLabelName, phoenix::ref(*this), qi::_1)];
    freshLabelName.name("fresh label name");

    labelDefinition = (qi::lit("label") > -qi::lit("\"") > freshLabelName > -qi::lit("\"") > qi::lit("=") > boolExpression >
                       qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createLabel, phoenix::ref(*this), qi::_1, qi::_2)];
    labelDefinition.name("label definition");

    freshObservationLabelName =
        (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isFreshObservationLabelName, phoenix::ref(*this), qi::_1)];
    freshObservationLabelName.name("fresh observable name");

    observableDefinition =
        (qi::lit("observable") > -qi::lit("\"") > freshObservationLabelName > -qi::lit("\"") > qi::lit("=") > (intExpression | boolExpression) >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createObservationLabel, phoenix::ref(*this), qi::_1, qi::_2)];
    observableDefinition.name("observable definition");

    assignmentDefinition = ((qi::lit("(") >> identifier >> qi::lit("'")) > qi::lit("=") > expression_ >
                            qi::lit(")"))[qi::_val = phoenix::bind(&PrismParserGrammar::createAssignment, phoenix::ref(*this), qi::_1, qi::_2)];
    assignmentDefinition.name("assignment");

    assignmentDefinitionList =
        (assignmentDefinition % "&")[qi::_val = qi::_1] | (qi::lit("true"))[qi::_val = phoenix::construct<std::vector<storm::prism::Assignment>>()];
    assignmentDefinitionList.name("assignment list");

    likelihoodDefinition =
        (numericalExpression >
         qi::lit(":"))[qi::_val = phoenix::construct<typename storm::prism::Update::ExpressionPair>(qi::_1, storm::expressions::Expression())] |
        (qi::lit("[") > numericalExpression > qi::lit(",") > numericalExpression > qi::lit("]") >
         qi::lit(":"))[qi::_val = phoenix::construct<typename storm::prism::Update::ExpressionPair>(qi::_1, qi::_2)];

    updateDefinition =
        (assignmentDefinitionList[qi::_val = phoenix::bind(&PrismParserGrammar::createUpdate, phoenix::ref(*this),
                                                           typename storm::prism::Update::ExpressionPair(), qi::_1, qi::_r1)] |
         ((likelihoodDefinition >
           assignmentDefinitionList)[qi::_val = phoenix::bind(&PrismParserGrammar::createUpdate, phoenix::ref(*this), qi::_1, qi::_2, qi::_r1)]));
    updateDefinition.name("update");

    updateListDefinition %= +updateDefinition(qi::_r1) % "+";
    updateListDefinition.name("update list");

    // This is a dummy command-definition (it ignores the actual contents of the command) that is overwritten when the parser is moved to the second run.
    commandDefinition = (((qi::lit("[") > -identifier > qi::lit("]")) | (qi::lit("<") > -identifier > qi::lit(">")[qi::_a = true])) >
                         +(qi::char_ - (qi::lit(";") | qi::lit("endmodule"))) >
                         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createDummyCommand, phoenix::ref(*this), qi::_1, qi::_r1)];
    commandDefinition.name("command definition");

    // We first check for a module renaming, i.e., for this rule we certainly have to see a module definition
    moduleDefinition =
        ((qi::lit("module") > freshModuleName > *(variableDefinition(qi::_a, qi::_b, qi::_c))) > -invariantConstruct > (*commandDefinition(qi::_r1)) >
         qi::lit("endmodule"))[qi::_val = phoenix::bind(&PrismParserGrammar::createModule, phoenix::ref(*this), qi::_1, qi::_a, qi::_b, qi::_c, qi::_2, qi::_3,
                                                        qi::_r1)];
    moduleDefinition.name("module definition");

    freshPlayerName = (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isFreshPlayerName, phoenix::ref(*this), qi::_1)];
    freshPlayerName.name("fresh player name");

    playerControlledActionName =
        ((qi::lit("[") > identifier >
          qi::lit("]"))[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isKnownActionName, phoenix::ref(*this), qi::_1, true)];
    playerControlledActionName.name("player controlled action name");

    playerControlledModuleName =
        (identifier[qi::_val = qi::_1])[qi::_pass = phoenix::bind(&PrismParserGrammar::isKnownModuleName, phoenix::ref(*this), qi::_1, true)];
    playerControlledModuleName.name("player controlled module name");

    playerConstruct =
        (qi::lit("player") > freshPlayerName[qi::_a = qi::_1] >
         +((playerControlledActionName[phoenix::push_back(qi::_c, qi::_1)] | playerControlledModuleName[phoenix::push_back(qi::_b, qi::_1)]) % ',') >
         qi::lit("endplayer"))[qi::_val = phoenix::bind(&PrismParserGrammar::createPlayer, phoenix::ref(*this), qi::_a, qi::_b, qi::_c)];
    playerConstruct.name("player construct");

    moduleRenaming =
        (qi::lit("[") >
         ((identifier > qi::lit("=") > identifier)[phoenix::insert(qi::_a, phoenix::construct<std::pair<std::string, std::string>>(qi::_1, qi::_2))] % ",") >
         qi::lit("]"))[qi::_val = phoenix::bind(&PrismParserGrammar::createModuleRenaming, phoenix::ref(*this), qi::_a)];
    moduleRenaming.name("Module renaming list");

    renamedModule =
        (((qi::lit("module") > freshModuleName) >> qi::lit("=")) > knownModuleName[qi::_a = qi::_1] >
         (moduleRenaming[qi::_b = qi::_1])[qi::_pass =
                                               phoenix::bind(&PrismParserGrammar::isValidModuleRenaming, phoenix::ref(*this), qi::_a, qi::_b, qi::_r1)] >
         qi::lit("endmodule"))[qi::_val = phoenix::bind(&PrismParserGrammar::createRenamedModule, phoenix::ref(*this), qi::_1, qi::_a, qi::_b, qi::_r1)];
    renamedModule.name("module definition via renaming");

    start =
        (qi::eps[phoenix::bind(&PrismParserGrammar::removeInitialConstruct, phoenix::ref(*this), phoenix::ref(globalProgramInformation))] >
         modelTypeDefinition[phoenix::bind(&PrismParserGrammar::setModelType, phoenix::ref(*this), phoenix::ref(globalProgramInformation), qi::_1)] >
         -observablesConstruct >
         *(definedConstantDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::constants, phoenix::ref(globalProgramInformation)), qi::_1)] |
           undefinedConstantDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::constants, phoenix::ref(globalProgramInformation)),
                                                          qi::_1)] |
           formulaDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::formulas, phoenix::ref(globalProgramInformation)), qi::_1)] |
           globalVariableDefinition(phoenix::ref(globalProgramInformation)) |
           (renamedModule(phoenix::ref(globalProgramInformation)) | moduleDefinition(phoenix::ref(globalProgramInformation)))[phoenix::push_back(
               phoenix::bind(&GlobalProgramInformation::modules, phoenix::ref(globalProgramInformation)), qi::_1)] |
           initialStatesConstruct(phoenix::ref(globalProgramInformation)) |
           rewardModelDefinition(phoenix::ref(globalProgramInformation))[phoenix::push_back(
               phoenix::bind(&GlobalProgramInformation::rewardModels, phoenix::ref(globalProgramInformation)), qi::_1)] |
           labelDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::labels, phoenix::ref(globalProgramInformation)), qi::_1)] |
           observableDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::observationLabels, phoenix::ref(globalProgramInformation)),
                                                   qi::_1)] |
           formulaDefinition[phoenix::push_back(phoenix::bind(&GlobalProgramInformation::formulas, phoenix::ref(globalProgramInformation)), qi::_1)] |
           playerConstruct(phoenix::ref(globalProgramInformation))[phoenix::push_back(
               phoenix::bind(&GlobalProgramInformation::players, phoenix::ref(globalProgramInformation)), qi::_1)]) >
         -(systemCompositionConstruct(phoenix::ref(globalProgramInformation))) >
         qi::eoi)[qi::_val = phoenix::bind(&PrismParserGrammar::createProgram, phoenix::ref(*this), phoenix::ref(globalProgramInformation))];
    start.name("probabilistic program");

    // Enable location tracking for important entities.
    auto setLocationInfoFunction = this->annotate(qi::_val, qi::_1, qi::_3);
    qi::on_success(undefinedBooleanConstantDefinition, setLocationInfoFunction);
    qi::on_success(undefinedIntegerConstantDefinition, setLocationInfoFunction);
    qi::on_success(undefinedDoubleConstantDefinition, setLocationInfoFunction);
    qi::on_success(definedBooleanConstantDefinition, setLocationInfoFunction);
    qi::on_success(definedIntegerConstantDefinition, setLocationInfoFunction);
    qi::on_success(definedDoubleConstantDefinition, setLocationInfoFunction);
    qi::on_success(booleanVariableDefinition, setLocationInfoFunction);
    qi::on_success(integerVariableDefinition, setLocationInfoFunction);
    qi::on_success(clockVariableDefinition, setLocationInfoFunction);
    qi::on_success(moduleDefinition, setLocationInfoFunction);
    qi::on_success(moduleRenaming, setLocationInfoFunction);
    qi::on_success(renamedModule, setLocationInfoFunction);
    qi::on_success(formulaDefinition, setLocationInfoFunction);
    qi::on_success(rewardModelDefinition, setLocationInfoFunction);
    qi::on_success(labelDefinition, setLocationInfoFunction);
    qi::on_success(observableDefinition, setLocationInfoFunction);
    qi::on_success(commandDefinition, setLocationInfoFunction);
    qi::on_success(updateDefinition, setLocationInfoFunction);
    qi::on_success(assignmentDefinition, setLocationInfoFunction);

    // Enable error reporting.
    qi::on_error<qi::fail>(start,
                           (phoenix::bind(&PrismParserGrammar::reportRejectedKeywordIdentifier, phoenix::ref(*this)), handler(qi::_1, qi::_2, qi::_3, qi::_4)));
}

void PrismParserGrammar::moveToSecondRun() {
    // In the second run, we actually need to parse the commands instead of just skipping them,
    // so we adapt the rule for parsing commands.
    STORM_LOG_THROW(observables.empty(), storm::exceptions::WrongFormatException,
                    "Some variables marked as observable, but never declared, e.g. " << *observables.begin() << ".");

    commandDefinition =
        (((qi::lit("[") > -identifier > qi::lit("]")) | (qi::lit("<") > -identifier > qi::lit(">")[qi::_a = true])) > *expressionParser > qi::lit("->") >
         updateListDefinition(qi::_r1) >
         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createCommand, phoenix::ref(*this), qi::_a, qi::_1, qi::_2, qi::_3, qi::_r1)];

    auto setLocationInfoFunction = this->annotate(qi::_val, qi::_1, qi::_3);
    qi::on_success(commandDefinition, setLocationInfoFunction);

    formulaDefinition = (qi::lit("formula") > identifier > qi::lit("=") > *expressionParser >
                         qi::lit(";"))[qi::_val = phoenix::bind(&PrismParserGrammar::createFormulaSecondRun, phoenix::ref(*this), qi::_1, qi::_2)];
    formulaDefinition.name("formula definition");
    this->secondRun = true;
    this->expressionParser->setIdentifierMapping(&this->identifiers_);

    // We need to parse the formula rhs between the first run and the second run, because
    // * in the first run, the type of the formula (int, bool, clock) is not known
    // * in the second run, formulas might be used before they are declared
    createFormulaIdentifiers(this->globalProgramInformation.formulas);

    this->globalProgramInformation.moveToSecondRun();
}

void PrismParserGrammar::createFormulaIdentifiers(std::vector<storm::prism::Formula> const& formulas) {
    STORM_LOG_THROW(formulas.size() == this->formulaExpressions.size(), storm::exceptions::UnexpectedException,
                    "Unexpected number of formulas and formula expressions.");
    this->formulaOrder.clear();
    storm::storage::BitVector unprocessed(formulas.size(), true);
    // It might be that formulas are declared in a weird order.
    // We follow a trial-and-error approach: If we can not parse the expression for one formula,
    // we assume a subsequent formula has to be evaluated first.
    // We cycle through the formulas until no further progress is made
    bool progress = true;
    while (progress) {
        progress = false;
        for (uint64_t formulaIndex = unprocessed.getNextSetIndex(0); formulaIndex < formulas.size();
             formulaIndex = unprocessed.getNextSetIndex(formulaIndex + 1)) {
            storm::expressions::Expression expression = this->expressionParser->parseFromString(formulaExpressions[formulaIndex], true);
            if (expression.isInitialized()) {
                progress = true;
                unprocessed.set(formulaIndex, false);
                formulaOrder.push_back(formulaIndex);
                storm::expressions::Variable variable;
                try {
                    if (expression.hasIntegerType()) {
                        variable = manager->declareIntegerVariable(formulas[formulaIndex].getName());
                    } else if (expression.hasBooleanType()) {
                        variable = manager->declareBooleanVariable(formulas[formulaIndex].getName());
                    } else {
                        STORM_LOG_ASSERT(expression.hasNumericalType(),
                                         "Unexpected type for formula expression of formula " << formulas[formulaIndex].getName());
                        variable = manager->declareRationalVariable(formulas[formulaIndex].getName());
                    }
                    this->identifiers_.add(formulas[formulaIndex].getName(), variable.getExpression());
                } catch (storm::exceptions::InvalidArgumentException const&) {
                    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                    "Parsing error in " << this->getFilename() << ": illegal identifier '" << formulas[formulaIndex].getName() << "' at line '"
                                                        << formulas[formulaIndex].getLineNumber() << ".");
                }
                this->expressionParser->setIdentifierMapping(&this->identifiers_);
            }
        }
    }
    if (!unprocessed.empty()) {
        for (auto formulaIndex : unprocessed) {
            STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Invalid expression for formula '" << formulas[formulaIndex].getName()
                                                << "' at line '" << formulas[formulaIndex].getLineNumber() << "':\n\t" << formulaExpressions[formulaIndex]);
        }
        STORM_LOG_THROW(unprocessed.getNumberOfSetBits() == 1, storm::exceptions::WrongFormatException,
                        "Unable to parse expressions for " << unprocessed.getNumberOfSetBits() << " formulas. This could be due to circular dependencies.");
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                        "Unable to parse expression for formula '" << formulas[unprocessed.getNextSetIndex(0)].getName() << "'.");
    }
}

void PrismParserGrammar::allowDoubleLiterals(bool flag) {
    this->expressionParser->setAcceptDoubleLiterals(flag);
}

std::string const& PrismParserGrammar::getFilename() const {
    return this->filename;
}

bool PrismParserGrammar::isValidIdentifier(std::string const& identifier) {
    if (this->keywords_.find(identifier) != nullptr) {
        // "player"/"endplayer" and "invariant"/"endinvariant" are only meaningful (and thus only reserved) for
        // SMGs resp. PTAs; the model type is already known at this point since it is always the first thing
        // parsed in the file.
        auto const modelType = this->globalProgramInformation.modelType;
        bool const isExemptSmgKeyword = (identifier == "player" || identifier == "endplayer") && modelType != storm::prism::Program::ModelType::SMG;
        bool const isExemptPtaKeyword = (identifier == "invariant" || identifier == "endinvariant") && modelType != storm::prism::Program::ModelType::PTA;
        if (isExemptSmgKeyword || isExemptPtaKeyword) {
            return true;
        }
        // Do not log here: this check also fires on harmless speculative backtracking during a successful parse.
        // Recorded for diagnostics in case parsing ultimately fails, see reportRejectedKeywordIdentifier.
        this->lastRejectedKeywordIdentifier = identifier;
        return false;
    }
    return true;
}

void PrismParserGrammar::reportRejectedKeywordIdentifier() {
    if (!this->lastRejectedKeywordIdentifier.empty()) {
        STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": '" << this->lastRejectedKeywordIdentifier
                                            << "' is a reserved keyword and cannot be used as an identifier.");
    }
}

bool PrismParserGrammar::isKnownModuleName(std::string const& moduleName, bool inSecondRun) {
    if ((this->secondRun == inSecondRun) && this->globalProgramInformation.moduleToIndexMap.count(moduleName) == 0) {
        STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Unknown module '" << moduleName << "'.");
        return false;
    }
    return true;
}

bool PrismParserGrammar::isFreshModuleName(std::string const& moduleName) {
    if (!this->secondRun && this->globalProgramInformation.moduleToIndexMap.count(moduleName) != 0) {
        STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Duplicate module name '" << moduleName << "'.");
        return false;
    }
    return true;
}

bool PrismParserGrammar::isKnownActionName(std::string const& actionName, bool inSecondRun) {
    if ((this->secondRun == inSecondRun) && this->globalProgramInformation.actionIndices.count(actionName) == 0) {
        STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Unknown action label '" << actionName << "'.");
        return false;
    }
    return true;
}

bool PrismParserGrammar::isFreshIdentifier(std::string const& identifier) {
    if (!this->secondRun && this->manager->hasVariable(identifier)) {
        STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Duplicate identifier '" << identifier << "'.");
        return false;
    }
    return true;
}

bool PrismParserGrammar::isFreshLabelName(std::string const& labelName) {
    if (!this->secondRun) {
        for (auto const& existingLabel : this->globalProgramInformation.labels) {
            if (labelName == existingLabel.getName()) {
                STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Duplicate label name '" << identifier << "'.");
                return false;
            }
        }
    }
    return true;
}

bool PrismParserGrammar::isFreshObservationLabelName(std::string const& labelName) {
    if (!this->secondRun) {
        for (auto const& existingLabel : this->globalProgramInformation.observationLabels) {
            if (labelName == existingLabel.getName()) {
                STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Duplicate observable name '" << identifier << "'.");
                return false;
            }
        }
    }
    return true;
}

bool PrismParserGrammar::isFreshRewardModelName(std::string const& rewardModelName) {
    if (!this->secondRun) {
        for (auto const& existingRewardModel : this->globalProgramInformation.rewardModels) {
            if (rewardModelName == existingRewardModel.getName()) {
                STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": Duplicate reward model name '" << identifier << "'.");
                return false;
            }
        }
    }
    return true;
}

bool PrismParserGrammar::isFreshPlayerName(std::string const& playerName) {
    return true;
}

bool PrismParserGrammar::isOfBoolType(storm::expressions::Expression const& expression) {
    return !this->secondRun || expression.hasBooleanType();
}

bool PrismParserGrammar::isOfIntType(storm::expressions::Expression const& expression) {
    return !this->secondRun || expression.hasIntegerType();
}

bool PrismParserGrammar::isOfNumericalType(storm::expressions::Expression const& expression) {
    return !this->secondRun || expression.hasNumericalType();
}

bool PrismParserGrammar::addInitialStatesConstruct(storm::expressions::Expression const& initialStatesExpression,
                                                   GlobalProgramInformation& globalProgramInformation) {
    STORM_LOG_THROW(!globalProgramInformation.hasInitialConstruct, storm::exceptions::WrongFormatException,
                    "Parsing error in " << this->getFilename() << ": Program must not define two initial constructs.");
    if (globalProgramInformation.hasInitialConstruct) {
        return false;
    }
    globalProgramInformation.hasInitialConstruct = true;
    globalProgramInformation.initialConstruct = storm::prism::InitialConstruct(initialStatesExpression, this->getFilename(), get_line(qi::_3));
    return true;
}

bool PrismParserGrammar::addSystemCompositionConstruct(std::shared_ptr<storm::prism::Composition> const& composition,
                                                       GlobalProgramInformation& globalProgramInformation) {
    globalProgramInformation.systemCompositionConstruct = storm::prism::SystemCompositionConstruct(composition, this->getFilename(), get_line(qi::_3));
    return true;
}

void PrismParserGrammar::setModelType(GlobalProgramInformation& globalProgramInformation, storm::prism::Program::ModelType const& modelType) {
    STORM_LOG_THROW(globalProgramInformation.modelType == storm::prism::Program::ModelType::UNDEFINED, storm::exceptions::WrongFormatException,
                    "Parsing error in " << this->getFilename() << ": Program must not set model type multiple times.");
    globalProgramInformation.modelType = modelType;
}

std::shared_ptr<storm::prism::Composition> PrismParserGrammar::createModuleComposition(std::string const& moduleName) const {
    return std::make_shared<storm::prism::ModuleComposition>(moduleName);
}

std::shared_ptr<storm::prism::Composition> PrismParserGrammar::createRenamingComposition(std::shared_ptr<storm::prism::Composition> const& subcomposition,
                                                                                         std::map<std::string, std::string> const& renaming) const {
    return std::make_shared<storm::prism::RenamingComposition>(subcomposition, renaming);
}

std::shared_ptr<storm::prism::Composition> PrismParserGrammar::createHidingComposition(std::shared_ptr<storm::prism::Composition> const& subcomposition,
                                                                                       std::set<std::string> const& actionsToHide) const {
    return std::make_shared<storm::prism::HidingComposition>(subcomposition, actionsToHide);
}

std::shared_ptr<storm::prism::Composition> PrismParserGrammar::createSynchronizingParallelComposition(
    std::shared_ptr<storm::prism::Composition> const& left, std::shared_ptr<storm::prism::Composition> const& right) const {
    return std::make_shared<storm::prism::SynchronizingParallelComposition>(left, right);
}

std::shared_ptr<storm::prism::Composition> PrismParserGrammar::createInterleavingParallelComposition(
    std::shared_ptr<storm::prism::Composition> const& left, std::shared_ptr<storm::prism::Composition> const& right) const {
    return std::make_shared<storm::prism::InterleavingParallelComposition>(left, right);
}

std::shared_ptr<storm::prism::Composition> PrismParserGrammar::createRestrictedParallelComposition(
    std::shared_ptr<storm::prism::Composition> const& left, std::set<std::string> const& synchronizingActions,
    std::shared_ptr<storm::prism::Composition> const& right) const {
    return std::make_shared<storm::prism::RestrictedParallelComposition>(left, synchronizingActions, right);
}

storm::prism::Constant PrismParserGrammar::createUndefinedBooleanConstant(std::string const& newConstant) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareBooleanVariable(newConstant, true);
            this->identifiers_.add(newConstant, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << newConstant << "'.");
        }
    }
    return storm::prism::Constant(manager->getVariable(newConstant), this->getFilename());
}

storm::prism::Constant PrismParserGrammar::createUndefinedIntegerConstant(std::string const& newConstant) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareIntegerVariable(newConstant, true);
            this->identifiers_.add(newConstant, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << newConstant << "'.");
        }
    }
    return storm::prism::Constant(manager->getVariable(newConstant), this->getFilename());
}

storm::prism::Constant PrismParserGrammar::createUndefinedDoubleConstant(std::string const& newConstant) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareRationalVariable(newConstant, true);
            this->identifiers_.add(newConstant, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << newConstant << "'.");
        }
    }
    return storm::prism::Constant(manager->getVariable(newConstant), this->getFilename());
}

storm::prism::Constant PrismParserGrammar::createDefinedBooleanConstant(std::string const& newConstant, storm::expressions::Expression expression) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareBooleanVariable(newConstant, true);
            this->identifiers_.add(newConstant, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << newConstant << "'.");
        }
    }
    return storm::prism::Constant(manager->getVariable(newConstant), expression, this->getFilename());
}

storm::prism::Constant PrismParserGrammar::createDefinedIntegerConstant(std::string const& newConstant, storm::expressions::Expression expression) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareIntegerVariable(newConstant, true);
            this->identifiers_.add(newConstant, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << newConstant << "'.");
        }
    }
    return storm::prism::Constant(manager->getVariable(newConstant), expression, this->getFilename());
}

storm::prism::Constant PrismParserGrammar::createDefinedDoubleConstant(std::string const& newConstant, storm::expressions::Expression expression) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareRationalVariable(newConstant, true);
            this->identifiers_.add(newConstant, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << newConstant << "'.");
        }
    }
    return storm::prism::Constant(manager->getVariable(newConstant), expression, this->getFilename());
}

storm::prism::Formula PrismParserGrammar::createFormulaFirstRun(std::string const& formulaName, std::string const& expression) {
    // Only store the expression as a string. It will be parsed between first and second run
    // This is necessary because the resulting type of the formula is only known after the first run.
    STORM_LOG_ASSERT(!this->secondRun, "This constructor should have only been called during the first run.");
    formulaExpressions.push_back(expression);
    return storm::prism::Formula(formulaName, this->getFilename());
}

storm::prism::Formula PrismParserGrammar::createFormulaSecondRun(std::string const& formulaName, storm::expressions::Expression const& expression) {
    // This is necessary because the resulting type of the formula is only known after the first run.
    STORM_LOG_ASSERT(this->secondRun, "This constructor should have only been called during the second run.");
    storm::expressions::Expression lhsExpression = *this->identifiers_.find(formulaName);
    return storm::prism::Formula(lhsExpression.getBaseExpression().asVariableExpression().getVariable(), expression, this->getFilename());
}

storm::prism::Label PrismParserGrammar::createLabel(std::string const& labelName, storm::expressions::Expression expression) const {
    return storm::prism::Label(labelName, expression, this->getFilename());
}

storm::prism::ObservationLabel PrismParserGrammar::createObservationLabel(std::string const& labelName, storm::expressions::Expression expression) const {
    return storm::prism::ObservationLabel(labelName, expression, this->getFilename());
}

storm::prism::RewardModel PrismParserGrammar::createRewardModel(std::string const& rewardModelName, std::vector<storm::prism::StateReward> const& stateRewards,
                                                                std::vector<storm::prism::StateActionReward> const& stateActionRewards,
                                                                std::vector<storm::prism::TransitionReward> const& transitionRewards) const {
    return storm::prism::RewardModel(rewardModelName, stateRewards, stateActionRewards, transitionRewards, this->getFilename());
}

storm::prism::StateReward PrismParserGrammar::createStateReward(storm::expressions::Expression statePredicateExpression,
                                                                storm::expressions::Expression rewardValueExpression) const {
    if (this->secondRun) {
        return storm::prism::StateReward(statePredicateExpression, rewardValueExpression, this->getFilename());
    } else {
        return storm::prism::StateReward();
    }
}

storm::prism::StateActionReward PrismParserGrammar::createStateActionReward(boost::optional<std::string> const& actionName,
                                                                            storm::expressions::Expression statePredicateExpression,
                                                                            storm::expressions::Expression rewardValueExpression,
                                                                            GlobalProgramInformation& globalProgramInformation) const {
    if (this->secondRun) {
        std::string realActionName = actionName ? actionName.get() : "";

        auto const& nameIndexPair = globalProgramInformation.actionIndices.find(realActionName);
        STORM_LOG_THROW(nameIndexPair != globalProgramInformation.actionIndices.end(), storm::exceptions::WrongFormatException,
                        "Action reward refers to illegal action '" << realActionName << "'.");
        return storm::prism::StateActionReward(nameIndexPair->second, realActionName, statePredicateExpression, rewardValueExpression, this->getFilename());
    } else {
        return storm::prism::StateActionReward();
    }
}

storm::prism::TransitionReward PrismParserGrammar::createTransitionReward(boost::optional<std::string> const& actionName,
                                                                          storm::expressions::Expression sourceStatePredicateExpression,
                                                                          storm::expressions::Expression targetStatePredicateExpression,
                                                                          storm::expressions::Expression rewardValueExpression,
                                                                          GlobalProgramInformation& globalProgramInformation) const {
    if (this->secondRun) {
        std::string realActionName = actionName ? actionName.get() : "";

        auto const& nameIndexPair = globalProgramInformation.actionIndices.find(realActionName);
        STORM_LOG_THROW(nameIndexPair != globalProgramInformation.actionIndices.end(), storm::exceptions::WrongFormatException,
                        "Transition reward refers to illegal action '" << realActionName << "'.");
        return storm::prism::TransitionReward(nameIndexPair->second, realActionName, sourceStatePredicateExpression, targetStatePredicateExpression,
                                              rewardValueExpression, this->getFilename());
    } else {
        return storm::prism::TransitionReward();
    }
}

storm::prism::Assignment PrismParserGrammar::createAssignment(std::string const& variableName, storm::expressions::Expression assignedExpression) const {
    return storm::prism::Assignment(manager->getVariable(variableName), assignedExpression, this->getFilename());
}

storm::prism::Update PrismParserGrammar::createUpdate(typename storm::prism::Update::ExpressionPair likelihoodExpressions,
                                                      std::vector<storm::prism::Assignment> const& assignments,
                                                      GlobalProgramInformation& globalProgramInformation) const {
    ++globalProgramInformation.currentUpdateIndex;
    if (!likelihoodExpressions.first.isInitialized()) {
        likelihoodExpressions.first = manager->rational(1);
    }
    return storm::prism::Update(globalProgramInformation.currentUpdateIndex - 1, likelihoodExpressions, assignments, this->getFilename());
}

storm::prism::Command PrismParserGrammar::createCommand(bool markovian, boost::optional<std::string> const& actionName,
                                                        storm::expressions::Expression guardExpression, std::vector<storm::prism::Update> const& updates,
                                                        GlobalProgramInformation& globalProgramInformation) const {
    ++globalProgramInformation.currentCommandIndex;
    std::string realActionName = actionName ? actionName.get() : "";

    uint_fast64_t actionIndex = 0;

    // If the action name was not yet seen, record it.
    auto nameIndexPair = globalProgramInformation.actionIndices.find(realActionName);
    if (nameIndexPair == globalProgramInformation.actionIndices.end()) {
        std::size_t nextIndex = globalProgramInformation.actionIndices.size();
        globalProgramInformation.actionIndices.emplace(realActionName, nextIndex);
        actionIndex = nextIndex;
    } else {
        actionIndex = nameIndexPair->second;
    }
    return storm::prism::Command(globalProgramInformation.currentCommandIndex - 1, markovian, actionIndex, realActionName, guardExpression, updates,
                                 this->getFilename());
}

storm::prism::Command PrismParserGrammar::createDummyCommand(boost::optional<std::string> const& actionName,
                                                             GlobalProgramInformation& globalProgramInformation) const {
    STORM_LOG_ASSERT(!this->secondRun, "Dummy procedure must not be called in second run.");
    std::string realActionName = actionName ? actionName.get() : "";

    // Register the action name if it has not appeared earlier.
    auto nameIndexPair = globalProgramInformation.actionIndices.find(realActionName);
    if (nameIndexPair == globalProgramInformation.actionIndices.end()) {
        std::size_t nextIndex = globalProgramInformation.actionIndices.size();
        globalProgramInformation.actionIndices.emplace(realActionName, nextIndex);
    }

    return storm::prism::Command();
}

storm::prism::BooleanVariable PrismParserGrammar::createBooleanVariable(std::string const& variableName,
                                                                        storm::expressions::Expression initialValueExpression) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareBooleanVariable(variableName);
            this->identifiers_.add(variableName, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << variableName << "'.");
        }
    }
    bool observable = this->observables.count(variableName) > 0;
    if (observable) {
        this->observables.erase(variableName);
    }
    return storm::prism::BooleanVariable(manager->getVariable(variableName), initialValueExpression, observable, this->getFilename());
}

storm::prism::IntegerVariable PrismParserGrammar::createIntegerVariable(std::string const& variableName, storm::expressions::Expression lowerBoundExpression,
                                                                        storm::expressions::Expression upperBoundExpression,
                                                                        storm::expressions::Expression initialValueExpression) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareIntegerVariable(variableName);
            this->identifiers_.add(variableName, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << variableName << "'.");
        }
    }
    bool observable = this->observables.count(variableName) > 0;
    if (observable) {
        this->observables.erase(variableName);
    }

    return storm::prism::IntegerVariable(manager->getVariable(variableName), lowerBoundExpression, upperBoundExpression, initialValueExpression, observable,
                                         this->getFilename());
}

storm::prism::ClockVariable PrismParserGrammar::createClockVariable(std::string const& variableName) const {
    if (!this->secondRun) {
        try {
            storm::expressions::Variable newVariable = manager->declareRationalVariable(variableName);
            this->identifiers_.add(variableName, newVariable.getExpression());
        } catch (storm::exceptions::InvalidArgumentException const&) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": illegal identifier '" << variableName << "'.");
        }
    }
    bool observable = this->observables.count(variableName) > 0;
    if (observable) {
        this->observables.erase(variableName);
    }

    return storm::prism::ClockVariable(manager->getVariable(variableName), observable, this->getFilename());
}

void PrismParserGrammar::createObservablesList(std::vector<std::string> const& observables) {
    this->observables.insert(observables.begin(), observables.end());
    // We need this list to be filled in both runs.
}

storm::prism::Player PrismParserGrammar::createPlayer(std::string const& playerName, std::vector<std::string> const& moduleNames,
                                                      std::vector<std::string> const& actionNames) {
    if (this->secondRun) {
        std::unordered_set<std::string> controlledModules;
        std::unordered_set<std::string> controlledActions;
        for (auto const& moduleName : moduleNames) {
            auto moduleIndexPair = globalProgramInformation.moduleToIndexMap.find(moduleName);
            STORM_LOG_ASSERT(moduleIndexPair != globalProgramInformation.moduleToIndexMap.end(),
                             "Parsing error in " << this->getFilename() << " for player " << playerName << ": No module named '" << moduleName << "' present.");
            controlledModules.insert(moduleIndexPair->first);
            bool moduleNotYetControlled = globalProgramInformation.playerControlledModules.insert(moduleIndexPair->second).second;
            STORM_LOG_THROW(moduleNotYetControlled, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << " for player " << playerName << ": Module '" << moduleName
                                                << "' already controlled by another player.");
        }
        for (std::string actionName : actionNames) {
            auto actionIndexPair = globalProgramInformation.actionIndices.find(actionName);
            STORM_LOG_ASSERT(actionIndexPair != globalProgramInformation.actionIndices.end(),
                             "Parsing error in " << this->getFilename() << " for player " << playerName << ": No action named '" << actionName << "' present.");
            controlledActions.insert(actionIndexPair->first);
            bool actionNotYetControlled = globalProgramInformation.playerControlledActions.insert(actionIndexPair->second).second;
            STORM_LOG_THROW(actionNotYetControlled, storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << " for player " << playerName << ": Command '" << actionName
                                                << "' already controlled by another player.");
        }
        return storm::prism::Player(playerName, controlledModules, controlledActions);
    } else {
        return storm::prism::Player();
    }
}

storm::prism::Module PrismParserGrammar::createModule(std::string const& moduleName, std::vector<storm::prism::BooleanVariable> const& booleanVariables,
                                                      std::vector<storm::prism::IntegerVariable> const& integerVariables,
                                                      std::vector<storm::prism::ClockVariable> const& clockVariables,
                                                      boost::optional<storm::expressions::Expression> const& invariant,
                                                      std::vector<storm::prism::Command> const& commands,
                                                      GlobalProgramInformation& globalProgramInformation) const {
    if (!this->secondRun) {
        globalProgramInformation.moduleToIndexMap[moduleName] = globalProgramInformation.modules.size();
    }
    // Assert that the module name is already known and has the expected  index.
    STORM_LOG_ASSERT(!this->secondRun || globalProgramInformation.moduleToIndexMap.count(moduleName) > 0, "Module name '" << moduleName << "' was not found.");
    STORM_LOG_ASSERT(!this->secondRun || globalProgramInformation.moduleToIndexMap[moduleName] == globalProgramInformation.modules.size(),
                     "The index for module '" << moduleName << "' does not match the index from the first parsing run.");
    return storm::prism::Module(moduleName, booleanVariables, integerVariables, clockVariables,
                                invariant.is_initialized() ? invariant.get() : storm::expressions::Expression(), commands, this->getFilename());
}

bool PrismParserGrammar::isValidModuleRenaming(std::string const& oldModuleName, storm::prism::ModuleRenaming const& moduleRenaming,
                                               GlobalProgramInformation const& globalProgramInformation) const {
    if (!this->secondRun) {
        auto const& renaming = moduleRenaming.getRenaming();
        auto const& moduleIndexPair = globalProgramInformation.moduleToIndexMap.find(oldModuleName);
        if (moduleIndexPair == globalProgramInformation.moduleToIndexMap.end()) {
            STORM_LOG_ERROR("Parsing error in " << this->getFilename() << ": No module named '" << oldModuleName << "' to rename.");
            return false;
        }
        storm::prism::Module const& moduleToRename = globalProgramInformation.modules[moduleIndexPair->second];
        // Check whether all varialbes are renamed.
        for (auto const& variable : moduleToRename.getBooleanVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            if (renamingPair == renaming.end()) {
                STORM_LOG_ERROR("Parsing error in renaming of module '" << oldModuleName << "': Boolean variable '" << variable.getName()
                                                                        << "' was not renamed.");
                return false;
            }
        }
        for (auto const& variable : moduleToRename.getIntegerVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            if (renamingPair == renaming.end()) {
                STORM_LOG_ERROR("Parsing error in renaming of module '" << oldModuleName << "': Integer variable '" << variable.getName()
                                                                        << "' was not renamed.");
                return false;
            }
        }
        for (auto const& variable : moduleToRename.getClockVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            if (renamingPair == renaming.end()) {
                STORM_LOG_ERROR("Parsing error in renaming of module '" << oldModuleName << "': Clock variable '" << variable.getName()
                                                                        << "' was not renamed.");
                return false;
            }
        }
    }
    return true;
}

storm::prism::ModuleRenaming PrismParserGrammar::createModuleRenaming(std::map<std::string, std::string> const& renaming) const {
    return storm::prism::ModuleRenaming(renaming);
}

storm::prism::Module PrismParserGrammar::createRenamedModule(std::string const& newModuleName, std::string const& oldModuleName,
                                                             storm::prism::ModuleRenaming const& moduleRenaming,
                                                             GlobalProgramInformation& globalProgramInformation) const {
    // Check whether the module to rename actually exists.
    auto const& moduleIndexPair = globalProgramInformation.moduleToIndexMap.find(oldModuleName);
    STORM_LOG_THROW(moduleIndexPair != globalProgramInformation.moduleToIndexMap.end(), storm::exceptions::WrongFormatException,
                    "Parsing error in " << this->getFilename() << ": No module named '" << oldModuleName << "' to rename.");
    storm::prism::Module const& moduleToRename = globalProgramInformation.modules[moduleIndexPair->second];
    STORM_LOG_THROW(!moduleToRename.isRenamedFromModule(), storm::exceptions::WrongFormatException,
                    "Parsing error in " << this->getFilename() << ": The module '" << newModuleName << "' can not be created from module '" << oldModuleName
                                        << "' through module renaming because '" << oldModuleName << "' is also a renamed module. Create '" << newModuleName
                                        << "' via a renaming from base module '" << moduleToRename.getBaseModule() << "' instead.");
    auto const& renaming = moduleRenaming.getRenaming();
    if (!this->secondRun) {
        // Add a mapping from the new module name to its (future) index.
        globalProgramInformation.moduleToIndexMap[newModuleName] = globalProgramInformation.modules.size();

        // Register all (renamed) variables for later use.
        // We already checked before, whether the renaiming is valid.
        for (auto const& variable : moduleToRename.getBooleanVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            STORM_LOG_THROW(renamingPair != renaming.end(), storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": Boolean variable '" << variable.getName() << " was not renamed.");
            storm::expressions::Variable renamedVariable = manager->declareBooleanVariable(renamingPair->second);
            this->identifiers_.add(renamingPair->second, renamedVariable.getExpression());
            if (this->observables.count(renamingPair->second) > 0) {
                this->observables.erase(renamingPair->second);
            }
        }
        for (auto const& variable : moduleToRename.getIntegerVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            STORM_LOG_THROW(renamingPair != renaming.end(), storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": Integer variable '" << variable.getName() << " was not renamed.");
            storm::expressions::Variable renamedVariable = manager->declareIntegerVariable(renamingPair->second);
            this->identifiers_.add(renamingPair->second, renamedVariable.getExpression());
            if (this->observables.count(renamingPair->second) > 0) {
                this->observables.erase(renamingPair->second);
            }
        }
        for (auto const& variable : moduleToRename.getClockVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            STORM_LOG_THROW(renamingPair != renaming.end(), storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": Clock variable '" << variable.getName() << " was not renamed.");
            storm::expressions::Variable renamedVariable = manager->declareRationalVariable(renamingPair->second);
            this->identifiers_.add(renamingPair->second, renamedVariable.getExpression());
            if (this->observables.count(renamingPair->second) > 0) {
                this->observables.erase(renamingPair->second);
            }
        }

        for (auto const& command : moduleToRename.getCommands()) {
            std::string newActionName = command.getActionName();
            auto const& renamingPair = renaming.find(command.getActionName());
            if (renamingPair != renaming.end()) {
                newActionName = renamingPair->second;
            }

            // Record any newly occurring action names/indices.
            auto nameIndexPair = globalProgramInformation.actionIndices.find(newActionName);
            if (nameIndexPair == globalProgramInformation.actionIndices.end()) {
                std::size_t nextIndex = globalProgramInformation.actionIndices.size();
                globalProgramInformation.actionIndices.emplace(newActionName, nextIndex);
            }
        }

        // Return a dummy module in the first pass.
        return storm::prism::Module();
    } else {
        // Assert that the module name is already known and has the expected  index.
        STORM_LOG_ASSERT(globalProgramInformation.moduleToIndexMap.count(newModuleName) > 0, "Module name '" << newModuleName << "' was not found.");
        STORM_LOG_ASSERT(globalProgramInformation.moduleToIndexMap[newModuleName] == globalProgramInformation.modules.size(),
                         "The index for module " << newModuleName << " does not match the index from the first parsing run.");

        // Create a mapping from identifiers to the expressions they need to be replaced with.
        std::map<storm::expressions::Variable, storm::expressions::Expression> expressionRenaming;
        for (auto const& namePair : renaming) {
            storm::expressions::Expression const* substitutedExpression = this->identifiers_.find(namePair.second);
            // If the mapped-to-value is an expression, we need to replace it.
            if (substitutedExpression != nullptr) {
                expressionRenaming.emplace(manager->getVariable(namePair.first), *substitutedExpression);
            }
        }

        // Rename the boolean variables.
        std::vector<storm::prism::BooleanVariable> booleanVariables;
        for (auto const& variable : moduleToRename.getBooleanVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            STORM_LOG_THROW(renamingPair != renaming.end(), storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": Boolean variable '" << variable.getName() << " was not renamed.");
            bool observable = this->observables.count(renamingPair->second) > 0;
            if (observable) {
                this->observables.erase(renamingPair->second);
            }
            booleanVariables.push_back(storm::prism::BooleanVariable(
                manager->getVariable(renamingPair->second),
                variable.hasInitialValue() ? variable.getInitialValueExpression().substitute(expressionRenaming) : variable.getInitialValueExpression(),
                observable, this->getFilename(), moduleRenaming.getLineNumber()));
        }

        // Rename the integer variables.
        std::vector<storm::prism::IntegerVariable> integerVariables;
        for (auto const& variable : moduleToRename.getIntegerVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            STORM_LOG_THROW(renamingPair != renaming.end(), storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": Integer variable '" << variable.getName() << " was not renamed.");
            bool observable = this->observables.count(renamingPair->second) > 0;
            if (observable) {
                this->observables.erase(renamingPair->second);
            }
            integerVariables.push_back(storm::prism::IntegerVariable(
                manager->getVariable(renamingPair->second), variable.getLowerBoundExpression().substitute(expressionRenaming),
                variable.getUpperBoundExpression().substitute(expressionRenaming),
                variable.hasInitialValue() ? variable.getInitialValueExpression().substitute(expressionRenaming) : variable.getInitialValueExpression(),
                observable, this->getFilename(), moduleRenaming.getLineNumber()));
        }

        // Rename the clock variables.
        std::vector<storm::prism::ClockVariable> clockVariables;
        for (auto const& variable : moduleToRename.getClockVariables()) {
            auto const& renamingPair = renaming.find(variable.getName());
            STORM_LOG_THROW(renamingPair != renaming.end(), storm::exceptions::WrongFormatException,
                            "Parsing error in " << this->getFilename() << ": Clock variable '" << variable.getName() << " was not renamed.");
            bool observable = this->observables.count(renamingPair->second) > 0;
            if (observable) {
                this->observables.erase(renamingPair->second);
            }
            clockVariables.push_back(
                storm::prism::ClockVariable(manager->getVariable(renamingPair->second), observable, this->getFilename(), moduleRenaming.getLineNumber()));
        }

        // Rename invariant (if present)
        storm::expressions::Expression invariant;
        if (moduleToRename.hasInvariant()) {
            invariant = moduleToRename.getInvariant().substitute(expressionRenaming);
        }

        // Rename commands.
        std::vector<storm::prism::Command> commands;
        for (auto const& command : moduleToRename.getCommands()) {
            std::vector<storm::prism::Update> updates;
            for (auto const& update : command.getUpdates()) {
                std::vector<storm::prism::Assignment> assignments;
                for (auto const& assignment : update.getAssignments()) {
                    auto const& renamingPair = renaming.find(assignment.getVariableName());
                    if (renamingPair != renaming.end()) {
                        assignments.emplace_back(manager->getVariable(renamingPair->second), assignment.getExpression().substitute(expressionRenaming),
                                                 this->getFilename(), moduleRenaming.getLineNumber());
                    } else {
                        assignments.emplace_back(assignment.getVariable(), assignment.getExpression().substitute(expressionRenaming), this->getFilename(),
                                                 moduleRenaming.getLineNumber());
                    }
                }
                if (update.isLikelihoodInterval()) {
                    typename storm::prism::Update::ExpressionPair likelihoodInterval{
                        update.getLikelihoodExpressionInterval().first.substitute(expressionRenaming),
                        update.getLikelihoodExpressionInterval().second.substitute(expressionRenaming)};
                    updates.emplace_back(globalProgramInformation.currentUpdateIndex, likelihoodInterval, assignments, this->getFilename(),
                                         moduleRenaming.getLineNumber());
                } else {
                    updates.emplace_back(globalProgramInformation.currentUpdateIndex, update.getLikelihoodExpression().substitute(expressionRenaming),
                                         assignments, this->getFilename(), moduleRenaming.getLineNumber());
                }
                ++globalProgramInformation.currentUpdateIndex;
            }

            std::string newActionName = command.getActionName();
            auto const& renamingPair = renaming.find(command.getActionName());
            if (renamingPair != renaming.end()) {
                newActionName = renamingPair->second;
            }

            uint_fast64_t actionIndex = 0;
            auto nameIndexPair = globalProgramInformation.actionIndices.find(newActionName);
            if (nameIndexPair == globalProgramInformation.actionIndices.end()) {
                std::size_t nextIndex = globalProgramInformation.actionIndices.size();
                globalProgramInformation.actionIndices.emplace(newActionName, nextIndex);
                actionIndex = nextIndex;
            } else {
                actionIndex = nameIndexPair->second;
            }

            commands.emplace_back(globalProgramInformation.currentCommandIndex, command.isMarkovian(), actionIndex, newActionName,
                                  command.getGuardExpression().substitute(expressionRenaming), updates, this->getFilename(), moduleRenaming.getLineNumber());
            ++globalProgramInformation.currentCommandIndex;
        }

        return storm::prism::Module(newModuleName, booleanVariables, integerVariables, clockVariables, invariant, commands, oldModuleName, renaming);
    }
}

storm::prism::Program PrismParserGrammar::createProgram(GlobalProgramInformation const& globalProgramInformation) const {
    storm::prism::Program::ModelType finalModelType = globalProgramInformation.modelType;
    if (globalProgramInformation.modelType == storm::prism::Program::ModelType::UNDEFINED) {
        STORM_LOG_WARN("Program does not specify model type. Implicitly assuming 'mdp'.");
        finalModelType = storm::prism::Program::ModelType::MDP;
    }

    // make sure formulas are stored in a proper order.
    std::vector<storm::prism::Formula> orderedFormulas;
    if (this->secondRun) {
        orderedFormulas.reserve(globalProgramInformation.formulas.size());
        for (uint64_t const& i : formulaOrder) {
            orderedFormulas.push_back(std::move(globalProgramInformation.formulas[i]));
        }
    }

    return storm::prism::Program(
        manager, finalModelType, globalProgramInformation.constants, globalProgramInformation.globalBooleanVariables,
        globalProgramInformation.globalIntegerVariables, orderedFormulas, globalProgramInformation.players, globalProgramInformation.modules,
        globalProgramInformation.actionIndices, globalProgramInformation.rewardModels, globalProgramInformation.labels,
        globalProgramInformation.observationLabels,
        secondRun && !globalProgramInformation.hasInitialConstruct ? boost::none : boost::make_optional(globalProgramInformation.initialConstruct),
        globalProgramInformation.systemCompositionConstruct, prismCompatibility, this->getFilename(), 1, this->secondRun);
}

void PrismParserGrammar::removeInitialConstruct(GlobalProgramInformation& globalProgramInformation) const {
    globalProgramInformation.hasInitialConstruct = false;
}
}  // namespace parser
}  // namespace storm
