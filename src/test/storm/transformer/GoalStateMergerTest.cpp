#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/storm.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/transformer/GoalStateMerger.h"

namespace {

class GoalStateMergerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

void testGoalStateMerger(std::string const& prismModelFile, std::string const& formulaString) {
    storm::prism::Program program = storm::parser::PrismParser::parse(prismModelFile, true);
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    auto model = storm::api::buildSparseModel<double>(program, formulas);
    auto task = storm::api::createTask<double>(formulas.front(), false);
    ASSERT_EQ(1ull, model->getInitialStates().getNumberOfSetBits()) << "For model " << prismModelFile << ".";

    auto computeValue = [&task](auto const& m) {
        auto result = storm::api::verifyWithSparseEngine(m, task);
        return result->template asExplicitQuantitativeCheckResult<double>()[*m->getInitialStates().begin()];
    };
    auto const modelVal = computeValue(model);

    storm::transformer::GoalStateMerger<double> merger(*model);
    for (bool dropUnreachableFromInit : {true, false}) {
        auto mergerResult = merger.mergeForFormula(*formulas.front(), dropUnreachableFromInit);
        ASSERT_TRUE(mergerResult.has_value()) << "Merging goal states failed for model " << prismModelFile << " and formula " << *formulas.front()
                                              << " (dropUnreachableFromInit=" << dropUnreachableFromInit << ").";
        ASSERT_EQ(1ull, mergerResult->model->getInitialStates().getNumberOfSetBits())
            << "For merged model of " << prismModelFile << " (dropUnreachableFromInit=" << dropUnreachableFromInit << ").";

        // The original (unmodified) formula must still be valid on the merged model and induce the same value at the initial state.
        auto const mergedVal = computeValue(mergerResult->model);
        EXPECT_LE(std::abs(mergedVal - modelVal) / modelVal, 1e-6)
            << "Relative difference between original model result (" << modelVal << ") and merged model result (" << mergedVal
            << ") is too high.\nFailed for model " << prismModelFile << " and formula " << *formulas.front()
            << " (dropUnreachableFromInit=" << dropUnreachableFromInit << ").";
    }
}

TEST_F(GoalStateMergerTest, BrpUntilTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "P=? [ true U \"target\"]");
}

TEST_F(GoalStateMergerTest, BrpBoundedUntilTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "P=? [ true U<=100 \"target\"]");
}

TEST_F(GoalStateMergerTest, DieUntilTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "P=? [ !\"done\" U \"one\"]");
}

TEST_F(GoalStateMergerTest, LeaderUntilTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm", "P=? [ true U \"elected\"]");
}

TEST_F(GoalStateMergerTest, CrowdsUntilTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-4-3.pm", "P=? [ true U \"observeIGreater1\"]");
}

TEST_F(GoalStateMergerTest, LeaderBoundedUntilTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm", "P=? [F<=5 \"elected\"]");
}

TEST_F(GoalStateMergerTest, DieReachabilityRewardTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", "R=? [F \"done\"]");
}

TEST_F(GoalStateMergerTest, TwoDiceUntilMinTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmin=? [ true U \"two\"]");
}

TEST_F(GoalStateMergerTest, TwoDiceUntilMaxTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Pmax=? [ true U \"two\"]");
}

TEST_F(GoalStateMergerTest, LeaderMdpBoundedUntilMinTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/mdp/leader4.nm", "Pmin=? [F<=25 \"elected\"]");
}

TEST_F(GoalStateMergerTest, LeaderMdpBoundedUntilMaxTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/mdp/leader4.nm", "Pmax=? [F<=25 \"elected\"]");
}

TEST_F(GoalStateMergerTest, ChainBoundedUntilTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/mdp/chain.nm", "Pmax=? [ x<3 U<=4 x=5]");
}

TEST_F(GoalStateMergerTest, TinyRewardsCumulativeRewardTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards.nm", "Rmax=? [C<=2]");
}

TEST_F(GoalStateMergerTest, Coin22ReachabilityRewardTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Rmax=? [ F \"finished\"]");
}

TEST_F(GoalStateMergerTest, TandemReachabilityRewardTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/ctmc/tandem5.sm", "R=? [F \"first_queue_full\"&\"second_queue_full\"]");
}

TEST_F(GoalStateMergerTest, ClusterCumulativeRewardTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", "R{\"num_repairs\"}=? [C<=200]");
}

TEST_F(GoalStateMergerTest, StreamReachabilityRewardTest) {
    testGoalStateMerger(STORM_TEST_RESOURCES_DIR "/ma/stream2.ma", "R{\"buffering\"}max=? [ F \"done\"]");
}

}  // namespace
