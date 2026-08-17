#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/jani/Property.h"
#include "storm/utility/constants.h"

class SparseMdpMultiDimensionalRewardUnfoldingTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, single_obj_one_dim_walk_small) {
    storm::Environment env;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/one_dim_walk.nm";
    std::string constantsDef = "N=2";
    std::string formulasAsString = "Pmax=? [ F{\"r\"}<=1 x=N ] ";
    formulasAsString += "; \n Pmax=? [ multi( F{\"r\"}<=1 x=N, F{\"l\"}<=2 x=0 )]";
    formulasAsString += "; \n Pmin=? [ multi( F{\"r\"}<=1 x=N, F{\"l\"}<=2 x=0 )]";
    formulasAsString += "; \n Pmax=? [ F{\"r\"}>=1 x=N ] ";
    formulasAsString += "; \n Pmax=? [ F{\"r\"}>=3 x=N ] ";
    formulasAsString += "; \n Pmin=? [ F{\"r\"}>=2 x=N ] ";
    formulasAsString += "; \n Pmax=? [ multi( F{\"r\"}>=1 x=N, F{\"l\"}>=2 x=0 )]";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(0.5), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(0.125), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[2], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(0.0), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[3], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(1.0), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[4], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(1.0), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[5], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(0.0), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[6], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(1.0), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, single_obj_one_dim_walk_large) {
    storm::Environment env;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/one_dim_walk.nm";
    std::string constantsDef = "N=10";
    std::string formulasAsString = "Pmax=? [ F{\"r\"}<=5 x=N ] ";
    formulasAsString += "; \n Pmax=? [ multi( F{\"r\"}<=5 x=N, F{\"l\"}<=10 x=0 )]";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>> mc(*mdp);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::pow(storm::utility::convertNumber<storm::RationalNumber>(0.5), 5);
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::pow(storm::utility::convertNumber<storm::RationalNumber>(0.5), 15);
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, single_obj_tiny_ec) {
    storm::Environment env;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/tiny_reward_bounded.nm";
    std::string constantsDef = "";
    std::string formulasAsString = "Pmax=? [ F{\"a\"}<=3 x=4 ] ";                                       // 0.2
    formulasAsString += "; \n Pmin=? [ F{\"a\"}<=3 x=4 ] ";                                             // 0.0
    formulasAsString += "; \n Pmin=? [ F{\"a\"}<1 x=1 ] ";                                              // 0.0
    formulasAsString += "; \n Pmax=? [multi( F{\"a\"}<=4 x=4, F{\"b\"}<=12 x=5 )] ";                    // 0.02
    formulasAsString += "; \n Pmin=? [multi( F{\"a\"}<=4 x=4, F{\"b\"}<=12 x=5 )] ";                    // 0.0
    formulasAsString += "; \n Pmax=? [multi( F{\"a\"}<=4 x=4, F{\"b\"}<13 x=5, F{\"c\"}<2/5 x=3 )] ";   // 0.02
    formulasAsString += "; \n Pmax=? [multi( F{\"a\"}<=0 x=3, F{\"b\"}<=17 x=4, F{\"c\"}<4/5 x=5 )] ";  // 0.02

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>> mc(*mdp);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/5");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("0");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[2], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("0");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[3], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/50");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[4], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("0");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[5], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/50");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[6], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/50");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, single_obj_zeroconf_dl) {
    storm::Environment env;
    auto const prec = storm::utility::convertNumber<double>(env.modelchecker().multi().getPrecision());

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/zeroconf_dl_not_unfolded.nm";
    std::string constantsDef = "N=1000,K=2,reset=true";
    std::string formulasAsString = "Pmin=? [ F{\"t\"}<50 \"ipfound\" ]";
    formulasAsString += "; \n Pmax=? [multi(F{\"t\"}<50 \"ipfound\", F{\"r\"}<=0 \"ipfound\") ]";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> mc(*mdp);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<double>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(0.9989804701, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<double>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(0.984621063, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, single_obj_csma) {
    storm::Environment env;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/csma2_2.nm";
    std::string constantsDef = "";
    std::string formulasAsString = "Pmax=? [ F{\"time\"}<=70 \"all_delivered\" ]";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>> mc(*mdp);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("29487882838281/35184372088832");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, single_obj_lower_bounds) {
    storm::Environment env;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/tiny_lower_reward_bounded.nm";
    std::string constantsDef = "";
    std::string formulasAsString = "Pmax=? [ F{\"a\"}>=1.1 x=1 ]";
    formulasAsString += "; \n Pmin=? [ F{\"a\"}>=1.1 x=1 ]";
    formulasAsString += "; \n Pmax=? [ F{\"b\"}>3 x=1 ]";
    formulasAsString += "; \n Pmax=? [ F{\"b\"}>=2 x=0 ]";
    formulasAsString += "; \n Pmax=? [ multi(F{\"a\"}<=0.2 x=1, F{\"b\"}>3 x=1) ]";
    formulasAsString += "; \n Pmax=? [ multi(F{\"a\"}>0.2 x=1, F{\"b\"}>3 x=1) ]";
    formulasAsString += "; \n Pmax=? [ multi(F{\"a\"}>=2/5 x=4, F{\"a\"}<=0 x=4) ]";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>> mc(*mdp);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("81/100");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[1], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("0");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[2], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("27/64");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[3], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("9/16");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[4], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("27/64");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[5], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("243/640");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[6], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("81/160");
    EXPECT_EQ(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState]);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, one_dim_walk_small) {
    storm::Environment env;
    env.solver().setForceExact(true);
    auto const prec = env.modelchecker().multi().getPrecision();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/one_dim_walk.nm";
    std::string constantsDef = "N=2";
    std::string formulasAsString = "multi(Pmax=? [ F{\"r\"}<=1 x=N ]) ";
    formulasAsString += "; \n multi(Pmax=? [ multi( F{\"r\"}<=1 x=N, F{\"l\"}<=2 x=0 )])";
    formulasAsString += "; \n multi(Pmin=? [ multi( F{\"r\"}<=1 x=N, F{\"l\"}<=2 x=0 )])";
    formulasAsString += "; \n multi(Pmax=? [ F{\"r\"}<=1 x=N], Pmax=? [ F{\"l\"}<=2 x=0])";
    formulasAsString += "; \n multi(Pmin=? [ F{\"r\"}<=1 x=N], Pmax=? [ F{\"l\"}<=2 x=0])";
    formulasAsString += "; \n multi(Pmin=? [ F{\"r\"}<=1 x=N], Pmin=? [ F{\"l\"}<=2 x=0])";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(storm::utility::convertNumber<storm::RationalNumber>(0.5), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(storm::utility::convertNumber<storm::RationalNumber>(0.125), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState],
                prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(storm::utility::convertNumber<storm::RationalNumber>(0.0), result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[3]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p1 = {storm::utility::convertNumber<storm::RationalNumber>(0.5),
                                             storm::utility::convertNumber<storm::RationalNumber>(0.5)};
    std::vector<storm::RationalNumber> q1 = {storm::utility::convertNumber<storm::RationalNumber>(0.125),
                                             storm::utility::convertNumber<storm::RationalNumber>(0.75)};
    auto expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p1, q1}));

    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[4]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p2 = {storm::utility::convertNumber<storm::RationalNumber>(0.0),
                                             storm::utility::convertNumber<storm::RationalNumber>(0.75)};
    expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p2}));
    std::vector<std::vector<storm::RationalNumber>> transformationMatrix(2,
                                                                         std::vector<storm::RationalNumber>(2, storm::utility::zero<storm::RationalNumber>()));
    transformationMatrix[0][0] = -storm::utility::one<storm::RationalNumber>();
    transformationMatrix[1][1] = storm::utility::one<storm::RationalNumber>();
    expectedAchievableValues = expectedAchievableValues->affineTransformation(
        transformationMatrix, std::vector<storm::RationalNumber>(2, storm::utility::zero<storm::RationalNumber>()));
    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[5]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p3 = {storm::utility::convertNumber<storm::RationalNumber>(0.0),
                                             storm::utility::convertNumber<storm::RationalNumber>(-0.75)};
    std::vector<storm::RationalNumber> q3 = {storm::utility::convertNumber<storm::RationalNumber>(-0.5),
                                             storm::utility::convertNumber<storm::RationalNumber>(0.0)};
    expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p3, q3}));
    transformationMatrix[1][1] = -storm::utility::one<storm::RationalNumber>();
    expectedAchievableValues = expectedAchievableValues->affineTransformation(
        transformationMatrix, std::vector<storm::RationalNumber>(2, storm::utility::zero<storm::RationalNumber>()));
    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, one_dim_walk_large) {
    storm::Environment env;
    env.solver().setForceExact(true);
    auto const prec = env.modelchecker().multi().getPrecision();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/one_dim_walk.nm";
    std::string constantsDef = "N=10";
    std::string formulasAsString = "multi(Pmax=? [ F{\"r\"}<=5 x=N ]) ";
    formulasAsString += "; \n multi(Pmax=? [ multi( F{\"r\"}<=5 x=N, F{\"l\"}<=10 x=0 )])";
    formulasAsString += "; \n multi(P>=1/512 [ F{\"r\"}<=5 x=N], Pmax=? [ F{\"l\"}<=10 x=0])";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::pow(storm::utility::convertNumber<storm::RationalNumber>(0.5), 5);
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::pow(storm::utility::convertNumber<storm::RationalNumber>(0.5), 15);
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("2539/4096");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, tiny_ec) {
    storm::Environment env;
    env.solver().setForceExact(true);
    auto const prec = env.modelchecker().multi().getPrecision();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/tiny_reward_bounded.nm";
    std::string constantsDef = "";
    std::string formulasAsString = "multi(Pmax=? [ F{\"a\"}<=3 x=4 ]) ";                                       // 0.2
    formulasAsString += "; \n multi(Pmin=? [ F{\"a\"}<=3 x=4 ]) ";                                             // 0.0
    formulasAsString += "; \n multi(Pmin=? [ F{\"a\"}<1 x=1 ]) ";                                              // 0.0
    formulasAsString += "; \n multi(Pmax=? [multi( F{\"a\"}<=4 x=4, F{\"b\"}<=12 x=5 )]) ";                    // 0.02
    formulasAsString += "; \n multi(Pmin=? [multi( F{\"a\"}<=4 x=4, F{\"b\"}<=12 x=5 )]) ";                    // 0.0
    formulasAsString += "; \n multi(Pmax=? [multi( F{\"a\"}<=4 x=4, F{\"b\"}<13 x=5, F{\"c\"}<2/5 x=3 )]) ";   // 0.02
    formulasAsString += "; \n multi(Pmax=? [multi( F{\"a\"}<=0 x=3, F{\"b\"}<=17 x=4, F{\"c\"}<4/5 x=5 )]) ";  // 0.02
    formulasAsString +=
        "; \n multi(Pmax=? [multi( F{\"a\"}<=0 x=3, F{\"c\"}<1/2 x=5 )], Pmax=? [multi( F{\"b\"}<=0 x=3, F{\"c\"}<=4/5 x=5 )]) ";  // (0.1, 0) , (0, 0.19)

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/5");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("0");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("0");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[3]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/50");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[4]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("0");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[5]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/50");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[6]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/50");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[7]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p = {storm::utility::convertNumber<storm::RationalNumber, std::string>("1/10"),
                                            storm::utility::convertNumber<storm::RationalNumber, std::string>("0")};
    std::vector<storm::RationalNumber> q = {storm::utility::convertNumber<storm::RationalNumber, std::string>("0"),
                                            storm::utility::convertNumber<storm::RationalNumber, std::string>("19/100")};
    auto expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p, q}));

    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, zeroconf_dl) {
    storm::Environment env;
    auto const prec = storm::utility::convertNumber<double>(env.modelchecker().multi().getPrecision());

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/zeroconf_dl_not_unfolded.nm";
    std::string constantsDef = "N=1000,K=2,reset=true";
    std::string formulasAsString = "multi(Pmin=? [ F{\"t\"}<50 \"ipfound\" ])";
    formulasAsString += "; \n multi(Pmax=? [multi(F{\"t\"}<50 \"ipfound\", F{\"r\"}<=0 \"ipfound\") ])";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(0.9989804701, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(0.984621063, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, csma) {
    storm::Environment env;
    env.solver().setForceExact(true);
    auto const prec = env.modelchecker().multi().getPrecision();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/csma2_2.nm";
    std::string constantsDef = "";
    std::string formulasAsString = "multi(Pmax=? [ F{\"time\"}<=70 \"all_delivered\" ])";
    formulasAsString += "; \n multi(Pmax=? [ F{\"time\"}<=70 \"all_delivered\" ], Pmax=? [ !\"collision_max_backoff\" U \"all_delivered\"] )";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("29487882838281/35184372088832");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p = {storm::utility::convertNumber<storm::RationalNumber, std::string>("29487882838281/35184372088832"),
                                            storm::utility::convertNumber<storm::RationalNumber, std::string>("7/8")};
    auto expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p}));
    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));
}

