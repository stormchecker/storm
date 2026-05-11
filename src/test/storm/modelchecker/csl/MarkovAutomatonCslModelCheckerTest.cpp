#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/csl/HybridMarkovAutomatonCslModelChecker.h"
#include "storm/modelchecker/csl/SparseMarkovAutomatonCslModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/jani/Property.h"

namespace {

enum class MaEngine { PrismSparse, JaniSparse, JaniHybrid };

class SparseDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MaEngine engine = MaEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::MarkovAutomaton<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration, true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class JaniSparseDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MaEngine engine = MaEngine::JaniSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::MarkovAutomaton<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration, true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class JaniHybridDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const MaEngine engine = MaEngine::JaniHybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::MarkovAutomaton<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration, true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class SparseDoubleIntervalIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MaEngine engine = MaEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::MarkovAutomaton<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::IntervalIteration, true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};
class SparseRationalPolicyIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MaEngine engine = MaEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::MarkovAutomaton<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration, true);
        return env;
    }
};
class SparseRationalRationalSearchEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MaEngine engine = MaEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::MarkovAutomaton<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::RationalSearch, true);
        return env;
    }
};

template<typename TestType>
class MarkovAutomatonCslModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::MarkovAutomaton<ValueType> SparseModelType;
    typedef typename storm::models::symbolic::MarkovAutomaton<TestType::ddType, ValueType> SymbolicModelType;

    MarkovAutomatonCslModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        if constexpr (TestType::engine == MaEngine::JaniHybrid) {
            TestType::checkLibraryAvailable();
        }
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
    bool isSymbolicModel() const {
        return std::is_same<typename TestType::ModelType, SymbolicModelType>::value;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value,
                            std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>>>::type
    buildModelFormulas(std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
        program = storm::utility::prism::preprocess(program, constantDefinitionString);
        if (TestType::engine == MaEngine::PrismSparse) {
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<MT>();
        } else if (TestType::engine == MaEngine::JaniSparse) {
            auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.second = storm::api::extractFormulasFromProperties(janiData.second);
            result.first = storm::api::buildSparseModel<ValueType>(janiData.first, result.second)->template as<MT>();
        }
        return result;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value,
                            std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>>>::type
    buildModelFormulas(std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
        program = storm::utility::prism::preprocess(program, constantDefinitionString);
        auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
        result.second = storm::api::extractFormulasFromProperties(janiData.second);
        result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(janiData.first, result.second)->template as<MT>();
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

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type createModelChecker(
        std::shared_ptr<MT> const& model) const {
        if (TestType::engine == MaEngine::PrismSparse || TestType::engine == MaEngine::JaniSparse) {
            return std::make_shared<storm::modelchecker::SparseMarkovAutomatonCslModelChecker<SparseModelType>>(*model);
        }
        return nullptr;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type
    createModelChecker(std::shared_ptr<MT> const& model) const {
        if (TestType::engine == MaEngine::JaniHybrid) {
            return std::make_shared<storm::modelchecker::HybridMarkovAutomatonCslModelChecker<SymbolicModelType>>(*model);
            //            } else if (TestType::engine == MaEngine::Dd) {
            //                return std::make_shared<storm::modelchecker::SymbolicMarkovAutomatonCslModelChecker<SymbolicModelType>>(*model);
        }
        return nullptr;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value, void>::type execute(std::shared_ptr<MT> const& model,
                                                                                          std::function<void()> const& f) const {
        f();
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, void>::type execute(std::shared_ptr<MT> const& model,
                                                                                            std::function<void()> const& f) const {
        model->getManager().execute(f);
    }

    bool getQualitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                            std::unique_ptr<storm::modelchecker::CheckResult>& result) {
        auto filter = getInitialStateFilter(model);
        result->filter(*filter);
        return result->asQualitativeCheckResult().forallTrue();
    }

    ValueType getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                                  std::unique_ptr<storm::modelchecker::CheckResult>& result) {
        auto filter = getInitialStateFilter(model);
        result->filter(*filter);
        return result->asQuantitativeCheckResult<ValueType>().getMin();
    }

   private:
    storm::Environment _environment;

    std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(std::shared_ptr<storm::models::Model<ValueType>> const& model) const {
        if (isSparseModel()) {
            return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<ValueType>>(model->template as<SparseModelType>()->getInitialStates());
        } else {
            return std::make_unique<storm::modelchecker::SymbolicQualitativeCheckResult<TestType::ddType>>(
                model->template as<SymbolicModelType>()->getReachableStates(), model->template as<SymbolicModelType>()->getInitialStates());
        }
    }
};

typedef ::testing::Types<SparseDoubleValueIterationEnvironment, JaniSparseDoubleValueIterationEnvironment, JaniHybridDoubleValueIterationEnvironment,
                         SparseDoubleIntervalIterationEnvironment, SparseRationalPolicyIterationEnvironment, SparseRationalRationalSearchEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(MarkovAutomatonCslModelCheckerTest, TestingTypes, );

TYPED_TEST(MarkovAutomatonCslModelCheckerTest, server) {
    std::string formulasString = "Tmax=? [F \"error\"]";
    formulasString += "; Pmax=? [F \"processB\"]";
    formulasString += "; Pmax=? [F<1 \"error\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ma/server.ma", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(6ul, model->getNumberOfStates());
        EXPECT_EQ(10ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::MarkovAutomaton);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("11/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("2/3"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        if (!storm::utility::isZero(this->precision())) {
            result = checker->check(this->env(), tasks[2]);
            EXPECT_NEAR(this->parseNumber("0.455504"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        }
    });
}

TYPED_TEST(MarkovAutomatonCslModelCheckerTest, simple) {
    std::string formulasString = "Pmin=? [F<1 s>2]";
    formulasString += "; Pmax=? [F<1.3 s=3]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ma/simple.ma", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(5ul, model->getNumberOfStates());
        EXPECT_EQ(8ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::MarkovAutomaton);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        if (!storm::utility::isZero(this->precision())) {
            result = checker->check(this->env(), tasks[0]);
            EXPECT_NEAR(this->parseNumber("0.6321205588"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[1]);
            EXPECT_NEAR(this->parseNumber("0.727468207"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        }
    });
}

TYPED_TEST(MarkovAutomatonCslModelCheckerTest, simple2) {
    std::string formulasString = "R{\"rew0\"}max=? [C]";
    formulasString += "; R{\"rew0\"}min=? [C]";
    formulasString += "; R{\"rew1\"}max=? [C]";
    formulasString += "; R{\"rew1\"}min=? [C]";
    formulasString += "; R{\"rew2\"}max=? [C]";
    formulasString += "; R{\"rew2\"}min=? [C]";
    formulasString += "; R{\"rew3\"}min=? [C]";
    formulasString += "; LRAmin=? [s=0 | s=3]";     // 0
    formulasString += "; R{\"rew3\"}max=?[ LRA ]";  // 407
    formulasString += "; R{\"rew3\"}min=?[ LRA ]";  // 27

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ma/simple2.ma", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(6ul, model->getNumberOfStates());
    EXPECT_EQ(11ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::MarkovAutomaton);
    auto checker = this->createModelChecker(model);
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    if (TypeParam::engine != MaEngine::JaniHybrid) {
        // Total Reward Formulas are not supported in the hybrid engine (for now).

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("2"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("7/8"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[4]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));

        result = checker->check(this->env(), tasks[5]);
        EXPECT_NEAR(this->parseNumber("7/8"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[6]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));
    }

    // Checking LRA properties exactly requires an exact LP solver.
    result = checker->check(this->env(), tasks[7]);
    EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

    result = checker->check(this->env(), tasks[8]);
    EXPECT_NEAR(this->parseNumber("407"), this->getQuantitativeResultAtInitialState(model, result),
                this->precision() * this->parseNumber("407"));  // use relative precision!

    result = checker->check(this->env(), tasks[9]);
    EXPECT_NEAR(this->parseNumber("27"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
}

TYPED_TEST(MarkovAutomatonCslModelCheckerTest, erlang) {
    std::string formulasString = "Pmin=?[F<=1 \"done\"]";
    formulasString += "; Pmax=?[F<=1 \"done\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ma/erlang.ma", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(9ul, model->getNumberOfStates());
        EXPECT_EQ(11ul, model->getNumberOfTransitions());
        EXPECT_EQ(10ul, model->getNumberOfChoices());
        ASSERT_EQ(model->getType(), storm::models::ModelType::MarkovAutomaton);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        if (!storm::utility::isZero(this->precision())) {
            result = checker->check(this->env(), tasks[0]);
            EXPECT_NEAR(this->parseNumber("0.13212055882856"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[1]);
            EXPECT_NEAR(this->parseNumber("0.50066835807513"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        }
    });
}

TYPED_TEST(MarkovAutomatonCslModelCheckerTest, LtlSimple) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "Pmax=? [X X s=3]";
    formulasString += "; Pmax=? [X X G s>2]";
    formulasString += "; Pmin=? [X X G s>2]";
    formulasString += "; Pmax=? [F ((s=0) U X(s=0) & X(s=2))]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ma/simple.ma", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::MarkovAutomaton);
    auto checker = this->createModelChecker(model);
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    // LTL not supported in all engines (Hybrid,  PrismDd, JaniDd)
    if (TypeParam::engine == MaEngine::PrismSparse || TypeParam::engine == MaEngine::JaniSparse) {
        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1/10"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("1/5"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("1/10"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("9/10"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
    }
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(MarkovAutomatonCslModelCheckerTest, HOASimple) {
    // "P=? [ F (s=3) & (X s=1)]"
    std::string formulasString = "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> (s>1), \"p1\" -> !(s=1) }]";
    // "P=? [ (s=2) U (s=1)]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> (s=2), \"p1\" -> (s=1) }]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ma/simple.ma", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::MarkovAutomaton);
    auto checker = this->createModelChecker(model);
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    // Not supported in all engines (Hybrid,  PrismDd, JaniDd)
    if (TypeParam::engine == MaEngine::PrismSparse || TypeParam::engine == MaEngine::JaniSparse) {
        result = checker->check(tasks[0]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(tasks[1]);
        EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
    }
}
}  // namespace
