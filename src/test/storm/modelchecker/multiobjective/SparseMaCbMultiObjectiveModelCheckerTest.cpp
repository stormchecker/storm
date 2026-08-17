#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/MultiObjectiveSettings.h"
#include "storm/storage/jani/Property.h"

class SparseMaCbMultiObjectiveModelCheckerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(SparseMaCbMultiObjectiveModelCheckerTest, server) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::ConstraintBased);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/server.ma";
    std::string formulasAsString = "multi(T>=5/3 [ F \"error\" ], P>=7/12 [ F \"processB\" ]) ";  // true
    formulasAsString += "; multi(T>=16/9 [ F \"error\" ], P>=7/12 [ F \"processB\" ]) ";          // false

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>> ma =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>();
    uint_fast64_t const initState = *ma->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_FALSE(result->template asExplicitQualitativeCheckResult<storm::RationalNumber>()[initState]);
}
