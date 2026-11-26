#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/JaniParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/expressions/Expressions.h"
#include "storm/expressions/SimpleValuation.h"
#include "storm/languages/jani/Model.h"
#include "storm/languages/jani/Property.h"
#include "storm/languages/jani/eliminator/FunctionEliminator.h"
#include "storm/languages/jani/expressions/JaniExpressions.h"
#include "storm/languages/jani/traverser/FunctionCallExpressionFinder.h"
#include "storm/languages/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/solver.h"
#include "test/SmtSolverTestUtils.h"

namespace {

class JaniModelFlattenTest : public ::testing::TestWithParam<storm::solver::SmtSolverType> {
   protected:
    void flattenAndCheck(std::string const& path, uint64_t expectedNumberOfEdges) const {
        storm::prism::Program program;
        ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(path));
        storm::jani::Model janiModel = program.toJani();

        auto smtSolverFactory = storm::test::smtSolverFactory(GetParam());
        janiModel.substituteFunctions();
        ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
        EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
        EXPECT_EQ(expectedNumberOfEdges, janiModel.getAutomaton(0).getNumberOfEdges());
    }
};

}  // namespace

TEST_P(JaniModelFlattenTest, Leader) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm", 74ull);
}

TEST_P(JaniModelFlattenTest, Wlan) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm", 179ull);
}

TEST_P(JaniModelFlattenTest, Csma) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/csma2_2.nm", 70ull);
}

STORM_EXPENSIVE_TEST_P(JaniModelFlattenTest, Firewire) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/firewire.nm", 5024ull);
}

TEST_P(JaniModelFlattenTest, Coin) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/coin2.nm", 13ull);
}

TEST_P(JaniModelFlattenTest, Dice) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", 16ull);
}

INSTANTIATE_TEST_SUITE_P(SmtSolvers, JaniModelFlattenTest, ::testing::ValuesIn(storm::test::smtSolversUnderTest()), storm::test::smtSolverName);
// The suite is empty if no SMT solver was compiled in.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(JaniModelFlattenTest);

namespace {
storm::expressions::Expression functionCall(storm::expressions::ExpressionManager const& manager, std::string const& name,
                                            std::vector<std::shared_ptr<storm::expressions::BaseExpression const>> const& args = {}) {
    return storm::expressions::Expression(std::make_shared<storm::expressions::FunctionCallExpression>(manager, manager.getIntegerType(), name, args));
}

storm::jani::Model sharedFunctionModel(uint64_t depth) {
    auto document = storm::json<double>::parse(R"({
        "jani-version":1,"name":"sharing","type":"mdp","features":["functions"],
        "variables":[{"name":"x","type":{"kind":"bounded","base":"int","lower-bound":0,"upper-bound":1},"initial-value":0}],
        "functions":[],
        "automata":[{"name":"main","locations":[{"name":"l"}],"initial-locations":["l"],"edges":[{
            "location":"l","guard":{"exp":true},"destinations":[{"location":"l","assignments":[{"ref":"x","value":{"op":"-","left":1,"right":"x"}}]}]
        }]}],"system":{"elements":[{"automaton":"main"}]},"properties":[]})");
    auto call = [](uint64_t i) { return storm::json<double>{{"op", "call"}, {"function", "f" + std::to_string(i)}, {"args", storm::json<double>::array()}}; };
    for (uint64_t i = 0; i <= depth; ++i) {
        storm::json<double> body = "x";
        if (i > 0) {
            body = {{"op", "+"}, {"left", call(i - 1)}, {"right", call(i - 1)}};
        }
        document["functions"].push_back({{"name", "f" + std::to_string(i)}, {"type", "int"}, {"parameters", storm::json<double>::array()}, {"body", body}});
    }
    document["automata"][0]["edges"][0]["guard"]["exp"] = {{"op", "≥"}, {"left", call(depth)}, {"right", 0}};
    return storm::parser::JaniParser<double>::parseFromString(document.dump()).first;
}
}  // namespace

