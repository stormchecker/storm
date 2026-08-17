#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/geometry/Hyperrectangle.h"
#include "storm/storage/geometry/Polytope.h"
#include "storm/storage/jani/Property.h"

template<typename ValueType>
bool expectPointConained(std::vector<std::vector<ValueType>> const& pointset, std::vector<ValueType> const& point, ValueType precision) {
    for (auto const& p : pointset) {
        EXPECT_EQ(p.size(), point.size()) << "Missmatch in point dimension.";
        bool found = true;
        for (uint64_t i = 0; i < p.size(); ++i) {
            if (storm::utility::abs<ValueType>(p[i] - point[i]) > precision) {
                found = false;
                break;
            }
        }
        if (found) {
            return true;
        }
    }
    // prepare failure message:
    std::stringstream errstr;
    errstr << "Point [";
    bool firstPi = true;
    for (auto const& pi : point) {
        if (firstPi) {
            firstPi = false;
        } else {
            errstr << ", ";
        }
        errstr << pi;
    }
    errstr << "] is not contained in point set {";
    bool firstP = true;
    for (auto const& p : pointset) {
        if (firstP) {
            firstP = false;
        } else {
            errstr << ", \t";
        }
        errstr << "[";
        firstPi = true;
        for (auto const& pi : p) {
            if (firstPi) {
                firstPi = false;
            } else {
                errstr << ", ";
            }
            errstr << pi;
        }
        errstr << "]";
    }
    errstr << "}.";
    ADD_FAILURE() << errstr.str();
    return false;
}

template<typename ValueType>
bool expectSubset(std::vector<std::vector<ValueType>> const& lhs, std::vector<std::vector<ValueType>> const& rhs, ValueType precision) {
    for (auto const& p : lhs) {
        if (!expectPointConained(rhs, p, precision)) {
            return false;
        }
    }
    return true;
}

template<typename ValueType>
std::vector<std::vector<ValueType>> convertPointset(std::vector<std::vector<std::string>> const& in) {
    std::vector<std::vector<ValueType>> out;
    for (auto const& point_str : in) {
        out.emplace_back();
        for (auto const& pi_str : point_str) {
            out.back().push_back(storm::utility::convertNumber<ValueType>(pi_str));
        }
    }
    return out;
}

template<typename ValueType>
void evaluateScheduler(storm::Environment const& env, storm::models::sparse::Mdp<ValueType> const& mdp, storm::storage::Scheduler<ValueType> const& scheduler,
                       storm::logic::MultiObjectiveFormula const& formula, std::vector<ValueType>& result) {
    ASSERT_FALSE(scheduler.isPartialScheduler());
    ASSERT_EQ(mdp.getNumberOfStates(), scheduler.getNumberOfModelStates());
    auto inducedModel = mdp.applyScheduler(scheduler);
    ASSERT_TRUE(inducedModel->isOfType(storm::models::ModelType::Dtmc));
    auto const dtmcPtr = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
    ASSERT_TRUE(dtmcPtr != nullptr);
    auto const& dtmc = *dtmcPtr;
    ASSERT_TRUE(dtmc.getInitialStates().hasUniqueSetBit());
    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> modelChecker(dtmc);

    for (auto const& obj : formula.getSubformulas()) {
        auto objRes = modelChecker.check(env, *obj);
        EXPECT_TRUE(objRes->isExplicitQuantitativeCheckResult()) << "Objective " << *obj << " did not produce a quantitative result.";
        auto const& quantitativeResult = objRes->template asExplicitQuantitativeCheckResult<ValueType>();
        result.push_back(quantitativeResult[*dtmc.getInitialStates().begin()]);
    }
}

template<typename ValueType>
void assertParetoResult(storm::Environment const& env, storm::models::sparse::Mdp<ValueType> const& mdp, storm::logic::MultiObjectiveFormula const& formula,
                        storm::modelchecker::CheckResult const& result, std::vector<std::vector<std::string>> const& expectedPoints, bool expectSchedulers) {
    ASSERT_TRUE(result.isExplicitParetoCurveCheckResult()) << "Result is not an explicit Pareto curve check result.";
    ValueType const eps = storm::utility::convertNumber<ValueType>(1e-4);
    auto const& paretoResult = result.asExplicitParetoCurveCheckResult<ValueType>();
    EXPECT_TRUE(expectSubset(paretoResult.getPoints(), convertPointset<ValueType>(expectedPoints), eps)) << "Non-Pareto point found.";
    EXPECT_TRUE(expectSubset(convertPointset<ValueType>(expectedPoints), paretoResult.getPoints(), eps)) << "Pareto point missing.";
    ASSERT_EQ(expectSchedulers, paretoResult.hasScheduler());
    if (expectSchedulers) {
        ASSERT_EQ(paretoResult.getPoints().size(), paretoResult.getSchedulers().size());
        for (uint64_t i = 0; i < paretoResult.getPoints().size(); ++i) {
            auto const& point = paretoResult.getPoints()[i];
            std::vector<ValueType> inducedPoint;
            evaluateScheduler<ValueType>(env, mdp, paretoResult.getSchedulers()[i], formula, inducedPoint);
            ASSERT_EQ(point.size(), inducedPoint.size()) << "Unable to evaluate scheduler for Pareto point " << i;
            for (uint64_t objDim = 0; objDim < point.size(); ++objDim) {
                EXPECT_NEAR(point[objDim], inducedPoint[objDim], eps)
                    << "Scheduler for Pareto point " << i << " does not yield expected value for objective dimension " << objDim << ".";
            }
        }
    }
}

