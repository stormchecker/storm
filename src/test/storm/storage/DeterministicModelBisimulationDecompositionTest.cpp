#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/storage/bisimulation/DeterministicModelBisimulationDecomposition.h"

namespace {
static constexpr double DefaultTestTolerance = 1e-6;
}

TEST(DeterministicModelBisimulationDecomposition, Die) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/die.tra", STORM_TEST_RESOURCES_DIR "/lab/die.lab", "", "");

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);
    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    using OptionsType = typename storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>>::Options;

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim(
        *dtmc, OptionsType::preservingAllLabels(DefaultTestTolerance));
    ASSERT_NO_THROW(bisim.computeBisimulationDecomposition());
    std::shared_ptr<storm::models::sparse::Model<double>> result;
    ASSERT_NO_THROW(result = bisim.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(13ul, result->getNumberOfStates());
    EXPECT_EQ(20ul, result->getNumberOfTransitions());

    OptionsType options = OptionsType::preservingAllLabels(DefaultTestTolerance);
    options.respectedAtomicPropositions = std::set<std::string>({"one"});

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim2(*dtmc, options);
    ASSERT_NO_THROW(bisim2.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim2.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(5ul, result->getNumberOfStates());
    EXPECT_EQ(8ul, result->getNumberOfTransitions());

    options.setType(storm::storage::BisimulationType::Weak);

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim3(*dtmc, options);
    ASSERT_NO_THROW(bisim3.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim3.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(5ul, result->getNumberOfStates());
    EXPECT_EQ(8ul, result->getNumberOfTransitions());

    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"one\"]");

    OptionsType options2(*dtmc, *formula, DefaultTestTolerance);

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim4(*dtmc, options2);
    ASSERT_NO_THROW(bisim4.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim4.getQuotient());
    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(5ul, result->getNumberOfStates());
    EXPECT_EQ(8ul, result->getNumberOfTransitions());
}

TEST(DeterministicModelBisimulationDecomposition, Crowds) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/crowds5_5.tra", STORM_TEST_RESOURCES_DIR "/lab/crowds5_5.lab", "", "");

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);
    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    using OptionsType = typename storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>>::Options;

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim(
        *dtmc, OptionsType::preservingAllLabels(DefaultTestTolerance));
    std::shared_ptr<storm::models::sparse::Model<double>> result;
    ASSERT_NO_THROW(bisim.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(334ul, result->getNumberOfStates());
    EXPECT_EQ(546ul, result->getNumberOfTransitions());

    OptionsType options = OptionsType::preservingAllLabels(DefaultTestTolerance);
    options.respectedAtomicPropositions = std::set<std::string>({"observe0Greater1"});

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim2(*dtmc, options);
    ASSERT_NO_THROW(bisim2.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim2.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(65ul, result->getNumberOfStates());
    EXPECT_EQ(105ul, result->getNumberOfTransitions());

    options.setType(storm::storage::BisimulationType::Weak);

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim3(*dtmc, options);
    ASSERT_NO_THROW(bisim3.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim3.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(43ul, result->getNumberOfStates());
    EXPECT_EQ(83ul, result->getNumberOfTransitions());

    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observe0Greater1\"]");

    OptionsType options3(*dtmc, *formula, DefaultTestTolerance);

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim5(*dtmc, options3);
    ASSERT_NO_THROW(bisim5.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim5.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(64ul, result->getNumberOfStates());
    EXPECT_EQ(104ul, result->getNumberOfTransitions());

    formula = formulaParser.parseSingleFormulaFromString("P=? [true U<=50 \"observe0Greater1\"] ");

    OptionsType options4(*dtmc, *formula, DefaultTestTolerance);

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Dtmc<double>> bisim6(*dtmc, options4);
    ASSERT_NO_THROW(bisim6.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim6.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Dtmc, result->getType());
    EXPECT_EQ(65ul, result->getNumberOfStates());
    EXPECT_EQ(105ul, result->getNumberOfTransitions());
}

// Suppress compiler warning on unreachable code due to skipped test
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
TEST(DeterministicModelBisimulationDecomposition, Cluster) {
    // TODO FIXME
    GTEST_SKIP() << "CTMC bisimulation currently yields unstable results.";
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", true);
    std::shared_ptr<storm::models::sparse::Model<double>> model =
        storm::builder::ExplicitModelBuilder<double>(program, storm::generator::NextStateGeneratorOptions(false, true)).build();

    ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
    std::shared_ptr<storm::models::sparse::Ctmc<double>> ctmc = model->as<storm::models::sparse::Ctmc<double>>();
    ASSERT_EQ(3478ul, ctmc->getNumberOfStates());
    ASSERT_EQ(14639ul, ctmc->getNumberOfTransitions());

    using OptionsType = typename storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<double>>::Options;

    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<double>> bisim(
        *ctmc, OptionsType::preservingAllLabels(DefaultTestTolerance));
    std::shared_ptr<storm::models::sparse::Model<double>> result;
    ASSERT_NO_THROW(bisim.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Ctmc, result->getType());
    EXPECT_EQ(1731ul, result->getNumberOfStates());
    EXPECT_EQ(8619ul, result->getNumberOfTransitions());

    OptionsType options = OptionsType::preservingAllLabels(DefaultTestTolerance);
    options.respectedAtomicPropositions = std::set<std::string>({"down"});
    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<double>> bisim2(*ctmc, options);
    ASSERT_NO_THROW(bisim2.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim2.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Ctmc, result->getType());
    EXPECT_EQ(1618ul, result->getNumberOfStates());
    EXPECT_EQ(8816ul, result->getNumberOfTransitions());

    options.setType(storm::storage::BisimulationType::Weak);
    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<double>> bisim3(*ctmc, options);
    ASSERT_NO_THROW(bisim3.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim3.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Ctmc, result->getType());
    EXPECT_EQ(41ul, result->getNumberOfStates());
    EXPECT_EQ(159ul, result->getNumberOfTransitions());

    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [ F<=10000 \"down\"]");
    OptionsType options3(*ctmc, *formula, DefaultTestTolerance);
    storm::storage::DeterministicModelBisimulationDecomposition<storm::models::sparse::Ctmc<double>> bisim5(*ctmc, options3);
    ASSERT_NO_THROW(bisim5.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim5.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Ctmc, result->getType());
    EXPECT_EQ(1618ul, result->getNumberOfStates());
    EXPECT_EQ(8816ul, result->getNumberOfTransitions());
}
#pragma clang diagnostic pop
