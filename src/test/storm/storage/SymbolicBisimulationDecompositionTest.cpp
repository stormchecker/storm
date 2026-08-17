#include "storm-config.h"
#include "storm/environment/Environment.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/DdPrismModelBuilder.h"
#include "storm/modelchecker/prctl/SymbolicDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SymbolicMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/solver/SymbolicLinearEquationSolver.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/dd/bisimulation/BisimulationDecomposition.h"

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
class SymbolicModelBisimulationDecomposition : public ::testing::Test {
   public:
    void SetUp() override {
        TestType::checkLibraryAvailable();
    }

    storm::Environment env;

    static const storm::dd::DdType DdType = TestType::DdType;
};

typedef ::testing::Types<Cudd, Sylvan> TestingTypes;
TYPED_TEST_SUITE(SymbolicModelBisimulationDecomposition, TestingTypes, );

TYPED_TEST(SymbolicModelBisimulationDecomposition, Die) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model = storm::builder::DdPrismModelBuilder<DdType, double>().build(this->env, program);
    model->getManager().execute([&]() {
        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, storm::storage::BisimulationType::Strong);
        decomposition.compute();
        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(11ul, quotient->getNumberOfStates());
        EXPECT_EQ(17ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Dtmc, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());

        storm::parser::FormulaParser formulaParser;
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"two\"]");

        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        formulas.push_back(formula);

        storm::dd::BisimulationDecomposition<DdType, double> decomposition2(*model, formulas, storm::storage::BisimulationType::Strong);
        decomposition2.compute();
        quotient = decomposition2.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(5ul, quotient->getNumberOfStates());
        EXPECT_EQ(8ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Dtmc, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());
    });
}

TYPED_TEST(SymbolicModelBisimulationDecomposition, DiePartialQuotient) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model = storm::builder::DdPrismModelBuilder<DdType, double>().build(this->env, program);

    model->getManager().execute([&]() {
        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, storm::storage::BisimulationType::Strong);

        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);
        ASSERT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        ASSERT_TRUE(quotient->isSymbolicModel());

        std::shared_ptr<storm::models::symbolic::Mdp<DdType, double>> quotientMdp = quotient->as<storm::models::symbolic::Mdp<DdType, double>>();

        storm::modelchecker::SymbolicMdpPrctlModelChecker<storm::models::symbolic::Mdp<DdType, double>> checker(*quotientMdp);

        storm::parser::FormulaParser formulaParser;
        std::shared_ptr<storm::logic::Formula const> minFormula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"one\"]");
        std::shared_ptr<storm::logic::Formula const> maxFormula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"one\"]");

        std::pair<double, double> resultBounds;

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(*minFormula);
        result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(quotientMdp->getReachableStates(), quotientMdp->getInitialStates()));
        resultBounds.first = result->asQuantitativeCheckResult<double>().sum();
        result = checker.check(*maxFormula);
        result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(quotientMdp->getReachableStates(), quotientMdp->getInitialStates()));
        resultBounds.second = result->asQuantitativeCheckResult<double>().sum();

        EXPECT_EQ(resultBounds.first, storm::utility::zero<double>());
        EXPECT_EQ(resultBounds.second, storm::utility::one<double>());

        // Perform only one step.
        decomposition.compute(1);

        quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);
        ASSERT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        ASSERT_TRUE(quotient->isSymbolicModel());
        quotientMdp = quotient->as<storm::models::symbolic::Mdp<DdType, double>>();

        storm::modelchecker::SymbolicMdpPrctlModelChecker<storm::models::symbolic::Mdp<DdType, double>> checker2(*quotientMdp);

        result = checker2.check(*minFormula);
        result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(quotientMdp->getReachableStates(), quotientMdp->getInitialStates()));
        resultBounds.first = result->asQuantitativeCheckResult<double>().sum();
        result = checker2.check(*maxFormula);
        result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(quotientMdp->getReachableStates(), quotientMdp->getInitialStates()));
        resultBounds.second = result->asQuantitativeCheckResult<double>().sum();

        EXPECT_EQ(resultBounds.first, storm::utility::zero<double>());
        EXPECT_NEAR(resultBounds.second, static_cast<double>(1) / 3, 1e-6);

        // Perform only one step.
        decomposition.compute(1);

        quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);
        ASSERT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        ASSERT_TRUE(quotient->isSymbolicModel());
        quotientMdp = quotient->as<storm::models::symbolic::Mdp<DdType, double>>();

        storm::modelchecker::SymbolicMdpPrctlModelChecker<storm::models::symbolic::Mdp<DdType, double>> checker3(*quotientMdp);

        result = checker3.check(*minFormula);
        result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(quotientMdp->getReachableStates(), quotientMdp->getInitialStates()));
        resultBounds.first = result->asQuantitativeCheckResult<double>().sum();
        result = checker3.check(*maxFormula);
        result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(quotientMdp->getReachableStates(), quotientMdp->getInitialStates()));
        resultBounds.second = result->asQuantitativeCheckResult<double>().sum();

        EXPECT_NEAR(resultBounds.first, static_cast<double>(1) / 6, 1e-6);
        EXPECT_NEAR(resultBounds.second, static_cast<double>(1) / 6, 1e-6);
        EXPECT_NEAR(resultBounds.first, resultBounds.second, 1e-6);

        decomposition.compute(1);

        quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);
        ASSERT_EQ(storm::models::ModelType::Dtmc, quotient->getType());
        ASSERT_TRUE(quotient->isSymbolicModel());
        std::shared_ptr<storm::models::symbolic::Dtmc<DdType, double>> quotientDtmc = quotient->as<storm::models::symbolic::Dtmc<DdType, double>>();

        storm::modelchecker::SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<DdType, double>> checker4(*quotientDtmc);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"one\"]");

        result = checker4.check(*formula);
        result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(quotientDtmc->getReachableStates(), quotientDtmc->getInitialStates()));
        resultBounds.first = resultBounds.second = result->asQuantitativeCheckResult<double>().sum();

        EXPECT_NEAR(resultBounds.first, static_cast<double>(1) / 6, 1e-6);
    });
}

