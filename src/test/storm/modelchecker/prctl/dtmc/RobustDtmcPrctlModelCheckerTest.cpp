#include "storm-config.h"
#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalNumberForward.h"
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
#include "storm/transformer/AddUncertainty.h"
#include "storm/utility/constants.h"

std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> const& model) {
    return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<double>>(model->getInitialStates());
}

std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(std::shared_ptr<storm::models::sparse::Model<double>> const& model) {
    return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<double>>(model->getInitialStates());
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

void expectThrow(std::string const& path, std::string const& formulaString,
                 std::optional<storm::UncertaintyResolutionMode> uncertaintyResolutionMode = std::nullopt) {
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::Interval>(path);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::Interval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto task = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);
    if (uncertaintyResolutionMode.has_value()) {
        task.setUncertaintyResolutionMode(uncertaintyResolutionMode.value());
    }

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::Interval>>(*dtmc);
    STORM_SILENT_EXPECT_THROW(checker.check(env, task), storm::exceptions::BaseException);
}

void expectThrowPrism(std::string const& path, std::string const& formulaString) {
    storm::prism::Program program = storm::api::parseProgram(path);
    program = storm::utility::prism::preprocess(program, "");

    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::api::buildSparseModel<storm::Interval>(program, formulas);

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::Interval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto task = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::Interval>>(*dtmc);
    STORM_SILENT_EXPECT_THROW(checker.check(env, task), storm::exceptions::InvalidArgumentException);
}

void checkExplicitModelForQuantitativeResult(std::string const& path, std::string const& formulaString, double min, double max) {
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::Interval>(path);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::Interval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::Interval>>(*dtmc);
    auto resultMax = checker.check(env, taskMax);
    EXPECT_NEAR(max, getQuantitativeResultAtInitialState(dtmc, resultMax), 0.0001);

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[1]);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);

    auto resultMin = checker.check(env, taskMin);
    EXPECT_NEAR(min, getQuantitativeResultAtInitialState(dtmc, resultMin), 0.0001);
}

void checkPrismModelForQuantitativeResult(std::string const& path, std::string const& formulaString, double min, double max) {
    storm::prism::Program program = storm::api::parseProgram(path);
    program = storm::utility::prism::preprocess(program, "");

    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::api::buildSparseModel<storm::Interval>(program, formulas);

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::Interval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::Interval>>(*dtmc);
    auto resultMax = checker.check(env, taskMax);
    EXPECT_NEAR(max, getQuantitativeResultAtInitialState(dtmc, resultMax), 0.0001);

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[1]);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);

    auto resultMin = checker.check(env, taskMin);
    EXPECT_NEAR(min, getQuantitativeResultAtInitialState(dtmc, resultMin), 0.0001);
}

void checkModelForQualitativeResult(std::string const& path, std::string const& formulaString, std::vector<storm::storage::BitVector> expectedResultVector) {
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::Interval>(path);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::Interval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::Interval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto task1 = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);

    auto checker = storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Dtmc<storm::Interval>>(*dtmc);
    auto result = checker.check(env, task1);

    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());

    for (size_t i = 0; i < expectedResultVector[0].size(); i++) {
        EXPECT_EQ(expectedResultVector[0].get(i), result->template asExplicitQualitativeCheckResult<double>()[i]);
    }

    auto task2 = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[1]);

    result = checker.check(env, task2);

    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());

    for (size_t i = 0; i < expectedResultVector[1].size(); i++) {
        EXPECT_EQ(expectedResultVector[1].get(i), result->template asExplicitQualitativeCheckResult<double>()[i]);
    }
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

storm::RationalNumber getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> const& model,
                                                          std::unique_ptr<storm::modelchecker::CheckResult>& result) {
    auto filter = getInitialStateFilter(model);
    result->filter(*filter);
    return result->asQuantitativeCheckResult<storm::RationalNumber>().getMin();
}

