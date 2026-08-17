#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm/api/builder.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/vector.h"

namespace {

enum class CtmcEngine { JaniSparse };

#ifdef STORM_HAVE_GMM
class SparseGmmxxGmresIluEnvironment {
   public:
    static const CtmcEngine engine = CtmcEngine::JaniSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        // env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6)); // Need to increase precision because eq sys yields
        // incorrect results
        return env;
    }
};
#endif

class SparseSoundEvtEnvironment {
   public:
    static const CtmcEngine engine = CtmcEngine::JaniSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().setSteadyStateDistributionAlgorithm(storm::SteadyStateDistributionAlgorithm::ExpectedVisitingTimes);
        env.solver().setForceSoundness(true);
        return env;
    }
};

class SparseClassicEnvironment {
   public:
    static const CtmcEngine engine = CtmcEngine::JaniSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().setSteadyStateDistributionAlgorithm(storm::SteadyStateDistributionAlgorithm::Classic);
        return env;
    }
};

class SparseEigenRationalLuEnvironment {
   public:
    static const CtmcEngine engine = CtmcEngine::JaniSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        env.solver().setForceExact(true);
        return env;
    }
};

template<typename TestType>
class SteadyStateCtmcCslModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::Ctmc<ValueType> SparseModelType;

    SteadyStateCtmcCslModelCheckerTest() : _environment(TestType::createEnvironment()) {}
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
    CtmcEngine getEngine() const {
        return TestType::engine;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value, std::shared_ptr<MT>>::type buildJaniModel(
        std::string const& pathToJaniModel, std::string const& constantDefinitionString = "") const {
        auto janiDescr = storm::storage::SymbolicModelDescription(storm::api::parseJaniModel(pathToJaniModel).first);
        janiDescr = janiDescr.preprocess(constantDefinitionString);
        return storm::api::buildSparseModel<ValueType>(janiDescr, storm::builder::BuilderOptions())->template as<MT>();
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value, std::shared_ptr<storm::modelchecker::SparseCtmcCslModelChecker<MT>>>::type
    createModelChecker(std::shared_ptr<MT> const& model) const {
        if (TestType::engine == CtmcEngine::JaniSparse) {
            return std::make_shared<storm::modelchecker::SparseCtmcCslModelChecker<SparseModelType>>(*model);
        }
        return nullptr;
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<
#ifdef STORM_HAVE_GMM
    SparseGmmxxGmresIluEnvironment,
#endif
    SparseSoundEvtEnvironment, SparseClassicEnvironment, SparseEigenRationalLuEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(SteadyStateCtmcCslModelCheckerTest, TestingTypes, );

TYPED_TEST(SteadyStateCtmcCslModelCheckerTest, steadystatetest) {
    typedef typename TestFixture::ValueType ValueType;

    auto model = this->buildJaniModel(STORM_TEST_RESOURCES_DIR "/ctmc/steadystatetest.jani");
    EXPECT_EQ(8ul, model->getNumberOfStates());
    EXPECT_EQ(12ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Ctmc);
    auto checker = this->createModelChecker(model);
    auto result = checker->computeSteadyStateDistribution(this->env());

    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    auto resultVector = result->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector();
    auto sortedVector = resultVector;
    std::sort(sortedVector.begin(), sortedVector.end());
    EXPECT_EQ(sortedVector[0], storm::utility::zero<ValueType>())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
    EXPECT_EQ(sortedVector[1], storm::utility::zero<ValueType>())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
    EXPECT_EQ(sortedVector[2], storm::utility::zero<ValueType>())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
    EXPECT_NEAR(sortedVector[3], this->parseNumber("1/35"), this->precision())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
    EXPECT_NEAR(sortedVector[4], this->parseNumber("2/35"), this->precision())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
    EXPECT_NEAR(sortedVector[5], this->parseNumber("4/35"), this->precision())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
    EXPECT_NEAR(sortedVector[6], this->parseNumber("1/5"), this->precision())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
    EXPECT_NEAR(sortedVector[7], this->parseNumber("3/5"), this->precision())
        << "Result of steady state computation is " << storm::utility::vector::toString(resultVector) << '\n';
}
}  // namespace
