#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/storm.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/transformer/StatePermuter.h"
#include "storm/utility/permutation.h"

namespace {

class StatePermuterTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

void testStatePermuter(std::string const& prismModelFile, std::string const& formulaString) {
    storm::prism::Program program = storm::parser::PrismParser::parse(prismModelFile, true);
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    auto model = storm::api::buildSparseModel<double>(program, formulas);
    auto task = storm::api::createTask<double>(formulas.front(), false);
    ASSERT_EQ(1ull, model->getInitialStates().getNumberOfSetBits()) << "For model " << prismModelFile << ".";

    auto computeValue = [&task](auto m) {
        auto result = storm::api::verifyWithSparseEngine(m, task);
        return result->template asExplicitQuantitativeCheckResult<double>()[*m->getInitialStates().begin()];
    };
    auto const modelVal = computeValue(model);

    auto checkOrder = [&model, &computeValue, &modelVal, &prismModelFile](storm::utility::permutation::OrderKind order) {
        auto permutation = storm::utility::permutation::createPermutation(order, model->getTransitionMatrix(), model->getInitialStates());
        auto permutedModel = storm::transformer::permuteStates(*model, permutation);
        EXPECT_EQ(permutedModel->getNumberOfStates(), model->getNumberOfStates())
            << "Failed for order " << storm::utility::permutation::orderKindtoString(order) << " and model " << prismModelFile << ".";
        EXPECT_EQ(permutedModel->getNumberOfTransitions(), model->getNumberOfTransitions())
            << "Failed for order " << storm::utility::permutation::orderKindtoString(order) << " and model " << prismModelFile << ".";
        auto permVal = computeValue(permutedModel);
        EXPECT_LE(std::abs(permVal - modelVal) / modelVal, 1e-4)
            << "Relative difference between original model result (" << modelVal << ") and permuted model result (" << permVal
            << ") is too high.\nFailed for order " << storm::utility::permutation::orderKindtoString(order) << " and model " << prismModelFile << ".";
        return permutedModel;
    };

    auto permutedModel = checkOrder(storm::utility::permutation::OrderKind::Random);
    permutedModel = checkOrder(storm::utility::permutation::OrderKind::Dfs);
    EXPECT_EQ(0ull, *permutedModel->getInitialStates().begin()) << "Failed for model " << prismModelFile;
    permutedModel = checkOrder(storm::utility::permutation::OrderKind::Bfs);
    EXPECT_EQ(0ull, *permutedModel->getInitialStates().begin()) << "Failed for model " << prismModelFile;
    permutedModel = checkOrder(storm::utility::permutation::OrderKind::ReverseDfs);
    EXPECT_EQ(permutedModel->getNumberOfStates() - 1, *permutedModel->getInitialStates().begin()) << "Failed for model " << prismModelFile;
    permutedModel = checkOrder(storm::utility::permutation::OrderKind::ReverseBfs);
    EXPECT_EQ(permutedModel->getNumberOfStates() - 1, *permutedModel->getInitialStates().begin()) << "Failed for model " << prismModelFile;
}
TEST_F(StatePermuterTest, BrpTest) {
    testStatePermuter(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "P=? [ F \"target\"]");
}

TEST_F(StatePermuterTest, ClusterTest) {
    testStatePermuter(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", " R{\"num_repairs\"}=? [C<=200]");
}

TEST_F(StatePermuterTest, Coin22Test) {
    testStatePermuter(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Rmax=? [ F \"finished\"]");
}

TEST_F(StatePermuterTest, StreamTest) {
    testStatePermuter(STORM_TEST_RESOURCES_DIR "/ma/stream2.ma", "R{\"buffering\"}max=? [ F \"done\"]");
}
}  // namespace
