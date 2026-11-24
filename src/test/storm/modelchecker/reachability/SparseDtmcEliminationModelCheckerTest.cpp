#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/reachability/SparseDtmcEliminationModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"

TEST(SparseDtmcEliminationModelCheckerTest, Die) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel = storm::parser::AutoParser<>::parseModel(
        STORM_TEST_RESOURCES_DIR "/tra/die.tra", STORM_TEST_RESOURCES_DIR "/lab/die.lab", "", STORM_TEST_RESOURCES_DIR "/rew/die.coin_flips.trans.rew");

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);

    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    ASSERT_EQ(dtmc->getNumberOfStates(), 13ull);
    ASSERT_EQ(dtmc->getNumberOfTransitions(), 20ull);

    storm::modelchecker::SparseDtmcEliminationModelChecker<storm::models::sparse::Dtmc<double>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"one\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 6.0, quantitativeResult1[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"two\"]");

    result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 6.0, quantitativeResult2[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"three\"]");

    result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 6.0, quantitativeResult3[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"done\"]");

    result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(11.0 / 3.0, quantitativeResult4[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
}

TEST(SparseDtmcEliminationModelCheckerTest, Crowds) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/crowds5_5.tra", STORM_TEST_RESOURCES_DIR "/lab/crowds5_5.lab", "", "");

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);

    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    ASSERT_EQ(8607ull, dtmc->getNumberOfStates());
    ASSERT_EQ(15113ull, dtmc->getNumberOfTransitions());

    storm::modelchecker::SparseDtmcEliminationModelChecker<storm::models::sparse::Dtmc<double>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observe0Greater1\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.3328800375801578281, quantitativeResult1[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observeIGreater1\"]");

    result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.1522194965, quantitativeResult2[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observeOnlyTrueSender\"]");

    result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.32153724292835045, quantitativeResult3[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observe0Greater1\" || F \"observeIGreater1\"]");

    result = checker.check(storm::modelchecker::CheckTask<storm::logic::Formula>(*formula).setOnlyInitialStatesRelevant(true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.15330064292476167, quantitativeResult4[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"observeOnlyTrueSender\" || F \"observe0Greater1\"]");

    result = checker.check(storm::modelchecker::CheckTask<storm::logic::Formula>(*formula).setOnlyInitialStatesRelevant(true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult5 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.96592521978041668, quantitativeResult5[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
}

STORM_EXPENSIVE_TEST(SparseDtmcEliminationModelCheckerTest, SynchronousLeader) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/leader4_8.tra", STORM_TEST_RESOURCES_DIR "/lab/leader4_8.lab", "",
                                                STORM_TEST_RESOURCES_DIR "/rew/leader4_8.pick.trans.rew");

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Dtmc);
    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = abstractModel->as<storm::models::sparse::Dtmc<double>>();

    ASSERT_EQ(12400ull, dtmc->getNumberOfStates());
    ASSERT_EQ(16495ull, dtmc->getNumberOfTransitions());

    storm::modelchecker::SparseDtmcEliminationModelChecker<storm::models::sparse::Dtmc<double>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"elected\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0, quantitativeResult1[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"elected\"]");

    result = checker.check(*formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0448979, quantitativeResult2[0], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
}
