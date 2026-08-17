#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace {

class EigenEnvironment {
   public:
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        return env;
    }
};

class EliminationEnvironment {
   public:
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Elimination);
        return env;
    }
};

template<typename TestType>
class ParametricDtmcPrctlModelCheckerTest : public ::testing::Test {
   public:
    ParametricDtmcPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    storm::Environment const& env() const {
        return _environment;
    }

   private:
    storm::Environment _environment;
};

storm::RationalFunctionCoefficient parseNumber(std::string const& input) {
    return storm::utility::convertNumber<storm::RationalFunctionCoefficient>(input);
}

void checkDie(storm::Environment const& env) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pdtmc/parametric_die.pm");
    storm::generator::NextStateGeneratorOptions options;
    options.setBuildAllLabels().setBuildAllRewardModels();
    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model =
        storm::builder::ExplicitModelBuilder<storm::RationalFunction>(program, options).build();

    // A parser that we use for conveniently constructing the formulas.

    auto expManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::parser::FormulaParser formulaParser(expManager);

    ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);

    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(dtmc->getNumberOfStates(), 13ull);
    ASSERT_EQ(dtmc->getNumberOfTransitions(), 20ull);

    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> instantiation;
    std::set<storm::RationalFunctionVariable> variables = storm::models::sparse::getProbabilityParameters(*dtmc);
    ASSERT_EQ(variables.size(), 1ull);
    instantiation.emplace(*variables.begin(), parseNumber("1/2"));

    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>> checker(*dtmc);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"one\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<storm::RationalFunction>& quantitativeResult1 =
        result->asExplicitQuantitativeCheckResult<storm::RationalFunction>();

    EXPECT_EQ(parseNumber("1/6"), quantitativeResult1[0].evaluate(instantiation));

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"two\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<storm::RationalFunction>& quantitativeResult2 =
        result->asExplicitQuantitativeCheckResult<storm::RationalFunction>();

    EXPECT_EQ(parseNumber("1/6"), quantitativeResult2[0].evaluate(instantiation));

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"three\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<storm::RationalFunction>& quantitativeResult3 =
        result->asExplicitQuantitativeCheckResult<storm::RationalFunction>();

    EXPECT_EQ(parseNumber("1/6"), quantitativeResult3[0].evaluate(instantiation));

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"done\"]");

    result = checker.check(env, *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<storm::RationalFunction>& quantitativeResult4 =
        result->asExplicitQuantitativeCheckResult<storm::RationalFunction>();

    EXPECT_EQ(parseNumber("11/3"), quantitativeResult4[0].evaluate(instantiation));
}

typedef ::testing::Types<EigenEnvironment, EliminationEnvironment> TestingTypes;

TYPED_TEST_SUITE(ParametricDtmcPrctlModelCheckerTest, TestingTypes, );
TYPED_TEST(ParametricDtmcPrctlModelCheckerTest, Die) {
    checkDie(this->env());
}
}  // namespace
