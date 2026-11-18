#include "storm-config.h"
#include "storm/environment/Environment.h"
#include "test/storm_gtest.h"

#include "storm-gamebased-ar/modelchecker/abstraction/GameBasedMdpModelChecker.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/api/storm.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/utility/constants.h"

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
class GameBasedMdpModelCheckerTest : public ::testing::Test {
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
TYPED_TEST_SUITE(GameBasedMdpModelCheckerTest, TestingTypes, );

TYPED_TEST(GameBasedMdpModelCheckerTest, Dice) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    typedef typename TestFixture::ValueType ValueType;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm";

    storm::prism::Program program = storm::api::parseProgram(programFile);

    // Build the die model
    typename storm::builder::DdPrismModelBuilder<DdType>::Options options;
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = storm::builder::DdPrismModelBuilder<DdType>().build(storm::Environment(), program, options);

    ASSERT_EQ(model->getNumberOfStates(), 169ull);
    ASSERT_EQ(model->getNumberOfTransitions(), 436ull);

    std::shared_ptr<storm::models::symbolic::Mdp<DdType>> mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();
    auto mdpModelchecker =
        std::make_shared<storm::gbar::modelchecker::GameBasedMdpModelChecker<DdType, storm::models::symbolic::Mdp<DdType, ValueType>>>(program);

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;

    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"two\"]");
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);

    std::unique_ptr<storm::modelchecker::CheckResult> result = mdpModelchecker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult1 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(0.0277777612209320068, storm::utility::convertNumber<double>(quantitativeResult1[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"two\"]");
    task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formula, true);

    result = mdpModelchecker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult2 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(0.0277777612209320068, storm::utility::convertNumber<double>(quantitativeResult2[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"three\"]");
    task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formula, true);

    result = mdpModelchecker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult3 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(0.0555555224418640136, storm::utility::convertNumber<double>(quantitativeResult3[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"three\"]");
    task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formula, true);

    result = mdpModelchecker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult4 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(0.0555555224418640136, storm::utility::convertNumber<double>(quantitativeResult4[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"four\"]");
    task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formula, true);

    result = mdpModelchecker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult5 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(0.083333283662796020508, storm::utility::convertNumber<double>(quantitativeResult5[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());

    formula = formulaParser.parseSingleFormulaFromString("Pmax=? [F \"four\"]");
    task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formula, true);

    result = mdpModelchecker->check(task);
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>& quantitativeResult6 = result->asExplicitQuantitativeCheckResult<ValueType>();

    EXPECT_NEAR(0.083333283662796020508, storm::utility::convertNumber<double>(quantitativeResult6[0]),
                storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>().getPrecision());
}
