#include "storm-config.h"
#include "test/storm-pars/scp/LpSolverTestUtils.h"
#include "test/storm_gtest.h"

#include "storm-pars/modelchecker/instantiation/SparseDtmcInstantiationModelChecker.h"
#include "storm-pars/scp/SparseScpInstantiationSearcher.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/constants.h"

using namespace storm::pars::scp;

namespace {

// Builds a FeasibilitySynthesisTask the same way storm-pars-cli's createFeasibilitySynthesisTaskFromSettings does:
// the bound is stripped from the formula and carried separately.
std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> makeTask(
    std::shared_ptr<storm::logic::Formula const> const& formulaWithBound,
    std::optional<storm::storage::ParameterRegion<storm::RationalFunction>> const& region = std::nullopt) {
    std::shared_ptr<storm::logic::Formula> clone = formulaWithBound->clone();
    clone->asOperatorFormula().removeBound();
    storm::pars::FeasibilitySynthesisTask t(clone->asSharedPointer());
    t.setBound(formulaWithBound->asOperatorFormula().getBound());
    if (region) {
        t.setRegion(*region);
    }
    return std::make_shared<storm::pars::FeasibilitySynthesisTask const>(std::move(t));
}

std::shared_ptr<storm::logic::Formula const> parseSingleFormula(std::string const& formulaString, storm::prism::Program const& program) {
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    return formulas.at(0);
}

}  // namespace

class SparseScpInstantiationSearcherTest : public storm::pars::scp::testutils::LpSolverTest {};

TEST_P(SparseScpInstantiationSearcherTest, FindsCertifiedWitness_Dtmc) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "const double p;\n"
        "module simple\n"
        "    s : [0..2] init 0;\n"
        "    [] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
        "    [] s=1 -> (s'=1);\n"
        "    [] s=2 -> (s'=2);\n"
        "endmodule\n"
        "label \"target\" = s=1;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_Dtmc");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>=0.51 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);

    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_GE(result.value, 0.5);

    // Independently re-verify with a fresh instantiation checker, not reusing anything internal to the searcher.
    std::shared_ptr<storm::logic::Formula> clone = formula->clone();
    clone->asOperatorFormula().removeBound();
    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double> verifier(env(), *dtmc);
    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> verifyTask(*clone, false);
    verifier.specifyFormula(verifyTask);
    auto verifyResult = verifier.check(env(), result.valuation);
    uint64_t initialState = dtmc->getInitialStates().getNextSetIndex(0);
    double verifiedValue = verifyResult->template asExplicitQuantitativeCheckResult<double>().getFiniteValueVector()[initialState];

    EXPECT_NEAR(result.value, verifiedValue, 1e-6);
    EXPECT_GE(verifiedValue, 0.5);
}

// Exercises ConstantType = storm::RationalNumber end-to-end (not just that it compiles, which the
// explicit template instantiation in SparseScpInstantiationSearcher.cpp already guarantees, but that
// it runs and actually returns an exact value): result.value is a storm::ExtendedRationalNumber (an
// exact rational extended with a genuine infinity) throughout, never silently downcast to double, so
// it can be compared for *exact* equality against an independently-computed exact re-verification and
// against a hand-built exact rational threshold -- something the double-precision test above can only
// approximate (EXPECT_NEAR/EXPECT_GE against a double-rounded 0.51).
TEST_P(SparseScpInstantiationSearcherTest, FindsCertifiedWitness_Dtmc_ExactPrecision) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "const double p;\n"
        "module simple\n"
        "    s : [0..2] init 0;\n"
        "    [] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
        "    [] s=1 -> (s'=1);\n"
        "    [] s=2 -> (s'=2);\n"
        "endmodule\n"
        "label \"target\" = s=1;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_ExactPrecision");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>=0.51 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);

    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber> searcher(*dtmc);
    auto result = searcher.run(env(), task);
    static_assert(std::is_same_v<decltype(result.value), storm::ExtendedRationalNumber>,
                  "ScpResult<storm::RationalNumber>::value must be held in an exact, infinity-capable rational type, not double.");

    ASSERT_EQ(ScpStatus::Feasible, result.status);

    // Exact, not approximate: 51/100 built from exact integer division, not a double literal.
    storm::RationalNumber exactThreshold = storm::utility::convertNumber<storm::RationalNumber>(51) / storm::utility::convertNumber<storm::RationalNumber>(100);
    EXPECT_GE(result.value, exactThreshold);

    // Independently re-verify with a fresh, RationalNumber-precision instantiation checker, not
    // reusing anything internal to the searcher -- and expect *exact* equality, since neither side
    // of this comparison ever passes through double.
    std::shared_ptr<storm::logic::Formula> clone = formula->clone();
    clone->asOperatorFormula().removeBound();
    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber> verifier(env(),
                                                                                                                                                   *dtmc);
    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> verifyTask(*clone, false);
    verifier.specifyFormula(verifyTask);
    auto verifyResult = verifier.check(env(), result.valuation);
    uint64_t initialState = dtmc->getInitialStates().getNextSetIndex(0);
    storm::RationalNumber verifiedValue =
        verifyResult->template asExplicitQuantitativeCheckResult<storm::RationalNumber>().getFiniteValueVector()[initialState];

    EXPECT_EQ(result.value, verifiedValue);
    EXPECT_GE(verifiedValue, exactThreshold);
}

