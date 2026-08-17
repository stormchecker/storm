#include "JaniParser.h"

#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/EdgeDestination.h"
#include "storm/storage/jani/Location.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ModelType.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/TemplateEdge.h"
#include "storm/storage/jani/expressions/JaniExpressions.h"
#include "storm/storage/jani/visitor/CompositionInformationVisitor.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"

#include "storm/storage/jani/types/ArrayType.h"
#include "storm/storage/jani/types/BasicType.h"
#include "storm/storage/jani/types/ClockType.h"
#include "storm/storage/jani/types/ContinuousType.h"
#include "storm/storage/jani/types/JaniType.h"

#include "storm/logic/RewardAccumulationEliminationVisitor.h"

#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/InvalidJaniException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"

#include "storm/modelchecker/results/FilterType.h"

#include <algorithm>  // std::iter_swap
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

#include "storm/io/file.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

////////////
// Defaults
////////////
template<typename ValueType>
const bool JaniParser<ValueType>::defaultVariableTransient = false;
const std::string VARIABLE_AUTOMATON_DELIMITER = "_";
template<typename ValueType>
const std::set<std::string> JaniParser<ValueType>::unsupportedOpstrings({"tan",  "cot",  "sec",   "csc",   "asin",  "acos",  "atan",
                                                                         "acot", "asec", "acsc",  "sinh",  "cosh",  "tanh",  "coth",
                                                                         "sech", "csch", "asinh", "acosh", "atanh", "asinh", "acosh"});

template<typename ValueType>
std::string getString(typename JaniParser<ValueType>::Json const& structure, std::string const& errorInfo) {
    STORM_LOG_THROW(structure.is_string(), storm::exceptions::InvalidJaniException,
                    "Expected a string in " << errorInfo << ", got '" << structure.dump() << "'.");
    return structure.front();
}

template<typename ValueType>
bool getBoolean(typename JaniParser<ValueType>::Json const& structure, std::string const& errorInfo) {
    STORM_LOG_THROW(structure.is_boolean(), storm::exceptions::InvalidJaniException,
                    "Expected a Boolean in " << errorInfo << ", got " << structure.dump() << "'.");
    return structure.front();
}

template<typename ValueType>
uint64_t getUnsignedInt(typename JaniParser<ValueType>::Json const& structure, std::string const& errorInfo) {
    STORM_LOG_THROW(structure.is_number(), storm::exceptions::InvalidJaniException,
                    "Expected a number in " << errorInfo << ", got '" << structure.dump() << "'.");
    int64_t num = structure.front();
    STORM_LOG_THROW(num >= 0, storm::exceptions::InvalidJaniException, "Expected a positive number in " << errorInfo << ", got '" << num << "'.");
    return static_cast<uint64_t>(num);
}

template<typename ValueType>
int64_t getSignedInt(typename JaniParser<ValueType>::Json const& structure, std::string const& errorInfo) {
    STORM_LOG_THROW(structure.is_number(), storm::exceptions::InvalidJaniException,
                    "Expected a number in " << errorInfo << ", got '" << structure.dump() << "'.");
    return structure.front();
}

template<typename ValueType>
std::pair<storm::jani::Model, std::vector<storm::jani::Property>> JaniParser<ValueType>::parse(std::string const& path, bool parseProperties) {
    JaniParser parser;
    parser.readFile(path);
    return parser.parseModel(parseProperties);
}

template<typename ValueType>
std::pair<storm::jani::Model, std::vector<storm::jani::Property>> JaniParser<ValueType>::parseFromString(std::string const& jsonstring, bool parseProperties) {
    JaniParser parser(jsonstring);
    return parser.parseModel(parseProperties);
}

template<typename ValueType>
JaniParser<ValueType>::JaniParser(std::string const& jsonstring) : expressionManager(new storm::expressions::ExpressionManager()) {
    parsedStructure = Json::parse(jsonstring);
}

template<typename ValueType>
void JaniParser<ValueType>::readFile(std::string const& path) {
    std::ifstream file;
    storm::io::openFile(path, file);
    file >> parsedStructure;
    storm::io::closeFile(file);
}

template<typename ValueType>
std::pair<storm::jani::Model, std::vector<storm::jani::Property>> JaniParser<ValueType>::parseModel(bool parseProperties) {
    // jani-version
    STORM_LOG_THROW(parsedStructure.count("jani-version") == 1, storm::exceptions::InvalidJaniException, "Jani-version must be given exactly once.");
    uint64_t version = getUnsignedInt<ValueType>(parsedStructure.at("jani-version"), "jani version");
    STORM_LOG_WARN_COND(version >= 1 && version <= 1, "JANI Version " << version << " is not supported. Results may be wrong.");
    // name
    STORM_LOG_THROW(parsedStructure.count("name") == 1, storm::exceptions::InvalidJaniException, "A model must have a (single) name.");
    std::string name = getString<ValueType>(parsedStructure.at("name"), "model name");
    // model type
    STORM_LOG_THROW(parsedStructure.count("type") == 1, storm::exceptions::InvalidJaniException, "A type must be given exactly once.");
    std::string modeltypestring = getString<ValueType>(parsedStructure.at("type"), "type of the model");
    storm::jani::ModelType type = storm::jani::getModelType(modeltypestring);
    STORM_LOG_THROW(type != storm::jani::ModelType::UNDEFINED, storm::exceptions::InvalidJaniException, "Model type " + modeltypestring + " not recognized.");
    storm::jani::Model model(name, type, version, expressionManager);
    uint_fast64_t featuresCount = parsedStructure.count("features");
    STORM_LOG_THROW(featuresCount < 2, storm::exceptions::InvalidJaniException, "Features-declarations can be given at most once.");
    if (featuresCount == 1) {
        const auto allKnownModelFeatures = storm::jani::getAllKnownModelFeatures();
        for (auto const& feature : parsedStructure.at("features")) {
            std::string featureStr = getString<ValueType>(feature, "Model feature");
            bool found = false;
            for (auto const& knownFeature : allKnownModelFeatures.asSet()) {
                if (featureStr == storm::jani::toString(knownFeature)) {
                    model.getModelFeatures().add(knownFeature);
                    found = true;
                    break;
                }
            }
            STORM_LOG_THROW(found, storm::exceptions::NotSupportedException, "Storm does not support the model feature " << featureStr << ".");
        }
    }
    uint_fast64_t actionCount = parsedStructure.count("actions");
    STORM_LOG_THROW(actionCount < 2, storm::exceptions::InvalidJaniException, "Action-declarations can be given at most once.");
    if (actionCount > 0) {
        parseActions(parsedStructure.at("actions"), model);
    }

    Scope scope(name);

    // Parse constants
    ConstantsMap constants;
    scope.constants = &constants;
    uint_fast64_t constantsCount = parsedStructure.count("constants");
    STORM_LOG_THROW(constantsCount < 2, storm::exceptions::InvalidJaniException, "Constant-declarations can be given at most once.");
    if (constantsCount == 1) {
        // Reserve enough space to make sure that pointers to constants remain valid after adding new ones.
        model.getConstants().reserve(parsedStructure.at("constants").size());
        for (auto const& constStructure : parsedStructure.at("constants")) {
            std::shared_ptr<storm::jani::Constant> constant =
                parseConstant(constStructure, scope.refine("constants[" + std::to_string(constants.size()) + "]"));
            model.addConstant(*constant);
            STORM_LOG_ASSERT(model.getConstants().back().getName() == constant->getName(), "Constant name mismatch.");
            constants.emplace(constant->getName(), &model.getConstants().back());
        }
    }

    // Parse variables
    uint_fast64_t variablesCount = parsedStructure.count("variables");
    STORM_LOG_THROW(variablesCount < 2, storm::exceptions::InvalidJaniException, "Variable-declarations can be given at most once for global variables.");
    VariablesMap globalVars;
    scope.globalVars = &globalVars;
    if (variablesCount == 1) {
        for (auto const& varStructure : parsedStructure.at("variables")) {
            std::shared_ptr<storm::jani::Variable> variable = parseVariable(varStructure, scope.refine("variables[" + std::to_string(globalVars.size())));
            globalVars.emplace(variable->getName(), &model.addVariable(*variable));
        }
    }

    uint64_t funDeclCount = parsedStructure.count("functions");
    STORM_LOG_THROW(funDeclCount < 2, storm::exceptions::InvalidJaniException, "Model '" << name << "' has more than one list of functions.");
    FunctionsMap globalFuns;
    scope.globalFunctions = &globalFuns;
    if (funDeclCount > 0) {
        // We require two passes through the function definitions array to allow referring to functions before they were defined.
        std::vector<storm::jani::FunctionDefinition> dummyFunctionDefinitions;
        for (auto const& funStructure : parsedStructure.at("functions")) {
            // Skip parsing of function body
            dummyFunctionDefinitions.push_back(
                parseFunctionDefinition(funStructure, scope.refine("functions[" + std::to_string(globalFuns.size()) + "] of model " + name), true));
        }
        // Store references to the dummy function definitions. This needs to happen in a separate loop since otherwise, references to FunDefs can be invalidated
        // after calling dummyFunctionDefinitions.push_back
        for (auto const& funDef : dummyFunctionDefinitions) {
            bool unused = globalFuns.emplace(funDef.getName(), &funDef).second;
            STORM_LOG_THROW(unused, storm::exceptions::InvalidJaniException,
                            "Multiple definitions of functions with the name " << funDef.getName() << " in " << scope.description << ".");
        }
        for (auto const& funStructure : parsedStructure.at("functions")) {
            // Actually parse the function body
            storm::jani::FunctionDefinition funDef =
                parseFunctionDefinition(funStructure, scope.refine("functions[" + std::to_string(globalFuns.size()) + "] of model " + name), false);
            STORM_LOG_ASSERT(globalFuns.count(funDef.getName()) == 1, "Global function not found.");
            globalFuns[funDef.getName()] = &model.addFunctionDefinition(funDef);
        }
    }

    // Parse Automata
    STORM_LOG_THROW(parsedStructure.count("automata") == 1, storm::exceptions::InvalidJaniException, "Exactly one list of automata must be given.");
    STORM_LOG_THROW(parsedStructure.at("automata").is_array(), storm::exceptions::InvalidJaniException, "Automata must be an array.");
    // Automatons can only be parsed after constants and variables.
    for (auto const& automataEntry : parsedStructure.at("automata")) {
        model.addAutomaton(parseAutomaton(automataEntry, model, scope.refine("automata[" + std::to_string(model.getNumberOfAutomata()) + "]")));
    }
    STORM_LOG_THROW(parsedStructure.count("restrict-initial") < 2, storm::exceptions::InvalidJaniException, "Model has multiple initial value restrictions.");
    storm::expressions::Expression initialValueRestriction = expressionManager->boolean(true);
    if (parsedStructure.count("restrict-initial") > 0) {
        STORM_LOG_THROW(parsedStructure.at("restrict-initial").count("exp") == 1, storm::exceptions::InvalidJaniException,
                        "Model needs an expression inside the initial restricion.");
        initialValueRestriction = parseExpression(parsedStructure.at("restrict-initial").at("exp"), scope.refine("Initial value restriction"));
    }
    model.setInitialStatesRestriction(initialValueRestriction);
    STORM_LOG_THROW(parsedStructure.count("system") == 1, storm::exceptions::InvalidJaniException, "Exactly one system description must be given.");
    std::shared_ptr<storm::jani::Composition> composition = parseComposition(parsedStructure.at("system"));
    model.setSystemComposition(composition);
    model.finalize();

    // Parse properties
    storm::logic::RewardAccumulationEliminationVisitor rewAccEliminator(model);
    STORM_LOG_THROW(parsedStructure.count("properties") <= 1, storm::exceptions::InvalidJaniException, "At most one list of properties can be given.");
    std::vector<storm::jani::Property> properties;
    if (parseProperties && parsedStructure.count("properties") == 1) {
        STORM_LOG_THROW(parsedStructure.at("properties").is_array(), storm::exceptions::InvalidJaniException, "Properties should be an array.");
        for (auto const& propertyEntry : parsedStructure.at("properties")) {
            try {
                auto prop = this->parseProperty(model, propertyEntry, scope.refine("property[" + std::to_string(properties.size()) + "]"));
                // Eliminate reward accumulations as much as possible
                rewAccEliminator.eliminateRewardAccumulations(prop);
                properties.push_back(prop);
            } catch (storm::exceptions::NotSupportedException const& ex) {
                STORM_LOG_WARN("Cannot handle property: " << ex.what());
            } catch (storm::exceptions::NotImplementedException const& ex) {
                STORM_LOG_WARN("Cannot handle property: " << ex.what());
            }
        }
    }
    return {model, properties};
}

template<typename ValueType>
std::vector<std::shared_ptr<storm::logic::Formula const>> JaniParser<ValueType>::parseUnaryFormulaArgument(storm::jani::Model& model,
                                                                                                           Json const& propertyStructure,
                                                                                                           storm::logic::FormulaContext formulaContext,
                                                                                                           std::string const& opstring, Scope const& scope) {
    STORM_LOG_THROW(propertyStructure.count("exp") == 1, storm::exceptions::InvalidJaniException,
                    "Expecting operand for operator " << opstring << " in " << scope.description << ".");
    return {parseFormula(model, propertyStructure.at("exp"), formulaContext, scope.refine("Operand of operator " + opstring))};
}

