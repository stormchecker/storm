#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/storage/jani/Property.h"

class SparseMdpCbMultiObjectiveModelCheckerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(SparseMdpCbMultiObjectiveModelCheckerTest, consensus) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::ConstraintBased);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_consensus2_3_2.nm";
    std::string formulasAsString = "multi(Pmax=? [ F \"one_proc_err\" ], P>=0.8916673903 [ G \"one_coin_ok\" ]) ";  // numerical
    formulasAsString += "; \n multi(P>=0.1 [ F \"one_proc_err\" ], P>=0.8916673903 [ G \"one_coin_ok\" ])";         // achievability (true)
    formulasAsString += "; \n multi(P>=0.11 [ F \"one_proc_err\" ], P>=0.8916673903 [ G \"one_coin_ok\" ])";        // achievability (false)

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_FALSE(result->template asExplicitQualitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseMdpCbMultiObjectiveModelCheckerTest, zeroconf) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::ConstraintBased);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_zeroconf4.nm";
    std::string formulasAsString = "multi(P>=0.0003 [ F l=4 & ip=1 ] , P>=0.993141[ G (error=0) ]) ";  // numerical

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<storm::RationalNumber>()[initState]);
}

/* This test takes a little bit too long ...
TEST_F(SparseMdpCbMultiObjectiveModelCheckerTest, team3with3objectives) {

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_team3.nm";
    std::string formulasAsString = "multi(P>=0.75 [ F \"task1_compl\" ], R{\"w_1_total\"}>=2.210204082 [ C ], P>=0.5 [ F \"task2_compl\" ])"; // numerical

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp = storm::api::buildSparseModel<storm::RationalNumber>(program,
formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>(); uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(*mdp,
formulas[0]->asMultiObjectiveFormula()) ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_FALSE(result->template asExplicitQualitativeCheckResult<storm::RationalNumber>()[initState]);
}
*/
