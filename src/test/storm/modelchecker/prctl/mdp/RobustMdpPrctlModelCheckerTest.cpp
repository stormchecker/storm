#include "storm-config.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/exceptions/NotImplementedException.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/DirectEncodingParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/api/verification.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/transformer/AddUncertainty.h"

std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> const& model) {
    return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<double>>(model->getInitialStates());
}

std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(std::shared_ptr<storm::models::sparse::Model<double>> const& model) {
    return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<double>>(model->getInitialStates());
}

std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> const& model) {
    return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<storm::RationalNumber>>(model->getInitialStates());
}

storm::RationalNumber getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> const& model,
                                                          std::unique_ptr<storm::modelchecker::CheckResult>& result) {
    auto filter = getInitialStateFilter(model);
    result->filter(*filter);
    return result->asQuantitativeCheckResult<storm::RationalNumber>().getMin();
}

std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> const& model) {
    return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<storm::RationalNumber>>(model->getInitialStates());
}

double getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::sparse::Model<storm::Interval>> const& model,
                                           std::unique_ptr<storm::modelchecker::CheckResult>& result) {
    auto filter = getInitialStateFilter(model);
    result->filter(*filter);
    return result->asQuantitativeCheckResult<double>().getMin();
}

double getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::sparse::Model<double>> const& model,
                                           std::unique_ptr<storm::modelchecker::CheckResult>& result) {
    auto filter = getInitialStateFilter(model);
    result->filter(*filter);
    return result->asQuantitativeCheckResult<double>().getMin();
}

storm::RationalNumber getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> const& model,
                                                          std::unique_ptr<storm::modelchecker::CheckResult>& result) {
    auto filter = getInitialStateFilter(model);
    result->filter(*filter);
    return result->asQuantitativeCheckResult<storm::RationalNumber>().getMin();
}

void expectThrow(std::string const& path, std::string const& formulaString) {
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::Interval>(path);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Mdp<storm::Interval>> mdp = modelPtr->as<storm::models::sparse::Mdp<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Mdp, modelPtr->getType());
    auto task = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);

    auto checker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::Interval>>(*mdp);
    STORM_SILENT_EXPECT_THROW(checker.check(env, task), storm::exceptions::NotImplementedException);
}

void checkModel(std::string const& path, std::string const& formulaString, double maxmin, double maxmax, double minmax, double minmin, bool produceScheduler) {
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::Interval>(path);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Mdp<storm::Interval>> mdp = modelPtr->as<storm::models::sparse::Mdp<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Mdp, modelPtr->getType());
    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Robust);
    taskMax.setProduceSchedulers(produceScheduler);

    auto checker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::Interval>>(*mdp);
    auto resultMax = checker.check(env, taskMax);
    EXPECT_NEAR(maxmin, getQuantitativeResultAtInitialState(mdp, resultMax), 0.0001);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Cooperative);
    auto resultMaxNonRobust = checker.check(env, taskMax);
    EXPECT_NEAR(maxmax, getQuantitativeResultAtInitialState(mdp, resultMaxNonRobust), 0.0001);

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[1]);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Robust);
    taskMin.setProduceSchedulers(produceScheduler);

    auto resultMin = checker.check(env, taskMin);
    EXPECT_NEAR(minmax, getQuantitativeResultAtInitialState(mdp, resultMin), 0.0001);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Cooperative);
    auto resultMinNonRobust = checker.check(env, taskMin);
    EXPECT_NEAR(minmin, getQuantitativeResultAtInitialState(mdp, resultMinNonRobust), 0.0001);
}

void checkPrismModelForQuantitativeResult(std::string const& path, std::string const& formulaString, double minmin, double minmax, double maxmin, double maxmax,
                                          std::string constantsString) {
    storm::prism::Program program = storm::api::parseProgram(path);
    program = storm::utility::prism::preprocess(program, constantsString);

    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::api::buildSparseModel<storm::Interval>(program, formulas);

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Mdp<storm::Interval>> mdp = modelPtr->as<storm::models::sparse::Mdp<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Mdp, modelPtr->getType());

    auto taskMinMin = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);
    taskMinMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);

    auto checker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::Interval>>(*mdp);
    auto resultMinMin = checker.check(env, taskMinMin);
    EXPECT_NEAR(minmin, getQuantitativeResultAtInitialState(mdp, resultMinMin), 0.0001);

    auto taskMinMax = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[1]);
    taskMinMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);

    auto resultMinMax = checker.check(env, taskMinMax);
    EXPECT_NEAR(minmax, getQuantitativeResultAtInitialState(mdp, resultMinMax), 0.0001);

    auto taskMaxMin = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[2]);
    taskMaxMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);

    auto resultMaxMin = checker.check(env, taskMaxMin);
    EXPECT_NEAR(maxmin, getQuantitativeResultAtInitialState(mdp, resultMaxMin), 0.0001);

    auto taskMaxMax = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[3]);
    taskMaxMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);

    auto resultMaxMax = checker.check(env, taskMaxMax);
    EXPECT_NEAR(maxmax, getQuantitativeResultAtInitialState(mdp, resultMaxMax), 0.0001);
}

