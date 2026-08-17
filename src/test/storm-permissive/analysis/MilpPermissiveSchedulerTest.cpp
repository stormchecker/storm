#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm-permissive/analysis/PermissiveSchedulers.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/environment/Environment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/storage/expressions/ExpressionManager.h"

#ifdef STORM_HAVE_GUROBI

TEST(MilpPermissiveSchedulerTest, DieSelection) {
    if (storm::test::noGurobi) {
        GTEST_SKIP();
    }
    storm::Environment env;

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/die_c1.nm");
    storm::parser::FormulaParser formulaParser(program.getManager().getSharedPointer());
    std::string formulaString = "";
    formulaString += "P>=0.10 [ F \"one\"];\n";
    formulaString += "P>=0.17 [ F \"one\"];\n";
    formulaString += "P<=0.10 [ F \"one\"];\n";
    formulaString += "P<=0.17 [ F \"one\"];\n";
    auto formulas = formulaParser.parseFromString(formulaString);

    auto const& formula02 = formulas[0].getRawFormula()->asProbabilityOperatorFormula();
    ASSERT_TRUE(storm::logic::isLowerBound(formula02.getComparisonType()));
    auto const& formula001 = formulas[1].getRawFormula()->asProbabilityOperatorFormula();
    auto const& formula02b = formulas[2].getRawFormula()->asProbabilityOperatorFormula();
    auto const& formula001b = formulas[3].getRawFormula()->asProbabilityOperatorFormula();

    // Customize and perform model-building.
    storm::generator::NextStateGeneratorOptions options;
    options.setBuildAllLabels().setBuildChoiceLabels(true);
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp =
        storm::builder::ExplicitModelBuilder<double>(program, options).build()->as<storm::models::sparse::Mdp<double>>();

    boost::optional<storm::ps::SubMDPPermissiveScheduler<>> perms = storm::ps::computePermissiveSchedulerViaMILP<>(env, *mdp, formula02);
    EXPECT_TRUE(perms.is_initialized());
    boost::optional<storm::ps::SubMDPPermissiveScheduler<>> perms2 = storm::ps::computePermissiveSchedulerViaMILP<>(env, *mdp, formula001);
    EXPECT_FALSE(perms2.is_initialized());

    boost::optional<storm::ps::SubMDPPermissiveScheduler<>> perms3 = storm::ps::computePermissiveSchedulerViaMILP<>(env, *mdp, formula02b);
    EXPECT_FALSE(perms3.is_initialized());
    boost::optional<storm::ps::SubMDPPermissiveScheduler<>> perms4 = storm::ps::computePermissiveSchedulerViaMILP<>(env, *mdp, formula001b);
    EXPECT_TRUE(perms4.is_initialized());

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker0(*mdp);

    std::unique_ptr<storm::modelchecker::CheckResult> result0 = checker0.check(env, formula02);
    storm::modelchecker::ExplicitQualitativeCheckResult<double>& qualitativeResult0 = result0->template asExplicitQualitativeCheckResult<double>();

    ASSERT_FALSE(qualitativeResult0[0]);

    auto submdp = perms->apply();
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker1(submdp);

    std::unique_ptr<storm::modelchecker::CheckResult> result1 = checker1.check(env, formula02);
    storm::modelchecker::ExplicitQualitativeCheckResult<double>& qualitativeResult1 = result1->template asExplicitQualitativeCheckResult<double>();

    EXPECT_TRUE(qualitativeResult1[0]);
}

#endif