TEST_P(SparseScpInstantiationSearcherTest, AtMost_Dtmc) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "const double p;\n"
        "module simple\n"
        "    s : [0..2] init 0;\n"
        "    [] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
        "    [] s=1 -> (s'=1);\n"
        "    [] s=2 -> (s'=2);\n"
        "endmodule\n"
        "label \"target\" = s=1;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_AtMost");
    program = program.preprocess("");

    // Reachability is exactly p; region center is 0.5, so P<=0.3 requires real downward search.
    auto formula = parseSingleFormula("P<=0.3 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_LE(result.value, 0.3);
}

TEST_P(SparseScpInstantiationSearcherTest, ReportsNonFeasible_WhenThresholdIsUnreachableInRegion) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "const double p;\n"
        "module simple\n"
        "    s : [0..2] init 0;\n"
        "    [] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
        "    [] s=1 -> (s'=1);\n"
        "    [] s=2 -> (s'=2);\n"
        "endmodule\n"
        "label \"target\" = s=1;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_Infeasible");
    program = program.preprocess("");

    // Reachability is exactly p, so restricting p to [0, 0.5] makes ">= 0.99" unreachable anywhere in the region.
    auto formula = parseSingleFormula("P>=0.99 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);
    ASSERT_EQ(1u, vars.size());
    carl::Variable var = *vars.begin();
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation lower{{var, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.0)}};
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation upper{{var, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.5)}};
    storm::storage::ParameterRegion<storm::RationalFunction> region(lower, upper);

    auto task = makeTask(formula, region);

    ScpOptions options;
    options.maxIterations = 200;
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc, options);
    auto result = searcher.run(env(), task);

    EXPECT_NE(ScpStatus::Feasible, result.status);
}

namespace {

// One parametrized action (p) that helps, one constant "escape hatch" action (0.1) that a
// worst-case scheduler could always fall back on. Pmax(s0) = max(p, 0.1); Pmin(s0) = min(p, 0.1).
std::string const TwoActionMdpSource =
    "mdp\n"
    "const double p;\n"
    "module simple\n"
    "    s : [0..2] init 0;\n"
    "    [a] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
    "    [b] s=0 -> 0.1 : (s'=1) + 0.9 : (s'=2);\n"
    "    [] s=1 -> (s'=1);\n"
    "    [] s=2 -> (s'=2);\n"
    "endmodule\n"
    "label \"target\" = s=1;\n";

// Two independently parametrized actions, so Pmin(s0) = min(p, q) genuinely depends on both.
std::string const TwoParameterMdpSource =
    "mdp\n"
    "const double p;\n"
    "const double q;\n"
    "module simple\n"
    "    s : [0..2] init 0;\n"
    "    [a] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
    "    [b] s=0 -> q : (s'=1) + (1-q) : (s'=2);\n"
    "    [] s=1 -> (s'=1);\n"
    "    [] s=2 -> (s'=2);\n"
    "endmodule\n"
    "label \"target\" = s=1;\n";

}  // namespace

// AtMost (P<=lambda) on an Mdp defaults to the robust reading Pmax<=lambda: even the worst-case
// (maximizing) scheduler must stay under the threshold. Pmax = max(p, 0.1), so this requires a
// real downward search on p (region center 0.5 does not satisfy Pmax<=0.3).
TEST_P(SparseScpInstantiationSearcherTest, AtMost_Mdp_UnqualifiedDefaultsToPmax) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(TwoActionMdpSource, "SparseScpInstantiationSearcherTest_AtMostMdp");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P<=0.3 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double> searcher(*mdp);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_LE(result.value, 0.3);
}