TYPED_TEST(SymbolicModelBisimulationDecomposition, Crowds) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::storage::SymbolicModelDescription smd = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm");

    // Preprocess model to substitute all constants.
    smd = smd.preprocess();

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model =
        storm::builder::DdPrismModelBuilder<DdType, double>().build(this->env, smd.asPrismProgram());

    model->getManager().execute([&]() {
        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, storm::storage::BisimulationType::Strong);
        decomposition.compute();
        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(2007ul, quotient->getNumberOfStates());
        EXPECT_EQ(3738ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Dtmc, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());

        storm::parser::FormulaParser formulaParser;
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observe0Greater1\"]");

        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        formulas.push_back(formula);

        storm::dd::BisimulationDecomposition<DdType, double> decomposition2(*model, formulas, storm::storage::BisimulationType::Strong);
        decomposition2.compute();
        quotient = decomposition2.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(65ul, quotient->getNumberOfStates());
        EXPECT_EQ(105ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Dtmc, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());
    });
}

TYPED_TEST(SymbolicModelBisimulationDecomposition, TwoDice) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model = storm::builder::DdPrismModelBuilder<DdType, double>().build(this->env, program);

    model->getManager().execute([&]() {
        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, storm::storage::BisimulationType::Strong);
        decomposition.compute();
        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(77ul, quotient->getNumberOfStates());
        EXPECT_EQ(210ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());
        EXPECT_EQ(116ul, (quotient->as<storm::models::symbolic::Mdp<DdType, double>>()->getNumberOfChoices()));

        storm::parser::FormulaParser formulaParser;
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"two\"]");

        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        formulas.push_back(formula);

        storm::dd::BisimulationDecomposition<DdType, double> decomposition2(*model, formulas, storm::storage::BisimulationType::Strong);
        decomposition2.compute();
        quotient = decomposition2.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(11ul, quotient->getNumberOfStates());
        EXPECT_EQ(34ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());
        EXPECT_EQ(19ul, (quotient->as<storm::models::symbolic::Mdp<DdType, double>>()->getNumberOfChoices()));
    });
}

TYPED_TEST(SymbolicModelBisimulationDecomposition, AsynchronousLeader) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::storage::SymbolicModelDescription smd = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader4.nm");

    // Preprocess model to substitute all constants.
    smd = smd.preprocess();

    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Rmax=? [F \"elected\"]");

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model = storm::builder::DdPrismModelBuilder<DdType, double>().build(
        this->env, smd.asPrismProgram(), typename storm::builder::DdPrismModelBuilder<DdType, double>::Options(*formula));

    model->getManager().execute([&]() {
        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, storm::storage::BisimulationType::Strong);
        decomposition.compute();
        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(252ul, quotient->getNumberOfStates());
        EXPECT_EQ(624ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());
        EXPECT_EQ(500ul, (quotient->as<storm::models::symbolic::Mdp<DdType, double>>()->getNumberOfChoices()));

        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        formulas.push_back(formula);

        storm::dd::BisimulationDecomposition<DdType, double> decomposition2(*model, formulas, storm::storage::BisimulationType::Strong);
        decomposition2.compute();
        quotient = decomposition2.getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);

        EXPECT_EQ(1107ul, quotient->getNumberOfStates());
        EXPECT_EQ(2684ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        EXPECT_TRUE(quotient->isSymbolicModel());
        EXPECT_EQ(2152ul, (quotient->as<storm::models::symbolic::Mdp<DdType, double>>()->getNumberOfChoices()));
    });
}