void checkModelRational(std::string const& path, std::string const& formulaString, storm::RationalNumber maxmin, storm::RationalNumber maxmax,
                        storm::RationalNumber minmax, storm::RationalNumber minmin, bool produceScheduler) {
    std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::RationalInterval>(path);

    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalInterval>> mdp = modelPtr->as<storm::models::sparse::Mdp<storm::RationalInterval>>();
    ASSERT_EQ(storm::models::ModelType::Mdp, modelPtr->getType());
    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    taskMax.setProduceSchedulers(produceScheduler);

    auto checker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalInterval>>(*mdp);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Robust);
    auto resultMax = checker.check(env, taskMax);
    EXPECT_EQ(maxmin, getQuantitativeResultAtInitialState(mdp, resultMax));
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Cooperative);
    auto resultMaxNonRobust = checker.check(env, taskMax);
    EXPECT_EQ(maxmax, getQuantitativeResultAtInitialState(mdp, resultMaxNonRobust));

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[1]);
    taskMin.setProduceSchedulers(produceScheduler);

    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Robust);
    auto resultMin = checker.check(env, taskMin);
    EXPECT_EQ(minmax, getQuantitativeResultAtInitialState(mdp, resultMin));
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Cooperative);
    auto resultMinNonRobust = checker.check(env, taskMin);
    EXPECT_EQ(minmin, getQuantitativeResultAtInitialState(mdp, resultMinNonRobust));
}

TEST(RobustMdpModelCheckerTest, RobotMinMaxTest) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    // Maxima reachability rewards using PRISM format.
    checkPrismModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/imdp/robot.prism",
                                         "Pmin=? [ F \"goal2\"];Pmin=? [ F \"goal2\"];Pmax=? [ F \"goal2\"];Pmax=? [ F \"goal2\"]", 0.4, 0.6, 1.0, 1.0,
                                         "delta=0.1");
}

void makeUncertainAndCheck(std::string const& path, std::string const& formulaString, double amountOfUncertainty) {
    storm::prism::Program program = storm::api::parseProgram(path);
    program = storm::utility::prism::preprocess(program, "");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    std::shared_ptr<storm::models::sparse::Model<double>> modelPtr = storm::api::buildSparseModel<double>(program, formulas);

    auto mdp = modelPtr->as<storm::models::sparse::Mdp<double>>();

    ASSERT_TRUE(formulas[0]->isProbabilityOperatorFormula());
    // These tests cases where written with max in mind.
    ASSERT_TRUE(formulas[0]->asProbabilityOperatorFormula().getOptimalityType() == storm::solver::OptimizationDirection::Maximize);
    auto task = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);
    storm::Environment env;

    // First compute certain value
    auto checker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>>(*mdp);
    auto exresult = checker.check(env, task);
    double certainValue = getQuantitativeResultAtInitialState(mdp, exresult);

    storm::Environment envIntervals;
    envIntervals.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    auto transformer = storm::transformer::AddUncertainty(modelPtr);
    auto imdp = transformer.transform(amountOfUncertainty)->as<storm::models::sparse::Mdp<storm::Interval>>();
    auto ichecker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::Interval>>(*imdp);
    auto iresultMin = checker.check(env, task);
    double minValue = getQuantitativeResultAtInitialState(mdp, iresultMin);
    EXPECT_LE(minValue, certainValue);
    task.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Cooperative);
    auto iresultMax = checker.check(env, task);
    double maxValue = getQuantitativeResultAtInitialState(mdp, iresultMax);
    EXPECT_LE(certainValue, maxValue);
}

void makeUncertainAndCheckRational(std::string const& path, std::string const& formulaString, storm::RationalNumber amountOfUncertainty) {
    storm::prism::Program program = storm::api::parseProgram(path);
    program = storm::utility::prism::preprocess(program, "");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> modelPtr = storm::api::buildSparseModel<storm::RationalNumber>(program, formulas);
    auto mdp = modelPtr->as<storm::models::sparse::Mdp<storm::RationalNumber>>();

    ASSERT_TRUE(formulas[0]->isProbabilityOperatorFormula());
    ASSERT_TRUE(formulas[0]->asProbabilityOperatorFormula().getOptimalityType() == storm::solver::OptimizationDirection::Maximize);

    storm::Environment env;
    auto taskCertain = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    auto checker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>>(*mdp);
    auto exresult = checker.check(env, taskCertain);
    storm::RationalNumber certainValue = getQuantitativeResultAtInitialState(modelPtr, exresult);

    storm::Environment envIntervals;
    envIntervals.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    auto transformer = storm::transformer::AddUncertainty<storm::RationalNumber>(modelPtr);
    auto imdp = transformer.transform(amountOfUncertainty)->as<storm::models::sparse::Mdp<storm::RationalInterval>>();
    auto ichecker = storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalInterval>>(*imdp);

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);
    auto iresultMin = ichecker.check(envIntervals, taskMin);
    storm::RationalNumber minValue = getQuantitativeResultAtInitialState(imdp, iresultMin);
    EXPECT_LE(minValue, certainValue);

    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);
    auto iresultMax = ichecker.check(envIntervals, taskMax);
    storm::RationalNumber maxValue = getQuantitativeResultAtInitialState(imdp, iresultMax);
    EXPECT_LE(certainValue, maxValue);
}

