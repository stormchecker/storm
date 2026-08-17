#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-gamebased-ar/abstraction/MenuGameRefiner.h"
#include "storm/environment/Environment.h"

#include "storm-gamebased-ar/abstraction/prism/PrismMenuGameAbstractor.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/DdPrismModelBuilder.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/utility/graph.h"
#include "storm/utility/solver.h"

class Cudd {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::CUDD;
};

class Sylvan {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::Sylvan;
};

template<typename TestType>
class GraphTestAR : public ::testing::Test {
   public:
    static const storm::dd::DdType DdType = TestType::DdType;

   protected:
    void SetUp() override {
#ifndef STORM_HAVE_MATHSAT
        GTEST_SKIP() << "MathSAT not available.";
#endif
        TestType::checkLibraryAvailable();
    }
};

typedef ::testing::Types<Cudd, Sylvan> TestingTypes;
TYPED_TEST_SUITE(GraphTestAR, TestingTypes, );

TYPED_TEST(GraphTestAR, SymbolicProb01StochasticGameDieSmall) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s") < manager.integer(3));

    auto smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();
    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    // The target states are those states where !(s < 3).
    storm::dd::Bdd<DdType> targetStates = !abstractor.getStates(initialPredicates[0]) && game.getReachableStates();

    storm::utility::graph::SymbolicGameProb01Result<DdType> result =
        storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                            storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, true, true);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());
    EXPECT_TRUE(result.hasPlayer1Strategy());
    EXPECT_TRUE(result.hasPlayer2Strategy());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(8ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(8ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, true, true);
    EXPECT_EQ(8ull, result.getPlayer1States().getNonZeroCount());
    EXPECT_TRUE(result.hasPlayer1Strategy());
    EXPECT_TRUE(result.hasPlayer2Strategy());

    refiner.refine({manager.getVariableExpression("s") < manager.integer(2)});
    game = abstractor.abstract();

    // We need to create a new BDD for the target states since the reachable states might have changed.
    targetStates = !abstractor.getStates(initialPredicates[0]) && game.getReachableStates();

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, true, true);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());
    ASSERT_TRUE(result.hasPlayer1Strategy());
    ASSERT_TRUE(result.hasPlayer2Strategy());

    // Check the validity of the strategies. Start by checking whether only prob0 states have a strategy.
    storm::dd::Bdd<DdType> nonProb0StatesWithStrategy = !result.getPlayer1States() && result.player1Strategy.get();
    EXPECT_TRUE(nonProb0StatesWithStrategy.isZero());

    // Proceed by checking whether they select exactly one action in each state.
    storm::dd::Add<DdType, double> stateDistributionsUnderStrategies =
        (game.getTransitionMatrix() * result.player1Strategy.get().template toAdd<double>() * result.player2Strategy.get().template toAdd<double>())
            .sumAbstract(game.getColumnVariables());
    EXPECT_EQ(0ull, stateDistributionsUnderStrategies.getNonZeroCount());

    // Check that the number of distributions per state is one (or zero in the case where there are no prob0 states).
    storm::dd::Add<DdType> stateDistributionCount = stateDistributionsUnderStrategies.sumAbstract(game.getNondeterminismVariables());
    EXPECT_EQ(0.0, stateDistributionCount.getMax());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(8ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(8ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(8ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(0ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, true, true);
    EXPECT_EQ(8ull, result.getPlayer1States().getNonZeroCount());
    EXPECT_TRUE(result.hasPlayer1Strategy());
    EXPECT_TRUE(result.hasPlayer2Strategy());

    // Check the validity of the strategies. Start by checking whether only prob1 states have a strategy.
    storm::dd::Bdd<DdType> nonProb1StatesWithStrategy = !result.getPlayer1States() && result.player1Strategy.get();
    EXPECT_TRUE(nonProb1StatesWithStrategy.isZero());

    // Proceed by checking whether they select exactly one action in each state.
    stateDistributionsUnderStrategies =
        (game.getTransitionMatrix() * result.player1Strategy.get().template toAdd<double>() * result.player2Strategy.get().template toAdd<double>())
            .sumAbstract(game.getColumnVariables());
    EXPECT_EQ(8ull, stateDistributionsUnderStrategies.getNonZeroCount());

    // Check that the number of distributions per state is one (or zero in the case where there are no prob1 states).
    stateDistributionCount = stateDistributionsUnderStrategies.sumAbstract(game.getNondeterminismVariables());
    EXPECT_EQ(1.0, stateDistributionCount.getMax());
}

TYPED_TEST(GraphTestAR, SymbolicProb01StochasticGameTwoDice) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_unique<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(6));

    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(6));

    auto smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();
    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    // The target states are those states where s1 == 7 & s2 == 7 & d1 + d2 == 2.
    storm::dd::Bdd<DdType> targetStates = abstractor.getStates(initialPredicates[7]) && abstractor.getStates(initialPredicates[22]) &&
                                          abstractor.getStates(initialPredicates[9]) && abstractor.getStates(initialPredicates[24]) &&
                                          game.getReachableStates();

    storm::utility::graph::SymbolicGameProb01Result<DdType> result =
        storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                            storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, true, true);
    EXPECT_EQ(153ull, result.getPlayer1States().getNonZeroCount());
    ASSERT_TRUE(result.hasPlayer1Strategy());
    ASSERT_TRUE(result.hasPlayer2Strategy());

    // Check the validity of the strategies. Start by checking whether only prob0 states have a strategy.
    storm::dd::Bdd<DdType> nonProb0StatesWithStrategy = !result.getPlayer1States() && result.player1Strategy.get();
    EXPECT_TRUE(nonProb0StatesWithStrategy.isZero());

    // Proceed by checking whether they select exactly one exaction in each state.
    storm::dd::Add<DdType, double> stateDistributionsUnderStrategies =
        (game.getTransitionMatrix() * result.player1Strategy.get().template toAdd<double>() * result.player2Strategy.get().template toAdd<double>())
            .sumAbstract(game.getColumnVariables());
    EXPECT_EQ(153ull, stateDistributionsUnderStrategies.getNonZeroCount());

    storm::dd::Add<DdType> stateDistributionCount = stateDistributionsUnderStrategies.sumAbstract(game.getNondeterminismVariables());
    EXPECT_EQ(1.0, stateDistributionCount.getMax());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(1ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(153ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(1ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(153ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(1ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(153ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, true, true);
    EXPECT_EQ(1ull, result.getPlayer1States().getNonZeroCount());
    EXPECT_TRUE(result.hasPlayer1Strategy());
    EXPECT_TRUE(result.hasPlayer2Strategy());

    // Check the validity of the strategies. Start by checking whether only prob1 states have a strategy.
    storm::dd::Bdd<DdType> nonProb1StatesWithStrategy = !result.getPlayer1States() && result.player1Strategy.get();
    EXPECT_TRUE(nonProb1StatesWithStrategy.isZero());

    // Proceed by checking whether they select exactly one action in each state.
    stateDistributionsUnderStrategies =
        (game.getTransitionMatrix() * result.player1Strategy.get().template toAdd<double>() * result.player2Strategy.get().template toAdd<double>())
            .sumAbstract(game.getColumnVariables());
    EXPECT_EQ(1ull, stateDistributionsUnderStrategies.getNonZeroCount());

    // Check that the number of distributions per state is one (or zero in the case where there are no prob1 states).
    stateDistributionCount = stateDistributionsUnderStrategies.sumAbstract(game.getNondeterminismVariables());
    EXPECT_EQ(1.0, stateDistributionCount.getMax());
}

TYPED_TEST(GraphTestAR, SymbolicProb01StochasticGameWlan) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0-2-4.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_unique<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("col") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("col") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("col") == manager.integer(2));

    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.integer(2));

    initialPredicates.push_back(manager.getVariableExpression("c2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("c2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("c2") == manager.integer(2));

    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(12));

    initialPredicates.push_back(manager.getVariableExpression("slot1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("slot1") == manager.integer(1));

    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(12));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(13));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(14));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(15));

    initialPredicates.push_back(manager.getVariableExpression("bc1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("bc1") == manager.integer(1));

    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(12));

    initialPredicates.push_back(manager.getVariableExpression("slot2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("slot2") == manager.integer(1));

    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(12));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(13));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(14));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(15));

    initialPredicates.push_back(manager.getVariableExpression("bc2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("bc2") == manager.integer(1));

    auto smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();
    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    // The target states are those states where col == 2.
    storm::dd::Bdd<DdType> targetStates = abstractor.getStates(initialPredicates[2]) && game.getReachableStates();

    storm::utility::graph::SymbolicGameProb01Result<DdType> result =
        storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                            storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, true, true);
    EXPECT_EQ(2831ull, result.getPlayer1States().getNonZeroCount());
    EXPECT_TRUE(result.hasPlayer1Strategy());
    EXPECT_TRUE(result.hasPlayer2Strategy());

    // Check the validity of the strategies. Start by checking whether only prob0 states have a strategy.
    storm::dd::Bdd<DdType> nonProb0StatesWithStrategy = !result.getPlayer1States() && result.player1Strategy.get();
    EXPECT_TRUE(nonProb0StatesWithStrategy.isZero());

    // Proceed by checking whether they select exactly one action in each state.
    storm::dd::Add<DdType, double> stateDistributionsUnderStrategies =
        (game.getTransitionMatrix() * result.player1Strategy.get().template toAdd<double>() * result.player2Strategy.get().template toAdd<double>())
            .sumAbstract(game.getColumnVariables());
    EXPECT_EQ(2831ull, stateDistributionsUnderStrategies.getNonZeroCount());

    // Check that the number of distributions per state is one (or zero in the case where there are no prob0 states).
    storm::dd::Add<DdType> stateDistributionCount = stateDistributionsUnderStrategies.sumAbstract(game.getNondeterminismVariables());
    EXPECT_EQ(1.0, stateDistributionCount.getMax());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(2692ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(2831ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(2692ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(2064ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Minimize);
    EXPECT_EQ(2884ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb0(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize);
    EXPECT_EQ(2064ull, result.getPlayer1States().getNonZeroCount());

    result = storm::utility::graph::performProb1(game, game.getQualitativeTransitionMatrix(), game.getReachableStates(), targetStates,
                                                 storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, true, true);
    EXPECT_EQ(2884ull, result.getPlayer1States().getNonZeroCount());
    EXPECT_TRUE(result.hasPlayer1Strategy());
    EXPECT_TRUE(result.hasPlayer2Strategy());

    // Check the validity of the strategies. Start by checking whether only prob1 states have a strategy.
    storm::dd::Bdd<DdType> nonProb1StatesWithStrategy = !result.getPlayer1States() && result.player1Strategy.get();
    EXPECT_TRUE(nonProb1StatesWithStrategy.isZero());

    // Proceed by checking whether they select exactly one action in each state.
    stateDistributionsUnderStrategies =
        (game.getTransitionMatrix() * result.player1Strategy.get().template toAdd<double>() * result.player2Strategy.get().template toAdd<double>())
            .sumAbstract(game.getColumnVariables());
    EXPECT_EQ(2884ull, stateDistributionsUnderStrategies.getNonZeroCount());

    // Check that the number of distributions per state is one (or zero in the case where there are no prob1 states).
    stateDistributionCount = stateDistributionsUnderStrategies.sumAbstract(game.getNondeterminismVariables());
    EXPECT_EQ(1.0, stateDistributionCount.getMax());
}