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
namespace {
/*!
 * Builds a small MDP that exercises the fixed point computed by performProb1E.
 */
storm::storage::SparseMatrix<double> buildProb1ETestModel(std::vector<uint_fast64_t>& rowGroupIndices) {
    uint64_t const numberOfStates = 10;
    storm::storage::SparseMatrixBuilder<double> builder(0, numberOfStates, 0, false, true, numberOfStates);
    auto newState = [&builder, &rowGroupIndices](uint64_t row) {
        rowGroupIndices.push_back(row);
        builder.newRowGroup(row);
    };
    newState(0);  // s0
    builder.addNextValue(0, 1, 1.0);
    newState(1);  // s1
    builder.addNextValue(1, 2, 1.0);
    newState(2);  // s2: leaving the safe states with positive probability or staying put forever
    builder.addNextValue(2, 3, 0.5);
    builder.addNextValue(2, 6, 0.5);
    builder.addNextValue(3, 2, 1.0);
    newState(4);  // s3: one choice towards the goal and one towards s6
    builder.addNextValue(4, 4, 1.0);
    builder.addNextValue(5, 6, 1.0);
    newState(6);  // s4
    builder.addNextValue(6, 5, 1.0);
    newState(7);  // s5
    builder.addNextValue(7, 5, 1.0);
    newState(8);  // s6
    builder.addNextValue(8, 6, 1.0);
    newState(9);  // s7: start of a chain that can only end up in s6
    builder.addNextValue(9, 8, 1.0);
    newState(10);  // s8
    builder.addNextValue(10, 9, 1.0);
    newState(11);  // s9
    builder.addNextValue(11, 6, 1.0);
    rowGroupIndices.push_back(12);
    return builder.build(12, numberOfStates, numberOfStates);
}

storm::storage::BitVector asBitVector(uint64_t size, std::vector<uint64_t> const& setIndices) {
    storm::storage::BitVector result(size);
    for (uint64_t index : setIndices) {
        result.set(index, true);
    }
    return result;
}
}  // namespace

TEST(GraphTestExplicitProb1E, SafetyAndReachabilityAlternation) {
    std::vector<uint_fast64_t> rowGroupIndices;
    auto matrix = buildProb1ETestModel(rowGroupIndices);
    auto backwardTransitions = matrix.transpose(true);
    // s6 is the only state that does not satisfy phi, s5 is the goal.
    auto phiStates = ~asBitVector(10, {6});
    auto psiStates = asBitVector(10, {5});

    // s7, s8 and s9 are removed one after the other since they can only reach s6.
    // Afterwards, s2 still has a choice that stays in the candidates (the self loop), but it can not reach s5 anymore.
    // Removing s2 for that reason in turn makes s1 and then s0 unsafe.
    auto result = storm::utility::graph::performProb1E(matrix, rowGroupIndices, backwardTransitions, phiStates, psiStates);
    EXPECT_EQ(asBitVector(10, {3, 4, 5}), result);

    // performProb01Max has to agree, in particular since it narrows down the phi states before calling performProb1E.
    auto prob01 = storm::utility::graph::performProb01Max(matrix, rowGroupIndices, backwardTransitions, phiStates, psiStates);
    EXPECT_EQ(asBitVector(10, {6, 7, 8, 9}), prob01.first);
    EXPECT_EQ(result, prob01.second);
}

TEST(GraphTestExplicitProb1E, ChoiceConstraint) {
    std::vector<uint_fast64_t> rowGroupIndices;
    auto matrix = buildProb1ETestModel(rowGroupIndices);
    auto backwardTransitions = matrix.transpose(true);
    auto phiStates = ~asBitVector(10, {6});
    auto psiStates = asBitVector(10, {5});

    // Disabling row 4 takes away the only choice of s3 that reaches the goal, so s3 is removed as well.
    auto choiceConstraint = ~asBitVector(matrix.getRowCount(), {4});
    auto result = storm::utility::graph::performProb1E(matrix, rowGroupIndices, backwardTransitions, phiStates, psiStates, choiceConstraint);
    EXPECT_EQ(asBitVector(10, {4, 5}), result);

    // Disabling the self loop of s2 (row 3) does not change the result, it only removes s2 for a different reason.
    choiceConstraint = ~asBitVector(matrix.getRowCount(), {3});
    result = storm::utility::graph::performProb1E(matrix, rowGroupIndices, backwardTransitions, phiStates, psiStates, choiceConstraint);
    EXPECT_EQ(asBitVector(10, {3, 4, 5}), result);
}

TEST(GraphTestExplicitProb1E, PsiStatesOutsidePhiStates) {
    std::vector<uint_fast64_t> rowGroupIndices;
    auto matrix = buildProb1ETestModel(rowGroupIndices);
    auto backwardTransitions = matrix.transpose(true);
    // Now s6 is the goal, but it is still not a phi state.
    auto phiStates = ~asBitVector(10, {6});
    auto psiStates = asBitVector(10, {6});

    // s3 reaches s6 via row 5, so s2 reaches it under every outcome of row 2, and so do s1 and s0.
    // s4 and s5 can never reach s6, which also takes away the choice of s3 that leads to s4.
    auto result = storm::utility::graph::performProb1E(matrix, rowGroupIndices, backwardTransitions, phiStates, psiStates);
    EXPECT_EQ(asBitVector(10, {0, 1, 2, 3, 6, 7, 8, 9}), result);
}