void expectThrowRational(std::string const& path, std::string const& formulaString,
                         std::optional<storm::UncertaintyResolutionMode> uncertaintyResolutionMode = std::nullopt) {
    std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::RationalInterval>(path);

    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalInterval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::RationalInterval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto task = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    if (uncertaintyResolutionMode.has_value()) {
        task.setUncertaintyResolutionMode(uncertaintyResolutionMode.value());
    }

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::RationalInterval>>(*dtmc);
    STORM_SILENT_EXPECT_THROW(checker.check(env, task), storm::exceptions::BaseException);
}

void checkExplicitModelForQuantitativeResultRational(std::string const& path, std::string const& formulaString, storm::RationalNumber min,
                                                     storm::RationalNumber max) {
    std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> modelPtr = storm::parser::parseDirectEncodingModel<storm::RationalInterval>(path);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalInterval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::RationalInterval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::RationalInterval>>(*dtmc);
    auto resultMax = checker.check(env, taskMax);
    EXPECT_EQ(max, getQuantitativeResultAtInitialState(dtmc, resultMax));

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[1]);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);

    auto resultMin = checker.check(env, taskMin);
    EXPECT_EQ(min, getQuantitativeResultAtInitialState(dtmc, resultMin));
}

void checkPrismModelForQuantitativeResultRational(std::string const& path, std::string const& formulaString, storm::RationalNumber min,
                                                  storm::RationalNumber max) {
    storm::prism::Program program = storm::api::parseProgram(path);
    program = storm::utility::prism::preprocess(program, "");

    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString));
    std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> modelPtr = storm::api::buildSparseModel<storm::RationalInterval>(program, formulas);

    storm::Environment env;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalInterval>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::RationalInterval>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::RationalInterval>>(*dtmc);
    auto resultMax = checker.check(env, taskMax);
    EXPECT_EQ(max, getQuantitativeResultAtInitialState(dtmc, resultMax));

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[1]);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);

    auto resultMin = checker.check(env, taskMin);
    EXPECT_EQ(min, getQuantitativeResultAtInitialState(dtmc, resultMin));
}

void makeUncertainAndCheckRational(std::string const& path, std::string const& formulaString, storm::RationalNumber amountOfUncertainty) {
    storm::prism::Program program = storm::api::parseProgram(path);
    program = storm::utility::prism::preprocess(program, "");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> modelPtr = storm::api::buildSparseModel<storm::RationalNumber>(program, formulas);
    auto dtmc = modelPtr->as<storm::models::sparse::Dtmc<storm::RationalNumber>>();

    storm::Environment env;
    auto taskCertain = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::RationalNumber>>(*dtmc);
    auto exresult = checker.check(env, taskCertain);
    storm::RationalNumber certainValue = getQuantitativeResultAtInitialState(modelPtr, exresult);

    storm::Environment envIntervals;
    envIntervals.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    auto transformer = storm::transformer::AddUncertainty<storm::RationalNumber>(modelPtr);
    auto idtmc = transformer.transform(amountOfUncertainty)->as<storm::models::sparse::Dtmc<storm::RationalInterval>>();
    auto ichecker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::RationalInterval>>(*idtmc);

    auto taskMin = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    taskMin.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Minimize);
    auto iresultMin = ichecker.check(envIntervals, taskMin);
    storm::RationalNumber minValue = getQuantitativeResultAtInitialState(idtmc, iresultMin);
    EXPECT_LE(minValue, certainValue);

    auto taskMax = storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber>(*formulas[0]);
    taskMax.setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Maximize);
    auto iresultMax = ichecker.check(envIntervals, taskMax);
    storm::RationalNumber maxValue = getQuantitativeResultAtInitialState(idtmc, iresultMax);
    EXPECT_LE(certainValue, maxValue);
}

TEST(RobustDtmcModelCheckerTest, Tiny01ReachMaxMinProbs) {
    // Maximal Reachability probabilities using explicit format.
    checkExplicitModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-01.drn", "P=? [ F \"target\"];P=? [ F \"target\"]", 0.3, 0.5);
}

