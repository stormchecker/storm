#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"

TEST(MarkovAutomatonTest, ZenoCycleCheck) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/simple.ma");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_FALSE(model->as<storm::models::sparse::MarkovAutomaton<double>>()->containsZenoCycle());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/hybrid_states.ma");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_FALSE(model->as<storm::models::sparse::MarkovAutomaton<double>>()->containsZenoCycle());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/stream2.ma");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(14ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_FALSE(model->as<storm::models::sparse::MarkovAutomaton<double>>()->containsZenoCycle());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/zeno.ma");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_TRUE(model->as<storm::models::sparse::MarkovAutomaton<double>>()->containsZenoCycle());
}
