#include "storm-config.h"
#include "storm/environment/Environment.h"
#include "test/storm_gtest.h"

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
#include "storm/utility/graph.h"

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
class GraphTestSymbolic : public ::testing::Test {
   public:
    storm::Environment env;

    static const storm::dd::DdType DdType = TestType::DdType;

   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Library Z3 not available.";
#endif
        TestType::checkLibraryAvailable();
    }
};

class GraphTestExplicit : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Library Z3 not available.";
#endif
    }
};

typedef ::testing::Types<Cudd, Sylvan> TestingTypes;
TYPED_TEST_SUITE(GraphTestSymbolic, TestingTypes, );

TYPED_TEST(GraphTestSymbolic, SymbolicProb01) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::storage::SymbolicModelDescription modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm");
    storm::prism::Program program = modelDescription.preprocess().asPrismProgram();
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = storm::builder::DdPrismModelBuilder<DdType>().build(this->env, program);

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Dtmc);

    {
        // This block is necessary, so the BDDs get disposed before the manager (contained in the model).
        std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> statesWithProbability01;

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01(*model->template as<storm::models::symbolic::Dtmc<DdType>>(),
                                                                                       model->getReachableStates(), model->getStates("observe0Greater1")));
        EXPECT_EQ(4409ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(1316ull, statesWithProbability01.second.getNonZeroCount());

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01(*model->template as<storm::models::symbolic::Dtmc<DdType>>(),
                                                                                       model->getReachableStates(), model->getStates("observeIGreater1")));
        EXPECT_EQ(1091ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(4802ull, statesWithProbability01.second.getNonZeroCount());

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01(*model->template as<storm::models::symbolic::Dtmc<DdType>>(),
                                                                                       model->getReachableStates(), model->getStates("observeOnlyTrueSender")));
        EXPECT_EQ(5829ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(1032ull, statesWithProbability01.second.getNonZeroCount());
    }
}

TYPED_TEST(GraphTestSymbolic, SymbolicProb01MinMax) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::storage::SymbolicModelDescription modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm");
    storm::prism::Program program = modelDescription.preprocess().asPrismProgram();
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = storm::builder::DdPrismModelBuilder<DdType>().build(this->env, program);

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Mdp);

    {
        // This block is necessary, so the BDDs get disposed before the manager (contained in the model).
        std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> statesWithProbability01;

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Min(*model->template as<storm::models::symbolic::Mdp<DdType>>(),
                                                                                          model->getReachableStates(), model->getStates("elected")));
        EXPECT_EQ(0ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(364ull, statesWithProbability01.second.getNonZeroCount());

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Max(*model->template as<storm::models::symbolic::Mdp<DdType>>(),
                                                                                          model->getReachableStates(), model->getStates("elected")));
        EXPECT_EQ(0ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(364ull, statesWithProbability01.second.getNonZeroCount());
    }

    modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm");
    program = modelDescription.preprocess().asPrismProgram();
    model = storm::builder::DdPrismModelBuilder<DdType>().build(this->env, program);

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Mdp);

    {
        // This block is necessary, so the BDDs get disposed before the manager (contained in the model).
        std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> statesWithProbability01;

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Min(*model->template as<storm::models::symbolic::Mdp<DdType>>(),
                                                                                          model->getReachableStates(), model->getStates("all_coins_equal_0")));
        EXPECT_EQ(77ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(149ull, statesWithProbability01.second.getNonZeroCount());

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Max(*model->template as<storm::models::symbolic::Mdp<DdType>>(),
                                                                                          model->getReachableStates(), model->getStates("all_coins_equal_0")));
        EXPECT_EQ(74ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(198ull, statesWithProbability01.second.getNonZeroCount());

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Min(*model->template as<storm::models::symbolic::Mdp<DdType>>(),
                                                                                          model->getReachableStates(), model->getStates("all_coins_equal_1")));
        EXPECT_EQ(94ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(33ull, statesWithProbability01.second.getNonZeroCount());

        ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Max(*model->template as<storm::models::symbolic::Mdp<DdType>>(),
                                                                                          model->getReachableStates(), model->getStates("all_coins_equal_1")));
        EXPECT_EQ(83ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(35ull, statesWithProbability01.second.getNonZeroCount());
    }

    modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/csma2-2.nm");
    program = modelDescription.preprocess().asPrismProgram();
    model = storm::builder::DdPrismModelBuilder<DdType>().build(this->env, program);

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Mdp);

    {
        // This block is necessary, so the BDDs get disposed before the manager (contained in the model).
        std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> statesWithProbability01;

        ASSERT_NO_THROW(statesWithProbability01 =
                            storm::utility::graph::performProb01Min(*model->template as<storm::models::symbolic::Mdp<DdType>>(), model->getReachableStates(),
                                                                    model->getStates("collision_max_backoff")));
        EXPECT_EQ(993ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(16ull, statesWithProbability01.second.getNonZeroCount());

        ASSERT_NO_THROW(statesWithProbability01 =
                            storm::utility::graph::performProb01Max(*model->template as<storm::models::symbolic::Mdp<DdType>>(), model->getReachableStates(),
                                                                    model->getStates("collision_max_backoff")));
        EXPECT_EQ(993ull, statesWithProbability01.first.getNonZeroCount());
        EXPECT_EQ(16ull, statesWithProbability01.second.getNonZeroCount());
    }
}