// AtLeast (P>=lambda) on an Mdp defaults to the robust reading Pmin>=lambda: even the worst-case
// (minimizing) scheduler must clear the threshold. With two independently parametrized actions,
// Pmin = min(p, q), so satisfying >=0.7 genuinely requires pushing *both* parameters up.
TEST_P(SparseScpInstantiationSearcherTest, AtLeast_Mdp_UnqualifiedDefaultsToPmin) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(TwoParameterMdpSource, "SparseScpInstantiationSearcherTest_AtLeastMdp");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>=0.7 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double> searcher(*mdp);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_GE(result.value, 0.7);
}

// An explicit Pmax=? formula, consistent with what AtMost would default to, must also work.
TEST_P(SparseScpInstantiationSearcherTest, ExplicitPmax_Mdp_ConsistentWithAtMost) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(TwoActionMdpSource, "SparseScpInstantiationSearcherTest_ExplicitPmax");
    program = program.preprocess("");

    auto formula = parseSingleFormula("Pmax<=0.3 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double> searcher(*mdp);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_LE(result.value, 0.3);
}

// An explicit direction that contradicts the threshold's robust-reading default (Pmax paired with
// a ">=" threshold, which would default to Pmin) must be rejected, not silently mishandled.
TEST_P(SparseScpInstantiationSearcherTest, RejectsExplicitDirectionMismatch) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(TwoActionMdpSource, "SparseScpInstantiationSearcherTest_Mismatch");
    program = program.preprocess("");

    auto formula = parseSingleFormula("Pmax>=0.7 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double> searcher(*mdp);
    STORM_SILENT_EXPECT_THROW(searcher.run(env(), task), storm::exceptions::NotSupportedException);
}

// The initial state satisfies the target label, so its value is 1 for every parameter value and no LP is built.
TEST_P(SparseScpInstantiationSearcherTest, InitialStateIsProb1_ReturnsFeasibleImmediately) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "module simple\n"
        "    s : [0..1] init 0;\n"
        "    [] s=0 -> (s'=0);\n"
        "    [] s=1 -> (s'=1);\n"
        "endmodule\n"
        "label \"target\" = s=0;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_Prob1Initial");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>=0.5 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_DOUBLE_EQ(1.0, result.value);
    EXPECT_EQ(0u, result.iterations);
}

// The initial state can never reach the target label, so it's Prob0 regardless of any parameter.
TEST_P(SparseScpInstantiationSearcherTest, InitialStateIsProb0_ReturnsNonFeasibleImmediately) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "module simple\n"
        "    s : [0..1] init 0;\n"
        "    [] s=0 -> (s'=0);\n"
        "    [] s=1 -> (s'=1);\n"
        "endmodule\n"
        "label \"target\" = s=1;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_Prob0Initial");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>=0.1 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_NE(ScpStatus::Feasible, result.status);
    EXPECT_DOUBLE_EQ(0.0, result.value);
    EXPECT_EQ(0u, result.iterations);
}

// A tiny iteration budget forces MaxIterationsReached specifically (as opposed to NotFound via
// trust-region collapse, which the default budget would eventually hit on the same instance).
TEST_P(SparseScpInstantiationSearcherTest, ReportsMaxIterationsReached_WithTightIterationBudget) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "const double p;\n"
        "module simple\n"
        "    s : [0..2] init 0;\n"
        "    [] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
        "    [] s=1 -> (s'=1);\n"
        "    [] s=2 -> (s'=2);\n"
        "endmodule\n"
        "label \"target\" = s=1;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_MaxIter");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>=0.99 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);
    ASSERT_EQ(1u, vars.size());
    carl::Variable var = *vars.begin();
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation lower{{var, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.0)}};
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation upper{{var, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.5)}};
    storm::storage::ParameterRegion<storm::RationalFunction> region(lower, upper);

    auto task = makeTask(formula, region);
    ScpOptions options;
    options.maxIterations = 2;  // trust-region collapse would take ~13 iterations at the defaults.
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc, options);
    auto result = searcher.run(env(), task);

    EXPECT_EQ(ScpStatus::MaxIterationsReached, result.status);
}

