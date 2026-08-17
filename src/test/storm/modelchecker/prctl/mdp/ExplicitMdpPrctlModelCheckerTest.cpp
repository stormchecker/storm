#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/StandardMinMaxLinearEquationSolver.h"

TEST(ExplicitMdpPrctlModelCheckerTest, Dice) {
    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel =
        storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/two_dice.tra", STORM_TEST_RESOURCES_DIR "/lab/two_dice.lab", "",
                                                STORM_TEST_RESOURCES_DIR "/rew/two_dice.flip.trans.rew");
    storm::Environment env;
    double const precision = 1e-6;
    // Increase precision a little to get more accurate results
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Mdp);

    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = abstractModel->as<storm::models::sparse::Mdp<double>>();

    ASSERT_EQ(mdp->getNumberOfStates(), 169ull);
    ASSERT_EQ(mdp->getNumberOfTransitions(), 436ull);

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker(*mdp);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"two\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 36.0, quantitativeResult1[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"two\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 36.0, quantitativeResult2[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"three\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(2.0 / 36.0, quantitativeResult3[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"three\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(2.0 / 36.0, quantitativeResult4[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"four\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult5 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(3.0 / 36.0, quantitativeResult5[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"four\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult6 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(3.0 / 36.0, quantitativeResult6[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Rmin=? [F \"done\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult7 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(22.0 / 3.0, quantitativeResult7[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Rmax=? [F \"done\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult8 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(22.0 / 3.0, quantitativeResult8[0], precision);

    abstractModel = storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/two_dice.tra", STORM_TEST_RESOURCES_DIR "/lab/two_dice.lab",
                                                            STORM_TEST_RESOURCES_DIR "/rew/two_dice.flip.state.rew", "");

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Mdp);

    std::shared_ptr<storm::models::sparse::Mdp<double>> stateRewardMdp = abstractModel->as<storm::models::sparse::Mdp<double>>();

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> stateRewardModelChecker(*mdp);

    formula = formulaParser.parseSingleFormulaFromString("Rmin=? [F \"done\"]");

    result = stateRewardModelChecker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult9 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(22.0 / 3.0, quantitativeResult9[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Rmax=? [F \"done\"]");

    result = stateRewardModelChecker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult10 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(22.0 / 3.0, quantitativeResult10[0], precision);

    abstractModel = storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/two_dice.tra", STORM_TEST_RESOURCES_DIR "/lab/two_dice.lab",
                                                            STORM_TEST_RESOURCES_DIR "/rew/two_dice.flip.state.rew",
                                                            STORM_TEST_RESOURCES_DIR "/rew/two_dice.flip.trans.rew");

    ASSERT_EQ(abstractModel->getType(), storm::models::ModelType::Mdp);

    std::shared_ptr<storm::models::sparse::Mdp<double>> stateAndTransitionRewardMdp = abstractModel->as<storm::models::sparse::Mdp<double>>();

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> stateAndTransitionRewardModelChecker(*stateAndTransitionRewardMdp);

    formula = formulaParser.parseSingleFormulaFromString("Rmin=? [F \"done\"]");

    result = stateAndTransitionRewardModelChecker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult11 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(44.0 / 3.0, quantitativeResult11[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Rmax=? [F \"done\"]");

    result = stateAndTransitionRewardModelChecker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult12 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(44.0 / 3.0, quantitativeResult12[0], precision);
}

TEST(ExplicitMdpPrctlModelCheckerTest, AsynchronousLeader) {
    storm::Environment env;
    double const precision = 1e-6;
    // Increase precision a little to get more accurate results
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));

    std::shared_ptr<storm::models::sparse::Model<double>> abstractModel = storm::parser::AutoParser<>::parseModel(
        STORM_TEST_RESOURCES_DIR "/tra/leader4.tra", STORM_TEST_RESOURCES_DIR "/lab/leader4.lab", "", STORM_TEST_RESOURCES_DIR "/rew/leader4.trans.rew");

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    ASSERT_EQ(storm::models::ModelType::Mdp, abstractModel->getType());

    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = abstractModel->as<storm::models::sparse::Mdp<double>>();

    ASSERT_EQ(3172ull, mdp->getNumberOfStates());
    ASSERT_EQ(7144ull, mdp->getNumberOfTransitions());

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker(*mdp);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"elected\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0, quantitativeResult1[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"elected\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0, quantitativeResult2[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F<=25 \"elected\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 16.0, quantitativeResult3[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F<=25 \"elected\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(1.0 / 16.0, quantitativeResult4[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Rmin=? [F \"elected\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult5 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(30.0 / 7.0, quantitativeResult5[0], precision);

    formula = formulaParser.parseSingleFormulaFromString("Rmax=? [F \"elected\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeResult6 = result->asExplicitQuantitativeCheckResult<double>();

    EXPECT_NEAR(30.0 / 7.0, quantitativeResult6[0], precision);
}