// TODO: Add next properties

TEST(RobustMDPModelCheckingTest, Tiny01maxmin) {
    checkModel(STORM_TEST_RESOURCES_DIR "/imdp/tiny-01.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", 0.4, 0.5, 0.5, 0.4, false);
}

TEST(RobustMDPModelCheckingTest, Tiny03maxmin) {
    checkModel(STORM_TEST_RESOURCES_DIR "/imdp/tiny-03.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", 0.4, 0.6, 0.5, 0.3, true);
}

TEST(RobustMDPModelCheckingTest, BoundedTiny03maxmin) {
    checkModel(STORM_TEST_RESOURCES_DIR "/imdp/tiny-03.drn", "Pmax=? [ F<=3 \"target\"];Pmin=? [ F<=3 \"target\"]", 0.4, 0.6, 0.5, 0.3, true);
}

TEST(RobustMDPModelCheckingTest, Tiny04maxmin) {
    checkModel(STORM_TEST_RESOURCES_DIR "/imdp/tiny-04.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", 1, 1, 0.42857, 0.42, false);
}

TEST(RobustMDPModelCheckingTest, Tiny05maxmin) {
    checkModel(STORM_TEST_RESOURCES_DIR "/imdp/tiny-05.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", 0.3, 0.4, 0.4, 0.3, false);
}

TEST(RobustMDPModelCheckingTest, Tiny04maxmin_rewards) {
    expectThrow(STORM_TEST_RESOURCES_DIR "/imdp/tiny-04.drn", "Rmin=? [ F \"target\"]");
}

TEST(RobustMDPModelCheckingTest, CrowdsQuotientIMDP) {
    // Ensuring equivalent behavior when checking identical model as IDTMC and IMDP (cf. CrowdsQuotientIDTMC)
    checkModel(STORM_TEST_RESOURCES_DIR "/imdp/crowds-quotient-3-5.drn", "Pmax=? [ F \"observe0Greater1\"]; Pmin=? [ F \"observe0Greater1\"]", 0.1383409,
               0.1383409, 0.1383409, 0.1383409, false);
}

// ---- RationalInterval tests (exact arithmetic) ----

TEST(RobustRationalMDPModelCheckingTest, Tiny01maxmin) {
    checkModelRational(STORM_TEST_RESOURCES_DIR "/imdp/tiny-01.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", storm::RationalNumber("2/5"),
                       storm::RationalNumber("1/2"), storm::RationalNumber("1/2"), storm::RationalNumber("2/5"), false);
}

TEST(RobustRationalMDPModelCheckingTest, Tiny03maxmin) {
    checkModelRational(STORM_TEST_RESOURCES_DIR "/imdp/tiny-03.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", storm::RationalNumber("2/5"),
                       storm::RationalNumber("3/5"), storm::RationalNumber("1/2"), storm::RationalNumber("3/10"), true);
}

TEST(RobustRationalMDPModelCheckingTest, BoundedTiny03maxmin) {
    checkModelRational(STORM_TEST_RESOURCES_DIR "/imdp/tiny-03.drn", "Pmax=? [ F<=3 \"target\"];Pmin=? [ F<=3 \"target\"]", storm::RationalNumber("2/5"),
                       storm::RationalNumber("3/5"), storm::RationalNumber("1/2"), storm::RationalNumber("3/10"), true);
}

TEST(RobustRationalMDPModelCheckingTest, Tiny04maxmin) {
    // Fill in exact rational values once test output is known.
    checkModelRational(STORM_TEST_RESOURCES_DIR "/imdp/tiny-04.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", storm::RationalNumber(1),
                       storm::RationalNumber(1), storm::RationalNumber("42857140807299/100000000000000"), storm::RationalNumber("21/50"), false);
}

TEST(RobustRationalMDPModelCheckingTest, Tiny05maxmin) {
    checkModelRational(STORM_TEST_RESOURCES_DIR "/imdp/tiny-05.drn", "Pmax=? [ F \"target\"];Pmin=? [ F \"target\"]", storm::RationalNumber("3/10"),
                       storm::RationalNumber("2/5"), storm::RationalNumber("2/5"), storm::RationalNumber("3/10"), false);
}

TEST(RobustMDPModelCheckingTest, AddUncertaintyCoin22max) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    makeUncertainAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Pmax=? [F \"all_coins_equal_1\"]", 0.1);
    makeUncertainAndCheck(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Pmax=? [F \"all_coins_equal_1\"]", 0.2);
}

TEST(RobustRationalMDPModelCheckingTest, AddUncertaintyCoin22max) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    makeUncertainAndCheckRational(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Pmax=? [F \"all_coins_equal_1\"]", storm::RationalNumber("1/10"));
    makeUncertainAndCheckRational(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", "Pmax=? [F \"all_coins_equal_1\"]", storm::RationalNumber("1/5"));
}
