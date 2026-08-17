#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace {

#ifdef STORM_HAVE_GMM
class GmmxxDoubleGmresEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class EigenDoubleDGmresEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::DGmres);
        env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Ilu);
        env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};
#endif

class EigenRationalLUEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class NativeSorEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SOR);
        env.solver().native().setSorOmega(storm::utility::convertNumber<storm::RationalNumber>(0.9));
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class NativePowerEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class NativeWalkerChaeEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::WalkerChae);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().native().setMaximalNumberOfIterations(50000);
        return env;
    }
};

template<typename TestType>
class ConditionalDtmcPrctlModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    ConditionalDtmcPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    storm::Environment const& env() const {
        return _environment;
    }
    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }
    ValueType precision() const {
        return TestType::isExact ? parseNumber("0") : parseNumber("1e-6");
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<
#ifdef STORM_HAVE_GMM
    GmmxxDoubleGmresEnvironment, EigenDoubleDGmresEnvironment,
#endif
    EigenRationalLUEnvironment, NativeSorEnvironment, NativePowerEnvironment, NativeWalkerChaeEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(ConditionalDtmcPrctlModelCheckerTest, TestingTypes, );

TYPED_TEST(ConditionalDtmcPrctlModelCheckerTest, Conditional) {
    typedef typename TestFixture::ValueType ValueType;

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/test_conditional.pm");

    storm::generator::NextStateGeneratorOptions options;
    options.setBuildAllLabels().setBuildAllRewardModels();
    std::shared_ptr<storm::models::sparse::Model<ValueType>> model = storm::builder::ExplicitModelBuilder<ValueType>(program, options).build();
    ASSERT_TRUE(model->getType() == storm::models::ModelType::Dtmc);
    ASSERT_EQ(4ul, model->getNumberOfStates());
    ASSERT_EQ(5ul, model->getNumberOfTransitions());

    std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> dtmc = model->template as<storm::models::sparse::Dtmc<ValueType>>();

    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> checker(*dtmc);

    // A parser that we use for conveniently constructing the formulas.

    auto expManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::parser::FormulaParser formulaParser(expManager);
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"target\"]");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[0], this->precision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"target\" || F \"condition\"]");

    result = checker.check(this->env(), *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(storm::utility::one<ValueType>(), quantitativeResult2[0], this->precision());

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"target\"]");

    result = checker.check(this->env(), *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_EQ(storm::utility::infinity<ValueType>(), quantitativeResult3[0]);

    formula = formulaParser.parseSingleFormulaFromString("R=? [F \"target\" || F \"condition\"]");

    result = checker.check(this->env(), *formula);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(storm::utility::one<ValueType>(), quantitativeResult4[0], this->precision());
}
}  // namespace
