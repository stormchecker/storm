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
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
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
        program = program.preprocess(constantDefinitionString);
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
        program = program.preprocess(constantDefinitionString);
        auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
        result.second = storm::api::extractFormulasFromProperties(janiData.second);
        result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(this->env(), janiData.first, result.second)->template as<MT>();
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

    storm::utility::ExtendedValueType<ValueType> getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                                                                     std::unique_ptr<storm::modelchecker::CheckResult>& result) {
        auto filter = getInitialStateFilter(model);
        result->filter(*filter);
        return result->asQuantitativeCheckResult<ValueType>().getMin();
    }

    /*!
     * Expects the value at the initial states to be the given one. Where the configuration reports sound bounds on
     * the solution, they must enclose that value as well.
     */
    void expectQuantitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                                std::unique_ptr<storm::modelchecker::CheckResult>& result, ValueType const& expected) {
        EXPECT_NEAR(expected, this->getQuantitativeResultAtInitialState(model, result), this->precision());
        if (!result->isExplicitQuantitativeCheckResult()) {
            return;  // Only explicit results carry bounds.
        }
        // The result is filtered to the initial states at this point, so the first entry belongs to the first initial state.
        auto const& explicitResult = result->template asExplicitQuantitativeCheckResult<ValueType>();
        ValueType const tolerance = TestType::isExact ? this->parseNumber("0") : this->parseNumber("1e-12");
        if (explicitResult.hasLowerBounds()) {
            EXPECT_LE(explicitResult.getLowerBoundVector().front(), expected + tolerance) << "The lower bound exceeds the expected value.";
        }
        if (explicitResult.hasUpperBounds()) {
            EXPECT_LE(expected, explicitResult.getUpperBoundVector().front() + tolerance) << "The upper bound falls below the expected value.";
        }
    }

    /*!
     * Expects the value at the initial states to be the given one and the result to report it as exact.
     */
    void expectExactResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                         std::unique_ptr<storm::modelchecker::CheckResult>& result, ValueType const& expected) {
        expectQuantitativeResultAtInitialState(model, result, expected);
        if (!result->isExplicitQuantitativeCheckResult()) {
            return;
        }
        auto const& explicitResult = result->template asExplicitQuantitativeCheckResult<ValueType>();
        ASSERT_TRUE(explicitResult.hasLowerBounds()) << "No lower bound was reported although the values are exact.";
        ASSERT_TRUE(explicitResult.hasUpperBounds()) << "No upper bound was reported although the values are exact.";
        EXPECT_EQ(explicitResult.getLowerBoundVector(), explicitResult.getUpperBoundVector()) << "The bounds do not pin the values down.";
    }

    /*!
     * Expects the value at the initial states to be the given one, enclosed by bounds the result must carry.
     */
    void expectBoundedResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                           std::unique_ptr<storm::modelchecker::CheckResult>& result, ValueType const& expected) {
        expectQuantitativeResultAtInitialState(model, result, expected);
        if (!result->isExplicitQuantitativeCheckResult()) {
            return;
        }
        auto const& explicitResult = result->template asExplicitQuantitativeCheckResult<ValueType>();
        EXPECT_TRUE(explicitResult.hasLowerBounds()) << "No lower bound on the solution was reported.";
        EXPECT_TRUE(explicitResult.hasUpperBounds()) << "No upper bound on the solution was reported.";
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
        // Plain value iteration bounds an expected time from below only, so do not insist on an enclosure here.
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("11/6"));

        result = checker->check(this->env(), tasks[1]);
        this->expectBoundedResultAtInitialState(model, result, this->parseNumber("2/3"));

        if (!storm::utility::isZero(this->precision())) {
            result = checker->check(this->env(), tasks[2]);
            this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0.455504"));
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
            this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0.6321205588"));

            result = checker->check(this->env(), tasks[1]);
            this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0.727468207"));
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
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("2"));

        result = checker->check(this->env(), tasks[1]);
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0"));

        result = checker->check(this->env(), tasks[2]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));

        result = checker->check(this->env(), tasks[3]);
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("7/8"));

        result = checker->check(this->env(), tasks[4]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));

        result = checker->check(this->env(), tasks[5]);
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("7/8"));

        result = checker->check(this->env(), tasks[6]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));
    }

    // Checking LRA properties exactly requires an exact LP solver.
    result = checker->check(this->env(), tasks[7]);
    this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0"));

    result = checker->check(this->env(), tasks[8]);
    EXPECT_NEAR(this->parseNumber("407"), this->getQuantitativeResultAtInitialState(model, result),
                this->precision() * this->parseNumber("407"));  // use relative precision!

    result = checker->check(this->env(), tasks[9]);
    EXPECT_NEAR(this->parseNumber("27"), this->getQuantitativeResultAtInitialState(model, result),
                this->precision() * this->parseNumber("27"));  // use relative precision!
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
            this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0.13212055882856"));

            result = checker->check(this->env(), tasks[1]);
            this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0.50066835807513"));
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
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("1/10"));

        result = checker->check(this->env(), tasks[1]);
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("1/5"));

        result = checker->check(this->env(), tasks[2]);
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("1/10"));

        result = checker->check(this->env(), tasks[3]);
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("9/10"));

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
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("1"));

        result = checker->check(tasks[1]);
        this->expectQuantitativeResultAtInitialState(model, result, this->parseNumber("0"));
    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
    }
}
}  // namespace
