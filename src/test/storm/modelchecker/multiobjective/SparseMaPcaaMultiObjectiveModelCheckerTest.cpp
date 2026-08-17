#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/MultiObjectiveSettings.h"
#include "storm/storage/geometry/Hyperrectangle.h"
#include "storm/storage/geometry/Polytope.h"
#include "storm/storage/jani/Property.h"

class SparseMaPcaaMultiObjectiveModelCheckerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, serverRationalNumbers) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/server.ma";
    std::string formulasAsString = "multi(Tmax=? [ F \"error\" ], Pmax=? [ F \"processB\" ]) ";  // pareto

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    storm::generator::NextStateGeneratorOptions options(formulas);
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>> ma =
        storm::builder::ExplicitModelBuilder<storm::RationalNumber>(program, options)
            .build()
            ->as<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());

    storm::RationalNumber p1 = storm::utility::convertNumber<storm::RationalNumber>(11.0);
    p1 /= storm::utility::convertNumber<storm::RationalNumber>(6.0);
    storm::RationalNumber p2 = storm::utility::convertNumber<storm::RationalNumber>(1.0);
    p2 /= storm::utility::convertNumber<storm::RationalNumber>(2.0);
    std::vector<storm::RationalNumber> p = {p1, p2};
    storm::RationalNumber q1 = storm::utility::convertNumber<storm::RationalNumber>(29.0);
    q1 /= storm::utility::convertNumber<storm::RationalNumber>(18.0);
    storm::RationalNumber q2 = storm::utility::convertNumber<storm::RationalNumber>(2.0);
    q2 /= storm::utility::convertNumber<storm::RationalNumber>(3.0);
    std::vector<storm::RationalNumber> q = {q1, q2};
    auto expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>({p, q}));
    EXPECT_TRUE(expectedAchievableValues->contains(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getUnderApproximation()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<storm::RationalNumber>().getOverApproximation()->contains(expectedAchievableValues));
}

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, server) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/server.ma";
    std::string formulasAsString = "multi(Tmax=? [ F \"error\" ], Pmax=? [ F \"processB\" ]) ";  // pareto

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> ma =
        storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::MarkovAutomaton<double>>();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());

    // we do our checks with rationals to avoid numerical issues when doing polytope computations...
    std::vector<double> p = {11.0 / 6.0, 1.0 / 2.0};
    std::vector<double> q = {29.0 / 18.0, 2.0 / 3.0};
    auto expectedAchievableValues =
        storm::storage::geometry::Polytope<storm::RationalNumber>::createDownwardClosure(std::vector<std::vector<storm::RationalNumber>>(
            {storm::utility::vector::convertNumericVector<storm::RationalNumber>(p), storm::utility::vector::convertNumericVector<storm::RationalNumber>(q)}));
    // due to precision issues, we enlarge one of the polytopes before checking containment
    storm::RationalNumber eps =
        storm::utility::convertNumber<storm::RationalNumber>(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
    eps += eps;
    std::vector<storm::RationalNumber> lb(2, -eps), ub(2, eps);
    auto bloatingBox = storm::storage::geometry::Hyperrectangle<storm::RationalNumber>(lb, ub).asPolytope();

#ifdef STORM_HAVE_Z3
    if (storm::test::z3AtLeastVersion(4, 8, 8) && !storm::test::z3AtLeastVersion(4, 13, 3)) {
        // z3 v4.8.8 is known to be broken here. It is working for v4.13.3.
        GTEST_SKIP() << "Test disabled since it triggers a bug in the installed version of z3.";
    }
#endif

    EXPECT_TRUE(
        expectedAchievableValues->minkowskiSum(bloatingBox)
            ->contains(result->asExplicitParetoCurveCheckResult<double>().getUnderApproximation()->convertNumberRepresentation<storm::RationalNumber>()));
    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<double>()
                    .getOverApproximation()
                    ->convertNumberRepresentation<storm::RationalNumber>()
                    ->minkowskiSum(bloatingBox)
                    ->contains(expectedAchievableValues))
        << "Result over-approximation is \n"
        << result->asExplicitParetoCurveCheckResult<double>().getOverApproximation()->toString(true);
}

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, jobscheduler_pareto_3Obj) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/jobscheduler.ma";
    std::string formulasAsString =
        "multi(Tmin=? [ F \"all_jobs_finished\" ], Pmax=? [ F<=0.2 \"half_of_jobs_finished\" ], Pmin=? [ F \"slowest_before_fastest\" ]) ";

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> ma =
        storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::MarkovAutomaton<double>>();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());

    std::vector<double> j12 = {1.266666667, 0.1617571721, 0.5};
    std::vector<double> j13 = {1.283333333, 0.1707737575, 0.25};
    std::vector<double> j23 = {1.333333333, 0.1978235137, 0.1};

    // we do our checks with rationals to avoid numerical issues when doing polytope computations...
    auto expectedAchievableValues = storm::storage::geometry::Polytope<storm::RationalNumber>::create(std::vector<std::vector<storm::RationalNumber>>(
        {storm::utility::vector::convertNumericVector<storm::RationalNumber>(j12), storm::utility::vector::convertNumericVector<storm::RationalNumber>(j13),
         storm::utility::vector::convertNumericVector<storm::RationalNumber>(j23)}));
    // due to precision issues, we enlarge one of the polytopes before checking containement
    storm::RationalNumber eps =
        storm::utility::convertNumber<storm::RationalNumber>(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
    std::vector<storm::RationalNumber> lb(3, -eps), ub(3, eps);
    auto bloatingBox = storm::storage::geometry::Hyperrectangle<storm::RationalNumber>(lb, ub).asPolytope();

    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<double>()
                    .getOverApproximation()
                    ->convertNumberRepresentation<storm::RationalNumber>()
                    ->minkowskiSum(bloatingBox)
                    ->contains(expectedAchievableValues));
}

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, jobscheduler_achievability_3Obj) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/jobscheduler.ma";
    std::string formulasAsString =
        "multi(T<=1.31 [ F \"all_jobs_finished\" ], P>=0.17 [ F<=0.2 \"half_of_jobs_finished\" ], P<=0.31 [ F \"slowest_before_fastest\" ]) ";  // true
    formulasAsString +=
        "; multi(T<=1.29 [ F \"all_jobs_finished\" ], P>=0.18 [ F<=0.2 \"half_of_jobs_finished\" ], P<=0.29 [ F \"slowest_before_fastest\" ])";  // false

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> ma =
        storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::MarkovAutomaton<double>>();
    uint_fast64_t const initState = *ma->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
    EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<double>()[initState]);

    std::unique_ptr<storm::modelchecker::CheckResult> result2 =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result2->isExplicitQualitativeCheckResult());
    EXPECT_FALSE(result2->template asExplicitQualitativeCheckResult<double>()[initState]);
}

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, jobscheduler_quantitative_3Obj) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/jobscheduler.ma";
    std::string formulasAsString =
        "multi(Tmin=? [ F \"all_jobs_finished\" ], P>=0.1797900683 [ F<=0.2 \"half_of_jobs_finished\" ], P<=0.3 [ F \"slowest_before_fastest\" ]) ";  // quantitative
    formulasAsString +=
        "; multi(T<=1.26 [ F \"all_jobs_finished\" ], P>=0.2 [ F<=0.2 \"half_of_jobs_finished\" ], Pmin=? [ F \"slowest_before_fastest\" ])";  // false

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> ma =
        storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::MarkovAutomaton<double>>();
    uint_fast64_t const initState = *ma->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    EXPECT_NEAR(1.3, result->asExplicitQuantitativeCheckResult<double>()[initState],
                storm::settings::getModule<storm::settings::modules::MultiObjectiveSettings>().getPrecision());

    std::unique_ptr<storm::modelchecker::CheckResult> result2 =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[1]->asMultiObjectiveFormula());
    ASSERT_TRUE(result2->isExplicitQualitativeCheckResult());
    EXPECT_FALSE(result2->template asExplicitQualitativeCheckResult<double>()[initState]);
}

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, jobscheduler_pareto_2Obj) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/jobscheduler.ma";
    std::string formulasAsString = "multi( Pmax=? [ F<=0.1 \"one_job_finished\"], Pmin=? [F<=0.2 \"all_jobs_finished\"]) ";

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess("");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<double>> ma =
        storm::api::buildSparseModel<double>(program, formulas)->as<storm::models::sparse::MarkovAutomaton<double>>();

    std::unique_ptr<storm::modelchecker::CheckResult> result =
        storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
    ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());

    std::vector<double> j12 = {0.2591835573, 0.01990529674};
    std::vector<double> j13 = {0.329682099, 0.01970194998};
    std::vector<double> j23 = {0.3934717664, 0.01948095743};

    // we do our checks with rationals to avoid numerical issues when doing polytope computations...
    auto expectedAchievableValues = storm::storage::geometry::Polytope<storm::RationalNumber>::create(std::vector<std::vector<storm::RationalNumber>>(
        {storm::utility::vector::convertNumericVector<storm::RationalNumber>(j12), storm::utility::vector::convertNumericVector<storm::RationalNumber>(j13),
         storm::utility::vector::convertNumericVector<storm::RationalNumber>(j23)}));
    // due to precision issues, we enlarge one of the polytopes before checking containement
    storm::RationalNumber eps =
        storm::utility::convertNumber<storm::RationalNumber>(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
    std::vector<storm::RationalNumber> lb(2, -eps), ub(2, eps);
    auto bloatingBox = storm::storage::geometry::Hyperrectangle<storm::RationalNumber>(lb, ub).asPolytope();

    EXPECT_TRUE(result->asExplicitParetoCurveCheckResult<double>()
                    .getOverApproximation()
                    ->convertNumberRepresentation<storm::RationalNumber>()
                    ->minkowskiSum(bloatingBox)
                    ->contains(expectedAchievableValues));
}