template<typename ValueType>
std::vector<std::shared_ptr<storm::logic::Formula const>> JaniParser<ValueType>::parseBinaryFormulaArguments(storm::jani::Model& model,
                                                                                                             Json const& propertyStructure,
                                                                                                             storm::logic::FormulaContext formulaContext,
                                                                                                             std::string const& opstring, Scope const& scope) {
    STORM_LOG_THROW(propertyStructure.count("left") == 1, storm::exceptions::InvalidJaniException,
                    "Expecting left operand for operator " << opstring << " in " << scope.description << ".");
    STORM_LOG_THROW(propertyStructure.count("right") == 1, storm::exceptions::InvalidJaniException,
                    "Expecting right operand for operator " << opstring << " in " << scope.description << ".");
    return {parseFormula(model, propertyStructure.at("left"), formulaContext, scope.refine("Operand of operator " + opstring)),
            parseFormula(model, propertyStructure.at("right"), formulaContext, scope.refine("Operand of operator " + opstring))};
}

template<typename ValueType>
storm::jani::PropertyInterval JaniParser<ValueType>::parsePropertyInterval(Json const& piStructure, Scope const& scope) {
    storm::jani::PropertyInterval pi;
    if (piStructure.count("lower") > 0) {
        pi.lowerBound = parseExpression(piStructure.at("lower"), scope.refine("Lower bound for property interval"));
    }
    if (piStructure.count("lower-exclusive") > 0) {
        STORM_LOG_THROW(pi.lowerBound.isInitialized(), storm::exceptions::InvalidJaniException, "Lower-exclusive can only be set if a lower bound is present.");
        pi.lowerBoundStrict = piStructure.at("lower-exclusive");
    }
    if (piStructure.count("upper") > 0) {
        pi.upperBound = parseExpression(piStructure.at("upper"), scope.refine("Upper bound for property interval"));
    }
    if (piStructure.count("upper-exclusive") > 0) {
        STORM_LOG_THROW(pi.upperBound.isInitialized(), storm::exceptions::InvalidJaniException, "Lower-exclusive can only be set if a lower bound is present.");
        pi.upperBoundStrict = piStructure.at("upper-exclusive");
    }
    STORM_LOG_THROW(pi.lowerBound.isInitialized() || pi.upperBound.isInitialized(), storm::exceptions::InvalidJaniException,
                    "Bounded operator must have a bounded interval, but no bounds found in '" << piStructure << "'.");
    return pi;
}

template<typename ValueType>
storm::logic::RewardAccumulation JaniParser<ValueType>::parseRewardAccumulation(Json const& accStructure, std::string const& context) {
    bool accTime = false;
    bool accSteps = false;
    bool accExit = false;
    STORM_LOG_THROW(accStructure.is_array(), storm::exceptions::InvalidJaniException, "Accumulate should be an array.");
    for (auto const& accEntry : accStructure) {
        if (accEntry == "steps") {
            accSteps = true;
        } else if (accEntry == "time") {
            accTime = true;
        } else if (accEntry == "exit") {
            accExit = true;
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                            "One may only accumulate either 'steps' or 'time' or 'exit', got " << accEntry.dump() << " in " << context << ".");
        }
    }
    return storm::logic::RewardAccumulation(accSteps, accTime, accExit);
}

void insertLowerUpperTimeBounds(std::vector<boost::optional<storm::logic::TimeBound>>& lowerBounds,
                                std::vector<boost::optional<storm::logic::TimeBound>>& upperBounds, storm::jani::PropertyInterval const& pi) {
    if (pi.hasLowerBound()) {
        lowerBounds.push_back(storm::logic::TimeBound(pi.lowerBoundStrict, pi.lowerBound));
    } else {
        lowerBounds.push_back(boost::none);
    }
    if (pi.hasUpperBound()) {
        upperBounds.push_back(storm::logic::TimeBound(pi.upperBoundStrict, pi.upperBound));
    } else {
        upperBounds.push_back(boost::none);
    }
}

