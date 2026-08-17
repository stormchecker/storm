#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/csl/HybridCtmcCslModelChecker.h"
#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"
#include "storm/modelchecker/csl/helper/SparseCtmcCslHelper.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/EigenLinearEquationSolver.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace {

enum class CtmcEngine { PrismSparse, JaniSparse, PrismHybrid, JaniHybrid };

#ifdef STORM_HAVE_GMM
class SparseGmmxxGmresIluEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const CtmcEngine engine = CtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class JaniSparseGmmxxGmresIluEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const CtmcEngine engine = CtmcEngine::JaniSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};
#endif

class SparseEigenDGmresEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const CtmcEngine engine = CtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::DGmres);
        env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Ilu);
        env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class SparseEigenDoubleLUEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const CtmcEngine engine = CtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class SparseNativeSorEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const CtmcEngine engine = CtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SOR);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-9));
        env.solver().native().setRelativeTerminationCriterion(false);
        env.solver().native().setMaximalNumberOfIterations(5000000);
        return env;
    }
};

#ifdef STORM_HAVE_GMM
class HybridCuddGmmxxGmresEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const CtmcEngine engine = CtmcEngine::PrismHybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Ctmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class JaniHybridCuddGmmxxGmresEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const CtmcEngine engine = CtmcEngine::JaniHybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Ctmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class HybridSylvanGmmxxGmresEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const CtmcEngine engine = CtmcEngine::PrismHybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Ctmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};
#endif

