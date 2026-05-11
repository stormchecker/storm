#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/modelchecker/ConditionalModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"

namespace {

class SparseDoubleRestartEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::Restart);
        env.modelchecker().conditional().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10),
                                                      false);  // restart algorithm requires a higher precision
        return env;
    }
};

class SparseDoubleBisectionEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::Bisection);
        return env;
    }
};

class SparseDoubleBisectionAdvancedEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::BisectionAdvanced);
        return env;
    }
};

class SparseDoubleBisectionPtEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::BisectionPolicyTracking);
        return env;
    }
};

class SparseDoubleBisectionAdvancedPtEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::BisectionAdvancedPolicyTracking);
        return env;
    }
};

class SparseDoublePiEnvironment {
   public:
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::PolicyIteration);
        return env;
    }
};

class SparseRationalNumberRestartEnvironment {
   public:
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::Restart);
        env.modelchecker().conditional().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10),
                                                      false);  // restart algorithm requires a higher precision
        return env;
    }
};

class SparseRationalNumberBisectionEnvironment {
   public:
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::Bisection);
        return env;
    }
};

class SparseRationalNumberBisectionAdvancedEnvironment {
   public:
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::BisectionAdvanced);
        return env;
    }
};

class SparseRationalNumberBisectionPtEnvironment {
   public:
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::BisectionPolicyTracking);
        return env;
    }
};

class SparseRationalNumberBisectionAdvancedPtEnvironment {
   public:
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::BisectionAdvancedPolicyTracking);
        return env;
    }
};

class SparseRationalNumberPiEnvironment {
   public:
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().conditional().setAlgorithm(storm::ConditionalAlgorithmSetting::PolicyIteration);
        return env;
    }
};