template<typename ValueType>
std::shared_ptr<storm::logic::Formula const> JaniParser<ValueType>::parseFormula(storm::jani::Model& model, Json const& propertyStructure,
                                                                                 storm::logic::FormulaContext formulaContext, Scope const& scope,
                                                                                 boost::optional<storm::logic::Bound> bound) {
    if (propertyStructure.is_boolean()) {
        return std::make_shared<storm::logic::BooleanLiteralFormula>(propertyStructure.template get<bool>());
    }
    if (propertyStructure.is_string()) {
        if (labels.count(propertyStructure.template get<std::string>()) > 0) {
            return std::make_shared<storm::logic::AtomicLabelFormula>(propertyStructure.template get<std::string>());
        }
    }
    storm::expressions::Expression expr = parseExpression(propertyStructure, scope.refine("expression in property"), true);
    if (expr.isInitialized()) {
        bool exprContainsLabel = false;
        auto varsInExpr = expr.getVariables();
        for (auto const& varInExpr : varsInExpr) {
            if (labels.count(varInExpr.getName()) > 0) {
                exprContainsLabel = true;
                break;
            }
        }
        if (!exprContainsLabel) {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for atomic expression formula.");
            return std::make_shared<storm::logic::AtomicExpressionFormula>(expr);
        }
    }
    if (propertyStructure.count("op") == 1) {
        std::string opString = getString<ValueType>(propertyStructure.at("op"), "Operation description");

        if (opString == "Pmin" || opString == "Pmax") {
            std::vector<std::shared_ptr<storm::logic::Formula const>> args =
                parseUnaryFormulaArgument(model, propertyStructure, storm::logic::FormulaContext::Probability, opString, scope);
            STORM_LOG_ASSERT(args.size() == 1, "Expected one argument for probability operator.");
            storm::logic::OperatorInformation opInfo;
            opInfo.optimalityType = opString == "Pmin" ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;
            opInfo.bound = bound;
            return std::make_shared<storm::logic::ProbabilityOperatorFormula>(args[0], opInfo);

        } else if (opString == "∀" || opString == "∃") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for forall/exists.");
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Forall and Exists are currently not supported in " << scope.description << ".");
        } else if (opString == "Emin" || opString == "Emax") {
            STORM_LOG_WARN_COND(model.getJaniVersion() == 1, "Model not compliant: Contains Emin/Emax property in " << scope.description << ".");
            STORM_LOG_THROW(propertyStructure.count("exp") == 1, storm::exceptions::InvalidJaniException,
                            "Expecting reward-expression for operator " << opString << " in " << scope.description << ".");
            storm::expressions::Expression rewExpr = parseExpression(propertyStructure.at("exp"), scope.refine("Reward expression"));
            STORM_LOG_THROW(rewExpr.hasNumericalType(), storm::exceptions::InvalidJaniException,
                            "Reward expression '" << rewExpr << "' does not have numerical type in " << scope.description << ".");
            std::string rewardName = rewExpr.toString();

            storm::logic::OperatorInformation opInfo;
            opInfo.optimalityType = opString == "Emin" ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;
            opInfo.bound = bound;

            storm::logic::RewardAccumulation rewardAccumulation(false, false, false);
            if (propertyStructure.count("accumulate") > 0) {
                rewardAccumulation = parseRewardAccumulation(propertyStructure.at("accumulate"), scope.description);
            }

            bool time = false;
            if (propertyStructure.count("step-instant") > 0) {
                STORM_LOG_THROW(propertyStructure.count("time-instant") == 0, storm::exceptions::NotSupportedException,
                                "Storm does not support to have a step-instant and a time-instant in " + scope.description + ".");
                STORM_LOG_THROW(propertyStructure.count("reward-instants") == 0, storm::exceptions::NotSupportedException,
                                "Storm does not support to have a step-instant and a reward-instant in " + scope.description + ".");

                storm::expressions::Expression stepInstantExpr = parseExpression(propertyStructure.at("step-instant"), scope.refine("Step instant"));
                if (!rewExpr.isVariable()) {
                    model.addNonTrivialRewardExpression(rewardName, rewExpr);
                }
                if (rewardAccumulation.isEmpty()) {
                    return std::make_shared<storm::logic::RewardOperatorFormula>(
                        std::make_shared<storm::logic::InstantaneousRewardFormula>(stepInstantExpr, storm::logic::TimeBoundType::Steps), rewardName, opInfo);
                } else {
                    return std::make_shared<storm::logic::RewardOperatorFormula>(
                        std::make_shared<storm::logic::CumulativeRewardFormula>(storm::logic::TimeBound(false, stepInstantExpr),
                                                                                storm::logic::TimeBoundReference(storm::logic::TimeBoundType::Steps),
                                                                                rewardAccumulation),
                        rewardName, opInfo);
                }
            } else if (propertyStructure.count("time-instant") > 0) {
                STORM_LOG_THROW(propertyStructure.count("reward-instants") == 0, storm::exceptions::NotSupportedException,
                                "Storm does not support to have a time-instant and a reward-instant in " + scope.description + ".");
                storm::expressions::Expression timeInstantExpr = parseExpression(propertyStructure.at("time-instant"), scope.refine("time instant"));
                if (!rewExpr.isVariable()) {
                    model.addNonTrivialRewardExpression(rewardName, rewExpr);
                }
                if (rewardAccumulation.isEmpty()) {
                    return std::make_shared<storm::logic::RewardOperatorFormula>(
                        std::make_shared<storm::logic::InstantaneousRewardFormula>(timeInstantExpr, storm::logic::TimeBoundType::Time), rewardName, opInfo);
                } else {
                    return std::make_shared<storm::logic::RewardOperatorFormula>(
                        std::make_shared<storm::logic::CumulativeRewardFormula>(storm::logic::TimeBound(false, timeInstantExpr),
                                                                                storm::logic::TimeBoundReference(storm::logic::TimeBoundType::Time),
                                                                                rewardAccumulation),
                        rewardName, opInfo);
                }
            } else if (propertyStructure.count("reward-instants") > 0) {
                std::vector<storm::logic::TimeBound> bounds;
                std::vector<storm::logic::TimeBoundReference> boundReferences;
                for (auto const& rewInst : propertyStructure.at("reward-instants")) {
                    storm::expressions::Expression rewInstRewardModelExpression =
                        parseExpression(rewInst.at("exp"), scope.refine("Reward expression at reward instant"));
                    STORM_LOG_THROW(rewInstRewardModelExpression.hasNumericalType(), storm::exceptions::InvalidJaniException,
                                    "Reward expression '" << rewInstRewardModelExpression << "' does not have numerical type in " << scope.description << ".");
                    storm::logic::RewardAccumulation boundRewardAccumulation = parseRewardAccumulation(rewInst.at("accumulate"), scope.description);
                    bool steps = (boundRewardAccumulation.isStepsSet() || boundRewardAccumulation.isExitSet()) && boundRewardAccumulation.size() == 1;
                    bool time = boundRewardAccumulation.isTimeSet() && boundRewardAccumulation.size() == 1 && !model.isDiscreteTimeModel();
                    if ((steps || time) && !rewInstRewardModelExpression.containsVariables() &&
                        storm::utility::isOne(rewInstRewardModelExpression.evaluateAsRational())) {
                        boundReferences.emplace_back(steps ? storm::logic::TimeBoundType::Steps : storm::logic::TimeBoundType::Time);
                    } else {
                        std::string rewInstRewardModelName = rewInstRewardModelExpression.toString();
                        if (!rewInstRewardModelExpression.isVariable()) {
                            model.addNonTrivialRewardExpression(rewInstRewardModelName, rewInstRewardModelExpression);
                        }
                        boundReferences.emplace_back(rewInstRewardModelName, boundRewardAccumulation);
                    }
                    storm::expressions::Expression rewInstantExpr = parseExpression(rewInst.at("instant"), scope.refine("reward instant"));
                    bounds.emplace_back(false, rewInstantExpr);
                }
                if (!rewExpr.isVariable()) {
                    model.addNonTrivialRewardExpression(rewardName, rewExpr);
                }
                return std::make_shared<storm::logic::RewardOperatorFormula>(
                    std::make_shared<storm::logic::CumulativeRewardFormula>(bounds, boundReferences, rewardAccumulation), rewardName, opInfo);
            } else {
                time = !rewExpr.containsVariables() && storm::utility::isOne(rewExpr.evaluateAsRational());
                std::shared_ptr<storm::logic::Formula const> subformula;
                if (propertyStructure.count("reach") > 0) {
                    auto formulaContext = time ? storm::logic::FormulaContext::Time : storm::logic::FormulaContext::Reward;
                    subformula = std::make_shared<storm::logic::EventuallyFormula>(
                        parseFormula(model, propertyStructure.at("reach"), formulaContext, scope.refine("Reach-expression of operator " + opString)),
                        formulaContext, rewardAccumulation);
                } else {
                    subformula = std::make_shared<storm::logic::TotalRewardFormula>(rewardAccumulation);
                }
                if (time) {
                    STORM_LOG_ASSERT(subformula->isTotalRewardFormula() || subformula->isTimePathFormula(), "Expected total reward or time path formula.");
                    return std::make_shared<storm::logic::TimeOperatorFormula>(subformula, opInfo);
                } else {
                    if (!rewExpr.isVariable()) {
                        model.addNonTrivialRewardExpression(rewardName, rewExpr);
                    }
                    return std::make_shared<storm::logic::RewardOperatorFormula>(subformula, rewardName, opInfo);
                }
            }
        } else if (opString == "Smin" || opString == "Smax") {
            storm::logic::OperatorInformation opInfo;
            opInfo.optimalityType = opString == "Smin" ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;
            opInfo.bound = bound;
            // Reward accumulation is optional as it was not available in the early days...
            boost::optional<storm::logic::RewardAccumulation> rewardAccumulation;
            if (propertyStructure.count("accumulate") > 0) {
                STORM_LOG_WARN_COND(model.getJaniVersion() == 1, "Unexpected accumulate field in " << scope.description << ".");
                rewardAccumulation = parseRewardAccumulation(propertyStructure.at("accumulate"), scope.description);
            }
            STORM_LOG_THROW(propertyStructure.count("exp") > 0, storm::exceptions::InvalidJaniException,
                            "Expected an expression at steady state property at " << scope.description << ".");
            auto exp = parseExpression(propertyStructure["exp"], scope.refine("steady-state operator"), true);
            if (!exp.isInitialized() || exp.hasBooleanType()) {
                STORM_LOG_THROW(!rewardAccumulation.is_initialized(), storm::exceptions::InvalidJaniException,
                                "Long-run average probabilities are not allowed to have a reward accumulation at" << scope.description << ".");
                std::shared_ptr<storm::logic::Formula const> subformula =
                    parseUnaryFormulaArgument(model, propertyStructure, formulaContext, opString, scope.refine("Steady-state operator"))[0];
                return std::make_shared<storm::logic::LongRunAverageOperatorFormula>(subformula, opInfo);
            }
            STORM_LOG_THROW(exp.hasNumericalType(), storm::exceptions::InvalidJaniException,
                            "Reward expression '" << exp << "' does not have numerical type in " << scope.description << ".");
            std::string rewardName = exp.toString();
            if (!exp.isVariable()) {
                model.addNonTrivialRewardExpression(rewardName, exp);
            }
            auto subformula = std::make_shared<storm::logic::LongRunAverageRewardFormula>(rewardAccumulation);
            return std::make_shared<storm::logic::RewardOperatorFormula>(subformula, rewardName, opInfo);

        } else if (opString == "U" || opString == "F") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for until/eventually.");
            std::vector<std::shared_ptr<storm::logic::Formula const>> args;
            if (opString == "U") {
                args = parseBinaryFormulaArguments(model, propertyStructure, formulaContext, opString, scope);
            } else {
                STORM_LOG_ASSERT(opString == "F", "Expected F operator.");
                args = parseUnaryFormulaArgument(model, propertyStructure, formulaContext, opString, scope);
                args.push_back(args[0]);
                args[0] = storm::logic::BooleanLiteralFormula::getTrueFormula();
            }

            std::vector<boost::optional<storm::logic::TimeBound>> lowerBounds, upperBounds;
            std::vector<storm::logic::TimeBoundReference> tbReferences;
            if (propertyStructure.count("step-bounds") > 0) {
                STORM_LOG_WARN_COND(model.getJaniVersion() == 1, "Jani model not compliant: Contains step-bounds in " << scope.description << ".");
                storm::jani::PropertyInterval pi =
                    parsePropertyInterval(propertyStructure.at("step-bounds"), scope.refine("step-bounded until").clearVariables());
                insertLowerUpperTimeBounds(lowerBounds, upperBounds, pi);
                tbReferences.emplace_back(storm::logic::TimeBoundType::Steps);
            }
            if (propertyStructure.count("time-bounds") > 0) {
                STORM_LOG_WARN_COND(model.getJaniVersion() == 1, "Jani model not compliant: Contains time-bounds in " << scope.description << ".");
                storm::jani::PropertyInterval pi =
                    parsePropertyInterval(propertyStructure.at("time-bounds"), scope.refine("time-bounded until").clearVariables());
                insertLowerUpperTimeBounds(lowerBounds, upperBounds, pi);
                tbReferences.emplace_back(storm::logic::TimeBoundType::Time);
            }
            if (propertyStructure.count("reward-bounds") > 0) {
                for (auto const& rbStructure : propertyStructure.at("reward-bounds")) {
                    storm::jani::PropertyInterval pi = parsePropertyInterval(rbStructure.at("bounds"), scope.refine("reward-bounded until").clearVariables());
                    insertLowerUpperTimeBounds(lowerBounds, upperBounds, pi);
                    STORM_LOG_THROW(rbStructure.count("exp") == 1, storm::exceptions::InvalidJaniException,
                                    "Expecting reward-expression for operator " << opString << " in " << scope.description << ".");
                    storm::expressions::Expression rewInstRewardModelExpression =
                        parseExpression(rbStructure.at("exp"), scope.refine("Reward expression at reward-bounds"));
                    STORM_LOG_THROW(rewInstRewardModelExpression.hasNumericalType(), storm::exceptions::InvalidJaniException,
                                    "Reward expression '" << rewInstRewardModelExpression << "' does not have numerical type in " << scope.description << ".");
                    storm::logic::RewardAccumulation boundRewardAccumulation = parseRewardAccumulation(rbStructure.at("accumulate"), scope.description);
                    bool steps = (boundRewardAccumulation.isStepsSet() || boundRewardAccumulation.isExitSet()) && boundRewardAccumulation.size() == 1;
                    bool time = boundRewardAccumulation.isTimeSet() && boundRewardAccumulation.size() == 1 && !model.isDiscreteTimeModel();
                    if ((steps || time) && !rewInstRewardModelExpression.containsVariables() &&
                        storm::utility::isOne(rewInstRewardModelExpression.evaluateAsRational())) {
                        tbReferences.emplace_back(steps ? storm::logic::TimeBoundType::Steps : storm::logic::TimeBoundType::Time);
                    } else {
                        std::string rewInstRewardModelName = rewInstRewardModelExpression.toString();
                        if (!rewInstRewardModelExpression.isVariable()) {
                            model.addNonTrivialRewardExpression(rewInstRewardModelName, rewInstRewardModelExpression);
                        }
                        tbReferences.emplace_back(rewInstRewardModelName, boundRewardAccumulation);
                    }
                }
            }
            if (!tbReferences.empty()) {
                return std::make_shared<storm::logic::BoundedUntilFormula const>(args[0], args[1], lowerBounds, upperBounds, tbReferences);
            } else if (args[0]->isTrueFormula()) {
                return std::make_shared<storm::logic::EventuallyFormula const>(args[1], formulaContext);
            } else {
                return std::make_shared<storm::logic::UntilFormula const>(args[0], args[1]);
            }
        } else if (opString == "G") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for globally.");
            std::vector<std::shared_ptr<storm::logic::Formula const>> args =
                parseUnaryFormulaArgument(model, propertyStructure, formulaContext, opString, scope.refine("Subformula of globally operator "));
            if (propertyStructure.count("step-bounds") > 0) {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Globally and step-bounds are not supported.");
            } else if (propertyStructure.count("time-bounds") > 0) {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Globally and time bounds are not supported.");
            } else if (propertyStructure.count("reward-bounds") > 0) {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Globally and reward bounded properties are not supported.");
            }
            return std::make_shared<storm::logic::GloballyFormula const>(args[0]);

        } else if (opString == "W") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for weak until.");
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Weak until is not supported.");
        } else if (opString == "R") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for release.");
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Release is not supported.");
        } else if (opString == "∧" || opString == "∨") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for conjunction/disjunction.");
            std::vector<std::shared_ptr<storm::logic::Formula const>> args =
                parseBinaryFormulaArguments(model, propertyStructure, formulaContext, opString, scope);
            STORM_LOG_ASSERT(args.size() == 2, "Expected two arguments for conjunction/disjunction.");
            storm::logic::BinaryBooleanStateFormula::OperatorType oper =
                opString == "∧" ? storm::logic::BinaryBooleanStateFormula::OperatorType::And : storm::logic::BinaryBooleanStateFormula::OperatorType::Or;
            return std::make_shared<storm::logic::BinaryBooleanStateFormula const>(oper, args[0], args[1]);
        } else if (opString == "⇒") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for implication.");
            std::vector<std::shared_ptr<storm::logic::Formula const>> args =
                parseBinaryFormulaArguments(model, propertyStructure, formulaContext, opString, scope);
            STORM_LOG_ASSERT(args.size() == 2, "Expected two arguments for implication.");
            std::shared_ptr<storm::logic::UnaryBooleanStateFormula const> tmp =
                std::make_shared<storm::logic::UnaryBooleanStateFormula const>(storm::logic::UnaryBooleanStateFormula::OperatorType::Not, args[0]);
            return std::make_shared<storm::logic::BinaryBooleanStateFormula const>(storm::logic::BinaryBooleanStateFormula::OperatorType::Or, tmp, args[1]);
        } else if (opString == "¬") {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for negation.");
            std::vector<std::shared_ptr<storm::logic::Formula const>> args =
                parseUnaryFormulaArgument(model, propertyStructure, formulaContext, opString, scope);
            STORM_LOG_ASSERT(args.size() == 1, "Expected one argument for negation.");
            return std::make_shared<storm::logic::UnaryBooleanStateFormula const>(storm::logic::UnaryBooleanStateFormula::OperatorType::Not, args[0]);
        } else if (!expr.isInitialized() && (opString == "≥" || opString == "≤" || opString == "<" || opString == ">" || opString == "=" || opString == "≠")) {
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for comparison.");
            storm::logic::ComparisonType ct;
            if (opString == "≥") {
                ct = storm::logic::ComparisonType::GreaterEqual;
            } else if (opString == "≤") {
                ct = storm::logic::ComparisonType::LessEqual;
            } else if (opString == "<") {
                ct = storm::logic::ComparisonType::Less;
            } else if (opString == ">") {
                ct = storm::logic::ComparisonType::Greater;
            }

            std::vector<std::string> const leftRight = {"left", "right"};
            for (uint64_t i = 0; i < 2; ++i) {
                if (propertyStructure.at(leftRight[i]).count("op") > 0) {
                    std::string propertyOperatorString = getString<ValueType>(propertyStructure.at(leftRight[i]).at("op"), "property-operator");
                    std::set<std::string> const propertyOperatorStrings = {"Pmin", "Pmax", "Emin", "Emax", "Smin", "Smax"};
                    if (propertyOperatorStrings.count(propertyOperatorString) > 0) {
                        auto boundExpr =
                            parseExpression(propertyStructure.at(leftRight[1 - i]),
                                            scope.refine("Threshold for operator " + propertyStructure.at(leftRight[i]).at("op").template get<std::string>()));
                        if ((opString == "=" || opString == "≠")) {
                            STORM_LOG_THROW(!boundExpr.containsVariables(), storm::exceptions::NotSupportedException,
                                            "Comparison operators '=' or '≠' in property specifications are currently not supported.");
                            auto boundValue = boundExpr.evaluateAsRational();
                            if (storm::utility::isZero(boundValue)) {
                                if (opString == "=") {
                                    ct = storm::logic::ComparisonType::LessEqual;
                                } else {
                                    ct = storm::logic::ComparisonType::Greater;
                                }
                            } else if (storm::utility::isOne(boundValue) && (propertyOperatorString == "Pmin" || propertyOperatorString == "Pmax")) {
                                if (opString == "=") {
                                    ct = storm::logic::ComparisonType::GreaterEqual;
                                } else {
                                    ct = storm::logic::ComparisonType::Less;
                                }
                            } else {
                                STORM_LOG_THROW(
                                    false, storm::exceptions::NotSupportedException,
                                    "Comparison operators '=' or '≠' in property specifications are currently not supported in " << scope.description << ".");
                            }
                        }
                        return parseFormula(model, propertyStructure.at(leftRight[i]), formulaContext, scope, storm::logic::Bound(ct, boundExpr));
                    }
                }
            }
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "No complex comparisons for properties are supported.");
        } else if (opString == "Multi") {
            STORM_LOG_WARN_COND(model.getModelFeatures().hasMultiObjectiveProperties(),
                                "Model feature " << storm::jani::toString(storm::jani::ModelFeature::MultiObjectiveProperties)
                                                 << " not enabled but model contains multi-objective property in " << scope.description
                                                 << ". Continuing with that property anyways");
            STORM_LOG_ASSERT(bound == boost::none, "Unexpected bound for multi-objective.");
            STORM_LOG_THROW(propertyStructure.count("properties") == 1, storm::exceptions::InvalidJaniException,
                            "Expecting properties for multi-objective operator in " << scope.description << ".");
            std::vector<std::shared_ptr<storm::logic::Formula const>> subformulas;
            uint64_t i = 0;
            for (auto const& subPropStructure : propertyStructure.at("properties")) {
                STORM_LOG_THROW(subPropStructure.count("exp") == 1, storm::exceptions::InvalidJaniException,
                                "Expecting property expression in subproperty #" << i << " in " << scope.description << ".");
                subformulas.push_back(parseFormula(model, subPropStructure["exp"], formulaContext,
                                                   scope.refine("Subproperty #" + std::to_string(i) + " of multi-objective operator")));
                if (subPropStructure.count("opt") == 1) {
                    STORM_LOG_THROW(subformulas.back()->hasQuantitativeResult(), storm::exceptions::InvalidJaniException,
                                    "Subformula #" << i << " has an optimization direction but is not numeric in " << scope.description << ".");
                    STORM_LOG_THROW(subformulas.back()->isOperatorFormula(), storm::exceptions::NotSupportedException,
                                    "Subformula #" << i << " is not an operator formula in " << scope.description << ".");
                    std::string const optString =
                        getString<ValueType>(subPropStructure.at("opt"), "optimization direction for subproperty #" + std::to_string(i));
                    STORM_LOG_THROW(optString == "min" || optString == "max", storm::exceptions::InvalidJaniException,
                                    "Unknown optimization direction " << optString << " for subproperty #" << i << " in " << scope.description << ".");
                    auto const opt = optString == "min" ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;
                    auto newFormula = subformulas.back()->clone();
                    newFormula->asOperatorFormula().setOptimalityType(opt);
                    subformulas.back() = newFormula;
                } else {
                    STORM_LOG_THROW(subformulas.back()->hasQualitativeResult(), storm::exceptions::InvalidJaniException,
                                    "Subformula #" << i << " has non-Boolean result but no optimization direction in " << scope.description << ".");
                }
                ++i;
            }
            STORM_LOG_THROW(propertyStructure.count("type") == 1, storm::exceptions::InvalidJaniException,
                            "Expecting type for multi-objective operator in " << scope.description << ".");
            std::string const typeString = getString<ValueType>(propertyStructure.at("type"), "type of multi-objective operator");
            STORM_LOG_THROW(typeString == "tradeoff" || typeString == "lexicographic", storm::exceptions::InvalidJaniException,
                            "Unknown type " << typeString << " for multi-objective operator in " << scope.description << ".");
            auto const type =
                typeString == "tradeoff" ? storm::logic::MultiObjectiveFormula::Type::Tradeoff : storm::logic::MultiObjectiveFormula::Type::Lexicographic;
            return std::make_shared<storm::logic::MultiObjectiveFormula const>(subformulas, type);
        } else if (expr.isInitialized()) {
            STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                            "Non-trivial Expression '" << expr << "' contains a boolean transient variable. Can not translate to PRCTL-like formula at "
                                                       << scope.description << ".");
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException, "Unknown operator " << opString << ".");
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                        "Looking for operator for formula " << propertyStructure.dump() << ", but did not find one.");
    }
}

