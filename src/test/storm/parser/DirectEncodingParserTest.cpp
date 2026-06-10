#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/DirectEncodingParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"

TEST(DirectEncodingParserTest, DtmcParsing) {
    std::shared_ptr<storm::models::sparse::Model<double>> modelPtr =
        storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.drn");

    // Test if parsed correctly.
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    ASSERT_EQ(8607ul, modelPtr->getNumberOfStates());
    ASSERT_EQ(15113ul, modelPtr->getNumberOfTransitions());
    ASSERT_TRUE(modelPtr->hasLabel("init"));
    ASSERT_EQ(1ul, modelPtr->getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("observeIGreater1"));
    ASSERT_EQ(4650ul, modelPtr->getStates("observeIGreater1").getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("observe0Greater1"));
    ASSERT_EQ(1260ul, modelPtr->getStates("observe0Greater1").getNumberOfSetBits());

    std::shared_ptr<storm::models::ModelBase> modelBasePtr =
        storm::parser::parseDirectEncodingModel(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.drn", storm::parser::DirectEncodingValueType::Default);
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto dtmc = modelBasePtr->as<storm::models::sparse::Dtmc<double>>();
    ASSERT_EQ(8607ul, dtmc->getNumberOfStates());
}

TEST(DirectEncodingParserTest, DtmcRationalParsing) {
    std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> modelPtr =
        storm::parser::parseDirectEncodingModel<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.drn");

    // Test if parsed correctly.
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    ASSERT_EQ(8607ul, modelPtr->getNumberOfStates());
    ASSERT_EQ(15113ul, modelPtr->getNumberOfTransitions());
    ASSERT_TRUE(modelPtr->hasLabel("init"));
    ASSERT_EQ(1ul, modelPtr->getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("observeIGreater1"));
    ASSERT_EQ(4650ul, modelPtr->getStates("observeIGreater1").getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("observe0Greater1"));
    ASSERT_EQ(1260ul, modelPtr->getStates("observe0Greater1").getNumberOfSetBits());

    std::shared_ptr<storm::models::ModelBase> modelBasePtr =
        storm::parser::parseDirectEncodingModel(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.drn", storm::parser::DirectEncodingValueType::Rational);
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto dtmc = modelBasePtr->as<storm::models::sparse::Dtmc<storm::RationalNumber>>();
    ASSERT_EQ(8607ul, dtmc->getNumberOfStates());
}

TEST(DirectEncodingParserTest, MdpParsing) {
    std::shared_ptr<storm::models::sparse::Model<double>> modelPtr =
        storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.drn");

    // Test if parsed correctly.
    ASSERT_EQ(storm::models::ModelType::Mdp, modelPtr->getType());
    ASSERT_EQ(169ul, modelPtr->getNumberOfStates());
    ASSERT_EQ(436ul, modelPtr->getNumberOfTransitions());
    ASSERT_EQ(254ul, modelPtr->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());
    ASSERT_TRUE(modelPtr->hasLabel("init"));
    ASSERT_EQ(1ul, modelPtr->getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("six"));
    ASSERT_EQ(5ul, modelPtr->getStates("six").getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("eleven"));
    ASSERT_EQ(2ul, modelPtr->getStates("eleven").getNumberOfSetBits());
    ASSERT_EQ(1ul, modelPtr->getNumberOfRewardModels());
    ASSERT_TRUE(modelPtr->hasRewardModel("coinflips"));
    ASSERT_TRUE(!modelPtr->getRewardModel("coinflips").hasStateRewards());
    ASSERT_TRUE(modelPtr->getRewardModel("coinflips").hasStateActionRewards());
    ASSERT_TRUE(!modelPtr->getRewardModel("coinflips").isAllZero());
}

TEST(DirectEncodingParserTest, CtmcParsing) {
    std::shared_ptr<storm::models::sparse::Model<double>> modelPtr =
        storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.drn");

    // Test if parsed correctly.
    ASSERT_EQ(storm::models::ModelType::Ctmc, modelPtr->getType());
    ASSERT_EQ(276ul, modelPtr->getNumberOfStates());
    ASSERT_EQ(1120ul, modelPtr->getNumberOfTransitions());
    ASSERT_TRUE(modelPtr->hasLabel("init"));
    ASSERT_EQ(1ul, modelPtr->getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("premium"));
    ASSERT_EQ(64ul, modelPtr->getStates("premium").getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("minimum"));
    ASSERT_EQ(132ul, modelPtr->getStates("minimum").getNumberOfSetBits());
    ASSERT_EQ(1ul, modelPtr->getNumberOfRewardModels());
    ASSERT_TRUE(modelPtr->hasRewardModel("num_repairs"));
    ASSERT_TRUE(!modelPtr->getRewardModel("num_repairs").hasStateRewards());
    ASSERT_TRUE(modelPtr->getRewardModel("num_repairs").hasStateActionRewards());
    ASSERT_TRUE(!modelPtr->getRewardModel("num_repairs").isAllZero());
}

TEST(DirectEncodingParserTest, MarkovAutomatonParsing) {
    std::shared_ptr<storm::models::sparse::Model<double>> modelPtr =
        storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/ma/jobscheduler.drn");
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> ma = modelPtr->as<storm::models::sparse::MarkovAutomaton<double>>();

    // Test if parsed correctly.
    ASSERT_EQ(storm::models::ModelType::MarkovAutomaton, modelPtr->getType());
    ASSERT_EQ(17ul, ma->getNumberOfStates());
    ASSERT_EQ(25ul, ma->getNumberOfTransitions());
    ASSERT_EQ(19ul, ma->getNumberOfChoices());
    ASSERT_EQ(10ul, ma->getMarkovianStates().getNumberOfSetBits());
    ASSERT_EQ(5, ma->getMaximalExitRate());
    ASSERT_EQ(1ul, ma->getNumberOfRewardModels());
    ASSERT_TRUE(ma->hasRewardModel("avg_waiting_time"));
    ASSERT_TRUE(ma->getRewardModel("avg_waiting_time").hasStateRewards());
    ASSERT_TRUE(!ma->getRewardModel("avg_waiting_time").hasStateActionRewards());
    ASSERT_TRUE(!ma->getRewardModel("avg_waiting_time").isAllZero());
    ASSERT_TRUE(modelPtr->hasLabel("init"));
    ASSERT_EQ(1ul, modelPtr->getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("one_job_finished"));
    ASSERT_EQ(6ul, modelPtr->getStates("one_job_finished").getNumberOfSetBits());
}

TEST(DirectEncodingParserTest, IntervalDtmcTest) {
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr =
        storm::parser::parseDirectEncodingModel<storm::Interval>(STORM_TEST_RESOURCES_DIR "/idtmc/brp-16-2.drn");
    std::shared_ptr<storm::models::sparse::Dtmc<storm::Interval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    ASSERT_EQ(613ul, dtmc->getNumberOfStates());
    EXPECT_TRUE(modelPtr->hasUncertainty());

    std::shared_ptr<storm::models::ModelBase> modelBasePtr =
        storm::parser::parseDirectEncodingModel(STORM_TEST_RESOURCES_DIR "/idtmc/brp-16-2.drn", storm::parser::DirectEncodingValueType::Default);
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelBasePtr->getType());
    dtmc = modelBasePtr->as<storm::models::sparse::Dtmc<storm::Interval>>();
    ASSERT_EQ(613ul, dtmc->getNumberOfStates());
    EXPECT_TRUE(modelPtr->hasUncertainty());
}

TEST(DirectEncodingParserTest, RationalIntervalDtmcTest) {
    std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> modelPtr =
        storm::parser::parseDirectEncodingModel<storm::RationalInterval>(STORM_TEST_RESOURCES_DIR "/idtmc/rational-brp-16-2.drn");
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalInterval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::RationalInterval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    ASSERT_EQ(613ul, dtmc->getNumberOfStates());
    EXPECT_TRUE(modelPtr->hasUncertainty());

    std::shared_ptr<storm::models::ModelBase> modelBasePtr =
        storm::parser::parseDirectEncodingModel(STORM_TEST_RESOURCES_DIR "/idtmc/rational-brp-16-2.drn", storm::parser::DirectEncodingValueType::Default);
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelBasePtr->getType());
    dtmc = modelBasePtr->as<storm::models::sparse::Dtmc<storm::RationalInterval>>();
    ASSERT_EQ(613ul, dtmc->getNumberOfStates());
    EXPECT_TRUE(modelPtr->hasUncertainty());
}