template<typename TestType>
class ConditionalMdpPrctlModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::Mdp<ValueType> SparseModelType;

    ConditionalMdpPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}

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
        storm::prism::Program const& inputProgram, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<SparseModelType>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        auto program = storm::utility::prism::preprocess(inputProgram, constantDefinitionString);
        result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
        result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<SparseModelType>();
        return result;
    }

    std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> getTasks(
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) const {
        std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> result;
        for (auto const& f : formulas) {
            result.emplace_back(*f, true);  // Set onlyInitialStatesRelevant to true for conditional tasks
        }
        return result;
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<SparseDoubleRestartEnvironment, SparseDoubleBisectionEnvironment, SparseDoubleBisectionAdvancedEnvironment,
                         SparseDoubleBisectionPtEnvironment, SparseDoubleBisectionAdvancedPtEnvironment, SparseDoublePiEnvironment,
                         SparseRationalNumberRestartEnvironment, SparseRationalNumberBisectionEnvironment, SparseRationalNumberBisectionAdvancedEnvironment,
                         SparseRationalNumberBisectionPtEnvironment, SparseRationalNumberBisectionAdvancedPtEnvironment, SparseRationalNumberPiEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(ConditionalMdpPrctlModelCheckerTest, TestingTypes, );

TYPED_TEST(ConditionalMdpPrctlModelCheckerTest, two_dice) {
    typedef typename TestFixture::ValueType ValueType;

    std::string const formulasString =
        "Pmax=? [F \"twelve\" || F d1=6];"
        "Pmin=? [F \"twelve\" || F d1=6];"
        "Pmax=? [F \"seven\" || F d2=4];"
        "Pmin=? [F \"seven\" || F d2=4];"
        "Pmax=? [F \"two\" || F d2=2];"
        "Pmin=? [F \"two\" || F d2=2];"
        "Pmax=? [F d1=6 || F \"twelve\"];"
        "Pmin=? [F d1=6 || F \"twelve\"];";

    auto program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");
    auto modelFormulas = this->buildModelFormulas(program, formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(169ul, model->getNumberOfStates());
    EXPECT_EQ(436ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    auto mdp = model->template as<storm::models::sparse::Mdp<ValueType>>();
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    auto result = checker.check(this->env(), tasks[0])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("1/6"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[1])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("1/6"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[2])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("1/6"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[3])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("1/6"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[4])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("0"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[5])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("0"), result[*mdp->getInitialStates().begin()], this->precision());

    // This Environment depending on the platform fails or does not fail an assertion. Thus this env is skipped.
    if constexpr (!std::is_same_v<TypeParam, SparseDoubleBisectionAdvancedPtEnvironment>) {
        result = checker.check(this->env(), tasks[6])->template asExplicitQuantitativeCheckResult<ValueType>();
        EXPECT_NEAR(this->parseNumber("1"), result[*mdp->getInitialStates().begin()], this->precision());
    }

    result = checker.check(this->env(), tasks[7])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("1"), result[*mdp->getInitialStates().begin()], this->precision());
}

TYPED_TEST(ConditionalMdpPrctlModelCheckerTest, consensus) {
    typedef typename TestFixture::ValueType ValueType;

    std::string const formulasString =
        "Pmax=? [F \"all_coins_equal_0\" & \"finished\" || F \"agree\" & \"finished\"];"
        "Pmin=? [F \"all_coins_equal_0\" & \"finished\" || F \"agree\" & \"finished\"];"
        "Pmax=? [F \"all_coins_equal_1\" & \"finished\" || F \"agree\" & \"finished\"];"
        "Pmin=? [F \"all_coins_equal_1\" & \"finished\" || F \"agree\" & \"finished\"];"
        "P<=560/953 [F \"all_coins_equal_1\" & \"finished\" || F \"agree\" & \"finished\"];"
        "P<562/953 [F \"all_coins_equal_1\" & \"finished\" || F \"agree\" & \"finished\"];"
        "P>393/953 [F \"all_coins_equal_1\" & \"finished\" || F \"agree\" & \"finished\"];"
        "P>=391/953 [F \"all_coins_equal_1\" & \"finished\" || F \"agree\" & \"finished\"];";

    auto program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm");
    auto modelFormulas = this->buildModelFormulas(program, formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(272ul, model->getNumberOfStates());
    EXPECT_EQ(492ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    auto mdp = model->template as<storm::models::sparse::Mdp<ValueType>>();
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    auto result = checker.check(this->env(), tasks[0])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("561/953"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[1])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("392/953"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[2])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("561/953"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[3])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("392/953"), result[*mdp->getInitialStates().begin()], this->precision());
    auto qualResult = checker.check(this->env(), tasks[4])->template asExplicitQualitativeCheckResult<ValueType>();
    EXPECT_FALSE(qualResult[*mdp->getInitialStates().begin()]);
    qualResult = checker.check(this->env(), tasks[5])->template asExplicitQualitativeCheckResult<ValueType>();
    EXPECT_TRUE(qualResult[*mdp->getInitialStates().begin()]);
    qualResult = checker.check(this->env(), tasks[6])->template asExplicitQualitativeCheckResult<ValueType>();
    EXPECT_FALSE(qualResult[*mdp->getInitialStates().begin()]);
    qualResult = checker.check(this->env(), tasks[7])->template asExplicitQualitativeCheckResult<ValueType>();
    EXPECT_TRUE(qualResult[*mdp->getInitialStates().begin()]);
}

TYPED_TEST(ConditionalMdpPrctlModelCheckerTest, simple) {
    typedef typename TestFixture::ValueType ValueType;

    std::string const programAsString = R"(mdp
module main
	x : [0..2];
	[] x=0 -> 0.7 : (x'=1) + 0.3 : (x'=2);
	[] x=1 -> 1 : (x'=1);
	[] x=2 -> 1 : (x'=1);
endmodule
)";

    std::string const formulasString =
        "Pmin=? [F x=2 || F x=1];"
        "Pmax=? [F x=2 || F x=1];";

    auto program = storm::parser::PrismParser::parseFromString(programAsString, "<no filename>");
    auto modelFormulas = this->buildModelFormulas(program, formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(3ul, model->getNumberOfStates());
    EXPECT_EQ(4ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    auto mdp = model->template as<storm::models::sparse::Mdp<ValueType>>();
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    auto result = checker.check(this->env(), tasks[0])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("3/10"), result[*mdp->getInitialStates().begin()], this->precision());
    result = checker.check(this->env(), tasks[1])->template asExplicitQuantitativeCheckResult<ValueType>();
    EXPECT_NEAR(this->parseNumber("3/10"), result[*mdp->getInitialStates().begin()], this->precision());
}

}  // namespace