template<typename ValueType>
storm::jani::Property JaniParser<ValueType>::parseProperty(storm::jani::Model& model, Json const& propertyStructure, Scope const& scope) {
    STORM_LOG_THROW(propertyStructure.count("name") == 1, storm::exceptions::InvalidJaniException, "Property must have a name.");
    // TODO check unique name
    std::string name = getString<ValueType>(propertyStructure.at("name"), "property-name");
    STORM_LOG_TRACE("Parsing property named: " << name);
    std::string comment = "";
    if (propertyStructure.count("comment") > 0) {
        comment = getString<ValueType>(propertyStructure.at("comment"), "comment for property named '" + name + "'.");
    }
    STORM_LOG_THROW(propertyStructure.count("expression") == 1, storm::exceptions::InvalidJaniException, "Property must have an expression.");
    // Parse filter expression.
    Json const& expressionStructure = propertyStructure.at("expression");

    STORM_LOG_THROW(expressionStructure.count("op") == 1, storm::exceptions::InvalidJaniException,
                    "Expression in property must have an operation description.");
    STORM_LOG_THROW(expressionStructure.at("op") == "filter", storm::exceptions::InvalidJaniException, "Top level operation of a property must be a filter.");
    STORM_LOG_THROW(expressionStructure.count("fun") == 1, storm::exceptions::InvalidJaniException, "Filter must have a function descritpion.");
    std::string funDescr = getString<ValueType>(expressionStructure.at("fun"), "Filter function in property named " + name);
    storm::modelchecker::FilterType ft;
    if (funDescr == "min") {
        ft = storm::modelchecker::FilterType::MIN;
    } else if (funDescr == "max") {
        ft = storm::modelchecker::FilterType::MAX;
    } else if (funDescr == "sum") {
        ft = storm::modelchecker::FilterType::SUM;
    } else if (funDescr == "avg") {
        ft = storm::modelchecker::FilterType::AVG;
    } else if (funDescr == "count") {
        ft = storm::modelchecker::FilterType::COUNT;
    } else if (funDescr == "∀") {
        ft = storm::modelchecker::FilterType::FORALL;
    } else if (funDescr == "∃") {
        ft = storm::modelchecker::FilterType::EXISTS;
    } else if (funDescr == "argmin") {
        ft = storm::modelchecker::FilterType::ARGMIN;
    } else if (funDescr == "argmax") {
        ft = storm::modelchecker::FilterType::ARGMAX;
    } else if (funDescr == "values") {
        ft = storm::modelchecker::FilterType::VALUES;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException, "Unknown filter description " << funDescr << " in property named " << name << ".");
    }

    STORM_LOG_THROW(expressionStructure.count("states") == 1, storm::exceptions::InvalidJaniException, "Filter must have a states description.");
    std::shared_ptr<storm::logic::Formula const> statesFormula;
    if (expressionStructure.at("states").count("op") > 0) {
        std::string statesDescr = getString<ValueType>(expressionStructure.at("states").at("op"), "Filtered states in property named " + name);
        if (statesDescr == "initial") {
            statesFormula = std::make_shared<storm::logic::AtomicLabelFormula>("init");
        }
    }
    if (!statesFormula) {
        try {
            // Try to parse the states as formula.
            statesFormula =
                parseFormula(model, expressionStructure.at("states"), storm::logic::FormulaContext::Undefined, scope.refine("Values of property " + name));
        } catch (storm::exceptions::NotSupportedException const& ex) {
            throw ex;
        } catch (storm::exceptions::NotImplementedException const& ex) {
            throw ex;
        }
    }
    STORM_LOG_THROW(statesFormula, storm::exceptions::NotImplementedException, "Could not derive states formula.");
    STORM_LOG_THROW(expressionStructure.count("values") == 1, storm::exceptions::InvalidJaniException, "Values as input for a filter must be given.");
    auto formula = parseFormula(model, expressionStructure.at("values"), storm::logic::FormulaContext::Undefined, scope.refine("Values of property " + name));
    return storm::jani::Property(name, storm::jani::FilterExpression(formula, ft, statesFormula), {}, comment);
}

template<typename ValueType>
std::shared_ptr<storm::jani::Constant> JaniParser<ValueType>::parseConstant(Json const& constantStructure, Scope const& scope) {
    STORM_LOG_THROW(constantStructure.count("name") == 1, storm::exceptions::InvalidJaniException,
                    "Variable (scope: " + scope.description + ") must have a name.");
    std::string name = getString<ValueType>(constantStructure.at("name"), "variable-name in " + scope.description + "-scope");
    // TODO check existance of name.
    // TODO store prefix in variable.
    std::string exprManagerName = name;

    STORM_LOG_THROW(constantStructure.count("type") == 1, storm::exceptions::InvalidJaniException,
                    "Constant '" + name + "' (scope: " + scope.description + ") must have a (single) type-declaration.");
    auto type = parseType(constantStructure.at("type"), name, scope);
    STORM_LOG_THROW((type.first->isBasicType() || type.first->isBoundedType()), storm::exceptions::InvalidJaniException,
                    "Constant '" + name + "' (scope: " + scope.description + ") has unexpected type.");

    uint_fast64_t valueCount = constantStructure.count("value");
    storm::expressions::Expression definingExpression;
    STORM_LOG_THROW(valueCount < 2, storm::exceptions::InvalidJaniException,
                    "Value for constant '" + name + "' (scope: " + scope.description + ") must be given at most once.");
    if (valueCount == 1) {
        // Read initial value before; that makes creation later on a bit easier, and has as an additional benefit that we do not need to check whether the
        // variable occurs also on the assignment.
        definingExpression = parseExpression(constantStructure.at("value"), scope.refine("Value of constant " + name));
        STORM_LOG_ASSERT(definingExpression.isInitialized(), "Defining expression not initialized.");
        // Check that the defined and actual expression value match OR the defined value is a rational and the actual value is a numerical type.
        STORM_LOG_THROW((type.second == definingExpression.getType() || (type.second.isRationalType() && definingExpression.getType().isNumericalType())),
                        storm::exceptions::InvalidJaniException,
                        "Type of value for constant '" + name + "' (scope: " + scope.description + ") does not match the given type '" +
                            type.first->getStringRepresentation() + ".");
    }

    storm::expressions::Variable var = expressionManager->declareVariable(exprManagerName, type.second);

    storm::expressions::Expression constraintExpression;
    if (type.first->isBoundedType()) {
        auto const& bndType = type.first->asBoundedType();
        if (bndType.hasLowerBound()) {
            constraintExpression = var.getExpression() >= bndType.getLowerBound();
            if (bndType.hasUpperBound()) {
                constraintExpression = constraintExpression && var.getExpression() <= bndType.getUpperBound();
            }
        } else if (bndType.hasUpperBound()) {
            constraintExpression = var.getExpression() <= bndType.getUpperBound();
        }
    }
    return std::make_shared<storm::jani::Constant>(name, std::move(var), definingExpression, constraintExpression);
}

template<typename ValueType>
std::pair<std::unique_ptr<storm::jani::JaniType>, storm::expressions::Type> JaniParser<ValueType>::parseType(Json const& typeStructure,
                                                                                                             std::string variableName, Scope const& scope) {
    std::pair<std::unique_ptr<storm::jani::JaniType>, storm::expressions::Type> result;
    if (typeStructure.is_string()) {
        if (typeStructure == "real") {
            result.first = std::make_unique<storm::jani::BasicType>(storm::jani::BasicType::Type::Real);
            result.second = expressionManager->getRationalType();
        } else if (typeStructure == "bool") {
            result.first = std::make_unique<storm::jani::BasicType>(storm::jani::BasicType::Type::Bool);
            result.second = expressionManager->getBooleanType();
        } else if (typeStructure == "int") {
            result.first = std::make_unique<storm::jani::BasicType>(storm::jani::BasicType::Type::Int);
            result.second = expressionManager->getIntegerType();
        } else if (typeStructure == "clock") {
            result.first = std::make_unique<storm::jani::ClockType>();
            result.second = expressionManager->getRationalType();
        } else if (typeStructure == "continuous") {
            result.first = std::make_unique<storm::jani::ContinuousType>();
            result.second = expressionManager->getRationalType();
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                            "Unsupported type " << typeStructure.dump() << " for variable '" << variableName << "' (scope: " << scope.description << ").");
        }
    } else if (typeStructure.is_object()) {
        STORM_LOG_THROW(typeStructure.count("kind") == 1, storm::exceptions::InvalidJaniException,
                        "For complex type as in variable " << variableName << "(scope: " << scope.description << ") kind must be given.");
        std::string kind =
            getString<ValueType>(typeStructure.at("kind"), "kind for complex type as in variable " + variableName + "(scope: " + scope.description + ") ");
        if (kind == "bounded") {
            STORM_LOG_THROW(
                typeStructure.count("lower-bound") + typeStructure.count("upper-bound") > 0, storm::exceptions::InvalidJaniException,
                "For bounded type as in variable " << variableName << "(scope: " << scope.description << ") lower-bound or upper-bound must be given.");
            storm::expressions::Expression lowerboundExpr;
            if (typeStructure.count("lower-bound") > 0) {
                lowerboundExpr = parseExpression(typeStructure.at("lower-bound"), scope.refine("Lower bound for variable " + variableName));
            }
            storm::expressions::Expression upperboundExpr;
            if (typeStructure.count("upper-bound") > 0) {
                upperboundExpr = parseExpression(typeStructure.at("upper-bound"), scope.refine("Upper bound for variable " + variableName));
            }
            STORM_LOG_THROW(typeStructure.count("base") == 1, storm::exceptions::InvalidJaniException,
                            "For bounded type as in variable " << variableName << "(scope: " << scope.description << ") base must be given.");
            std::string basictype =
                getString<ValueType>(typeStructure.at("base"), "base for bounded type as in variable " + variableName + "(scope: " + scope.description + ") ");
            if (basictype == "int") {
                STORM_LOG_THROW(!lowerboundExpr.isInitialized() || lowerboundExpr.hasIntegerType(), storm::exceptions::InvalidJaniException,
                                "Lower bound for bounded integer variable " << variableName << "(scope: " << scope.description << ") must be integer-typed.");
                STORM_LOG_THROW(!upperboundExpr.isInitialized() || upperboundExpr.hasIntegerType(), storm::exceptions::InvalidJaniException,
                                "Upper bound for bounded integer variable " << variableName << "(scope: " << scope.description << ") must be integer-typed.");
                if (lowerboundExpr.isInitialized() && upperboundExpr.isInitialized() && !lowerboundExpr.containsVariables() &&
                    !upperboundExpr.containsVariables()) {
                    STORM_LOG_THROW(lowerboundExpr.evaluateAsInt() <= upperboundExpr.evaluateAsInt(), storm::exceptions::InvalidJaniException,
                                    "Lower bound must not be larger than upper bound for bounded integer variable " << variableName
                                                                                                                    << "(scope: " << scope.description << ").");
                }
                result.first = std::make_unique<storm::jani::BoundedType>(storm::jani::BoundedType::BaseType::Int, lowerboundExpr, upperboundExpr);
                result.second = expressionManager->getIntegerType();
            } else if (basictype == "real") {
                STORM_LOG_THROW(!lowerboundExpr.isInitialized() || lowerboundExpr.hasNumericalType(), storm::exceptions::InvalidJaniException,
                                "Lower bound for bounded real variable " << variableName << "(scope: " << scope.description << ") must be numeric.");
                STORM_LOG_THROW(!upperboundExpr.isInitialized() || upperboundExpr.hasNumericalType(), storm::exceptions::InvalidJaniException,
                                "Upper bound for bounded real variable " << variableName << "(scope: " << scope.description << ") must be numeric.");
                if (lowerboundExpr.isInitialized() && upperboundExpr.isInitialized() && !lowerboundExpr.containsVariables() &&
                    !upperboundExpr.containsVariables()) {
                    using SubMap = std::map<storm::expressions::Variable, storm::expressions::Expression>;
                    storm::expressions::JaniExpressionSubstitutionVisitor<SubMap> transcendentalVisitor(SubMap(), true);
                    const storm::RationalNumber lowerboundValue = transcendentalVisitor.substitute(lowerboundExpr).evaluateAsRational();
                    const storm::RationalNumber upperboundValue = transcendentalVisitor.substitute(upperboundExpr).evaluateAsRational();
                    STORM_LOG_THROW(lowerboundValue <= upperboundValue, storm::exceptions::InvalidJaniException,
                                    "Lower bound must not be larger than upper bound for bounded real variable " << variableName
                                                                                                                 << "(scope: " << scope.description << ").");
                }
                result.first = std::make_unique<storm::jani::BoundedType>(storm::jani::BoundedType::BaseType::Real, lowerboundExpr, upperboundExpr);
                result.second = expressionManager->getRationalType();
            } else {
                STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                                "Unsupported base " << basictype << " for bounded variable " << variableName << "(scope: " << scope.description << ").");
            }
        } else if (kind == "array") {
            STORM_LOG_THROW(typeStructure.count("base") == 1, storm::exceptions::InvalidJaniException,
                            "For array type as in variable " << variableName << "(scope: " << scope.description << ") base must be given.");
            auto base = parseType(typeStructure.at("base"), variableName, scope);
            result.first = std::make_unique<storm::jani::ArrayType>(std::move(base.first));
            result.second = expressionManager->getArrayType(base.second);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                            "Unsupported kind " << kind << " for complex type of variable " << variableName << "(scope: " << scope.description << ").");
        }
    }
    return result;
}

