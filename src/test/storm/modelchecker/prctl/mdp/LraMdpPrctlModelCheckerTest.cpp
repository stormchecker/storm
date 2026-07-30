#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/solver/StandardMinMaxLinearEquationSolver.h"

namespace {

class SparseValueTypeValueIterationEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setNondetLraMethod(storm::solver::LraMethod::ValueIteration);
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};

class SparseValueTypeLinearProgrammingEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setNondetLraMethod(storm::solver::LraMethod::LinearProgramming);
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};

class SparseSoundEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        return env;
    }
};

class SparseRationalLinearProgrammingEnvironment {
   public:
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setNondetLraMethod(storm::solver::LraMethod::LinearProgramming);
        return env;
    }
};

template<typename TestType>
class LraMdpPrctlModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::Mdp<ValueType> SparseModelType;

    LraMdpPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}

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
    bool isSparseModel() const {
        return std::is_same<typename TestType::ModelType, SparseModelType>::value;
    }

    std::pair<std::shared_ptr<SparseModelType>, std::vector<std::shared_ptr<storm::logic::Formula const>>> buildModelFormulas(
        std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<SparseModelType>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
        program = program.preprocess(constantDefinitionString);
        result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
        result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<SparseModelType>();
        return result;
    }

    std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> getTasks(
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) const {
        std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> result;
        for (auto const& f : formulas) {
            result.emplace_back(*f);
        }
        return result;
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<SparseValueTypeValueIterationEnvironment, SparseValueTypeLinearProgrammingEnvironment, SparseSoundEnvironment,
                         SparseRationalLinearProgrammingEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(LraMdpPrctlModelCheckerTest, TestingTypes, );

TYPED_TEST(LraMdpPrctlModelCheckerTest, LRA_SingleMec) {
    typedef typename TestFixture::ValueType ValueType;

    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder;
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp;

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

        mdp.reset(new storm::models::sparse::Mdp<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[1], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"a\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult2[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult2[1], this->precision());
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

        mdp.reset(new storm::models::sparse::Mdp<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[1], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"a\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult2[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult2[1], this->precision());
    }
    {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(4, 3, 4, true, true, 3);
        matrixBuilder.newRowGroup(0);
        matrixBuilder.addNextValue(0, 1, this->parseNumber("1"));
        matrixBuilder.newRowGroup(1);
        matrixBuilder.addNextValue(1, 0, this->parseNumber("1"));
        matrixBuilder.addNextValue(2, 2, this->parseNumber("1"));
        matrixBuilder.newRowGroup(3);
        matrixBuilder.addNextValue(3, 0, this->parseNumber("1"));
        storm::storage::SparseMatrix<ValueType> transitionMatrix = matrixBuilder.build();

        storm::models::sparse::StateLabeling ap(3);
        ap.addLabel("a");
        ap.addLabelToState("a", 2);
        ap.addLabel("b");
        ap.addLabelToState("b", 0);
        ap.addLabel("c");
        ap.addLabelToState("c", 0);
        ap.addLabelToState("c", 2);

        mdp.reset(new storm::models::sparse::Mdp<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[1], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"a\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[1], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"b\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult3[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult3[1], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult3[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"b\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult4[0], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult4[1], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult4[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"c\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult5 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("2/3"), quantitativeResult5[0], this->precision());
        EXPECT_NEAR(this->parseNumber("2/3"), quantitativeResult5[1], this->precision());
        EXPECT_NEAR(this->parseNumber("2/3"), quantitativeResult5[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"c\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult6 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult6[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult6[1], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult6[2], this->precision());
    }
}

TYPED_TEST(LraMdpPrctlModelCheckerTest, LRA) {
    typedef typename TestFixture::ValueType ValueType;

    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder;
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp;

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(4, 3, 4, true, true, 3);
        matrixBuilder.newRowGroup(0);
        matrixBuilder.addNextValue(0, 1, this->parseNumber("1"));
        matrixBuilder.newRowGroup(1);
        matrixBuilder.addNextValue(1, 1, this->parseNumber("1"));
        matrixBuilder.addNextValue(2, 2, this->parseNumber("1"));
        matrixBuilder.newRowGroup(3);
        matrixBuilder.addNextValue(3, 2, this->parseNumber("1"));
        storm::storage::SparseMatrix<ValueType> transitionMatrix = matrixBuilder.build();

        storm::models::sparse::StateLabeling ap(3);
        ap.addLabel("a");
        ap.addLabelToState("a", 0);
        ap.addLabel("b");
        ap.addLabelToState("b", 1);
        ap.addLabel("c");
        ap.addLabelToState("c", 2);

        mdp.reset(new storm::models::sparse::Mdp<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult1[1], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult1[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"a\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[1], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"b\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("1"), quantitativeResult3[0], this->precision());
        EXPECT_NEAR(this->parseNumber("1"), quantitativeResult3[1], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult3[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"b\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult4[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult4[1], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult4[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"c\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult5 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("1"), quantitativeResult5[0], this->precision());
        EXPECT_NEAR(this->parseNumber("1"), quantitativeResult5[1], this->precision());
        EXPECT_NEAR(this->parseNumber("1"), quantitativeResult5[2], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"c\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult6 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult6[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult6[1], this->precision());
        EXPECT_NEAR(this->parseNumber("1"), quantitativeResult6[2], this->precision());
    }
    {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(22, 15, 28, true, true, 15);
        matrixBuilder.newRowGroup(0);
        matrixBuilder.addNextValue(0, 1, this->parseNumber("1"));
        matrixBuilder.newRowGroup(1);
        matrixBuilder.addNextValue(1, 0, this->parseNumber("1"));
        matrixBuilder.addNextValue(2, 2, this->parseNumber("1"));
        matrixBuilder.addNextValue(3, 4, this->parseNumber("0.7"));
        matrixBuilder.addNextValue(3, 6, this->parseNumber("0.3"));
        matrixBuilder.newRowGroup(4);
        matrixBuilder.addNextValue(4, 0, this->parseNumber("1"));

        matrixBuilder.newRowGroup(5);
        matrixBuilder.addNextValue(5, 4, this->parseNumber("1"));
        matrixBuilder.addNextValue(6, 5, this->parseNumber("0.8"));
        matrixBuilder.addNextValue(6, 9, this->parseNumber("0.2"));
        matrixBuilder.newRowGroup(7);
        matrixBuilder.addNextValue(7, 3, this->parseNumber("1"));
        matrixBuilder.addNextValue(8, 5, this->parseNumber("1"));
        matrixBuilder.newRowGroup(9);
        matrixBuilder.addNextValue(9, 3, this->parseNumber("1"));

        matrixBuilder.newRowGroup(10);
        matrixBuilder.addNextValue(10, 7, this->parseNumber("1"));
        matrixBuilder.newRowGroup(11);
        matrixBuilder.addNextValue(11, 6, this->parseNumber("1"));
        matrixBuilder.addNextValue(12, 8, this->parseNumber("1"));
        matrixBuilder.newRowGroup(13);
        matrixBuilder.addNextValue(13, 6, this->parseNumber("1"));

        matrixBuilder.newRowGroup(14);
        matrixBuilder.addNextValue(14, 10, this->parseNumber("1"));
        matrixBuilder.newRowGroup(15);
        matrixBuilder.addNextValue(15, 9, this->parseNumber("1"));
        matrixBuilder.addNextValue(16, 11, this->parseNumber("1"));
        matrixBuilder.newRowGroup(17);
        matrixBuilder.addNextValue(17, 9, this->parseNumber("1"));

        matrixBuilder.newRowGroup(18);
        matrixBuilder.addNextValue(18, 5, this->parseNumber("0.4"));
        matrixBuilder.addNextValue(18, 8, this->parseNumber("0.3"));
        matrixBuilder.addNextValue(18, 11, this->parseNumber("0.3"));

        matrixBuilder.newRowGroup(19);
        matrixBuilder.addNextValue(19, 7, this->parseNumber("0.7"));
        matrixBuilder.addNextValue(19, 12, this->parseNumber("0.3"));

        matrixBuilder.newRowGroup(20);
        matrixBuilder.addNextValue(20, 12, this->parseNumber("0.1"));
        matrixBuilder.addNextValue(20, 13, this->parseNumber("0.9"));
        matrixBuilder.addNextValue(21, 12, this->parseNumber("1"));

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

        mdp.reset(new storm::models::sparse::Mdp<ValueType>(transitionMatrix, ap));

        storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("LRAmax=? [\"a\"]");

        std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("37 / 60"), quantitativeResult1[0], this->precision());
        EXPECT_NEAR(this->parseNumber("2/3"), quantitativeResult1[3], this->precision());
        EXPECT_NEAR(this->parseNumber("0.5"), quantitativeResult1[6], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult1[9], this->precision());
        EXPECT_NEAR(this->parseNumber("31 / 60"), quantitativeResult1[12], this->precision());
        EXPECT_NEAR(this->parseNumber("101 / 200"), quantitativeResult1[13], this->precision());
        EXPECT_NEAR(this->parseNumber("31 / 60"), quantitativeResult1[14], this->precision());

        formula = formulaParser.parseSingleFormulaFromString("LRAmin=? [\"a\"]");

        result = checker.check(this->env(), *formula);
        storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();

        EXPECT_NEAR(this->parseNumber("0.1"), quantitativeResult2[0], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[3], this->precision());
        EXPECT_NEAR(this->parseNumber("1/3"), quantitativeResult2[6], this->precision());
        EXPECT_NEAR(this->parseNumber("0"), quantitativeResult2[9], this->precision());
        EXPECT_NEAR(this->parseNumber("0.1"), quantitativeResult2[12], this->precision());
        EXPECT_NEAR(this->parseNumber("79 / 300"), quantitativeResult2[13], this->precision());
        EXPECT_NEAR(this->parseNumber("0.1"), quantitativeResult2[14], this->precision());
    }
}

TYPED_TEST(LraMdpPrctlModelCheckerTest, cs_nfail) {
    typedef typename TestFixture::ValueType ValueType;

    std::string formulasString = "R{\"grants\"}max=? [ MP ]; R{\"grants\"}min=? [ MP ];";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/cs_nfail3.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(184ul, model->getNumberOfStates());
    EXPECT_EQ(541ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    auto mdp = model->template as<storm::models::sparse::Mdp<ValueType>>();
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    auto result = checker.check(this->env(), tasks[0])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("333/1000"), result[*mdp->getInitialStates().begin()], this->precision());

    result = checker.check(this->env(), tasks[1])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("0"), result[*mdp->getInitialStates().begin()], this->precision());
}

}  // namespace
