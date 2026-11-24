#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-gamebased-ar/modelchecker/abstraction/GameBasedMdpModelChecker.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/builder/DdPrismModelBuilder.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/solver.h"

class Cudd {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::CUDD;
    typedef double ValueType;
};

class Sylvan {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::Sylvan;
    typedef double ValueType;
};

class SylvanExact {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::Sylvan;
    typedef storm::RationalNumber ValueType;
};

template<typename TestType>
class GameBasedDtmcModelCheckerTest : public ::testing::Test {
   public:
    static const storm::dd::DdType DdType = TestType::DdType;
    typedef typename TestType::ValueType ValueType;

   protected:
    void SetUp() override {
#ifndef STORM_HAVE_MATHSAT
        GTEST_SKIP() << "MathSAT not available.";
#endif
        TestType::checkLibraryAvailable();
    }
};

typedef ::testing::Types<Cudd, Sylvan, SylvanExact> TestingTypes;
TYPED_TEST_SUITE(GameBasedDtmcModelCheckerTest, TestingTypes, );

TYPED_TEST(GameBasedDtmcModelCheckerTest, Die) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    typedef typename TestFixture::ValueType ValueType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");
    auto checker = std::make_shared<storm::gbar::modelchecker::GameBasedMdpModelChecker<DdType, storm::models::symbolic::Dtmc<DdType, ValueType>>>(program);

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"one\"]");
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(1.0 / 6.0, storm::utility::convertNumber<double>(quantitativeResult1[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"two\"]");
    task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formula, true);

    result = checker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(1.0 / 6.0, storm::utility::convertNumber<double>(quantitativeResult2[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("P=? [F \"three\"]");
    task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formula, true);

    result = checker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(1.0 / 6.0, storm::utility::convertNumber<double>(quantitativeResult3[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());
}

TYPED_TEST(GameBasedDtmcModelCheckerTest, SynchronousLeader) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    typedef typename TestFixture::ValueType ValueType;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm");
    program = program.substituteConstantsFormulas();

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    auto checker = std::make_shared<storm::gbar::modelchecker::GameBasedMdpModelChecker<DdType, storm::models::symbolic::Dtmc<DdType, ValueType>>>(program);

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F \"elected\"]");
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);

    std::unique_ptr<storm::modelchecker::CheckResult> result = checker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(1.0, storm::utility::convertNumber<double>(quantitativeResult1[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());
}
