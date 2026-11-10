#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/languages/jani/Model.h"
#include "storm/utility/solver.h"
#include "test/SmtSolverTestUtils.h"

namespace {

class PrismProgramFlattenTest : public ::testing::TestWithParam<storm::solver::SmtSolverType> {
   protected:
    void flattenAndCheck(std::string const& path, uint64_t expectedNumberOfCommands) const {
        storm::prism::Program program;
        ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(path));

        auto smtSolverFactory = storm::test::smtSolverFactory(GetParam());
        ASSERT_NO_THROW(program = program.substituteFormulas().flattenModules(smtSolverFactory));
        EXPECT_EQ(1ull, program.getNumberOfModules());
        EXPECT_EQ(expectedNumberOfCommands, program.getModule(0).getNumberOfCommands());
    }
};

}  // namespace

TEST_P(PrismProgramFlattenTest, Leader) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm", 74ull);
}

TEST_P(PrismProgramFlattenTest, Wlan) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm", 179ull);
}

TEST_P(PrismProgramFlattenTest, Csma) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/csma2_2.nm", 70ull);
}

STORM_EXPENSIVE_TEST_P(PrismProgramFlattenTest, Firewire) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/firewire.nm", 5024ull);
}

TEST_P(PrismProgramFlattenTest, Coin) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/coin2.nm", 13ull);
}

TEST_P(PrismProgramFlattenTest, Dice) {
    flattenAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", 16ull);
}

INSTANTIATE_TEST_SUITE_P(SmtSolvers, PrismProgramFlattenTest, ::testing::ValuesIn(storm::test::smtSolversUnderTest()), storm::test::smtSolverName);
// The suite is empty if no SMT solver was compiled in.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(PrismProgramFlattenTest);

TEST(PrismProgramTest, ConvertToJani) {
    storm::prism::Program prismProgram;
    storm::jani::Model janiModel;

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/nand-5-2.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/unbounded.nm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());
}

TEST(PrismProgramTest, ReplaceInitialStates) {
    storm::prism::Program origPrismProgram;
    storm::prism::Program transformedPrismProgram;
    ASSERT_NO_THROW(origPrismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm"));
    ASSERT_NO_THROW(transformedPrismProgram = origPrismProgram.replaceVariableInitializationByInitExpression());
    EXPECT_TRUE(origPrismProgram.getInitialStatesExpression().isSyntacticallyEqual(transformedPrismProgram.getInitialStatesExpression()));
    EXPECT_TRUE(transformedPrismProgram.hasInitialConstruct());
}

TEST(PrismProgramTest, ReplaceConstantByVariable) {
    storm::prism::Program origPrismProgram;
    storm::prism::Program transformedPrismProgram;
    ASSERT_NO_THROW(origPrismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-4-3.pm"));
    ASSERT_NO_THROW(transformedPrismProgram = origPrismProgram.replaceConstantByVariable(
                        origPrismProgram.getConstant("CrowdSize"), origPrismProgram.getManager().integer(0), origPrismProgram.getManager().integer(20), true));
    EXPECT_NO_THROW(transformedPrismProgram.getGlobalIntegerVariable("CrowdSize"));
    EXPECT_FALSE(transformedPrismProgram.hasConstant("CrowdSize"));
}