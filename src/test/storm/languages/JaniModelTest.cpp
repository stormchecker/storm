#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/languages/jani/Model.h"
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
