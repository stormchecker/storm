#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/csl/HybridCtmcCslModelChecker.h"
#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace {

enum class CtmcEngine { PrismSparse, JaniSparse, JaniHybrid };

#ifdef STORM_HAVE_GMM
class GBSparseGmmxxGmresIluEnvironment {
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
        env.solver().gmmxx().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().lra().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        return env;
    }
};

class GBJaniSparseGmmxxGmresIluEnvironment {
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
        env.solver().gmmxx().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().lra().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        return env;
    }
};

class GBJaniHybridCuddGmmxxGmresEnvironment {
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
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().lra().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        return env;
    }
};

class GBJaniHybridSylvanGmmxxGmresEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const CtmcEngine engine = CtmcEngine::JaniHybrid;
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
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().lra().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        return env;
    }
};
#endif

class GBSparseEigenDGmresEnvironment {
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
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        return env;
    }
};

class GBSparseEigenDoubleLUEnvironment {
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
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        return env;
    }
};

class GBSparseNativeSorEnvironment {
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
        env.solver().native().setSorOmega(storm::utility::convertNumber<storm::RationalNumber>(0.7));  // LRA computation fails for 0.9
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations);
        env.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-9));
        return env;
    }
};

#ifdef STORM_HAVE_GMM
class DistrSparseGmmxxGmresIluEnvironment {
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
        env.solver().gmmxx().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::LraDistributionEquations);
        env.solver().lra().setPrecision(
            storm::utility::convertNumber<storm::RationalNumber>(1e-8));  // Need to increase precision because eq sys yields incorrect results
        return env;
    }
};
#endif

class DistrSparseEigenDoubleLUEnvironment {
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
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::LraDistributionEquations);
        return env;
    }
};

class ValueIterationSparseEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const CtmcEngine engine = CtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().lra().setDetLraMethod(storm::solver::LraMethod::ValueIteration);
        return env;
    }
};

class SoundEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const CtmcEngine engine = CtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        return env;
    }
};

template<typename TestType>
class LraCtmcCslModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::Ctmc<ValueType> SparseModelType;
    typedef typename storm::models::symbolic::Ctmc<TestType::ddType, ValueType> SymbolicModelType;

    LraCtmcCslModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        if constexpr (TestType::engine == CtmcEngine::JaniHybrid) {
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
        if (TestType::engine == CtmcEngine::JaniHybrid) {
            auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            janiData.first.substituteFunctions();
            result.second = storm::api::extractFormulasFromProperties(janiData.second);
            result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(janiData.first, result.second)->template as<MT>();
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
        return nullptr;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type
    createModelChecker(std::shared_ptr<MT> const& model) const {
        if (TestType::engine == CtmcEngine::JaniHybrid) {
            return std::make_shared<storm::modelchecker::HybridCtmcCslModelChecker<SymbolicModelType>>(*model);
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

typedef ::testing::Types<
#ifdef STORM_HAVE_GMM
    GBSparseGmmxxGmresIluEnvironment, GBJaniSparseGmmxxGmresIluEnvironment, GBJaniHybridCuddGmmxxGmresEnvironment, GBJaniHybridSylvanGmmxxGmresEnvironment,
    DistrSparseGmmxxGmresIluEnvironment,
#endif
    GBSparseEigenDGmresEnvironment, GBSparseEigenDoubleLUEnvironment, GBSparseNativeSorEnvironment, DistrSparseEigenDoubleLUEnvironment,
    ValueIterationSparseEnvironment, SoundEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(LraCtmcCslModelCheckerTest, TestingTypes, );

TYPED_TEST(LraCtmcCslModelCheckerTest, Cluster) {
    std::string formulasString = "LRA=? [\"minimum\"]";

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
        EXPECT_NEAR(this->parseNumber("0.99999766034263426"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(LraCtmcCslModelCheckerTest, Embedded) {
    std::string formulasString = "LRA=? [\"fail_sensors\"]";

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
        EXPECT_NEAR(this->parseNumber("6201111489217/6635130141055"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(LraCtmcCslModelCheckerTest, Polling) {
    std::string formulasString = "LRA=?[\"target\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/polling2.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(12ul, model->getNumberOfStates());
        EXPECT_EQ(22ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("0.20079750055570736"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(LraCtmcCslModelCheckerTest, Tandem) {
    std::string formulasString = "LRA=? [\"first_queue_full\"]";

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
        EXPECT_NEAR(this->parseNumber("0.9100373532"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(LraCtmcCslModelCheckerTest, Rewards) {
    std::string formulasString = "R=? [ LRA ]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/lrarewards.sm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(4ul, model->getNumberOfStates());
        EXPECT_EQ(6ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("11/15"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(LraCtmcCslModelCheckerTest, kanban) {
    std::string formulasString = "R{\"throughput\"}=? [ LRA ]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/ctmc/kanban.prism", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(160ul, model->getNumberOfStates());
        EXPECT_EQ(616ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(
            this->parseNumber("1132372552133951639536770152429724263999896896549676426094918302160613340902023133969841067385167041200690481843915870926707"
                              "11590526535239899047608853509681914074220789038015289373871985431257486278/"
                              "1223067474012215838745994023624181143448435715858923491568712969277184634645504346456117443333632535902774869182827010789201"
                              "972713368729205674432492059242349591780604188152950845769793378621446766887"),
            this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

}  // namespace
