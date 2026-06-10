#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm-pomdp/transformer/ObservationTraceUnfolder.h"
#include "storm/api/storm.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"

TEST(ObservationTraceUnfolder, Simple) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism");
    program = storm::utility::prism::preprocess(program, "slippery=0.4");
    std::shared_ptr<storm::logic::Formula const> formula = storm::api::parsePropertiesForPrismProgram("Pmax=? [F \"goal\" ]", program).front().getRawFormula();
    std::shared_ptr<storm::models::sparse::Pomdp<double>> pomdp =
        storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();

    std::vector<double> risk(pomdp->getNumberOfStates(), storm::utility::zero<double>());
    std::shared_ptr<storm::expressions::ExpressionManager> exprManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::pomdp::ObservationTraceUnfolderOptions options;

    storm::pomdp::ObservationTraceUnfolder<double> unfolder(*pomdp, risk, exprManager, options);

    uint64_t initialState = pomdp->getInitialStates().getNextSetIndex(0);
    std::vector<uint32_t> observations = {pomdp->getObservation(initialState), pomdp->getObservation(initialState), pomdp->getObservation(initialState)};

    std::shared_ptr<storm::models::sparse::Mdp<double>> unfolded = unfolder.transform(observations);
    EXPECT_TRUE(unfolded != nullptr);
    EXPECT_GT(unfolded->getNumberOfStates(), 0u);
    EXPECT_TRUE(unfolded->getStateLabeling().containsLabel("_goal"));
    EXPECT_TRUE(unfolded->getStateLabeling().containsLabel("_end"));
    EXPECT_TRUE(unfolded->getStateLabeling().containsLabel("init"));
    EXPECT_EQ(1u, unfolded->getInitialStates().getNumberOfSetBits());
}
