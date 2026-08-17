#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/solver/NativeLinearEquationSolver.h"

namespace {

#ifdef STORM_HAVE_GMM
class GBGmmxxDoubleGmresEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class GBEigenDoubleDGmresEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::DGmres);
        env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Ilu);
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};
#endif

class GBEigenRationalLUEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class GBNativeSorEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SOR);
        env.solver().native().setSorOmega(storm::utility::convertNumber<storm::RationalNumber>(0.8));  // A test fails if this is set to 0.9...
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class GBNativeWalkerChaeEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::WalkerChae);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().native().setMaximalNumberOfIterations(50000);
        return env;
    }
};

#ifdef STORM_HAVE_GMM
class DistrGmmxxDoubleGmresEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::LraDistributionEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};
#endif

class DistrEigenRationalLUEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static const bool isExact = true;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::LraDistributionEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class DistrNativeWalkerChaeEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::WalkerChae);
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().native().setMaximalNumberOfIterations(50000);
        return env;
    }
};

class ValueIterationEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::ValueIteration);
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

template<typename TestType>
class LraDtmcPrctlModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    LraDtmcPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}
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
    GBGmmxxDoubleGmresEnvironment, GBEigenDoubleDGmresEnvironment, DistrGmmxxDoubleGmresEnvironment,
#endif
    GBEigenRationalLUEnvironment, GBNativeSorEnvironment, GBNativeWalkerChaeEnvironment, DistrEigenRationalLUEnvironment, DistrNativeWalkerChaeEnvironment,
    ValueIterationEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(LraDtmcPrctlModelCheckerTest, TestingTypes, );

TYPED_TEST(LraDtmcPrctlModelCheckerTest, LRASingleBscc) {
    typedef typename TestFixture::ValueType ValueType;

    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder;
    std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> dtmc;

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(2, 2, 2);
        matrixBuilder.addNextValue(0, 1, this->parseNumber("1"));
        matrixBuilder.addNextValue(1, 0, this->parseNumber("1"));
        storm::storage::SparseMatrix<ValueType> transitionMatrix = matrixBuilder.build();

        storm::models::sparse::StateLabeling ap(2);
        ap.addLabel("a");
        ap.addLabelToState("a", 1);

        dtmc.reset(new storm::models::sparse::Dtmc<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> checker(*dtmc);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRA=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[1], this->precision());
    }
    {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(2, 2, 4);
        matrixBuilder.addNextValue(0, 0, this->parseNumber("0.5"));
        matrixBuilder.addNextValue(0, 1, this->parseNumber("0.5"));
        matrixBuilder.addNextValue(1, 0, this->parseNumber("0.5"));
        matrixBuilder.addNextValue(1, 1, this->parseNumber("0.5"));
        storm::storage::SparseMatrix<ValueType> transitionMatrix = matrixBuilder.build();

        storm::models::sparse::StateLabeling ap(2);
        ap.addLabel("a");
        ap.addLabelToState("a", 1);

        dtmc.reset(new storm::models::sparse::Dtmc<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> checker(*dtmc);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRA=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[1], this->precision());
    }

    {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(3, 3, 3);
        matrixBuilder.addNextValue(0, 1, this->parseNumber("1"));
        matrixBuilder.addNextValue(1, 2, this->parseNumber("1"));
        matrixBuilder.addNextValue(2, 0, this->parseNumber("1"));
        storm::storage::SparseMatrix<ValueType> transitionMatrix = matrixBuilder.build();

        storm::models::sparse::StateLabeling ap(3);
        ap.addLabel("a");
        ap.addLabelToState("a", 2);

        dtmc.reset(new storm::models::sparse::Dtmc<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> checker(*dtmc);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRA=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[1], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[2], this->precision());
    }
}

TYPED_TEST(LraDtmcPrctlModelCheckerTest, LRA) {
    typedef typename TestFixture::ValueType ValueType;

    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder;
    std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> dtmc;

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(15, 15, 20, true);
        matrixBuilder.addNextValue(0, 1, this->parseNumber("1"));
        matrixBuilder.addNextValue(1, 4, this->parseNumber("0.7"));
        matrixBuilder.addNextValue(1, 6, this->parseNumber("0.3"));
        matrixBuilder.addNextValue(2, 0, this->parseNumber("1"));

        matrixBuilder.addNextValue(3, 5, this->parseNumber("0.8"));
        matrixBuilder.addNextValue(3, 9, this->parseNumber("0.2"));
        matrixBuilder.addNextValue(4, 3, this->parseNumber("1"));
        matrixBuilder.addNextValue(5, 3, this->parseNumber("1"));

        matrixBuilder.addNextValue(6, 7, this->parseNumber("1"));
        matrixBuilder.addNextValue(7, 8, this->parseNumber("1"));
        matrixBuilder.addNextValue(8, 6, this->parseNumber("1"));

        matrixBuilder.addNextValue(9, 10, this->parseNumber("1"));
        matrixBuilder.addNextValue(10, 9, this->parseNumber("1"));
        matrixBuilder.addNextValue(11, 9, this->parseNumber("1"));

        matrixBuilder.addNextValue(12, 5, this->parseNumber("0.4"));
        matrixBuilder.addNextValue(12, 8, this->parseNumber("0.3"));
        matrixBuilder.addNextValue(12, 11, this->parseNumber("0.3"));

        matrixBuilder.addNextValue(13, 7, this->parseNumber("0.7"));
        matrixBuilder.addNextValue(13, 12, this->parseNumber("0.3"));

        matrixBuilder.addNextValue(14, 12, this->parseNumber("1"));

        storm::storage::SparseMatrix<ValueType> transitionMatrix = matrixBuilder.build();

        storm::models::sparse::StateLabeling ap(15);
        ap.addLabel("a");
        ap.addLabelToState("a", 1);
        ap.addLabelToState("a", 4);
        ap.addLabelToState("a", 5);
        ap.addLabelToState("a", 7);
        ap.addLabelToState("a", 11);
        ap.addLabelToState("a", 13);
        ap.addLabelToState("a", 14);

        dtmc.reset(new storm::models::sparse::Dtmc<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> checker(*dtmc);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRA=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("1/10"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult1[3], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[6], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult1[9], this->precision());
        EXPECT_NEAR(this->parseNumber("1/10"), quantitativeResult1[12], this->precision());
        EXPECT_NEAR(this->parseNumber("79/300"), quantitativeResult1[13], this->precision());
        EXPECT_NEAR(this->parseNumber("1/10"), quantitativeResult1[14], this->precision());
    }
}
}  // namespace