template<typename ValueType>
storm::jani::FunctionDefinition JaniParser<ValueType>::parseFunctionDefinition(Json const& functionDefinitionStructure, Scope const& scope, bool firstPass,
                                                                               std::string const& parameterNamePrefix) {
    STORM_LOG_THROW(functionDefinitionStructure.count("name") == 1, storm::exceptions::InvalidJaniException,
                    "Function definition (scope: " + scope.description + ") must have a name.");
    std::string functionName = getString<ValueType>(functionDefinitionStructure.at("name"), "function-name in " + scope.description);
    STORM_LOG_THROW(functionDefinitionStructure.count("type") == 1, storm::exceptions::InvalidJaniException,
                    "Function definition '" + functionName + "' (scope: " + scope.description + ") must have a (single) type-declaration.");
    auto type = parseType(functionDefinitionStructure.at("type"), functionName, scope);
    STORM_LOG_THROW(
        !type.first->isClockType() && !type.first->isContinuousType(), storm::exceptions::InvalidJaniException,
        "Function definition '" + functionName + "' (scope: " + scope.description + ") uses illegal type '" + type.first->getStringRepresentation() + "'.");

    std::unordered_map<std::string, storm::expressions::Variable> parameterNameToVariableMap;
    std::vector<storm::expressions::Variable> parameters;
    if (!firstPass && functionDefinitionStructure.count("parameters") > 0) {
        STORM_LOG_THROW(functionDefinitionStructure.count("parameters") == 1, storm::exceptions::InvalidJaniException,
                        "Function definition '" + functionName + "' (scope: " + scope.description + ") must have exactly one list of parameters.");
        for (auto const& parameterStructure : functionDefinitionStructure.at("parameters")) {
            STORM_LOG_THROW(parameterStructure.count("name") == 1, storm::exceptions::InvalidJaniException,
                            "Parameter declaration of parameter " + std::to_string(parameters.size()) + " of Function definition '" + functionName +
                                "' (scope: " + scope.description + ") must have a name.");
            std::string parameterName =
                getString<ValueType>(parameterStructure.at("name"), "parameter-name of parameter " + std::to_string(parameters.size()) +
                                                                        " of Function definition '" + functionName + "' (scope: " + scope.description + ").");
            STORM_LOG_THROW(parameterStructure.count("type") == 1, storm::exceptions::InvalidJaniException,
                            "Parameter declaration of parameter " + std::to_string(parameters.size()) + " of Function definition '" + functionName +
                                "' (scope: " + scope.description + ") must have exactly one type.");
            auto parameterType =
                parseType(parameterStructure.at("type"), parameterName,
                          scope.refine("parameter declaration of parameter " + std::to_string(parameters.size()) + " of Function definition " + functionName));
            STORM_LOG_THROW(!parameterType.first->isClockType() && !parameterType.first->isContinuousType(), storm::exceptions::InvalidJaniException,
                            "Type of parameter " + std::to_string(parameters.size()) + " of function definition '" + functionName +
                                "' (scope: " + scope.description + ") uses illegal type '" + parameterType.first->getStringRepresentation() + "'.");
            STORM_LOG_WARN_COND(!parameterType.first->isBoundedType(),
                                "Bounds on parameter" + parameterName + " of function definition " + functionName + " will be ignored.");

            std::string exprParameterName = parameterNamePrefix + functionName + VARIABLE_AUTOMATON_DELIMITER + parameterName;
            parameters.push_back(expressionManager->declareVariable(exprParameterName, parameterType.second));
            parameterNameToVariableMap.emplace(parameterName, parameters.back());
        }
    }

    STORM_LOG_THROW(functionDefinitionStructure.count("body") == 1, storm::exceptions::InvalidJaniException,
                    "Function definition '" + functionName + "' (scope: " + scope.description + ") must have a (single) body.");
    storm::expressions::Expression functionBody;
    if (!firstPass) {
        functionBody = parseExpression(functionDefinitionStructure.at("body"), scope.refine("body of function definition " + functionName), false,
                                       parameterNameToVariableMap);
        STORM_LOG_WARN_COND(functionBody.getType() == type.second || (functionBody.getType().isIntegerType() && type.second.isRationalType()),
                            "Type of body of function " + functionName + "' (scope: " + scope.description + ") has type "
                                << functionBody.getType() << " although the function type is given as " << type.second);
    }
    return storm::jani::FunctionDefinition(functionName, type.second, parameters, functionBody);
}

template<typename ValueType>
std::shared_ptr<storm::jani::Variable> JaniParser<ValueType>::parseVariable(Json const& variableStructure, Scope const& scope, std::string const& namePrefix) {
    STORM_LOG_THROW(variableStructure.count("name") == 1, storm::exceptions::InvalidJaniException,
                    "Variable (scope: " + scope.description + ") must have a name.");
    std::string name = getString<ValueType>(variableStructure.at("name"), "variable-name in " + scope.description + "-scope");
    // TODO check existance of name.
    // TODO store prefix in variable.
    std::string exprManagerName = namePrefix + name;
    bool transientVar = defaultVariableTransient;  // Default value for variables.
    uint_fast64_t tvarcount = variableStructure.count("transient");
    STORM_LOG_THROW(tvarcount <= 1, storm::exceptions::InvalidJaniException,
                    "Multiple definitions of transient not allowed in variable '" + name + "' (scope: " + scope.description + ").");
    if (tvarcount == 1) {
        transientVar =
            getBoolean<ValueType>(variableStructure.at("transient"), "transient-attribute in variable '" + name + "' (scope: " + scope.description + ").");
    }
    STORM_LOG_THROW(variableStructure.count("type") == 1, storm::exceptions::InvalidJaniException,
                    "Variable '" + name + "' (scope: " + scope.description + ") must have a (single) type-declaration.");
    auto type = parseType(variableStructure.at("type"), name, scope);

    uint_fast64_t initvalcount = variableStructure.count("initial-value");
    if (transientVar) {
        STORM_LOG_THROW(initvalcount == 1, storm::exceptions::InvalidJaniException,
                        "Initial value must be given once for transient variable '" + name + "' (scope: " + scope.description + ") " + name +
                            "' (scope: " + scope.description + ").");
    } else {
        STORM_LOG_THROW(initvalcount <= 1, storm::exceptions::InvalidJaniException,
                        "Initial value can be given at most one for variable " + name + "' (scope: " + scope.description + ").");
    }
    boost::optional<storm::expressions::Expression> initVal;
    if (initvalcount == 1 && !variableStructure.at("initial-value").is_null()) {
        initVal = parseExpression(variableStructure.at("initial-value"), scope.refine("Initial value for variable " + name));
        // STORM_LOG_THROW((type.second == initVal->getType() || type.second.isRationalType() && initVal->getType().isIntegerType()),
        // storm::exceptions::InvalidJaniException,"Type of initial value for variable " + name + "' (scope: " + scope.description + ") does not match the
        // variable type '" + type.first->getStringRepresentation() + "'.");
    } else {
        STORM_LOG_ASSERT(!transientVar, "Unexpected transient variable.");
    }

    if (transientVar && type.first->isBasicType() && type.first->asBasicType().isBooleanType()) {
        labels.insert(name);
    }

    auto expressionVariable = expressionManager->declareVariable(exprManagerName, type.second);
    return storm::jani::Variable::makeVariable(name, *type.first, expressionVariable, initVal, transientVar);
}

/**
 * Helper for parse expression.
 */
void ensureNumberOfArguments(uint64_t expected, uint64_t actual, std::string const& opstring, std::string const& errorInfo) {
    STORM_LOG_THROW(expected == actual, storm::exceptions::InvalidJaniException,
                    "Operator " << opstring << " expects " << expected << " arguments, but got " << actual << " in " << errorInfo << ".");
}