TEST_F(GraphTestExplicit, ExplicitProb01) {
    storm::storage::SymbolicModelDescription modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm");
    storm::prism::Program program = modelDescription.preprocess().asPrismProgram();
    std::shared_ptr<storm::models::sparse::Model<double>> model =
        storm::builder::ExplicitModelBuilder<double>(program, storm::generator::NextStateGeneratorOptions(false, true)).build();

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Dtmc);

    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01;

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01(*model->as<storm::models::sparse::Dtmc<double>>(),
                                                                                   storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                   model->getStates("observe0Greater1")));
    EXPECT_EQ(4409ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(1316ull, statesWithProbability01.second.getNumberOfSetBits());

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01(*model->as<storm::models::sparse::Dtmc<double>>(),
                                                                                   storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                   model->getStates("observeIGreater1")));
    EXPECT_EQ(1091ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(4802ull, statesWithProbability01.second.getNumberOfSetBits());

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01(*model->as<storm::models::sparse::Dtmc<double>>(),
                                                                                   storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                   model->getStates("observeOnlyTrueSender")));
    EXPECT_EQ(5829ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(1032ull, statesWithProbability01.second.getNumberOfSetBits());
}

TEST_F(GraphTestExplicit, ExplicitProb01MinMax) {
    storm::storage::SymbolicModelDescription modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm");
    storm::prism::Program program = modelDescription.preprocess().asPrismProgram();
    std::shared_ptr<storm::models::sparse::Model<double>> model =
        storm::builder::ExplicitModelBuilder<double>(program, storm::generator::NextStateGeneratorOptions(false, true)).build();

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Mdp);

    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01;

    ASSERT_NO_THROW(statesWithProbability01 =
                        storm::utility::graph::performProb01Min(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                storm::storage::BitVector(model->getNumberOfStates(), true), model->getStates("elected")));
    EXPECT_EQ(0ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(364ull, statesWithProbability01.second.getNumberOfSetBits());

    ASSERT_NO_THROW(statesWithProbability01 =
                        storm::utility::graph::performProb01Max(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                storm::storage::BitVector(model->getNumberOfStates(), true), model->getStates("elected")));
    EXPECT_EQ(0ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(364ull, statesWithProbability01.second.getNumberOfSetBits());

    modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm");
    program = modelDescription.preprocess().asPrismProgram();
    model = storm::builder::ExplicitModelBuilder<double>(program, storm::generator::NextStateGeneratorOptions(false, true)).build();

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Mdp);

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Min(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                                      storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                      model->getStates("all_coins_equal_0")));
    EXPECT_EQ(77ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(149ull, statesWithProbability01.second.getNumberOfSetBits());

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Max(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                                      storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                      model->getStates("all_coins_equal_0")));
    EXPECT_EQ(74ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(198ull, statesWithProbability01.second.getNumberOfSetBits());

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Min(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                                      storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                      model->getStates("all_coins_equal_1")));
    EXPECT_EQ(94ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(33ull, statesWithProbability01.second.getNumberOfSetBits());

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Max(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                                      storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                      model->getStates("all_coins_equal_1")));
    EXPECT_EQ(83ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(35ull, statesWithProbability01.second.getNumberOfSetBits());

    modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/csma2-2.nm");
    program = modelDescription.preprocess().asPrismProgram();
    model = storm::builder::ExplicitModelBuilder<double>(program, storm::generator::NextStateGeneratorOptions(false, true)).build();

    ASSERT_TRUE(model->getType() == storm::models::ModelType::Mdp);

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Min(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                                      storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                      model->getStates("collision_max_backoff")));
    EXPECT_EQ(993ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(16ull, statesWithProbability01.second.getNumberOfSetBits());

    ASSERT_NO_THROW(statesWithProbability01 = storm::utility::graph::performProb01Max(*model->as<storm::models::sparse::Mdp<double>>(),
                                                                                      storm::storage::BitVector(model->getNumberOfStates(), true),
                                                                                      model->getStates("collision_max_backoff")));
    EXPECT_EQ(993ull, statesWithProbability01.first.getNumberOfSetBits());
    EXPECT_EQ(16ull, statesWithProbability01.second.getNumberOfSetBits());
}