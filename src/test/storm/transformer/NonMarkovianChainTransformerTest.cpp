#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/storm.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/storage/jani/Property.h"

TEST(NonMarkovianChainTransformerTest, StreamExampleTest) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/stream2.ma");
    std::string formulasString = "Pmin=? [ F \"done\"];Pmin=? [ F<=1 \"done\"];Tmin=? [ F \"done\" ]";
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    auto model = storm::api::buildSparseModel<double>(program, formulas)->template as<storm::models::sparse::MarkovAutomaton<double>>();

    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(14ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    size_t initState = 0;

    auto labeling = model->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("done", 10));

    auto result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[1], true));
    EXPECT_NEAR(0.6487584849, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[2], true));
    EXPECT_NEAR(0.7888888889, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Keep labels
    auto transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::KeepLabels);
    ASSERT_EQ(9ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(11ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("done", 8));
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.6487584849, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(0.7888888889, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Merge labels
    transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::MergeLabels);
    ASSERT_EQ(8ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(10ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("done", 7));
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.6487584849, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(0.7888888889, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Delete labels
    transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::DeleteLabels);
    ASSERT_EQ(8ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(10ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("done", 7));
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.6487584849, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(0.7888888889, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
}

TEST(NonMarkovianChainTransformerTest, ChainElimination1ExampleTest) {
    auto model = storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/ma/chain_elimination1.drn")
                     ->template as<storm::models::sparse::MarkovAutomaton<double>>();
    std::string formulasString = "Pmin=? [ F \"Fail\"];Pmin=? [ F<=300 \"Fail\"];Tmin=? [ F \"Fail\" ]";
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulasString));

    EXPECT_EQ(43ul, model->getNumberOfStates());
    EXPECT_EQ(59ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    size_t initState = 0;

    auto labeling = model->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    for (size_t i = 10; i < 43; ++i) {
        ASSERT_TRUE(labeling.getStateHasLabel("Fail", i));
    }

    auto result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[1], true));
    EXPECT_NEAR(0.08606881472, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[2], true));
    EXPECT_NEAR(3333.333333, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Keep labels
    auto transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::KeepLabels);
    ASSERT_EQ(13ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(29ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    for (size_t i = 5; i < 13; ++i) {
        ASSERT_TRUE(labeling.getStateHasLabel("Fail", i));
    }
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.08606881472, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(3333.333333, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Merge labels
    transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::MergeLabels);
    ASSERT_EQ(8ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(24ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    for (size_t i = 0; i < 8; ++i) {
        ASSERT_TRUE(labeling.getStateHasLabel("Fail", i));
    }
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.08606881472, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(3333.333333, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Delete labels
    transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::DeleteLabels);
    ASSERT_EQ(8ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(24ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_FALSE(labeling.getStateHasLabel("Fail", 0));
    for (size_t i = 1; i < 8; ++i) {
        ASSERT_TRUE(labeling.getStateHasLabel("Fail", i));
    }
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.08606881472, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(3333.333333, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
}

TEST(NonMarkovianChainTransformerTest, ChainElimination2ExampleTest) {
    auto model = storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/ma/chain_elimination2.drn")
                     ->template as<storm::models::sparse::MarkovAutomaton<double>>();
    std::string formulasString = "Pmin=? [ F \"Fail\"];Pmin=? [ F<=300 \"Fail\"];Tmin=? [ F \"Fail\" ]";
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulasString));

    EXPECT_EQ(10ul, model->getNumberOfStates());
    EXPECT_EQ(14ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    size_t initState = 0;

    auto labeling = model->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 4));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 7));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 8));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 9));

    auto result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[1], true));
    EXPECT_NEAR(0.791015319, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formulas[2], true));
    EXPECT_NEAR(190, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Keep labels
    auto transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::KeepLabels);
    ASSERT_EQ(7ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(11ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 2));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 5));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 6));
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.791015319, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(190, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Merge labels
    transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::MergeLabels);
    ASSERT_EQ(5ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(10ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 1));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 2));
    ASSERT_FALSE(labeling.getStateHasLabel("Fail", 3));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 4));
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.791015319, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(190, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);

    // Delete labels
    transformed = storm::api::eliminateNonMarkovianChains(model, formulas, storm::transformer::EliminationLabelBehavior::DeleteLabels);
    ASSERT_EQ(5ul, transformed.first->getNumberOfStates());
    ASSERT_EQ(10ul, transformed.first->getNumberOfTransitions());
    labeling = transformed.first->getStateLabeling();
    ASSERT_TRUE(labeling.getStateHasLabel("init", initState));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 1));
    ASSERT_FALSE(labeling.getStateHasLabel("Fail", 2));
    ASSERT_FALSE(labeling.getStateHasLabel("Fail", 3));
    ASSERT_TRUE(labeling.getStateHasLabel("Fail", 4));
    EXPECT_EQ(2ul, transformed.second.size());

    result = storm::api::verifyWithSparseEngine(transformed.first, storm::api::createTask<double>(transformed.second[0], true));
    EXPECT_EQ(1, result->asExplicitQuantitativeCheckResult<double>()[initState]);
    result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[1], true));
    EXPECT_NEAR(0.791015319, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
    // result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(transformed.second[2], true));
    // EXPECT_NEAR(190, result->asExplicitQuantitativeCheckResult<double>()[initState], 1e-6);
}