TEST(JaniFunctions, SharedFunctionChain) {
    constexpr uint64_t depth = 28;
    auto model = sharedFunctionModel(depth);
    model.substituteFunctions();
    EXPECT_FALSE(model.getModelFeatures().hasFunctions());
    EXPECT_TRUE(model.getGlobalFunctionDefinitions().empty());
    EXPECT_FALSE(storm::jani::containsFunctionCallExpression(model));
    auto expression = storm::expressions::Expression(model.getAutomaton(0).getEdges()[0].getGuard().getBaseExpression().getOperand(0));
    auto current = expression;
    for (uint64_t i = 0; i < depth; ++i) {
        auto const& node = current.getBaseExpression();
        ASSERT_TRUE(node.isBinaryNumericalFunctionExpression());
        EXPECT_EQ(node.getOperand(0).get(), node.getOperand(1).get());
        current = storm::expressions::Expression(node.getOperand(0));
    }
    EXPECT_TRUE(current.getBaseExpression().isVariableExpression());
    auto x = model.getExpressionManager().getVariable("x");
    for (int64_t value : {0, 1}) {
        std::map<storm::expressions::Variable, storm::expressions::Expression> mapping{{x, model.getExpressionManager().integer(value)}};
        EXPECT_EQ(value << depth, expression.substitute(mapping).simplify().evaluateAsInt());
    }
}

TEST(JaniFunctions, ParametersAndNestedCalls) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto p = manager->declareIntegerVariable("p");
    storm::jani::Model model("functions", storm::jani::ModelType::MDP, 1, manager);
    model.getModelFeatures().add(storm::jani::ModelFeature::Functions);
    auto shared = p + p;
    model.addFunctionDefinition(storm::jani::FunctionDefinition("f", manager->getIntegerType(), {p}, shared + shared));
    auto one = manager->integer(1).getBaseExpressionPointer();
    auto two = manager->integer(2).getBaseExpressionPointer();
    auto f1 = functionCall(*manager, "f", {one});
    auto f2 = functionCall(*manager, "f", {two});
    auto nested = functionCall(*manager, "f", {f2.getBaseExpressionPointer()});
    auto result = storm::jani::eliminateFunctionCallsInExpression(f1 + f2 + nested, model);
    EXPECT_EQ(44, result.evaluateAsInt());
    EXPECT_TRUE(storm::jani::getOccurringFunctionCalls(result).empty());
    STORM_SILENT_ASSERT_THROW(storm::jani::eliminateFunctionCallsInExpression(functionCall(*manager, "f"), model), storm::exceptions::InvalidArgumentException);
}

TEST(JaniFunctions, DefinitionEnvironmentsAndCycles) {
    auto model = sharedFunctionModel(0);
    auto& manager = model.getExpressionManager();
    model.addFunctionDefinition(storm::jani::FunctionDefinition("global", manager.getIntegerType(), {}, manager.integer(2)));
    model.getAutomaton(0).addFunctionDefinition(storm::jani::FunctionDefinition("local", manager.getIntegerType(), {}, functionCall(manager, "global")));
    // Copy the automaton with a different local function of the same name.
    auto second = model.getAutomaton(0).clone(manager, "second", "second_");
    second.getFunctionDefinitions().at("local").setFunctionBody(manager.integer(3));
    model.addAutomaton(second);
    for (auto& automaton : model.getAutomata()) {
        automaton.getEdges()[0].getTemplateEdge()->setGuard(functionCall(manager, "local") == manager.integer(2));
    }
    model.substituteFunctions();
    EXPECT_TRUE(model.getAutomaton(0).getEdges()[0].getGuard().evaluateAsBool());
    EXPECT_FALSE(model.getAutomaton(1).getEdges()[0].getGuard().evaluateAsBool());

    auto cycle = sharedFunctionModel(1);
    cycle.getGlobalFunctionDefinitions().at("f0").setFunctionBody(functionCall(cycle.getExpressionManager(), "f1"));
    STORM_SILENT_ASSERT_THROW(cycle.substituteFunctions(), storm::exceptions::NotSupportedException);
    auto recursive = sharedFunctionModel(0);
    recursive.getGlobalFunctionDefinitions().at("f0").setFunctionBody(functionCall(recursive.getExpressionManager(), "f0"));
    STORM_SILENT_ASSERT_THROW(recursive.substituteFunctions(), storm::exceptions::NotSupportedException);
}

