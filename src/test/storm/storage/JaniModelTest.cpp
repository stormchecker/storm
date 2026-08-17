#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/storage/jani/Model.h"
#include "storm/utility/solver.h"

#ifdef STORM_HAVE_MATHSAT
TEST(JaniModelTest, FlattenComposition) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(74ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Wlan_Mathsat) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(179ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Csma_Mathsat) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/csma2_2.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(70ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Firewire_Mathsat) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/firewire.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(5024ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Coin_Mathsat) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(13ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Dice_Mathsat) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(16ull, janiModel.getAutomaton(0).getNumberOfEdges());
}
#endif

#ifdef STORM_HAVE_Z3
TEST(JaniModelTest, FlattenComposition_Leader_Z3) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(74ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Wlan_Z3) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(179ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Csma_Z3) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/csma2_2.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(70ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Firewire_Z3) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/firewire.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(5024ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Coin_Z3) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(13ull, janiModel.getAutomaton(0).getNumberOfEdges());
}

TEST(JaniModelTest, FlattenComposition_Dice_Z3) {
    storm::prism::Program program;
    ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm"));
    storm::jani::Model janiModel = program.toJani();

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();

    janiModel.substituteFunctions();
    ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
    EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
    EXPECT_EQ(16ull, janiModel.getAutomaton(0).getNumberOfEdges());
}
#endif