// A strict threshold (as opposed to the non-strict >=/<= used elsewhere) must not be satisfied by
// a value that only reaches the threshold exactly -- storm's Bound::isSatisfied is trusted to get
// this right, but SCP's own accept path (comparing the model-checked value, not the LP's estimate)
// needs to actually route through it rather than special-casing non-strict comparisons somewhere.
// The region center (0.5) exactly equals the threshold, so a naive ">=" implementation would
// wrongly short-circuit as Feasible at iteration 0; a correct one must keep searching.
TEST_P(SparseScpInstantiationSearcherTest, StrictThreshold_RequiresStrictlyExceedingValue) {
    carl::VariablePool::getInstance().clear();
    std::string prismSource =
        "dtmc\n"
        "const double p;\n"
        "module simple\n"
        "    s : [0..2] init 0;\n"
        "    [] s=0 -> p : (s'=1) + (1-p) : (s'=2);\n"
        "    [] s=1 -> (s'=1);\n"
        "    [] s=2 -> (s'=2);\n"
        "endmodule\n"
        "label \"target\" = s=1;\n";
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(prismSource, "SparseScpInstantiationSearcherTest_Strict");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>0.5 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_GT(result.value, 0.5);
    EXPECT_GT(result.iterations, 0u);
}

// A region that doesn't cover a parameter actually occurring in the model must fail cleanly, not
// with an uncaught std::out_of_range from an internal map lookup.
TEST_P(SparseScpInstantiationSearcherTest, RegionMissingAModelParameter_ThrowsCleanly) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(TwoParameterMdpSource, "SparseScpInstantiationSearcherTest_MissingParam");
    program = program.preprocess("");

    auto formula = parseSingleFormula("P>=0.7 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    // Region only covers "p", but the model also has "q".
    auto vars = storm::models::sparse::getProbabilityParameters(*mdp);
    carl::Variable varP;
    for (auto const& v : vars) {
        if (v.name() == "p") {
            varP = v;
        }
    }
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation lower{{varP, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.0)}};
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation upper{{varP, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(1.0)}};
    storm::storage::ParameterRegion<storm::RationalFunction> region(lower, upper);

    auto task = makeTask(formula, region);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double> searcher(*mdp);
    STORM_SILENT_EXPECT_THROW(searcher.run(env(), task), storm::exceptions::NotSupportedException);
}

namespace {

// A "retry until success" DTMC: each attempt at s=0 costs 1 and succeeds (reaching the target)
// with probability p, or loops back to try again with probability 1-p. Reachability reward is
// exactly 1/p -- a genuinely nonlinear function of p, even though the transitions and reward are
// both individually affine, so satisfying a threshold requires real (non-trivial-at-the-region-
// center) search, exactly like the probability tests above.
std::string const RetryUntilSuccessDtmcSource =
    "dtmc\n"
    "const double p;\n"
    "module simple\n"
    "    s : [0..1] init 0;\n"
    "    [] s=0 -> p : (s'=1) + (1-p) : (s'=0);\n"
    "    [] s=1 -> (s'=1);\n"
    "endmodule\n"
    "rewards\n"
    "    [] s=0 : 1;\n"
    "endrewards\n"
    "label \"target\" = s=1;\n";

}  // namespace

// AtMost (R<=lambda) on a Dtmc: reachability reward 1/p <= 1.5 needs p >= 2/3, well above the
// region center's 0.5 (reward 2) -- requires a real upward search on p, not just accepting the
// center. Independently re-verified with a fresh instantiation checker, not reusing anything
// internal to the searcher, mirroring FindsCertifiedWitness_Dtmc for the probability case.
TEST_P(SparseScpInstantiationSearcherTest, AtMost_Dtmc_Reward) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(RetryUntilSuccessDtmcSource, "SparseScpInstantiationSearcherTest_RewardAtMost");
    program = program.preprocess("");

    auto formula = parseSingleFormula("R<=1.5 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_LE(result.value, 1.5);

    std::shared_ptr<storm::logic::Formula> clone = formula->clone();
    clone->asOperatorFormula().removeBound();
    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double> verifier(env(), *dtmc);
    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> verifyTask(*clone, false);
    verifier.specifyFormula(verifyTask);
    auto verifyResult = verifier.check(env(), result.valuation);
    uint64_t initialState = dtmc->getInitialStates().getNextSetIndex(0);
    double verifiedValue = verifyResult->template asExplicitQuantitativeCheckResult<double>().getFiniteValueVector()[initialState];

    EXPECT_NEAR(result.value, verifiedValue, 1e-6);
    EXPECT_LE(verifiedValue, 1.5);
}