template<typename ValueType>
bool expectPointContained(std::vector<std::vector<ValueType>> const& pointset, std::vector<ValueType> const& point, ValueType precision) {
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
        if (!expectPointContained(rhs, p, precision)) {
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

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, simple_lra) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);
    env.solver().setForceSoundness(true);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/multiobj_simple_lra.ma";
    std::string formulasAsString = "multi(R{\"first\"}max=? [LRA], LRAmax=? [ x=4 ] );\n";                // pareto
    formulasAsString += "multi(R{\"first\"}max=? [LRA], LRAmin=? [ x=4] );\n";                            // pareto
    formulasAsString += "multi(R{\"first\"}max=? [LRA], LRAmin=? [ x=4] , R{\"second\"}max=? [LRA]);\n";  // pareto
    formulasAsString += "multi(R{\"first\"}max=? [LRA], LRAmax=? [ x=4] , R{\"second\"}max=? [C]);\n";    // pareto
    formulasAsString += "multi(R{\"first\"}max=? [LRA], LRAmax=? [ x=4] , R{\"second\"}min=? [C]);\n";    // pareto

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    storm::generator::NextStateGeneratorOptions options(formulas);
    auto ma = storm::builder::ExplicitModelBuilder<double>(program, options).build()->as<storm::models::sparse::MarkovAutomaton<double>>();

    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"98", "3/10"}));
        expectedPoints.emplace_back(std::vector<std::string>({"33", "7/10"}));
        double eps = 1e-4;
        EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps))
            << "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[1]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"98", "3/10"}));
        double eps = 1e-4;
        EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps))
            << "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[2]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"98", "3/10", "0"}));
        double eps = 1e-4;
        EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps))
            << "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[3]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"98", "3/10", "42/10"}));
        expectedPoints.emplace_back(std::vector<std::string>({"33", "7/10", "42/10"}));
        double eps = 1e-4;
        EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps))
            << "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[4]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"98", "3/10", "0"}));
        expectedPoints.emplace_back(std::vector<std::string>({"33", "7/10", "0"}));
        // TODO: due to numerical reasons, the next point is also included in the solution (but can be ignored)
        expectedPoints.emplace_back(std::vector<std::string>({"98", "3/10", "42/10"}));

        double eps = 1e-4;
        EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps))
            << "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
}