TEST(RobustDtmcModelCheckerTest, Tiny01MaxReachProbNoUncertaintyResolutionMode) {
    // Nature requires a resolution mode, expect thrown.
    expectThrow(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-01.drn", "P=? [ F \"target\"];",
                std::make_optional<storm::UncertaintyResolutionMode>(storm::UncertaintyResolutionMode::Unset));
}

TEST(RobustDtmcModelCheckerTest, Tiny01MaxReachProbNoOptimizationDirectionButRobust) {
    // Nature requires a resolution mode, expect thrown.
    expectThrow(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-01.drn", "P=? [ F \"target\"];",
                std::make_optional<storm::UncertaintyResolutionMode>(storm::UncertaintyResolutionMode::Robust));
}

TEST(RobustDtmcModelCheckerTest, Tiny02GloballyMaxMinProbs) {
    // Globally not yet supported, expect throw.
    expectThrow(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-02.drn", "P=? [ G \"target\"];P=? [ G \"target\"]");
}

TEST(RobustDtmcModelCheckerTest, DieIntervalsMaxMin) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    // Maxima reachability probabilities using PRISM format.
    checkPrismModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/die-intervals.pm", "P=? [ F \"one\"];P=? [ F \"one\"]", 9.0 / 189.0, 72.0 / 189.0);
}

TEST(RobustDtmcModelCheckerTest, BrpIntervalsMaxMin) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    // Maxima reachability probabilities using PRISM format.
    checkPrismModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/brp-32-2-intervals.pm", "P=? [ F \"error\" ];P=? [ F \"error\" ]",
                                         2.559615918664207e-10, 0.0008464876763422187);
}

TEST(RobustDtmcModelCheckerTest, DieIntervalsMaxMinRewards) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    // Maxima reachability rewards using PRISM format.
    checkPrismModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/die-intervals.pm", "R=? [ F \"done\"];R=? [ F \"done\"]", 3.25, 4.6);
}

TEST(RobustDtmcModelCheckerTest, Tiny03MaxMinRewards) {
    // Maxima reachability rewards using explicit format.
    checkExplicitModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-03.drn", "R=? [ F \"target\"];R=? [ F \"target\"]", 6.5, 8.5);
}