// AtLeast (R>=lambda) on a Dtmc: reachability reward 1/p >= 5 needs p <= 0.2, below the region
// center's 0.5 (reward 2) -- requires a real downward search.
TEST_P(SparseScpInstantiationSearcherTest, AtLeast_Dtmc_Reward) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program =
        storm::parser::PrismParser::parseFromString(RetryUntilSuccessDtmcSource, "SparseScpInstantiationSearcherTest_RewardAtLeast");
    program = program.preprocess("");

    auto formula = parseSingleFormula("R>=5 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_GE(result.value, 5.0);
}

namespace {

// Same retry-until-success structure, but the per-attempt cost is itself a second parameter "c"
// that occurs nowhere in the transition matrix -- only in the reward function.
std::string const RetryUntilSuccessWithParametricCostSource =
    "dtmc\n"
    "const double p;\n"
    "const double c;\n"
    "module simple\n"
    "    s : [0..1] init 0;\n"
    "    [] s=0 -> p : (s'=1) + (1-p) : (s'=0);\n"
    "    [] s=1 -> (s'=1);\n"
    "endmodule\n"
    "rewards\n"
    "    [] s=0 : c;\n"
    "endrewards\n"
    "label \"target\" = s=1;\n";

}  // namespace

// A region covering only the transition parameter "p" but not the reward-only parameter "c" must
// fail cleanly -- this is only possible at all because AffinePMdpDecomposition's reward
// decomposition folds a reward-only parameter into getParameters() (see
// AffinePMdpDecompositionTest.RewardDecomposition_PicksUpRewardOnlyParameter); this test is the
// end-to-end confirmation that the searcher's own region-coverage check actually benefits from
// that, not just the decomposition in isolation.
TEST_P(SparseScpInstantiationSearcherTest, RewardOnlyParameterMissingFromRegion_ThrowsCleanly) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program =
        storm::parser::PrismParser::parseFromString(RetryUntilSuccessWithParametricCostSource, "SparseScpInstantiationSearcherTest_RewardOnlyParamMissing");
    program = program.preprocess("");

    auto formula = parseSingleFormula("R<=2 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto vars = storm::models::sparse::getRewardParameters(*dtmc);
    ASSERT_EQ(1u, vars.size());
    carl::Variable varC = *vars.begin();
    // Deliberately covers "c" only -- the model also has "p", which is a transition parameter, so
    // this region is missing it (the reverse of RegionMissingAModelParameter_ThrowsCleanly, which
    // covers a transition parameter but misses a probability one).
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation lower{{varC, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.5)}};
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation upper{{varC, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(2.0)}};
    storm::storage::ParameterRegion<storm::RationalFunction> region(lower, upper);

    auto task = makeTask(formula, region);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    STORM_SILENT_EXPECT_THROW(searcher.run(env(), task), storm::exceptions::NotSupportedException);
}

// With both "p" and the reward-only "c" covered by the region, the search must actually be able to
// move both to satisfy the threshold -- confirms the reward-only parameter isn't just *discovered*
// (the test above) but is genuinely usable as an LP variable throughout the search.
TEST_P(SparseScpInstantiationSearcherTest, RewardOnlyParameter_UsableInSearch) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program =
        storm::parser::PrismParser::parseFromString(RetryUntilSuccessWithParametricCostSource, "SparseScpInstantiationSearcherTest_RewardOnlyParamUsable");
    program = program.preprocess("");

    auto formula = parseSingleFormula("R<=2 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto probVars = storm::models::sparse::getProbabilityParameters(*dtmc);
    auto rewVars = storm::models::sparse::getRewardParameters(*dtmc);
    ASSERT_EQ(1u, probVars.size());
    ASSERT_EQ(1u, rewVars.size());
    carl::Variable varP = *probVars.begin();
    carl::Variable varC = *rewVars.begin();

    // Region center: p=0.5, c=1.25 -> R = c/p = 2.5, which does not satisfy R<=2.
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation lower{{varP, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.1)},
                                                                              {varC, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.5)}};
    storm::storage::ParameterRegion<storm::RationalFunction>::Valuation upper{{varP, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.9)},
                                                                              {varC, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(2.0)}};
    storm::storage::ParameterRegion<storm::RationalFunction> region(lower, upper);

    auto task = makeTask(formula, region);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_LE(result.value, 2.0);
}

