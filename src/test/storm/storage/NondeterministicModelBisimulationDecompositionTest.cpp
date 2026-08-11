#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/bisimulation/NondeterministicModelBisimulationDecomposition.h"

namespace {
static constexpr double DefaultTestTolerance = 1e-6;
}

TEST(NondeterministicModelBisimulationDecomposition, TwoDice) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");

    // Build the die model without its reward model.
    std::shared_ptr<storm::models::sparse::Model<double>> model =
        storm::builder::ExplicitModelBuilder<double>(program, storm::generator::NextStateGeneratorOptions(false, true)).build();

    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = model->as<storm::models::sparse::Mdp<double>>();

    using OptionsType = typename storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>>::Options;

    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim(
        *mdp, OptionsType::preservingAllLabels(DefaultTestTolerance));
    ASSERT_NO_THROW(bisim.computeBisimulationDecomposition());
    std::shared_ptr<storm::models::sparse::Model<double>> result;
    ASSERT_NO_THROW(result = bisim.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Mdp, result->getType());
    EXPECT_EQ(77ul, result->getNumberOfStates());
    EXPECT_EQ(183ul, result->getNumberOfTransitions());
    EXPECT_EQ(97ul, result->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());

    OptionsType options = OptionsType::preservingAllLabels(DefaultTestTolerance);
    options.respectedAtomicPropositions = std::set<std::string>({"two"});

    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim2(*mdp, options);
    ASSERT_NO_THROW(bisim2.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim2.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Mdp, result->getType());
    EXPECT_EQ(11ul, result->getNumberOfStates());
    EXPECT_EQ(26ul, result->getNumberOfTransitions());
    EXPECT_EQ(14ul, result->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"two\"]");

    OptionsType options2(*mdp, *formula, DefaultTestTolerance);

    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim3(*mdp, options2);
    ASSERT_NO_THROW(bisim3.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim3.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Mdp, result->getType());
    EXPECT_EQ(11ul, result->getNumberOfStates());
    EXPECT_EQ(26ul, result->getNumberOfTransitions());
    EXPECT_EQ(14ul, result->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());
}
