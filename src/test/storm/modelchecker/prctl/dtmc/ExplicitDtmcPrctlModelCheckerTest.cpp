#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/storage/expressions/ExpressionManager.h"

TEST(ExplicitDtmcPrctlModelCheckerTest, Die) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel = storm::parser::AutoParser<>::parseModel(
        STORM_TEST_RESOURCES_DIR "/tra/die.tra", STORM_TEST_RESOURCES_DIR "/lab/die.lab", "", STORM_TEST_RESOURCES_DIR "/rew/die.coin_flips.trans.rew");

    storm::Environment env;
    double const precision = 1e-6;
    // Increase precision a little to get more accurate results
    env.solver().setLinearEquationSolverPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));

    // A parser that we use for conveniently constructing the formulas.

    auto expManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::parser::FormulaParser formulaParser(expManager);

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);

    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    ASSERT_EQ(dtmc->getNumberOfStates(), 13ull);
    ASSERT_EQ(dtmc->getNumberOfTransitions(), 20ull);

    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"one\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 6.0, quantitativeResult1[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"two\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 6.0, quantitativeResult2[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"three\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 6.0, quantitativeResult3[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"done\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(11.0 / 3.0, quantitativeResult4[0], precision);
}

TEST(ExplicitDtmcPrctlModelCheckerTest, Crowds) {
    storm::Environment env;
    double const precision = 1e-6;
    // Increase precision a little to get more accurate results
    env.solver().setLinearEquationSolverPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));

    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/crowds5_5.tra", STORM_TEST_RESOURCES_DIR "/lab/crowds5_5.lab", "", "");

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);

    // A parser that we use for conveniently constructing the formulas.

    auto expManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::parser::FormulaParser formulaParser(expManager);

    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    ASSERT_EQ(8607ull, dtmc->getNumberOfStates());
    ASSERT_EQ(15113ull, dtmc->getNumberOfTransitions());

    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observe0Greater1\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.3328800375801578281, quantitativeResult1[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observeIGreater1\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.1522194965, quantitativeResult2[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observeOnlyTrueSender\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.32153724292835045, quantitativeResult3[0], precision);
}

TEST(ExplicitDtmcPrctlModelCheckerTest, SynchronousLeader) {
    storm::Environment env;
    double const precision = 1e-6;
    // Increase precision a little to get more accurate results
    env.solver().setLinearEquationSolverPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));

    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/leader4_8.tra", STORM_TEST_RESOURCES_DIR "/lab/leader4_8.lab", "",
                                                STORM_TEST_RESOURCES_DIR "/rew/leader4_8.pick.trans.rew");

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);

    // A parser that we use for conveniently constructing the formulas.

    auto expManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::parser::FormulaParser formulaParser(expManager);

    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    ASSERT_EQ(12400ull, dtmc->getNumberOfStates());
    ASSERT_EQ(16495ull, dtmc->getNumberOfTransitions());

    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"elected\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0, quantitativeResult1[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("P=? [F<=20 \"elected\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.9999965911265462636, quantitativeResult2[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"elected\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0448979591836789, quantitativeResult3[0], precision);
}