namespace {

// One parametrized "retry until success" action (a, reward 1 per attempt) and one constant
// "give up" action (b) that goes straight to an unlabeled absorbing trap state that never reaches
// the target -- exercises the RewInf handling: Rmin(s0) = 1/p (a minimizing scheduler never picks
// the infinite-reward action b), but Rmax(s0) = infinity (a maximizing scheduler always can, and
// does, pick b). Mirrors TwoActionMdpSource's probability-property structure.
std::string const RetryOrGiveUpMdpSource =
    "mdp\n"
    "const double p;\n"
    "module simple\n"
    "    s : [0..2] init 0;\n"
    "    [a] s=0 -> p : (s'=1) + (1-p) : (s'=0);\n"
    "    [b] s=0 -> (s'=2);\n"
    "    [] s=1 -> (s'=1);\n"
    "    [] s=2 -> (s'=2);\n"
    "endmodule\n"
    "rewards\n"
    "    [a] s=0 : 1;\n"
    "endrewards\n"
    "label \"target\" = s=1;\n";

}  // namespace

// AtLeast (R>=lambda) on an Mdp defaults to the robust reading Rmin>=lambda. Rmin = 1/p (the
// give-up action is never chosen by a minimizing scheduler), so satisfying >=5 needs p<=0.2 -- a
// real downward search. This is also the row-level-exclusion case: state 0 remains a genuine
// "maybe" LP variable (Rmin is finite), but action b's row must be dropped from its Bellman
// constraint (it leads straight to an infinite-value successor) without dropping the whole state,
// which is exactly what SparseScpInstantiationSearcher's excludedRows computation (via
// SparseMatrix::getRowFilter) is for.
TEST_P(SparseScpInstantiationSearcherTest, AtLeast_Mdp_Reward_RowLevelExclusion) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(RetryOrGiveUpMdpSource, "SparseScpInstantiationSearcherTest_RewardMdpAtLeast");
    program = program.preprocess("");

    auto formula = parseSingleFormula("R>=5 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double> searcher(*mdp);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_GE(result.value, 5.0);
}

// AtMost (R<=lambda) on the same Mdp defaults to the robust reading Rmax<=lambda. Rmax(s0) is
// infinity (a maximizing scheduler always has the give-up action available), so s0 is classified
// as a RewInf state regardless of p -- this must be reported as NotFound immediately via the
// initial-state short-circuit (see run()'s zeroStates/oneStates/infiniteStates check), not by
// building an LP that can never converge.
TEST_P(SparseScpInstantiationSearcherTest, AtMost_Mdp_Reward_InitialStateIsRewInf_ReportsNotFoundImmediately) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(RetryOrGiveUpMdpSource, "SparseScpInstantiationSearcherTest_RewardMdpAtMost");
    program = program.preprocess("");

    auto formula = parseSingleFormula("R<=3 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, double> searcher(*mdp);
    auto result = searcher.run(env(), task);

    ASSERT_NE(ScpStatus::Feasible, result.status);
    EXPECT_EQ(0u, result.iterations);
    EXPECT_TRUE(storm::utility::isInfinity(result.value));
}

// The exact-precision counterpart of the test above: the result's value must be a genuine
// +infinity, not the 100000000000 sentinel that storm::utility::infinity yields for a type that has
// no infinity of its own -- i.e. ScpResult holds it in a storm::ExtendedRationalNumber.
TEST_P(SparseScpInstantiationSearcherTest, AtMost_Mdp_Reward_InitialStateIsRewInf_ExactPrecision) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program =
        storm::parser::PrismParser::parseFromString(RetryOrGiveUpMdpSource, "SparseScpInstantiationSearcherTest_RewardMdpAtMostExact");
    program = program.preprocess("");

    auto formula = parseSingleFormula("R<=3 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formula})->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    auto task = makeTask(formula);
    SparseScpInstantiationSearcher<storm::models::sparse::Mdp<storm::RationalFunction>, storm::RationalNumber> searcher(*mdp);
    auto result = searcher.run(env(), task);

    static_assert(std::is_same_v<decltype(result.value), storm::ExtendedRationalNumber>,
                  "ScpResult<storm::RationalNumber>::value must be held in an extended (infinity-capable) rational type.");
    ASSERT_NE(ScpStatus::Feasible, result.status);
    EXPECT_EQ(0u, result.iterations);
    EXPECT_TRUE(storm::utility::isInfinity(result.value));
    EXPECT_TRUE(result.value.isPositiveInfinity());
}