template<typename ValueType>
std::vector<storm::expressions::Expression> JaniParser<ValueType>::parseUnaryExpressionArguments(
    Json const& expressionDecl, std::string const& opstring, Scope const& scope, bool returnNoneInitializedOnUnknownOperator,
    std::unordered_map<std::string, storm::expressions::Variable> const& auxiliaryVariables) {
    storm::expressions::Expression left =
        parseExpression(expressionDecl.at("exp"), scope.refine("Argument of operator " + opstring), returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
    return {left};
}

template<typename ValueType>
std::vector<storm::expressions::Expression> JaniParser<ValueType>::parseBinaryExpressionArguments(
    Json const& expressionDecl, std::string const& opstring, Scope const& scope, bool returnNoneInitializedOnUnknownOperator,
    std::unordered_map<std::string, storm::expressions::Variable> const& auxiliaryVariables) {
    storm::expressions::Expression left = parseExpression(expressionDecl.at("left"), scope.refine("Left argument of operator " + opstring),
                                                          returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
    storm::expressions::Expression right = parseExpression(expressionDecl.at("right"), scope.refine("Right argument of operator " + opstring),
                                                           returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
    return {left, right};
}
/**
 * Helper for parse expression.
 */
void ensureBooleanType(storm::expressions::Expression const& expr, std::string const& opstring, unsigned argNr, std::string const& errorInfo) {
    STORM_LOG_THROW(expr.hasBooleanType(), storm::exceptions::InvalidJaniException,
                    "Operator " << opstring << " expects argument[" << argNr << "]: '" << expr << "' to be Boolean in " << errorInfo << ".");
}

/**
 * Helper for parse expression.
 */
void ensureNumericalType(storm::expressions::Expression const& expr, std::string const& opstring, unsigned argNr, std::string const& errorInfo) {
    STORM_LOG_THROW(expr.hasNumericalType(), storm::exceptions::InvalidJaniException,
                    "Operator " << opstring << " expects argument " + std::to_string(argNr) + " to be numerical in " << errorInfo << ".");
}

/**
 * Helper for parse expression.
 */
void ensureIntegerType(storm::expressions::Expression const& expr, std::string const& opstring, unsigned argNr, std::string const& errorInfo) {
    STORM_LOG_THROW(expr.hasIntegerType(), storm::exceptions::InvalidJaniException,
                    "Operator " << opstring << " expects argument " + std::to_string(argNr) + " to be numerical in " << errorInfo << ".");
}

/**
 * Helper for parse expression.
 */
void ensureArrayType(storm::expressions::Expression const& expr, std::string const& opstring, unsigned argNr, std::string const& errorInfo) {
    STORM_LOG_THROW(expr.getType().isArrayType(), storm::exceptions::InvalidJaniException,
                    "Operator " << opstring << " expects argument " + std::to_string(argNr) + " to be of type 'array' in " << errorInfo << ".");
}

template<typename ValueType>
storm::jani::LValue JaniParser<ValueType>::parseLValue(Json const& lValueStructure, Scope const& scope) {
    if (lValueStructure.is_string()) {
        std::string ident = getString<ValueType>(lValueStructure, scope.description);
        storm::jani::Variable const* var = nullptr;
        if (scope.localVars != nullptr) {
            auto localVar = scope.localVars->find(ident);
            if (localVar != scope.localVars->end()) {
                var = localVar->second;
            }
        }
        if (var == nullptr) {
            STORM_LOG_THROW(scope.globalVars != nullptr, storm::exceptions::InvalidJaniException,
                            "Unknown identifier '" << ident << "' occurs in " << scope.description << ".");
            auto globalVar = scope.globalVars->find(ident);
            STORM_LOG_THROW(globalVar != scope.globalVars->end(), storm::exceptions::InvalidJaniException,
                            "Unknown identifier '" << ident << "' occurs in " << scope.description << ".");
            var = globalVar->second;
        }

        return storm::jani::LValue(*var);
    } else if (lValueStructure.count("op") == 1) {
        // structure will be something like "op": "aa", "exp": {}, "index": {}
        // in exp we have something that is either a variable, or some other array access.
        // e.g. a[1][4] will look like: "op": "aa", "exp": {"op": "aa", "exp": "a", "index": {1}}, "index": {4}
        std::string opstring = getString<ValueType>(lValueStructure.at("op"), scope.description);
        STORM_LOG_THROW(opstring == "aa", storm::exceptions::InvalidJaniException,
                        "Unknown operation '" << opstring << "' occurs in " << scope.description << ".");
        STORM_LOG_THROW(lValueStructure.count("exp") == 1, storm::exceptions::InvalidJaniException,
                        "Missing 'exp' in array access at " << scope.description << ".");
        auto expLValue = parseLValue(lValueStructure.at("exp"), scope.refine("Expression of array access"));
        STORM_LOG_THROW(expLValue.isArray(), storm::exceptions::InvalidJaniException,
                        "Array access considers non-array expression at " << scope.description << ".");
        STORM_LOG_THROW(lValueStructure.count("index"), storm::exceptions::InvalidJaniException,
                        "Missing 'index' in array access at " << scope.description << ".");
        auto indexExpression = parseExpression(lValueStructure.at("index"), scope.refine("Index of array access"));
        expLValue.addArrayAccessIndex(indexExpression);
        return expLValue;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                        "Unknown LValue '" << lValueStructure.dump() << "' occurs in " << scope.description << ".");
        // Silly warning suppression.
        return storm::jani::LValue(*scope.globalVars->end()->second);
    }
}

template<typename ValueType>
storm::expressions::Variable JaniParser<ValueType>::getVariableOrConstantExpression(
    std::string const& ident, Scope const& scope, std::unordered_map<std::string, storm::expressions::Variable> const& auxiliaryVariables) {
    {
        auto it = auxiliaryVariables.find(ident);
        if (it != auxiliaryVariables.end()) {
            return it->second;
        }
    }
    if (scope.localVars != nullptr) {
        auto it = scope.localVars->find(ident);
        if (it != scope.localVars->end()) {
            return it->second->getExpressionVariable();
        }
    }
    if (scope.globalVars != nullptr) {
        auto it = scope.globalVars->find(ident);
        if (it != scope.globalVars->end()) {
            return it->second->getExpressionVariable();
        }
    }
    if (scope.constants != nullptr) {
        auto it = scope.constants->find(ident);
        if (it != scope.constants->end()) {
            return it->second->getExpressionVariable();
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException, "Unknown identifier '" << ident << "' occurs in " << scope.description << ".");
    // Silly warning suppression.
    return storm::expressions::Variable();
}

template<typename ValueType>
storm::expressions::Expression JaniParser<ValueType>::parseExpression(Json const& expressionStructure, Scope const& scope,
                                                                      bool returnNoneInitializedOnUnknownOperator,
                                                                      std::unordered_map<std::string, storm::expressions::Variable> const& auxiliaryVariables) {
    if (expressionStructure.is_boolean()) {
        if (expressionStructure.template get<bool>()) {
            return expressionManager->boolean(true);
        } else {
            return expressionManager->boolean(false);
        }
    } else if (expressionStructure.is_number_integer()) {
        return expressionManager->integer(expressionStructure.template get<int64_t>());
    } else if (expressionStructure.is_number_float()) {
        return expressionManager->rational(storm::utility::convertNumber<storm::RationalNumber>(expressionStructure.template get<ValueType>()));
    } else if (expressionStructure.is_string()) {
        std::string ident = expressionStructure.template get<std::string>();
        return storm::expressions::Expression(getVariableOrConstantExpression(ident, scope, auxiliaryVariables));
    } else if (expressionStructure.is_object()) {
        if (expressionStructure.count("distribution") == 1) {
            STORM_LOG_THROW(
                false, storm::exceptions::InvalidJaniException,
                "Distributions are not supported by storm expressions, cannot import " << expressionStructure.dump() << " in " << scope.description << ".");
        }
        if (expressionStructure.count("op") == 1) {
            std::string opstring = getString<ValueType>(expressionStructure.at("op"), scope.description);
            std::vector<storm::expressions::Expression> arguments = {};
            if (opstring == "ite") {
                STORM_LOG_THROW(expressionStructure.count("if") == 1, storm::exceptions::InvalidJaniException, "If operator required.");
                STORM_LOG_THROW(expressionStructure.count("else") == 1, storm::exceptions::InvalidJaniException, "Else operator required.");
                STORM_LOG_THROW(expressionStructure.count("then") == 1, storm::exceptions::InvalidJaniException, "Then operator required.");
                arguments.push_back(
                    parseExpression(expressionStructure.at("if"), scope.refine("if-formula"), returnNoneInitializedOnUnknownOperator, auxiliaryVariables));
                arguments.push_back(
                    parseExpression(expressionStructure.at("then"), scope.refine("then-formula"), returnNoneInitializedOnUnknownOperator, auxiliaryVariables));
                arguments.push_back(
                    parseExpression(expressionStructure.at("else"), scope.refine("else-formula"), returnNoneInitializedOnUnknownOperator, auxiliaryVariables));
                ensureNumberOfArguments(3, arguments.size(), opstring, scope.description);
                STORM_LOG_ASSERT(arguments.size() == 3, "Expected three arguments.");
                ensureBooleanType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::ite(arguments[0], arguments[1], arguments[2]);
            } else if (opstring == "∨") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureBooleanType(arguments[0], opstring, 0, scope.description);
                ensureBooleanType(arguments[1], opstring, 1, scope.description);
                return arguments[0] || arguments[1];
            } else if (opstring == "∧") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureBooleanType(arguments[0], opstring, 0, scope.description);
                ensureBooleanType(arguments[1], opstring, 1, scope.description);
                return arguments[0] && arguments[1];
            } else if (opstring == "⇒") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureBooleanType(arguments[0], opstring, 0, scope.description);
                ensureBooleanType(arguments[1], opstring, 1, scope.description);
                return (!arguments[0]) || arguments[1];
            } else if (opstring == "¬") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                if (!arguments[0].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureBooleanType(arguments[0], opstring, 0, scope.description);
                return !arguments[0];
            } else if (opstring == "=") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                if (arguments[0].hasBooleanType()) {
                    ensureBooleanType(arguments[1], opstring, 1, scope.description);
                    return storm::expressions::iff(arguments[0], arguments[1]);
                } else {
                    ensureNumericalType(arguments[1], opstring, 1, scope.description);
                    return arguments[0] == arguments[1];
                }
            } else if (opstring == "≠") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                if (arguments[0].hasBooleanType()) {
                    ensureBooleanType(arguments[1], opstring, 1, scope.description);
                    return storm::expressions::xclusiveor(arguments[0], arguments[1]);
                } else {
                    ensureNumericalType(arguments[1], opstring, 1, scope.description);
                    return arguments[0] != arguments[1];
                }
            } else if (opstring == "<") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] < arguments[1];
            } else if (opstring == "≤") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] <= arguments[1];
            } else if (opstring == ">") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] > arguments[1];
            } else if (opstring == "≥") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                if (!arguments[0].isInitialized() || !arguments[1].isInitialized()) {
                    return storm::expressions::Expression();
                }
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] >= arguments[1];
            } else if (opstring == "+") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] + arguments[1];
            } else if (opstring == "-" && expressionStructure.count("left") > 0) {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] - arguments[1];
            } else if (opstring == "-") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return -arguments[0];
            } else if (opstring == "*") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] * arguments[1];
            } else if (opstring == "/") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] / arguments[1];
            } else if (opstring == "%") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return arguments[0] % arguments[1];
            } else if (opstring == "max") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return storm::expressions::maximum(arguments[0], arguments[1]);
            } else if (opstring == "min") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return storm::expressions::minimum(arguments[0], arguments[1]);
            } else if (opstring == "floor") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::floor(arguments[0]);
            } else if (opstring == "ceil") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::ceil(arguments[0]);
            } else if (opstring == "abs") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::abs(arguments[0]);
            } else if (opstring == "sgn") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::sign(arguments[0]);
            } else if (opstring == "trc") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::truncate(arguments[0]);
            } else if (opstring == "pow") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return storm::expressions::pow(arguments[0], arguments[1]);
            } else if (opstring == "exp") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                // TODO implement
                STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Exp operation is not yet implemented.");
            } else if (opstring == "log") {
                arguments = parseBinaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 2, "Expected two arguments.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                ensureNumericalType(arguments[1], opstring, 1, scope.description);
                return storm::expressions::logarithm(arguments[0], arguments[1]);
            } else if (opstring == "cos") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::cos(arguments[0]);
            } else if (opstring == "sin") {
                arguments = parseUnaryExpressionArguments(expressionStructure, opstring, scope, returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_ASSERT(arguments.size() == 1, "Expected one argument.");
                ensureNumericalType(arguments[0], opstring, 0, scope.description);
                return storm::expressions::sin(arguments[0]);
            } else if (opstring == "aa") {
                STORM_LOG_THROW(expressionStructure.count("exp") == 1, storm::exceptions::InvalidJaniException,
                                "Array access operator requires exactly one exp (at " + scope.description + ").");
                storm::expressions::Expression exp = parseExpression(expressionStructure.at("exp"), scope.refine("'exp' of array access operator"),
                                                                     returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                STORM_LOG_THROW(expressionStructure.count("index") == 1, storm::exceptions::InvalidJaniException,
                                "Array access operator requires exactly one index (at " + scope.description + ").");
                storm::expressions::Expression index = parseExpression(expressionStructure.at("index"), scope.refine("index of array access operator"),
                                                                       returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                ensureArrayType(exp, opstring, 0, scope.description);
                ensureIntegerType(index, opstring, 1, scope.description);
                return std::make_shared<storm::expressions::ArrayAccessExpression>(exp.getManager(), exp.getType().getElementType(),
                                                                                   exp.getBaseExpressionPointer(), index.getBaseExpressionPointer())
                    ->toExpression();
            } else if (opstring == "av") {
                STORM_LOG_THROW(expressionStructure.count("elements") == 1, storm::exceptions::InvalidJaniException,
                                "Array value operator requires exactly one 'elements' (at " + scope.description + ").");
                std::vector<std::shared_ptr<storm::expressions::BaseExpression const>> elements;
                storm::expressions::Type commonType;
                bool first = true;
                for (auto const& element : expressionStructure.at("elements")) {
                    elements.push_back(parseExpression(element, scope.refine("element " + std::to_string(elements.size()) + " of array value expression"),
                                                       returnNoneInitializedOnUnknownOperator, auxiliaryVariables)
                                           .getBaseExpressionPointer());
                    if (first) {
                        commonType = elements.back()->getType();
                        first = false;
                    } else if (commonType != elements.back()->getType()) {
                        if (commonType.isIntegerType() && elements.back()->getType().isRationalType()) {
                            commonType = elements.back()->getType();
                        } else {
                            STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                                            "Incompatible element types " << commonType << " and " << elements.back()->getType()
                                                                          << " of array value expression at " << scope.description << ".");
                        }
                    }
                }
                return std::make_shared<storm::expressions::ValueArrayExpression>(*expressionManager, expressionManager->getArrayType(commonType), elements)
                    ->toExpression();
            } else if (opstring == "ac") {
                STORM_LOG_THROW(expressionStructure.count("length") == 1, storm::exceptions::InvalidJaniException,
                                "Array access operator requires exactly one length (at " + scope.description + ").");
                storm::expressions::Expression length = parseExpression(expressionStructure.at("length"), scope.refine("index of array constructor expression"),
                                                                        returnNoneInitializedOnUnknownOperator, auxiliaryVariables);
                ensureIntegerType(length, opstring, 1, scope.description);
                STORM_LOG_THROW(expressionStructure.count("var") == 1, storm::exceptions::InvalidJaniException,
                                "Array access operator requires exactly one var (at " + scope.description + ").");
                std::string indexVarName =
                    getString<ValueType>(expressionStructure.at("var"), "Field 'var' of Array access operator (at " + scope.description + ").");
                STORM_LOG_THROW(auxiliaryVariables.find(indexVarName) == auxiliaryVariables.end(), storm::exceptions::InvalidJaniException,
                                "Index variable " << indexVarName << " is already defined as an auxiliary variable (at " + scope.description + ").");
                auto newAuxVars = auxiliaryVariables;
                storm::expressions::Variable indexVar = expressionManager->declareFreshIntegerVariable(false, "ac_" + indexVarName);
                newAuxVars.emplace(indexVarName, indexVar);
                STORM_LOG_THROW(expressionStructure.count("exp") == 1, storm::exceptions::InvalidJaniException,
                                "Array constructor operator requires exactly one exp (at " + scope.description + ").");
                storm::expressions::Expression exp = parseExpression(expressionStructure.at("exp"), scope.refine("exp of array constructor"),
                                                                     returnNoneInitializedOnUnknownOperator, newAuxVars);
                return std::make_shared<storm::expressions::ConstructorArrayExpression>(*expressionManager, expressionManager->getArrayType(exp.getType()),
                                                                                        length.getBaseExpressionPointer(), indexVar,
                                                                                        exp.getBaseExpressionPointer())
                    ->toExpression();
            } else if (opstring == "call") {
                STORM_LOG_THROW(expressionStructure.count("function") == 1, storm::exceptions::InvalidJaniException,
                                "Function call operator requires exactly one function (at " + scope.description + ").");
                std::string functionName =
                    getString<ValueType>(expressionStructure.at("function"), "in function call operator (at " + scope.description + ").");
                storm::jani::FunctionDefinition const* functionDefinition;
                if (scope.localFunctions != nullptr && scope.localFunctions->count(functionName) > 0) {
                    functionDefinition = scope.localFunctions->at(functionName);
                } else if (scope.globalFunctions != nullptr && scope.globalFunctions->count(functionName) > 0) {
                    functionDefinition = scope.globalFunctions->at(functionName);
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                                    "Function call operator calls unknown function '" + functionName + "' (at " + scope.description + ").");
                }
                STORM_LOG_THROW(expressionStructure.count("args") == 1, storm::exceptions::InvalidJaniException,
                                "Function call operator requires exactly one args (at " + scope.description + ").");
                std::vector<std::shared_ptr<storm::expressions::BaseExpression const>> args;
                if (expressionStructure.count("args") > 0) {
                    STORM_LOG_THROW(expressionStructure.count("args") == 1, storm::exceptions::InvalidJaniException,
                                    "Function call operator requires exactly one args (at " + scope.description + ").");
                    for (auto const& arg : expressionStructure.at("args")) {
                        args.push_back(parseExpression(arg, scope.refine("argument " + std::to_string(args.size()) + " of function call expression"),
                                                       returnNoneInitializedOnUnknownOperator, auxiliaryVariables)
                                           .getBaseExpressionPointer());
                    }
                }
                return std::make_shared<storm::expressions::FunctionCallExpression>(*expressionManager, functionDefinition->getType(), functionName, args)
                    ->toExpression();
            } else if (unsupportedOpstrings.count(opstring) > 0) {
                STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException, "Opstring " + opstring + " is not supported by storm.");
            } else {
                if (returnNoneInitializedOnUnknownOperator) {
                    return storm::expressions::Expression();
                }
                STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException, "Unknown operator " << opstring << " in " << scope.description << ".");
            }
        }
        if (expressionStructure.count("constant") == 1) {
            // Convert constants to a numeric value (only PI and Euler number, as in Jani specification)
            const std::string constantStr = getString<ValueType>(expressionStructure.at("constant"), scope.description);
            if (constantStr == "π") {
                return std::make_shared<storm::expressions::TranscendentalNumberLiteralExpression>(
                           *expressionManager, storm::expressions::TranscendentalNumberLiteralExpression::TranscendentalNumber::PI)
                    ->toExpression();
            }
            if (constantStr == "e") {
                return std::make_shared<storm::expressions::TranscendentalNumberLiteralExpression>(
                           *expressionManager, storm::expressions::TranscendentalNumberLiteralExpression::TranscendentalNumber::E)
                    ->toExpression();
            }
        }
        STORM_LOG_THROW(
            false, storm::exceptions::InvalidJaniException,
            "No supported operator declaration found for complex expressions as " << expressionStructure.dump() << " in " << scope.description << ".");
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                    "No supported expression found at " << expressionStructure.dump() << " in " << scope.description << ".");
    // Silly warning suppression.
    return storm::expressions::Expression();
}