TEST_F(SparseMaPcaaMultiObjectiveModelCheckerTest, polling) {
    storm::Environment env;
    env.modelchecker().multi().setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa);

    std::string programFile = STORM_TEST_RESOURCES_DIR "/ma/polling.ma";
    std::string constantsDefStr = "N=1,Q=1";
    std::string formulasAsString = "multi(LRAmin=? [\"q1full\"], LRAmin=? [\"q2full\"]);\n";  // pareto
    formulasAsString += "multi(Pmin=? [F<0.1 \"q1full\"], LRAmin=? [\"q1full\"]);\n";         // pareto

    // programm, model,  formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsDefStr);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    storm::generator::NextStateGeneratorOptions options(formulas);
    auto ma = storm::builder::ExplicitModelBuilder<double>(program, options).build()->as<storm::models::sparse::MarkovAutomaton<double>>();

    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[0]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"62771/102086", "89585/102086"}));
        expectedPoints.emplace_back(std::vector<std::string>({"77531/89546", "64985/89546"}));
        double eps = 1e-4;
        EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps))
            << "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
    {
        std::unique_ptr<storm::modelchecker::CheckResult> result =
            storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *ma, formulas[1]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitParetoCurveCheckResult());
        std::vector<std::vector<std::string>> expectedPoints;
        expectedPoints.emplace_back(std::vector<std::string>({"0.25918177931828", "62771/102086"}));
        double eps = 1e-4;
        // TODO: Right now, there is a non-optimal point included due to numerical imprecisions. We therefore skip this check:
        // EXPECT_TRUE(expectSubset(result->asExplicitParetoCurveCheckResult<double>().getPoints(), convertPointset<double>(expectedPoints), eps)) <<
        // "Non-Pareto point found.";
        EXPECT_TRUE(expectSubset(convertPointset<double>(expectedPoints), result->asExplicitParetoCurveCheckResult<double>().getPoints(), eps))
            << "Pareto point missing.";
    }
}