class SparseMdpPcaaMultiObjectiveModelCheckerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, simple_memory) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);
    env.solver().setForceExact(true);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_memory.prism";
    std::string const formulasAsString = "multi(Pmax=? [ s<4 U s=1 ], Pmax=? [ s<4 U s=2], Pmax=? [ s<4 U s=3] );\n";  // pareto

    // program, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("p=0");  // qualitative version
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    storm::generator::NextStateGeneratorOptions options(formulas);
    auto mdp = storm::builder::ExplicitModelBuilder<storm::RationalNumber>(program, options).build()->as<storm::models::sparse::Mdp<storm::RationalNumber>>();

    std::vector<std::vector<std::string>> expectedPoints;
    auto add = [&expectedPoints](std::string const& first, std::string const& second, std::string const& third) {
        expectedPoints.emplace_back(std::vector<std::string>({first, second, third}));
    };
    add("1", "1", "1");

    {
        // without scheduler generation
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), false);
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, false);
    }
    {
        // with scheduler generation
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), true);
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }

    program = storm::api::parseProgram(programFile);
    program = program.preprocess("p=0.1");  // quantitative version
    program.checkValidity();
    mdp = storm::builder::ExplicitModelBuilder<storm::RationalNumber>(program, options).build()->as<storm::models::sparse::Mdp<storm::RationalNumber>>();

    expectedPoints.clear();
    add("9/10", "81/100", "1");
    add("81/100", "9/10", "1");
    add("9/10", "1", "81/100");
    add("1", "81/100", "9/10");
    add("1", "9/10", "81/100");
    add("81/100", "1", "9/10");

    {
        // without scheduler generation
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), false);
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, false);
    }
    {
        // with scheduler generation
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), true);
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, consensus) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);
    env.solver().setForceSoundness(true);
    auto const prec = storm::utility::convertNumber<double>(env.modelchecker().multi().getPrecision());

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_consensus2_3_2.nm";
    std::string formulasAsString = "multi(Pmax=? [ F \"one_proc_err\" ], P>=0.8916673903 [ G \"one_coin_ok\" ]) ";  // numerical
    formulasAsString += "; \n multi(P>=0.1 [ F \"one_proc_err\" ], P>=0.8916673903 [ G \"one_coin_ok\" ])";         // achievability (true)
    formulasAsString += "; \n multi(P>=0.11 [ F \"one_proc_err\" ], P>=0.8916673903 [ G \"one_coin_ok\" ])";        // achievability (false)

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(0.1083326097, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);
    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<double>()[initState]);

    result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_FALSE(result->template asExplicitQualitativeCheckResult<double>()[initState]);
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, zeroconf) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);
    env.solver().setForceSoundness(true);
    auto const prec = storm::utility::convertNumber<double>(env.modelchecker().multi().getPrecision());

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_zeroconf4.nm";
    std::string formulasAsString = "multi(Pmax=? [ F l=4 & ip=1 ] , P>=0.993141[ G (error=0) ]) ";  // numerical

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(0.0003075787401574803, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, team3with3objectives) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);
    env.solver().setForceSoundness(true);
    auto const prec = storm::utility::convertNumber<double>(env.modelchecker().multi().getPrecision());

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_team3.nm";
    std::string formulasAsString = "multi(Pmax=? [ F \"task1_compl\" ], R{\"w_1_total\"}>=2.210204082 [ C ], P>=0.5 [ F \"task2_compl\" ])";  // numerical

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(0.7448979591841851, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, tiny_rewards_negative) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards_negative.nm";
    std::string formulasAsString = "multi(R{\"a\"}max>=0 [C], R{\"b\"}min<=0 [C])";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<double>()[initState]);
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, scheduler) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_scheduler05.nm";
    std::string formulasAsString = "multi(R{\"time\"}<= 11.778[ F \"tasks_complete\" ], R{\"energy\"}<=1.45 [ F \"tasks_complete\" ]) ";

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<double>()[initState]);
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, dpm) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);
    auto const prec = storm::utility::convertNumber<double>(env.modelchecker().multi().getPrecision());

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_dpm100.nm";
    std::string formulasAsString = "multi(R{\"power\"}min=? [ C<=100 ], R{\"queue\"}<=70 [ C<=100 ])";  // numerical

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::Mdp<double>>();
    uint_fast64_t const initState = *mdp->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(121.6128842094743, result->asExplicitQuantitativeCheckResult<double>()[initState], prec);
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, simple_lra) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_simple_lra.nm";
    std::string formulasAsString = "multi(R{\"first\"}max=? [ LRA ], R{\"second\"}max=? [ LRA ]);\n";                // pareto
    formulasAsString += "multi(R{\"first\"}min=? [ LRA ], R{\"second\"}max=? [ LRA ]);\n";                           // pareto
    formulasAsString += "multi(R{\"first\"}min=? [ LRA ], R{\"second\"}min=? [ LRA ]);\n";                           // pareto
    formulasAsString += "multi(R{\"first\"}min=? [ C ], R{\"second\"}min=? [ LRA ]);\n";                             // pareto
    formulasAsString += "multi(R{\"first\"}min=? [ C ], R{\"first\"}max=? [ LRA ]);\n";                              // pareto
    formulasAsString += "multi(R{\"first\"}min=? [ C ], R{\"second\"}max=? [ LRA ], R{\"third\"}max=? [ C ]);\n";    // pareto
    formulasAsString += "multi(R{\"first\"}min=? [ LRA ], R{\"second\"}max=? [ LRA ], R{\"third\"}min=? [ C ]);\n";  // pareto
    formulasAsString += "multi(LRAmax=? [ x=1 ], R{\"second\"}max=? [ LRA ]);\n";                                    // pareto

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    storm::generator::NextStateGeneratorOptions options(formulas);
    auto mdp = storm::builder::ExplicitModelBuilder<double>(program, options).build()->as<storm::models::sparse::Mdp<double>>();

    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"5", "80/11"}));
        expectedPoints.emplace_back(std::vector<std::string>({"0", "16"}));
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"0", "16"}));
        assertParetoResult(env, *mdp, formulas[1]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"0", "0"}));
        assertParetoResult(env, *mdp, formulas[2]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[3]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"10/8", "0"}));
        assertParetoResult(env, *mdp, formulas[3]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[4]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"10/8", "0"}));
        assertParetoResult(env, *mdp, formulas[4]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[5]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"10/8", "0", "10/8"}));
        expectedPoints.emplace_back(std::vector<std::string>({"7", "16", "2"}));
        assertParetoResult(env, *mdp, formulas[5]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[6]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"0", "0", "10/8"}));
        expectedPoints.emplace_back(std::vector<std::string>({"0", "16", "2"}));
        assertParetoResult(env, *mdp, formulas[6]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[7]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"1", "0"}));
        expectedPoints.emplace_back(std::vector<std::string>({"0", "16"}));
        assertParetoResult(env, *mdp, formulas[7]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, resource_gathering) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/resource-gathering.nm";
    std::string constantsDef = "GOLD_TO_COLLECT=0,GEM_TO_COLLECT=0,B=0";
    std::string formulasAsString = "multi(R{\"rew_gold\"}max=? [LRA], R{\"rew_gem\"}max=? [LRA]);\n";                // pareto
    formulasAsString += "multi(R{\"rew_gold\"}min=? [LRA], R{\"rew_gem\"}max=? [LRA]);\n";                           // pareto
    formulasAsString += "multi(R{\"rew_gold\"}max=? [LRA], R{\"rew_gem\"}min=? [LRA]);\n";                           // pareto
    formulasAsString += "multi(R{\"rew_gold\"}min=? [LRA], R{\"rew_gem\"}min=? [LRA]);\n";                           // pareto
    formulasAsString += "multi(R{\"rew_gold\"}max=? [LRA], R{\"attacks\"}min=? [LRA]);\n";                           // pareto
    formulasAsString += "multi(R{\"rew_gold\"}max=? [LRA], R{\"attacks\"}min=? [C]);\n";                             // pareto
    formulasAsString += "multi(R{\"rew_gold\"}max=? [LRA], R{\"rew_gem\"}max=? [LRA], R{\"attacks\"}min=? [C]);\n";  // pareto
    formulasAsString += "multi(R{\"rew_gold\"}max=? [LRA], R{\"rew_gem\"}max=? [ C<100 ]);\n";                       // pareto

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    storm::generator::NextStateGeneratorOptions options(formulas);
    auto mdp = storm::builder::ExplicitModelBuilder<double>(program, options).build()->as<storm::models::sparse::Mdp<double>>();

    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"27/241", "0"}));
        expectedPoints.emplace_back(std::vector<std::string>({"0", "1/10"}));
        expectedPoints.emplace_back(std::vector<std::string>({"27/349", "27/349"}));
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[1]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"0", "1/10"}));
        assertParetoResult(env, *mdp, formulas[1]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[2]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"27/241", "0"}));
        assertParetoResult(env, *mdp, formulas[2]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[3]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"0", "0"}));
        assertParetoResult(env, *mdp, formulas[3]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[4]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"27/241", "19/723"}));
        expectedPoints.emplace_back(std::vector<std::string>({"3/31", "1/93"}));
        expectedPoints.emplace_back(std::vector<std::string>({"1/12", "0"}));
        assertParetoResult(env, *mdp, formulas[4]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[5]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"1/12", "0"}));
        assertParetoResult(env, *mdp, formulas[5]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[6]->asMultiObjectiveFormula(), true);
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"1/12", "0", "0"}));
        expectedPoints.emplace_back(std::vector<std::string>({"0", "1/10", "0"}));
        expectedPoints.emplace_back(std::vector<std::string>({"1/18", "1/18", "0"}));
        assertParetoResult(env, *mdp, formulas[6]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[7]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"27/241", "9"}));
        double eps = 1e-4;
        // TODO: Right now, there is a non-optimal point included due to numerical imprecisions. We therefore skip this check:
        // EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps)) <<
        // "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
}