TEST_F(SparseMdpMultiDimensionalRewardUnfoldingTest, lower_bounds) {
    storm::Environment env;
    env.solver().setForceExact(true);
    auto const prec = env.modelchecker().multi().getPrecision();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/tiny_lower_reward_bounded.nm";
    std::string constantsDef = "";
    std::string formulasAsString = "multi(Pmax=? [ F{\"a\"}>=1.1 x=1 ])";
    formulasAsString += "; \n multi(Pmax=? [ F{\"a\"}<=0.2 x=1], Pmax=? [F{\"b\"}>3 x=1 ])";
    formulasAsString += "; \n multi(Pmax=? [ F{\"a\"}>0.2 x=1 ], Pmax=? [F{\"b\"}>3 x=1 ])";
    formulasAsString += "; \n multi(Pmax=? [ multi(F{\"a\"}>=2/5 x=4, F{\"a\"}<=0 x=4)], Pmax=? [F {\"b\"}>3 x = 1])";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalNumber>> mdp =
        storm::api::buildSparseModel<storm::RationalNumber>(program, formulas)->as<storm::models::sparse::Mdp<storm::RationalNumber>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result;

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>> mc(*mdp);

    result = storm::api::verifyWithSparseEngine(mdp, storm::api::createTask<storm::RationalNumber>(formulas[0], true));
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    storm::RationalNumber expectedResult = storm::utility::convertNumber<storm::RationalNumber, std::string>("81/100");
    EXPECT_NEAR(expectedResult, result->asExplicitQuantitativeCheckResult<storm::RationalNumber>()[initState], prec);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p1 = {storm::utility::convertNumber<storm::RationalNumber, std::string>("1"),
                                             storm::utility::convertNumber<storm::RationalNumber, std::string>("27/64")};
    auto expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p1}));
    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p2 = {storm::utility::convertNumber<storm::RationalNumber, std::string>("1"),
                                             storm::utility::convertNumber<storm::RationalNumber, std::string>("243/640")};
    std::vector<storm::RationalNumber> q2 = {storm::utility::convertNumber<storm::RationalNumber, std::string>("81/256"),
                                             storm::utility::convertNumber<storm::RationalNumber, std::string>("27/64")};
    expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p2, q2}));
    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[3]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
    std::vector<storm::RationalNumber> p3 = {storm::utility::convertNumber<storm::RationalNumber, std::string>("81/160"),
                                             storm::utility::convertNumber<storm::RationalNumber, std::string>("243/640")};
    std::vector<storm::RationalNumber> q3 = {storm::utility::convertNumber<storm::RationalNumber, std::string>("2187/10240"),
                                             storm::utility::convertNumber<storm::RationalNumber, std::string>("27/64")};
    expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p3, q3}));
    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));
}