TEST(DirectEncodingParserTest, PomdpParsing) {
    std::shared_ptr<storm::models::sparse::Model<double>> modelPtr =
        storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/pomdp/maze2_sl0.drn");

    // Test if parsed correctly.
    ASSERT_EQ(storm::models::ModelType::Pomdp, modelPtr->getType());
    ASSERT_EQ(15ul, modelPtr->getNumberOfStates());
    ASSERT_EQ(66ul, modelPtr->getNumberOfTransitions());
    ASSERT_EQ(54ul, modelPtr->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());
    ASSERT_TRUE(modelPtr->hasLabel("init"));
    ASSERT_EQ(1ul, modelPtr->getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("bad"));
    ASSERT_EQ(2ul, modelPtr->getStates("bad").getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("goal"));
    ASSERT_EQ(1ul, modelPtr->getStates("goal").getNumberOfSetBits());
    ASSERT_EQ(1ul, modelPtr->getNumberOfRewardModels());
    ASSERT_TRUE(modelPtr->hasRewardModel("rew"));
    ASSERT_TRUE(!modelPtr->getRewardModel("rew").hasStateRewards());
    ASSERT_TRUE(modelPtr->getRewardModel("rew").hasStateActionRewards());
    ASSERT_TRUE(!modelPtr->getRewardModel("rew").isAllZero());

    modelPtr = storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/pomdp/maze2_sl0_no_rew.drn");

    // Test if parsed correctly.
    ASSERT_EQ(storm::models::ModelType::Pomdp, modelPtr->getType());
    ASSERT_EQ(15ul, modelPtr->getNumberOfStates());
    ASSERT_EQ(66ul, modelPtr->getNumberOfTransitions());
    ASSERT_EQ(54ul, modelPtr->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());
    ASSERT_TRUE(modelPtr->hasLabel("init"));
    ASSERT_EQ(1ul, modelPtr->getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("bad"));
    ASSERT_EQ(2ul, modelPtr->getStates("bad").getNumberOfSetBits());
    ASSERT_TRUE(modelPtr->hasLabel("goal"));
    ASSERT_EQ(1ul, modelPtr->getStates("goal").getNumberOfSetBits());
    ASSERT_EQ(0ul, modelPtr->getNumberOfRewardModels());
    ASSERT_FALSE(modelPtr->hasRewardModel());
}

TEST(DirectEncodingParserTest, CompressedParsing) {
#ifndef STORM_HAVE_LIBARCHIVE
    STORM_SILENT_EXPECT_THROW(storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.drn.gz"),
                              storm::exceptions::MissingLibraryException);
    STORM_SILENT_EXPECT_THROW(storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.drn.xz"),
                              storm::exceptions::MissingLibraryException);
    GTEST_SKIP() << "libarchive not available.";
#endif
    {
        auto modelPtr = storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.drn.gz");
        auto dtmc = modelPtr->as<storm::models::sparse::Dtmc<double>>();
        ASSERT_EQ(677ul, dtmc->getNumberOfStates());
    }
    {
        auto modelPtr = storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.drn.xz");
        auto dtmc = modelPtr->as<storm::models::sparse::Dtmc<double>>();
        ASSERT_EQ(677ul, dtmc->getNumberOfStates());
    }
}