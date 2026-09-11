#include "storm-config.h"
#include "storm/environment/Environment.h"
#include "test/storm_gtest.h"

#include <algorithm>

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

// Regression test for https://github.com/moves-rwth/storm/issues/91: extracting a *sparse* quotient from a
// symbolic (dd) bisimulation used to keep redundant nondeterministic choices that became identical distributions
// after collapsing states into blocks, whereas the sparse engine's own bisimulation implementation already
// deduplicated them. This made "-e dd-to-sparse" quotients gratuitously larger (in choice/transition count, not
// state count) than the equivalent quotient computed directly by the sparse engine. Values below were confirmed
// to match storm's own (post-fix) sparse-engine bisimulation on the same model/property.
TYPED_TEST(SymbolicModelBisimulationDecomposition, SparseQuotientChoiceDeduplication) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm");
    program = program.preprocess("COL=1,TRANS_TIME_MAX=10");

    storm::parser::FormulaParser formulaParser(program);
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F col=1]");

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model = storm::builder::DdPrismModelBuilder<DdType, double>().build(program, *formula);

    model->getManager().execute([&]() {
        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        formulas.push_back(formula);

        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, formulas, storm::storage::BisimulationType::Strong);
        decomposition.compute();

        // Extracting a dd-format quotient does not go through the sparse quotient extractor at all, so it cannot
        // exercise the bug/fix; only the sparse-format extraction below does.
        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Sparse);

        ASSERT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        EXPECT_FALSE(quotient->isSymbolicModel());

        auto mdp = quotient->as<storm::models::sparse::Mdp<double>>();
        ASSERT_NE(nullptr, mdp);

        EXPECT_EQ(15ul, quotient->getNumberOfStates());
        EXPECT_EQ(24ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(24ul, mdp->getNumberOfChoices());
    });
}

// Regression test for the review comment on https://github.com/stormchecker/storm/pull/991: when removing
// redundant (duplicate) choices while building a *sparse* quotient, choices with equal transition probabilities
// but different state-action rewards must not be merged, since that would silently drop the rewards of the
// removed choice. The model below has three choices in state 0 that all induce the same distribution (to the
// goal state s=1) but carry rewards 0, 1 and 2, respectively; only choices that coincide w.r.t. both the
// distribution and all state-action rewards may be merged.
TYPED_TEST(SymbolicModelBisimulationDecomposition, SparseQuotientChoiceDeduplicationRewards) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/reward_distinct.nm");

    storm::parser::FormulaParser formulaParser(program);
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Rmax=? [F \"goal\"]");

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model = storm::builder::DdPrismModelBuilder<DdType, double>().build(program, *formula);

    model->getManager().execute([&]() {
        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        formulas.push_back(formula);

        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, formulas, storm::storage::BisimulationType::Strong);
        decomposition.compute();

        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Sparse);

        ASSERT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        EXPECT_FALSE(quotient->isSymbolicModel());

        auto mdp = quotient->as<storm::models::sparse::Mdp<double>>();
        ASSERT_NE(nullptr, mdp);

        // Block {0} keeps all three choices (identical distribution, but rewards 0, 1 and 2 must be preserved);
        // block {1} keeps its single choice. Without the reward-aware deduplication the three choices of
        // block {0} would be merged into one.
        EXPECT_EQ(2ul, quotient->getNumberOfStates());
        EXPECT_EQ(4ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(4ul, mdp->getNumberOfChoices());

        auto const& quotientRewardModel = mdp->getRewardModel("r");
        EXPECT_TRUE(quotientRewardModel.hasStateActionRewards());
        auto rewards = quotientRewardModel.getStateActionRewardVector();
        std::sort(rewards.begin(), rewards.end());
        std::vector<double> expectedRewards = {0.0, 0.0, 1.0, 2.0};
        EXPECT_EQ(expectedRewards, rewards);
    });
}

// Regression test for the review comment on https://github.com/stormchecker/storm/pull/991: a single choice
// may reach several original states of the same block, which the sparse quotient extractor stores as multiple
// matrix entries with the same column for that choice. The duplicate-choice removal must aggregate these
// entries first; otherwise two choices that induce the same distribution over blocks are not recognized as
// duplicates. Here the two choices of state 0 both reach the block {1, 2} with probability 1 (via different
// per-state probabilities 0.5/0.5 and 0.3/0.7), so after quotienting they are identical and must be merged.
TYPED_TEST(SymbolicModelBisimulationDecomposition, SparseQuotientChoiceDeduplicationSameBlock) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/choice_dedup_same_block.nm");

    storm::parser::FormulaParser formulaParser(program);
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"goal\"]");

    std::shared_ptr<storm::models::symbolic::Model<DdType, double>> model = storm::builder::DdPrismModelBuilder<DdType, double>().build(program, *formula);

    model->getManager().execute([&]() {
        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        formulas.push_back(formula);

        storm::dd::BisimulationDecomposition<DdType, double> decomposition(*model, formulas, storm::storage::BisimulationType::Strong);
        decomposition.compute();

        std::shared_ptr<storm::models::Model<double>> quotient = decomposition.getQuotient(storm::dd::bisimulation::QuotientFormat::Sparse);

        ASSERT_EQ(storm::models::ModelType::Mdp, quotient->getType());
        EXPECT_FALSE(quotient->isSymbolicModel());

        auto mdp = quotient->as<storm::models::sparse::Mdp<double>>();
        ASSERT_NE(nullptr, mdp);

        // Blocks {0} and {1, 2}: the two choices of state 0 are identical distributions over blocks and must be
        // merged into one, otherwise the quotient would have an extra choice.
        EXPECT_EQ(2ul, quotient->getNumberOfStates());
        EXPECT_EQ(2ul, quotient->getNumberOfTransitions());
        EXPECT_EQ(2ul, mdp->getNumberOfChoices());
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