template<typename ValueType>
void JaniParser<ValueType>::parseActions(Json const& actionStructure, storm::jani::Model& parentModel) {
    std::set<std::string> actionNames;
    for (auto const& actionEntry : actionStructure) {
        STORM_LOG_THROW(actionEntry.count("name") == 1, storm::exceptions::InvalidJaniException, "Actions must have exactly one name.");
        std::string actionName = getString<ValueType>(actionEntry.at("name"), "name of action");
        STORM_LOG_THROW(actionNames.count(actionName) == 0, storm::exceptions::InvalidJaniException, "Action with name " + actionName + " already exists.");
        parentModel.addAction(storm::jani::Action(actionName));
        actionNames.emplace(actionName);
    }
}

template<typename ValueType>
storm::jani::Automaton JaniParser<ValueType>::parseAutomaton(Json const& automatonStructure, storm::jani::Model const& parentModel, Scope const& globalScope) {
    STORM_LOG_THROW(automatonStructure.count("name") == 1, storm::exceptions::InvalidJaniException, "Each automaton must have a name.");
    std::string name = getString<ValueType>(automatonStructure.at("name"), " the name field for automaton");
    Scope scope = globalScope.refine(name);
    storm::jani::Automaton automaton(name, expressionManager->declareIntegerVariable("_loc_" + name));

    uint64_t varDeclCount = automatonStructure.count("variables");
    STORM_LOG_THROW(varDeclCount < 2, storm::exceptions::InvalidJaniException, "Automaton '" << name << "' has more than one list of variables.");
    VariablesMap localVars;
    scope.localVars = &localVars;
    if (varDeclCount > 0) {
        for (auto const& varStructure : automatonStructure.at("variables")) {
            std::shared_ptr<storm::jani::Variable> var = parseVariable(
                varStructure, scope.refine("variables[" + std::to_string(localVars.size()) + "] of automaton " + name), name + VARIABLE_AUTOMATON_DELIMITER);
            STORM_LOG_ASSERT(localVars.count(var->getName()) == 0, "Local variable already exists.");
            localVars.emplace(var->getName(), &automaton.addVariable(*var));
        }
    }

    uint64_t funDeclCount = automatonStructure.count("functions");
    STORM_LOG_THROW(funDeclCount < 2, storm::exceptions::InvalidJaniException, "Automaton '" << name << "' has more than one list of functions.");
    FunctionsMap localFuns;
    scope.localFunctions = &localFuns;
    if (funDeclCount > 0) {
        // We require two passes through the function definitions array to allow referring to functions before they were defined.
        std::vector<storm::jani::FunctionDefinition> dummyFunctionDefinitions;
        for (auto const& funStructure : automatonStructure.at("functions")) {
            // Skip parsing of function body
            dummyFunctionDefinitions.push_back(
                parseFunctionDefinition(funStructure, scope.refine("functions[" + std::to_string(localFuns.size()) + "] of automaton " + name), true));
        }
        // Store references to the dummy function definitions. This needs to happen in a separate loop since otherwise, references to FunDefs can be invalidated
        // after calling dummyFunctionDefinitions.push_back
        for (auto const& funDef : dummyFunctionDefinitions) {
            bool unused = localFuns.emplace(funDef.getName(), &funDef).second;
            STORM_LOG_THROW(unused, storm::exceptions::InvalidJaniException,
                            "Multiple definitions of functions with the name " << funDef.getName() << " in " << scope.description << ".");
        }
        for (auto const& funStructure : automatonStructure.at("functions")) {
            // Actually parse the function body
            storm::jani::FunctionDefinition funDef =
                parseFunctionDefinition(funStructure, scope.refine("functions[" + std::to_string(localFuns.size()) + "] of automaton " + name), false,
                                        name + VARIABLE_AUTOMATON_DELIMITER);
            STORM_LOG_ASSERT(localFuns.count(funDef.getName()) == 1, "Local function not found.");
            localFuns[funDef.getName()] = &automaton.addFunctionDefinition(funDef);
        }
    }

    STORM_LOG_THROW(automatonStructure.count("locations") > 0, storm::exceptions::InvalidJaniException, "Automaton '" << name << "' does not have locations.");
    std::unordered_map<std::string, uint64_t> locIds;
    for (auto const& locEntry : automatonStructure.at("locations")) {
        STORM_LOG_THROW(locEntry.count("name") == 1, storm::exceptions::InvalidJaniException,
                        "Locations for automaton '" << name << "' must have exactly one name.");
        std::string locName = getString<ValueType>(locEntry.at("name"), "location of automaton " + name);
        STORM_LOG_THROW(locIds.count(locName) == 0, storm::exceptions::InvalidJaniException,
                        "Location with name '" + locName + "' already exists in automaton '" + name + "'.");
        STORM_LOG_THROW(locEntry.count("invariant") == 0, storm::exceptions::InvalidJaniException,
                        "Invariants in locations as in '" + locName + "' in automaton '" + name + "' are not supported.");
        // STORM_LOG_THROW(locEntry.count("invariant") > 0 && !supportsInvariants(parentModel.getModelType()), storm::exceptions::InvalidJaniException,
        // "Invariants are not supported in the model type " + to_string(parentModel.getModelType()));
        std::vector<storm::jani::Assignment> transientAssignments;
        if (locEntry.count("transient-values") > 0) {
            for (auto const& transientValueEntry : locEntry.at("transient-values")) {
                STORM_LOG_THROW(transientValueEntry.count("ref") == 1, storm::exceptions::InvalidJaniException,
                                "Transient values in location " << locName << " need exactly one ref that is assigned to.");
                STORM_LOG_THROW(transientValueEntry.count("value") == 1, storm::exceptions::InvalidJaniException,
                                "Transient values in location " << locName << " need exactly one assigned value.");
                storm::jani::LValue lValue = parseLValue(transientValueEntry.at("ref"), scope.refine("LHS of assignment in location " + locName));
                STORM_LOG_THROW(lValue.isTransient(), storm::exceptions::InvalidJaniException,
                                "Assigned non-transient variable " << lValue << " in location " + locName + " (automaton: '" + name + "').");
                storm::expressions::Expression rhs =
                    parseExpression(transientValueEntry.at("value"), scope.refine("Assignment of lValue in location " + locName));
                transientAssignments.emplace_back(lValue, rhs);
            }
        }
        uint64_t id = automaton.addLocation(storm::jani::Location(locName, transientAssignments));
        locIds.emplace(locName, id);
    }
    STORM_LOG_THROW(automatonStructure.count("initial-locations") == 1, storm::exceptions::InvalidJaniException,
                    "Automaton '" << name << "' does not have initial locations.");
    for (Json const& initLocStruct : automatonStructure.at("initial-locations")) {
        automaton.addInitialLocation(getString<ValueType>(initLocStruct, "Initial locations for automaton '" + name + "'."));
    }
    STORM_LOG_THROW(automatonStructure.count("restrict-initial") < 2, storm::exceptions::InvalidJaniException,
                    "Automaton '" << name << "' has multiple initial value restrictions.");
    storm::expressions::Expression initialValueRestriction = expressionManager->boolean(true);
    if (automatonStructure.count("restrict-initial") > 0) {
        STORM_LOG_THROW(automatonStructure.at("restrict-initial").count("exp") == 1, storm::exceptions::InvalidJaniException,
                        "Automaton '" << name << "' needs an expression inside the initial restricion.");
        initialValueRestriction = parseExpression(automatonStructure.at("restrict-initial").at("exp"), scope.refine("Initial value restriction"));
    }
    automaton.setInitialStatesRestriction(initialValueRestriction);

    STORM_LOG_THROW(automatonStructure.count("edges") > 0, storm::exceptions::InvalidJaniException, "Automaton '" << name << "' must have a list of edges.");
    for (auto const& edgeEntry : automatonStructure.at("edges")) {
        // source location
        STORM_LOG_THROW(edgeEntry.count("location") == 1, storm::exceptions::InvalidJaniException,
                        "Each edge in automaton '" << name << "' must have a source.");
        std::string sourceLoc = getString<ValueType>(edgeEntry.at("location"), "source location for edge in automaton '" + name + "'");
        STORM_LOG_THROW(locIds.count(sourceLoc) == 1, storm::exceptions::InvalidJaniException,
                        "Source of edge has unknown location '" << sourceLoc << "' in automaton '" << name << "'.");
        // action
        STORM_LOG_THROW(edgeEntry.count("action") < 2, storm::exceptions::InvalidJaniException,
                        "Edge from " << sourceLoc << " in automaton " << name << " has multiple actions.");
        std::string action = storm::jani::Model::SILENT_ACTION_NAME;  // def is tau
        if (edgeEntry.count("action") > 0) {
            action = getString<ValueType>(edgeEntry.at("action"), "action name in edge from '" + sourceLoc + "' in automaton '" + name + "'");
            // TODO check if action is known
            STORM_LOG_ASSERT(action != "", "Action is empty.");
        }
        // rate
        STORM_LOG_THROW(edgeEntry.count("rate") < 2, storm::exceptions::InvalidJaniException,
                        "Edge from '" << sourceLoc << "' in automaton '" << name << "' has multiple rates.");
        storm::expressions::Expression rateExpr;
        if (edgeEntry.count("rate") > 0) {
            STORM_LOG_THROW(edgeEntry.at("rate").count("exp") == 1, storm::exceptions::InvalidJaniException,
                            "Rate in edge from '" << sourceLoc << "' in automaton '" << name << "' must have a defing expression.");
            rateExpr = parseExpression(edgeEntry.at("rate").at("exp"), scope.refine("rate expression in edge from '" + sourceLoc));
            STORM_LOG_THROW(rateExpr.hasNumericalType(), storm::exceptions::InvalidJaniException, "Rate '" << rateExpr << "' has not a numerical type.");
            STORM_LOG_THROW(rateExpr.containsVariables() || rateExpr.evaluateAsRational() > storm::utility::zero<storm::RationalNumber>(),
                            storm::exceptions::InvalidJaniException, "Only positive rates are allowed but rate '" << rateExpr << " was found.");
        }
        // guard
        STORM_LOG_THROW(edgeEntry.count("guard") <= 1, storm::exceptions::InvalidJaniException,
                        "Guard can be given at most once in edge from '" << sourceLoc << "' in automaton '" << name << "'.");
        storm::expressions::Expression guardExpr = expressionManager->boolean(true);
        if (edgeEntry.count("guard") == 1) {
            STORM_LOG_THROW(edgeEntry.at("guard").count("exp") == 1, storm::exceptions::InvalidJaniException,
                            "Guard in edge from '" + sourceLoc + "' in automaton '" + name + "' must have one expression.");
            guardExpr = parseExpression(edgeEntry.at("guard").at("exp"), scope.refine("guard expression in edge from '" + sourceLoc));
            STORM_LOG_THROW(guardExpr.hasBooleanType(), storm::exceptions::InvalidJaniException, "Guard " << guardExpr << " does not have Boolean type.");
        }
        STORM_LOG_ASSERT(guardExpr.isInitialized(), "Guard expression not initialized.");
        std::shared_ptr<storm::jani::TemplateEdge> templateEdge = std::make_shared<storm::jani::TemplateEdge>(guardExpr);

        // edge assignments
        if (edgeEntry.count("assignments") > 0) {
            STORM_LOG_THROW(edgeEntry.count("assignments") == 1, storm::exceptions::InvalidJaniException,
                            "Multiple edge assignments in edge from '" + sourceLoc + "' in automaton '" + name + "'.");
            for (auto const& assignmentEntry : edgeEntry.at("assignments")) {
                // ref
                STORM_LOG_THROW(assignmentEntry.count("ref") == 1, storm::exceptions::InvalidJaniException,
                                "Assignment in edge from '" << sourceLoc << "' in automaton '" << name << "'must have one ref field.");
                storm::jani::LValue lValue =
                    parseLValue(assignmentEntry.at("ref"), scope.refine("Assignment variable in edge from '" + sourceLoc + "' in automaton '" + name + "'"));
                // value
                STORM_LOG_THROW(assignmentEntry.count("value") == 1, storm::exceptions::InvalidJaniException,
                                "Assignment in edge from '" << sourceLoc << "' in automaton '" << name << "' must have one value field.");
                storm::expressions::Expression assignmentExpr =
                    parseExpression(assignmentEntry.at("value"), scope.refine("assignment in edge from '" + sourceLoc + "' in automaton '" + name + "'"));
                // TODO check types
                // index
                int64_t assignmentIndex = 0;  // default.
                if (assignmentEntry.count("index") > 0) {
                    assignmentIndex =
                        getSignedInt<ValueType>(assignmentEntry.at("index"), "assignment index in edge from '" + sourceLoc + "' in automaton '" + name + "'");
                }
                templateEdge->getAssignments().add(storm::jani::Assignment(lValue, assignmentExpr, assignmentIndex));
            }
        }

        // destinations
        STORM_LOG_THROW(edgeEntry.count("destinations") == 1, storm::exceptions::InvalidJaniException,
                        "A single list of destinations must be given in edge from '" << sourceLoc << "' in automaton '" << name << "'.");
        std::vector<std::pair<uint64_t, storm::expressions::Expression>> destinationLocationsAndProbabilities;
        for (auto const& destEntry : edgeEntry.at("destinations")) {
            // target location
            STORM_LOG_THROW(destEntry.count("location") == 1, storm::exceptions::InvalidJaniException,
                            "Each destination in edge from '" << sourceLoc << "' in automaton '" << name << "' must have a target location.");
            std::string targetLoc =
                getString<ValueType>(destEntry.at("location"), "target location for edge from '" + sourceLoc + "' in automaton '" + name + "'");
            STORM_LOG_THROW(locIds.count(targetLoc) == 1, storm::exceptions::InvalidJaniException,
                            "Target of edge has unknown location '" << targetLoc << "' in automaton '" << name << "'.");
            // probability
            storm::expressions::Expression probExpr;
            unsigned probDeclCount = destEntry.count("probability");
            STORM_LOG_THROW(probDeclCount < 2, storm::exceptions::InvalidJaniException,
                            "Destination in edge from '" << sourceLoc << "' to '" << targetLoc << "' in automaton '" << name << "' has multiple probabilites.");
            if (probDeclCount == 0) {
                probExpr = expressionManager->rational(1.0);
            } else {
                STORM_LOG_THROW(destEntry.at("probability").count("exp") == 1, storm::exceptions::InvalidJaniException,
                                "Destination in edge from '" << sourceLoc << "' to '" << targetLoc << "' in automaton '" << name
                                                             << "' must have a probability expression.");
                probExpr = parseExpression(destEntry.at("probability").at("exp"), scope.refine("probability expression in edge from '" + sourceLoc + "' to '" +
                                                                                               targetLoc + "' in automaton '" + name + "'"));
            }
            STORM_LOG_ASSERT(probExpr.isInitialized(), "Probability expression not initialized.");
            STORM_LOG_THROW(probExpr.hasNumericalType(), storm::exceptions::InvalidJaniException,
                            "Probability expression " << probExpr << " does not have a numerical type.");
            // assignments
            std::vector<storm::jani::Assignment> assignments;
            unsigned assignmentDeclCount = destEntry.count("assignments");
            STORM_LOG_THROW(
                assignmentDeclCount < 2, storm::exceptions::InvalidJaniException,
                "Destination in edge from '" << sourceLoc << "' to '" << targetLoc << "' in automaton '" << name << "' has multiple assignment lists.");
            if (assignmentDeclCount > 0) {
                for (auto const& assignmentEntry : destEntry.at("assignments")) {
                    // ref
                    STORM_LOG_THROW(
                        assignmentEntry.count("ref") == 1, storm::exceptions::InvalidJaniException,
                        "Assignment in edge from '" << sourceLoc << "' to '" << targetLoc << "' in automaton '" << name << "' must have one ref field.");
                    storm::jani::LValue lValue = parseLValue(assignmentEntry.at("ref"), scope.refine("Assignment variable in edge from '" + sourceLoc +
                                                                                                     "' to '" + targetLoc + "' in automaton '" + name + "'"));
                    // value
                    STORM_LOG_THROW(
                        assignmentEntry.count("value") == 1, storm::exceptions::InvalidJaniException,
                        "Assignment in edge from '" << sourceLoc << "' to '" << targetLoc << "' in automaton '" << name << "' must have one value field.");
                    storm::expressions::Expression assignmentExpr =
                        parseExpression(assignmentEntry.at("value"),
                                        scope.refine("assignment in edge from '" + sourceLoc + "' to '" + targetLoc + "' in automaton '" + name + "'"));
                    // TODO check types
                    // index
                    int64_t assignmentIndex = 0;  // default.
                    if (assignmentEntry.count("index") > 0) {
                        assignmentIndex = getSignedInt<ValueType>(assignmentEntry.at("index"), "assignment index in edge from '" + sourceLoc + "' to '" +
                                                                                                   targetLoc + "' in automaton '" + name + "'");
                    }
                    assignments.emplace_back(lValue, assignmentExpr, assignmentIndex);
                }
            }
            destinationLocationsAndProbabilities.emplace_back(locIds.at(targetLoc), probExpr);
            templateEdge->addDestination(storm::jani::TemplateEdgeDestination(assignments));
        }
        automaton.addEdge(storm::jani::Edge(locIds.at(sourceLoc), parentModel.getActionIndex(action),
                                            rateExpr.isInitialized() ? boost::optional<storm::expressions::Expression>(rateExpr) : boost::none, templateEdge,
                                            destinationLocationsAndProbabilities));
    }

    return automaton;
}

