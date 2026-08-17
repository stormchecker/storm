#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/exploration/SparseExplorationModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/ExplorationSettings.h"

class SparseExplorationModelCheckerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(SparseExplorationModelCheckerTest, Dice) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    storm::modelchecker::SparseExplorationModelChecker<storm::models::sparse::Mdp<double>, uint32_t> checker(program);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"two\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.0277777612209320068, quantitativeResult1[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"two\"]");

    result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.0277777612209320068, quantitativeResult2[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"three\"]");

    result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.0555555224418640136, quantitativeResult3[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"three\"]");

    result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.0555555224418640136, quantitativeResult4[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"four\"]");

    result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult5 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.083333283662796020508, quantitativeResult5[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"four\"]");

    result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult6 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.083333283662796020508, quantitativeResult6[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());
}

TEST_F(SparseExplorationModelCheckerTest, AsynchronousLeader) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader4.nm");

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    storm::modelchecker::SparseExplorationModelChecker<storm::models::sparse::Mdp<double>, uint32_t> checker(program);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"elected\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1, quantitativeResult1[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"elected\"]");

    result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1, quantitativeResult2[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());
}

TEST_F(SparseExplorationModelCheckerTest, Cicle) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/cicle.nm");

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    storm::modelchecker::SparseExplorationModelChecker<storm::models::sparse::Mdp<double>, uint32_t> checker(program);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmax=? [ F \"done\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(storm::modelchecker::CheckTask<>(*formula, true));
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> const& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(0.875, quantitativeResult1[0], storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision());
}