namespace {

std::string const TwoNamedRewardModelsDtmcSource =
    "dtmc\n"
    "const double p;\n"
    "module simple\n"
    "    s : [0..1] init 0;\n"
    "    [] s=0 -> p : (s'=1) + (1-p) : (s'=0);\n"
    "    [] s=1 -> (s'=1);\n"
    "endmodule\n"
    "rewards \"cost1\"\n"
    "    [] s=0 : 1;\n"
    "endrewards\n"
    "rewards \"cost2\"\n"
    "    [] s=0 : 2;\n"
    "endrewards\n"
    "label \"target\" = s=1;\n";

}  // namespace

// A formula that explicitly names its reward model ("cost1") must resolve to that specific one
// (not, say, silently falling back to some other reward model) -- distinguishable here because
// cost1's and cost2's per-attempt cost differ by a factor of 2, so using the wrong one would still
// "succeed" but at a value inconsistent with the intended reward model.
TEST_P(SparseScpInstantiationSearcherTest, NamedRewardModel_ResolvesCorrectly) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(TwoNamedRewardModelsDtmcSource, "SparseScpInstantiationSearcherTest_NamedRM");
    program = program.preprocess("");

    auto formulaCost1 = parseSingleFormula("R{\"cost1\"}<=1.5 [F \"target\"]", program);
    auto formulaCost2 = parseSingleFormula("R{\"cost2\"}<=1.5 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formulaCost1, formulaCost2})
            ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // cost1: reward 1/attempt, so R<=1.5 needs p>=2/3 (same threshold/model as AtMost_Dtmc_Reward).
    auto task = makeTask(formulaCost1);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    auto result = searcher.run(env(), task);

    ASSERT_EQ(ScpStatus::Feasible, result.status);
    EXPECT_LE(result.value, 1.5);

    // Independently re-verify with a fresh checker specifying "cost1" explicitly, confirming the
    // certified valuation genuinely satisfies *that* reward model's threshold.
    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double> verifier(env(), *dtmc);
    std::shared_ptr<storm::logic::Formula> clone = formulaCost1->clone();
    clone->asOperatorFormula().removeBound();
    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> verifyTask(*clone, false);
    verifier.specifyFormula(verifyTask);
    auto verifyResult = verifier.check(env(), result.valuation);
    uint64_t initialState = dtmc->getInitialStates().getNextSetIndex(0);
    double verifiedValue = verifyResult->template asExplicitQuantitativeCheckResult<double>().getFiniteValueVector()[initialState];
    EXPECT_NEAR(result.value, verifiedValue, 1e-6);
}

// A reward formula that does not name a reward model, on a model with more than one, has no way
// to pick one and must fail cleanly rather than silently guessing.
TEST_P(SparseScpInstantiationSearcherTest, UnnamedRewardModel_WithoutUniqueRewardModel_ThrowsCleanly) {
    carl::VariablePool::getInstance().clear();
    storm::prism::Program program =
        storm::parser::PrismParser::parseFromString(TwoNamedRewardModelsDtmcSource, "SparseScpInstantiationSearcherTest_UnnamedNoUnique");
    program = program.preprocess("");

    auto formulaCost1 = parseSingleFormula("R{\"cost1\"}<=1.5 [F \"target\"]", program);
    auto formulaCost2 = parseSingleFormula("R{\"cost2\"}<=1.5 [F \"target\"]", program);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::api::buildSparseModel<storm::RationalFunction>(program, {formulaCost1, formulaCost2})
            ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    ASSERT_FALSE(dtmc->hasUniqueRewardModel());

    auto unnamedFormula = parseSingleFormula("R<=1.5 [F \"target\"]", program);
    auto task = makeTask(unnamedFormula);
    SparseScpInstantiationSearcher<storm::models::sparse::Dtmc<storm::RationalFunction>, double> searcher(*dtmc);
    STORM_SILENT_EXPECT_THROW(searcher.run(env(), task), storm::exceptions::NotSupportedException);
}

INSTANTIATE_TEST_SUITE_P(LpSolvers, SparseScpInstantiationSearcherTest, testing::ValuesIn(storm::pars::scp::testutils::lpSolversUnderTest()),
                         storm::pars::scp::testutils::LpSolverTest::solverName);
// The suite is empty if no LP solver was compiled in.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SparseScpInstantiationSearcherTest);