template<typename ValueType>
std::vector<storm::jani::SynchronizationVector> parseSyncVectors(typename JaniParser<ValueType>::Json const& syncVectorStructure) {
    std::vector<storm::jani::SynchronizationVector> syncVectors;
    // TODO add error checks
    for (auto const& syncEntry : syncVectorStructure) {
        std::vector<std::string> inputs;
        for (auto const& syncInput : syncEntry.at("synchronise")) {
            if (syncInput.is_null()) {
                inputs.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
            } else {
                inputs.push_back(syncInput);
            }
        }
        std::string syncResult;
        if (syncEntry.count("result")) {
            syncResult = syncEntry.at("result");
        } else {
            syncResult = storm::jani::Model::SILENT_ACTION_NAME;
        }
        syncVectors.emplace_back(inputs, syncResult);
    }
    return syncVectors;
}

template<typename ValueType>
std::shared_ptr<storm::jani::Composition> JaniParser<ValueType>::parseComposition(Json const& compositionStructure) {
    if (compositionStructure.count("automaton")) {
        std::set<std::string> inputEnabledActions;
        if (compositionStructure.count("input-enable")) {
            for (auto const& actionDecl : compositionStructure.at("input-enable")) {
                inputEnabledActions.insert(actionDecl.template get<std::string>());
            }
        }
        return std::shared_ptr<storm::jani::AutomatonComposition>(
            new storm::jani::AutomatonComposition(compositionStructure.at("automaton").template get<std::string>(), inputEnabledActions));
    }

    STORM_LOG_THROW(compositionStructure.count("elements") == 1, storm::exceptions::InvalidJaniException,
                    "Elements of a composition must be given, got " << compositionStructure.dump() << ".");

    if (compositionStructure.at("elements").size() == 1 && compositionStructure.count("syncs") == 0) {
        // We might have an automaton.
        STORM_LOG_THROW(compositionStructure.at("elements").back().count("automaton") == 1, storm::exceptions::InvalidJaniException,
                        "Automaton must be given in composition.");
        if (compositionStructure.at("elements").back().at("automaton").is_string()) {
            std::string name = compositionStructure.at("elements").back().at("automaton");
            // TODO check whether name exist?
            return std::shared_ptr<storm::jani::AutomatonComposition>(new storm::jani::AutomatonComposition(name));
        }
        STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException, "Trivial nesting parallel composition is not yet supported.");
    }

    std::vector<std::shared_ptr<storm::jani::Composition>> compositions;
    for (auto const& elemDecl : compositionStructure.at("elements")) {
        if (!allowRecursion) {
            STORM_LOG_THROW(elemDecl.count("automaton") == 1, storm::exceptions::InvalidJaniException, "Automaton must be given in the element.");
        }
        compositions.push_back(parseComposition(elemDecl));
    }

    STORM_LOG_THROW(compositionStructure.count("syncs") < 2, storm::exceptions::InvalidJaniException, "Sync vectors can be given at most once.");
    std::vector<storm::jani::SynchronizationVector> syncVectors;
    if (compositionStructure.count("syncs") > 0) {
        syncVectors = parseSyncVectors<ValueType>(compositionStructure.at("syncs"));
    }

    return std::shared_ptr<storm::jani::Composition>(new storm::jani::ParallelComposition(compositions, syncVectors));
}

template class JaniParser<double>;
template class JaniParser<storm::RationalNumber>;
}  // namespace parser
}  // namespace storm