TEST(RobustDtmcModelCheckerTest, Tiny03RewardsNoUncertaintyResolutionMode) {
    // Nature requires a resolution mode, expect thrown.
    expectThrow(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-03.drn", "R=? [ F \"target\"]", storm::UncertaintyResolutionMode::Unset);
}

TEST(RobustDtmcModelCheckerTest, Tiny04MaxMinRewards) {
    // Maxima reachability rewards using explicit format - infinite reward case.
    checkExplicitModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-04.drn", "R=? [ F \"target\"];R=? [ F \"target\"]",
                                            std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
}

TEST(RobustDtmcModelCheckerTest, CrowdsQuotientIDTMC) {
    // Ensuring equivalent behavior when checking identical model as IDTMC and IMDP (cf. CrowdsQuotientIMDP)
    checkExplicitModelForQuantitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/crowds-quotient-3-5.drn",
                                            "P=? [ F \"observe0Greater1\"];P=? [ F \"observe0Greater1\"]", 0.1383409, 0.1383409);
}

TEST(RobustDtmcModelCheckerTest, TinyO2Propositional) {
    // Propositional formula using explicit format.
    std::vector<storm::storage::BitVector> expectedResults;
    auto result1 = storm::storage::BitVector(3);
    result1.set(0);
    result1.set(2);
    expectedResults.push_back(result1);
    result1.complement();
    expectedResults.push_back(result1);

    checkModelForQualitativeResult(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-02.drn", "\"target\";!\"target\"", expectedResults);
}

// ---- RationalInterval tests (exact arithmetic) ----

TEST(RobustRationalDtmcModelCheckerTest, Tiny01ReachMaxMinProbs) {
    checkExplicitModelForQuantitativeResultRational(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-01.drn", "P=? [ F \"target\"];P=? [ F \"target\"]",
                                                    storm::RationalNumber("3/10"), storm::RationalNumber("1/2"));
}

TEST(RobustRationalDtmcModelCheckerTest, Tiny01MaxReachProbNoUncertaintyResolutionMode) {
    expectThrowRational(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-01.drn", "P=? [ F \"target\"];",
                        std::make_optional<storm::UncertaintyResolutionMode>(storm::UncertaintyResolutionMode::Unset));
}

TEST(RobustRationalDtmcModelCheckerTest, Tiny01MaxReachProbNoOptimizationDirectionButRobust) {
    expectThrowRational(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-01.drn", "P=? [ F \"target\"];",
                        std::make_optional<storm::UncertaintyResolutionMode>(storm::UncertaintyResolutionMode::Robust));
}

TEST(RobustRationalDtmcModelCheckerTest, Tiny02GloballyMaxMinProbs) {
    expectThrowRational(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-02.drn", "P=? [ G \"target\"];P=? [ G \"target\"]");
}

TEST(RobustRationalDtmcModelCheckerTest, DieIntervalsMaxMin) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    checkPrismModelForQuantitativeResultRational(STORM_TEST_RESOURCES_DIR "/idtmc/die-intervals.pm", "P=? [ F \"one\"];P=? [ F \"one\"]",
                                                 storm::RationalNumber("4483008223/94143178827"), storm::RationalNumber("35864065784/94143178827"));
}

TEST(RobustRationalDtmcModelCheckerTest, BrpIntervalsMaxMin) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    checkPrismModelForQuantitativeResultRational(
        STORM_TEST_RESOURCES_DIR "/idtmc/brp-32-2-intervals.pm", "P=? [ F \"error\" ];P=? [ F \"error\" ]",
        // The number is to large to be represented as a literal, so we construct it from strings.
        storm::RationalNumber("10238464074071514998168131748974289312176706851339248682605279417683104172439385084862067764341447739354976890683145110072150529"
                              "29484271217452553595194641992099205093706027267658909966207049695357884534755950707204946503657943965652247014748702342577938440"
                              "579441829932769922685028147340905022682454110960938051"
                              "/"
                              "40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                              "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                              "000000000000000000000000000000000000000000000000000000000000000"),
        storm::RationalNumber("73421101667166357614487085157865792442422423664718103404519649530149236548299533815461648662459394997073289369183682645650367121"
                              "4626220291672876565353557862092334897"
                              "/"
                              "86736173798840354720596224069595336914062500000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                              "0000000000000000000000000000000000000000"));
}

TEST(RobustRationalDtmcModelCheckerTest, DieIntervalsMaxMinRewards) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    checkPrismModelForQuantitativeResultRational(STORM_TEST_RESOURCES_DIR "/idtmc/die-intervals.pm", "R=? [ F \"done\"];R=? [ F \"done\"]",
                                                 storm::RationalNumber("15544649/4782969"), storm::RationalNumber("76715008330675523/16677181699666569"));
}

TEST(RobustRationalDtmcModelCheckerTest, Tiny03MaxMinRewards) {
    checkExplicitModelForQuantitativeResultRational(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-03.drn", "R=? [ F \"target\"];R=? [ F \"target\"]",
                                                    storm::RationalNumber("13/2"), storm::RationalNumber("17/2"));
}

TEST(RobustRationalDtmcModelCheckerTest, Tiny03RewardsNoUncertaintyResolutionMode) {
    expectThrowRational(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-03.drn", "R=? [ F \"target\"]", storm::UncertaintyResolutionMode::Unset);
}

TEST(RobustRationalDtmcModelCheckerTest, Tiny04MaxMinRewards) {
    checkExplicitModelForQuantitativeResultRational(STORM_TEST_RESOURCES_DIR "/idtmc/tiny-04.drn", "R=? [ F \"target\"];R=? [ F \"target\"]",
                                                    storm::utility::infinity<storm::RationalNumber>(), storm::utility::infinity<storm::RationalNumber>());
}

TEST(RobustRationalDtmcModelCheckerTest, AddUncertaintyBrpMax) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    makeUncertainAndCheckRational(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "P=? [ F \"target\"]", storm::RationalNumber("1/10"));
    makeUncertainAndCheckRational(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "P=? [ F \"target\"]", storm::RationalNumber("1/20"));
}