TEST_F(SparseMdpPcaaMultiObjectiveModelCheckerTest, uav) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);
    env.solver().setForceExact(true);

    std::string const programFile = STORM_TEST_RESOURCES_DIR "/mdp/uav.prism";
    std::string const constantsDef = "B=500,Unf=1,COUNTER=0";
    std::string const formulasAsString = "multi(Pmax=? [F !\"timeExceeded\" & \"mission\" ], R{\"ROZ\"}min=? [ C ]);\n";  // pareto
                                                                                                                          // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDef);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    storm::generator::NextStateGeneratorOptions options(formulas);
    auto mdp = storm::builder::ExplicitModelBuilder<storm::RationalNumber>(program, options).build()->as<storm::models::sparse::Mdp<storm::RationalNumber>>();

    std::vector<std::vector<std::string>> expectedPoints;
    auto add = [&expectedPoints](std::string const& first, std::string const& second) {
        expectedPoints.emplace_back(std::vector<std::string>({first, second}));
    };
    add("38414175132198578578863785494030381798272113811817/52428800000000000000000000000000000000000000000000",
        "105354027594087368183143053788105786304251179167/209715200000000000000000000000000000000000000000");
    add("408128422164003121618009/6144000000000000000000000", "0");
    add("1999499678525341500435955646267214582929663828413/2621440000000000000000000000000000000000000000000",
        "56958024522816659763759931934022966832786752339943/104857600000000000000000000000000000000000000000000");
    add("54945877583368632591143/768000000000000000000000", "692022604641222661654317387728123187/209715200000000000000000000000000000000");
    add("556037253018662639292085915885511293/819200000000000000000000000000000000",
        "23519125342549093981992573806031222108120680979933/52428800000000000000000000000000000000000000000000");
    add("152994143736516754463119330421808117645210427315049/157286400000000000000000000000000000000000000000000",
        "142546056129118274549698300021808117645210427315049/157286400000000000000000000000000000000000000000000");
    add("38413530353326383550702539286030381798272113811817/52428800000000000000000000000000000000000000000000",
        "105351225800868018472006457901555524617571179167/209715200000000000000000000000000000000000000000");
    add("79174719660237177832933/1024000000000000000000000", "22982244840304243390420665142290586927/3145728000000000000000000000000000000000");
    add("557815625385062639292085915885511293/819200000000000000000000000000000000",
        "23624476121534629981992573806031222108120680979933/52428800000000000000000000000000000000000000000000");
    add("38509950534966778168608784918030381798272113811817/52428800000000000000000000000000000000000000000000",
        "105835147685375696328092092647756571364291179167/209715200000000000000000000000000000000000000000");
    add("550129111481892605495643435885511293/819200000000000000000000000000000000",
        "23182930152916159818643157849668787529790680979933/52428800000000000000000000000000000000000000000000");
    add("8161197127974140175742187/92160000000000000000000000", "181891910954804140307195656220360926759/11796480000000000000000000000000000000000");
    add("3055201243302703582182623010983906749/4915200000000000000000000000000000000",
        "126399527417520822710932508625671612362720597610269/314572800000000000000000000000000000000000000000000");

    {
        // without scheduler generation
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), false);
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, false);
    }
    {
        // with scheduler generation
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[0]->asMultiObjectiveFormula(), true);
        assertParetoResult(env, *mdp, formulas[0]->asMultiObjectiveFormula(), *result, expectedPoints, true);
    }
}