TEST(JaniFunctions, ArraysAndTranscendentalSubstitution) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = manager->declareIntegerVariable("x");
    auto i = manager->declareIntegerVariable("i");
    auto child = (x + manager->integer(1)).getBaseExpressionPointer();
    auto array = storm::expressions::Expression(std::make_shared<storm::expressions::ValueArrayExpression>(
        *manager, manager->getArrayType(manager->getIntegerType()), std::vector<std::shared_ptr<storm::expressions::BaseExpression const>>{child, child}));
    std::map<storm::expressions::Variable, storm::expressions::Expression> mapping{{x, manager->integer(4)}};
    auto rewritten = storm::jani::substituteJaniExpression(array, mapping, false).simplify();
    auto const& values = dynamic_cast<storm::expressions::ValueArrayExpression const&>(rewritten.getBaseExpression());
    EXPECT_EQ(values.at(0).get(), values.at(1).get());
    EXPECT_EQ(5, values.at(0)->evaluateAsInt());
    auto constructor = storm::expressions::Expression(std::make_shared<storm::expressions::ConstructorArrayExpression>(
        *manager, manager->getArrayType(manager->getIntegerType()), manager->integer(2).getBaseExpressionPointer(), i, (i + x).getBaseExpressionPointer()));
    auto newConstructor = storm::jani::substituteJaniExpression(constructor, mapping, false);
    EXPECT_FALSE(newConstructor.containsVariable({i}));
    auto const& constructed = dynamic_cast<storm::expressions::ConstructorArrayExpression const&>(newConstructor.getBaseExpression());
    EXPECT_EQ(i, constructed.getIndexVar());
    EXPECT_EQ(5, constructed.at(1)->simplify()->evaluateAsInt());
    EXPECT_FALSE(newConstructor.containsVariable({x}));
    mapping.emplace(i, manager->integer(0));
    STORM_SILENT_ASSERT_THROW(storm::jani::substituteJaniExpression(constructor, mapping, false), storm::exceptions::InvalidArgumentException);

    auto pi = storm::expressions::Expression(std::make_shared<storm::expressions::TranscendentalNumberLiteralExpression>(
        *manager, storm::expressions::TranscendentalNumberLiteralExpression::TranscendentalNumber::PI));
    storm::jani::FunctionDefinition f("pi", manager->getRationalType(), {}, pi + pi);
    auto result = f.call({});
    EXPECT_NEAR(2 * pi.evaluateAsDouble(), result.simplify().evaluateAsDouble(), 1e-12);
    EXPECT_TRUE(result.getBaseExpression().getOperand(0)->isRationalLiteralExpression());
    EXPECT_EQ(result.getBaseExpression().getOperand(0).get(), result.getBaseExpression().getOperand(1).get());
}

TEST(JaniFunctions, SharedVariablesAcrossArrayBindings) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto i = manager->declareIntegerVariable("i");
    auto j = manager->declareIntegerVariable("j");
    auto x = manager->declareIntegerVariable("x");
    auto shared = (i + x).getBaseExpressionPointer();
    auto arrayType = manager->getArrayType(manager->getIntegerType());
    auto size = manager->integer(2).getBaseExpressionPointer();
    auto bound = std::make_shared<storm::expressions::ConstructorArrayExpression>(*manager, arrayType, size, i, shared);
    auto free = std::make_shared<storm::expressions::ConstructorArrayExpression>(*manager, arrayType, size, j, shared);
    storm::expressions::Expression arrays(std::make_shared<storm::expressions::ValueArrayExpression>(
        *manager, manager->getArrayType(arrayType), std::vector<std::shared_ptr<storm::expressions::BaseExpression const>>{bound, free, bound}));
    EXPECT_EQ((std::set<storm::expressions::Variable>{i, x}), arrays.getVariables());
    EXPECT_EQ((std::set<storm::expressions::Variable>{x}), bound->toExpression().getVariables());
    std::set<storm::expressions::Variable> existing{i};
    bound->gatherVariables(existing);
    EXPECT_EQ((std::set<storm::expressions::Variable>{i, x}), existing);
    auto call = functionCall(*manager, "uninterpreted", {shared, shared});
    EXPECT_EQ((std::set<storm::expressions::Variable>{i, x}), call.getVariables());
}
