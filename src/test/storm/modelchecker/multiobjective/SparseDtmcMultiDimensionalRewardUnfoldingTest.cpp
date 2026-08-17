#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/storage/jani/Property.h"
#include "storm/utility/constants.h"

class SparseDtmcMultiDimensionalRewardUnfoldingTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(SparseDtmcMultiDimensionalRewardUnfoldingTest, cost_bounded_die) {
    storm::Environment env;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/dtmc/die.pm";
    std::string formulasAsString = "P=? [ F{\"coin_flips\"}<=2 \"two\" ] ";
    formulasAsString += "; P=? [ F{\"coin_flips\"}<=3 \"two\" ] ";
    formulasAsString += "; P=? [ F{\"coin_flips\"}<=8 \"two\" ] ";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalNumber>> dtmc =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalNumber>>();
    uint_fast64_t const initState = *dtmc->getInitialStates().begin();
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("0")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("1/8")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[2], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("21/128")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseDtmcMultiDimensionalRewardUnfoldingTest, cost_bounded_leader) {
    storm::Environment env;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm";
    std::string formulasAsString = "P=? [ F{\"num_rounds\"}<=1 \"elected\" ] ";
    formulasAsString += "; P=? [ F{\"num_rounds\"}<=2 \"elected\" ] ";
    formulasAsString += "; P=? [ F{\"num_rounds\"}>2 \"elected\" ] ";
    formulasAsString += "; P=? [ F{\"num_rounds\"}>=2,{\"num_rounds\"}<3 \"elected\" ] ";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalNumber>> dtmc =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalNumber>>();
    uint_fast64_t const initState = *dtmc->getInitialStates().begin();
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("24/25")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("624/625")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[2], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("1/625")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[3], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("24/625")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseDtmcMultiDimensionalRewardUnfoldingTest, cost_bounded_crowds) {
    storm::Environment env;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/dtmc/crowds_cost_bounded.pm";
    std::string formulasAsString = "P=? [F{\"num_runs\"}<=3,{\"observe0\"}>1 true]";
    formulasAsString += "; P=? [F{\"num_runs\"}<=3,{\"observe1\"}>1 true]";
    formulasAsString += "; R{\"observe0\"}=? [C{\"num_runs\"}<=3]";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("CrowdSize=4");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalNumber>> dtmc =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalNumber>>();
    uint_fast64_t const initState = *dtmc->getInitialStates().begin();
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("78686542099694893/1268858272000000000")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("13433618626105041/1268858272000000000")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(dtmc, storm::api::createTask<storm::RationalNumber>(formulas[2], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("620529/1364000")),
              result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}
