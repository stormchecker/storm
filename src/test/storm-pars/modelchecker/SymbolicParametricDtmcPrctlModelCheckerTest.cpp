#include "storm-config.h"
#include "storm/environment/Environment.h"
#include "test/storm_gtest.h"

#include "storm-pars/utility/parametric.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/builder/DdPrismModelBuilder.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SymbolicDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/storage/SymbolicModelDescription.h"

TEST(SymbolicDtmcPrctlModelCheckerTest, Die_RationalFunction_Sylvan) {
#ifdef STORM_HAVE_SYLVAN
    storm::storage::SymbolicModelDescription modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pdtmc/parametric_die.pm");
    storm::prism::Program program = modelDescription.preprocess().asPrismProgram();

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    // Build the die model with its reward model.
    typename storm::builder::DdPrismModelBuilder<storm::dd::DdType::Sylvan, storm::RationalFunction>::Options options;
    options.buildAllRewardModels = false;
    options.rewardModelsToBuild.insert("coin_flips");
    std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction>> model =
        storm::builder::DdPrismModelBuilder<storm::dd::DdType::Sylvan, storm::RationalFunction>().build(storm::Environment(), program, options);
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);

    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> instantiation;
    std::set<storm::RationalFunctionVariable> variables = model->getParameters();
    ASSERT_EQ(1ull, variables.size());
    instantiation.emplace(*variables.begin(), storm::utility::convertNumber<storm::RationalFunctionCoefficient>(std::string("1/2")));

    std::shared_ptr<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalFunction>> dtmc =
        model->as<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalFunction>>();

    storm::modelchecker::SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalFunction>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"one\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(*formula);
    result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<storm::dd::DdType::Sylvan>(model->getReachableStates(), model->getInitialStates()));
    storm::modelchecker::SymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>& quantitativeResult1 =
        result->asSymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>();

    EXPECT_EQ(storm::utility::parametric::evaluate<storm::RationalFunctionCoefficient>(quantitativeResult1.sum(), instantiation),
              storm::utility::convertNumber<storm::RationalFunctionCoefficient>(std::string("1/6")));

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"two\"]");

    result = checker.check(*formula);
    result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<storm::dd::DdType::Sylvan>(model->getReachableStates(), model->getInitialStates()));
    storm::modelchecker::SymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>& quantitativeResult2 =
        result->asSymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>();

    EXPECT_EQ(storm::utility::parametric::evaluate<storm::RationalFunctionCoefficient>(quantitativeResult2.sum(), instantiation),
              storm::utility::convertNumber<storm::RationalFunctionCoefficient>(std::string("1/6")));

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"three\"]");

    result = checker.check(*formula);
    result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<storm::dd::DdType::Sylvan>(model->getReachableStates(), model->getInitialStates()));
    storm::modelchecker::SymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>& quantitativeResult3 =
        result->asSymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>();

    EXPECT_EQ(storm::utility::parametric::evaluate<storm::RationalFunctionCoefficient>(quantitativeResult3.sum(), instantiation),
              storm::utility::convertNumber<storm::RationalFunctionCoefficient>(std::string("1/6")));

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"done\"]");

    result = checker.check(*formula);
    result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<storm::dd::DdType::Sylvan>(model->getReachableStates(), model->getInitialStates()));
    storm::modelchecker::SymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>& quantitativeResult4 =
        result->asSymbolicQuantitativeCheckResult<storm::dd::DdType::Sylvan, storm::RationalFunction>();

    EXPECT_EQ(storm::utility::parametric::evaluate<storm::RationalFunctionCoefficient>(quantitativeResult4.sum(), instantiation),
              storm::utility::convertNumber<storm::RationalFunctionCoefficient>(std::string("11/3")));
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}