template<typename TestType>
class CtmcCslModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::Ctmc<ValueType> SparseModelType;
    typedef typename storm::models::symbolic::Ctmc<TestType::ddType, ValueType> SymbolicModelType;

    CtmcCslModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        if constexpr (TestType::engine == CtmcEngine::PrismHybrid || TestType::engine == CtmcEngine::JaniHybrid) {
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
    CtmcEngine getEngine() const {
        return TestType::engine;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value,
                            std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>>>::type
    buildModelFormulas(std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile, true);
        program = program.preprocess(constantDefinitionString);
        if (TestType::engine == CtmcEngine::PrismSparse) {
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<MT>();
        } else if (TestType::engine == CtmcEngine::JaniSparse) {
            auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            janiData.first.substituteFunctions();
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
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile, true);
        program = program.preprocess(constantDefinitionString);
        if (TestType::engine == CtmcEngine::PrismHybrid) {
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(this->env(), program, result.second)->template as<MT>();
        } else if (TestType::engine == CtmcEngine::JaniHybrid) {
            auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            janiData.first.substituteFunctions();
            result.second = storm::api::extractFormulasFromProperties(janiData.second);
            result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(this->env(), janiData.first, result.second)->template as<MT>();
        }
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
        if (TestType::engine == CtmcEngine::PrismSparse || TestType::engine == CtmcEngine::JaniSparse) {
            return std::make_shared<storm::modelchecker::SparseCtmcCslModelChecker<SparseModelType>>(*model);
        }
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type
    createModelChecker(std::shared_ptr<MT> const& model) const {
        if (TestType::engine == CtmcEngine::PrismHybrid || TestType::engine == CtmcEngine::JaniHybrid) {
            return std::make_shared<storm::modelchecker::HybridCtmcCslModelChecker<SymbolicModelType>>(*model);
        }
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

typedef ::testing::Types<
#ifdef STORM_HAVE_GMM
    SparseGmmxxGmresIluEnvironment, JaniSparseGmmxxGmresIluEnvironment, HybridCuddGmmxxGmresEnvironment, JaniHybridCuddGmmxxGmresEnvironment,
    HybridSylvanGmmxxGmresEnvironment,
#endif
    SparseNativeSorEnvironment, SparseEigenDGmresEnvironment, SparseEigenDoubleLUEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(CtmcCslModelCheckerTest, TestingTypes, );

TYPED_TEST(CtmcCslModelCheckerTest, Cluster) {
    std::string formulasString = "P=? [ F<=100 !\"minimum\"]";
    formulasString += "; P=? [ F[100,100] !\"minimum\"]";
    formulasString += "; P=? [ F[100,2000] !\"minimum\"]";
    formulasString += "; P=? [ \"minimum\" U<=10 \"premium\"]";
    formulasString += "; P=? [ !\"minimum\" U>=1 \"minimum\"]";
    formulasString += "; P=? [ \"minimum\" U>=1 !\"minimum\"]";
    formulasString += "; R=? [C<=100]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(276ul, model->getNumberOfStates());
        EXPECT_EQ(1120ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("5.5461254704419085E-5"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("2.3397873548343415E-6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("0.001105335651670241"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[4]);
        EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[5]);
        EXPECT_NEAR(this->parseNumber("0.9999999033633374"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[6]);
        EXPECT_NEAR(this->parseNumber("0.8602815057967503"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(CtmcCslModelCheckerTest, Embedded) {
    std::string formulasString = "P=? [ F<=10000 \"down\"]";
    formulasString += "; P=? [ !\"down\" U<=10000 \"fail_actuators\"]";
    formulasString += "; P=? [ !\"down\" U<=10000 \"fail_io\"]";
    formulasString += "; P=? [ !\"down\" U<=10000 \"fail_sensors\"]";
    formulasString += "; R{\"up\"}=? [C<=10000]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(3478ul, model->getNumberOfStates());
        EXPECT_EQ(14639ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("0.0019216435246119591"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("3.7079151806696567E-6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("0.001556839327673734"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("4.429620626755424E-5"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[4]);
        EXPECT_NEAR(this->parseNumber("2.7745274082080154"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(CtmcCslModelCheckerTest, Tandem) {
    std::string formulasString = "P=? [ F<=10 \"network_full\" ]";
    formulasString += "; P=? [ F<=10 \"first_queue_full\" ]";
    formulasString += "; P=? [\"second_queue_full\" U<=1 !\"second_queue_full\"]";
    formulasString += "; R=? [I=10]";
    formulasString += "; R=? [C<=10]";
    formulasString += "; R=? [F \"first_queue_full\"&\"second_queue_full\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/tandem5.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(66ul, model->getNumberOfStates());
        EXPECT_EQ(189ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("0.015446370562428037"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("0.999999837225515"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("5.679243850315877"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[4]);
        EXPECT_NEAR(this->parseNumber("55.44792186036232"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[5]);
        EXPECT_NEAR(this->parseNumber("262.85103824276308"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(CtmcCslModelCheckerTest, simple1) {
    std::string formulasString = "P=? [ F<=0.5 x=1 ]";
    formulasString += "; R=? [ C<=0.1 ]";
    formulasString += "; R=? [ C<=1 ]";
    formulasString += "; R=? [ C<=10 ]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/simple1.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(2ul, model->getNumberOfStates());
        EXPECT_EQ(2ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        uint64_t propertyIndex = 0;
        auto expected = this->parseNumber("0.9502129316");  // integrate  6* e^(-6*t) dt from 0 to 0.5 = 1 - 1/(e^3)
        result = checker->check(this->env(), tasks[propertyIndex++]);
        EXPECT_NEAR(expected, this->getQuantitativeResultAtInitialState(model, result), this->precision());

        expected = this->parseNumber("0.075198060651");  // integrate min(t,0.1) * 6* e^(-6*t) dt from 0 to infty = 1/6 - 1/(6*e^0.6)
        result = checker->check(this->env(), tasks[propertyIndex++]);
        EXPECT_NEAR(expected, this->getQuantitativeResultAtInitialState(model, result), this->precision());

        expected = this->parseNumber("0.1662535413");  // =  1/6 - 1/(6*e^6)
        result = checker->check(this->env(), tasks[propertyIndex++]);
        EXPECT_NEAR(expected, this->getQuantitativeResultAtInitialState(model, result), this->precision());

        expected = this->parseNumber("0.16666666667");  // = 1/6 - 1/(6*e^60)
        result = checker->check(this->env(), tasks[propertyIndex++]);
        EXPECT_NEAR(expected, this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(CtmcCslModelCheckerTest, simple2) {
    std::string formulasString = "R{\"rew1\"}=? [ C ]";
    formulasString += "; R{\"rew2\"}=? [ C ]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/simple2.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
    auto checker = this->createModelChecker(model);
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    // Total reward formulas are currently not supported for non-sparse models.
    if (this->isSparseModel()) {
        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("23/8"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));
    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
    }
}

TEST(CtmcCslModelCheckerTest, TransientProbabilities) {
    // Example from lecture
    storm::storage::SparseMatrixBuilder<double> matrixBuilder;
    matrixBuilder.addNextValue(0, 1, 3.0);
    matrixBuilder.addNextValue(1, 0, 2.0);
    storm::storage::SparseMatrix<double> matrix = matrixBuilder.build();

    std::vector<double> exitRates = {3, 2};
    storm::storage::BitVector initialStates(2);
    initialStates.set(0);
    storm::storage::BitVector phiStates(2);
    storm::storage::BitVector psiStates(2);
    storm::Environment env;
    std::vector<double> result =
        storm::modelchecker::helper::SparseCtmcCslHelper::computeAllTransientProbabilities(env, matrix, initialStates, phiStates, psiStates, exitRates, 1.0);

    EXPECT_NEAR(0.404043, result[0], 1e-6);
    EXPECT_NEAR(0.595957, result[1], 1e-6);
}

TYPED_TEST(CtmcCslModelCheckerTest, LtlProbabilitiesEmbedded) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "P=?  [ X F (!\"down\" U \"fail_sensors\") ]";
    formulasString += "; P=?  [  (! \"down\" U ( F \"fail_actuators\" U \"fail_io\")) ]";
    formulasString += "; P=?  [ F \"down\" & X G \"fail_sensors\" ]";    // F ("down" & (X (G "fail_sensors")) )
    formulasString += "; P<1  [ F \"down\" & X G \"fail_sensors\" ]";    // F ("down" & (X (G "fail_sensors")) )
    formulasString += "; P>0.9  [ F \"down\" & X G \"fail_sensors\" ]";  // F ("down" & (X (G "fail_sensors")) )
    formulasString += "; P=?  [ ( X X X X X X !\"down\") ]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(3478ul, model->getNumberOfStates());
    EXPECT_EQ(14639ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);

    auto checker = this->createModelChecker(model);
    std::unique_ptr<storm::modelchecker::CheckResult> result;
    if (TypeParam::engine == CtmcEngine::PrismSparse || TypeParam::engine == CtmcEngine::JaniSparse) {
        result = checker->check(this->env(), tasks[0]);

        EXPECT_NEAR(this->parseNumber("6201111489217/6635130141055"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber(
                        "18202096756782580932452575931626387221032158256811408997544247061630654697057679936888914805428640014003923523854237983369404268771063"
                        "94069377761908861354080901656982334306495936806479819524633477460248734957778578143867737103318041699906745291007545836953677741618755"
                        "07044994661278270139591275881461260070117701448841596602041739425021618556458197240253807881255308059191194657575523127649292873946401"
                        "60070574497678932806876628747257457080365284815733250063282059801375609508589060775771365121570763916254686586313389233475877585811662"
                        "0357147078085971884542677483086820336410289852315689173054718889915988181632722920483222289764504589120520736097022590652379/"
                        "20728856609142464902872005939672257918093593973189631748243740034008572994831983832074458796252899535111673563261006250198416596416082"
                        "47438209685029840433936315271626219407003724379672398699027456844707912472180250116299363836831808191736954813578721274144593836954580"
                        "34829702750095798077157671737142972112983783526417917830063407814131534204102733837172200651731758231462705370319722286895248244604663"
                        "52533471344327110886363060826404895467465893956592712512371350202090241564215213495275079722307576230931018615138366081056985311311073"
                        "6677679624520398243456708277522978182665363672039087497825748539279661186260492946647251487454498033763681938956076478285935"),
                    this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("6201111489217/6635130141055"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_TRUE(this->getQualitativeResultAtInitialState(model, result));

        result = checker->check(this->env(), tasks[4]);
        EXPECT_TRUE(this->getQualitativeResultAtInitialState(model, result));

        result = checker->check(this->env(), tasks[5]);
        EXPECT_NEAR(
            this->parseNumber("12029468814241604838132837548112161585296814078767944885625781581320425536116175992062768102403002605220660026092168995568067825"
                              "92159369214025808794108439870476812421358247413140536419394633974952856222783873484713998841710951907938016323396243234904692402"
                              "52961677855027461283258094829047603758584033514598048083454101721311299129841296115907033485846297142252703801497367159453680266"
                              "24792167429109658151655487647994259314643886424142766790442953987818429468012101992798588110824350651761547347356873144826620057"
                              "74077569191240855183945543277103593833128730289929192543135438680962528094118605758445585995100073617/"
                              "12029720509287104832319318812972923934077469133245668896466828391759984671516390780365666074295318137833660807349293929200056244"
                              "56488274509559191073647430577878963431232759904608946782891839010213213206349818700590576273106941528872428420715245020013732520"
                              "85567053299253798937031071047697289051751242127258744937438520681995840650837777808189738580512102669860241858552660166073849086"
                              "67980634152813607901785527175246314720373131268228013598128613517599905951566964079825880226357974174647948493893065216449623389"
                              "15119549986021747121828809226061546105390273235103310319147341214480736689719855550298904337697187500"),
            this->getQuantitativeResultAtInitialState(model, result), this->precision());

    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
    }
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(CtmcCslModelCheckerTest, LtlProbabilitiesPolling) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "P=? [ X X !(s=1 & a=1) U (s=2 & a=1) ]";  // (X X a) U b
    formulasString += "; P=? [ X (( !(s=1) U (a=1)) U (s=1)) ]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/polling2.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(22ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);

    auto checker = this->createModelChecker(model);
    std::unique_ptr<storm::modelchecker::CheckResult> result;
    // LTL not supported in all engines (Hybrid,  PrismDd, JaniDd)
    if (TypeParam::engine == CtmcEngine::PrismSparse || TypeParam::engine == CtmcEngine::JaniSparse) {
        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("80400/160801"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("601/80601"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
    }
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(CtmcCslModelCheckerTest, HOAProbabilitiesPolling) {
    // "P=? [ F "target" & (X s=1)]"
    std::string formulasString = "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> \"target\", \"p1\" -> (s=1) }]";
    // "P=? [!(s=1) U (s=1)]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> !(s=1) , \"p1\" -> \"target\" }]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/polling2.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(22ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
    auto checker = this->createModelChecker(model);
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    // Not supported in all engines (Hybrid,  PrismDd, JaniDd)
    if (TypeParam::engine == CtmcEngine::PrismSparse || TypeParam::engine == CtmcEngine::JaniSparse) {
        result = checker->check(tasks[0]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(tasks[1]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
    }
}
}  // namespace
