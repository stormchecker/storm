#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/bisimulation.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/api/verification.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/storage/prism/Program.h"
#include "storm/utility/constants.h"

namespace {
class DoubleSVIEnvironment {
   public:
    typedef double ValueType;
    static storm::modelchecker::RegionCheckEngine const regionEngine = storm::modelchecker::RegionCheckEngine::ParameterLifting;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class RationalPiEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static storm::modelchecker::RegionCheckEngine const regionEngine = storm::modelchecker::RegionCheckEngine::ExactParameterLifting;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        return env;
    }
};

template<typename TestType>
class SparseDtmcParameterLiftingMonotonicityTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    SparseDtmcParameterLiftingMonotonicityTest() : _environment(TestType::createEnvironment()) {}
    storm::Environment const& env() const {
        return _environment;
    }
    virtual void SetUp() {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        carl::VariablePool::getInstance().clear();
    }
    virtual void TearDown() {
        carl::VariablePool::getInstance().clear();
    }

   private:
    storm::Environment _environment;
};

struct MonotonicityTestData {
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model;
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas;
    std::set<storm::RationalFunctionVariable> modelParameters;
};

void buildMonotonicityModel(std::string const& programFile, std::string const& formulaAsString, std::string const& constantsAsString,
                            bool simplifyAndBisimulate, MonotonicityTestData& data) {
    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    if (simplifyAndBisimulate) {
        // Simplify model
        auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
        ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
        model = simplifier.getSimplifiedModel()->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
        formulas[0] = simplifier.getSimplifiedFormula();

        // Apply bisimulation
        storm::storage::BisimulationType bisimType = storm::storage::BisimulationType::Strong;
        if (storm::settings::getModule<storm::settings::modules::BisimulationSettings>().isWeakBisimulationSet()) {
            bisimType = storm::storage::BisimulationType::Weak;
        }
        model = storm::api::performBisimulationMinimization<storm::RationalFunction>(model, formulas, bisimType)
                    ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    }

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    data.model = std::move(model);
    data.formulas = std::move(formulas);
    data.modelParameters = std::move(modelParameters);
}

template<typename ValueType>
void checkBrpMonotonicity(storm::Environment const& env, storm::modelchecker::RegionCheckEngine regionEngine, MonotonicityTestData data,
                          std::vector<std::string> const& regionStrings) {
    // Reachability order, as it is already done building we don't need to recreate the order for each region
    storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType> monHelper(data.model, data.formulas, {});
    auto monRes = monHelper.checkMonotonicityInBuild(std::cout);
    auto order = monRes.begin()->first;
    ASSERT_EQ(order->getNumberOfAddedStates(), data.model->getTransitionMatrix().getColumnCount());
    ASSERT_TRUE(order->getDoneBuilding());

    // Modelcheckers
    auto regionCheckerMon = storm::api::initializeRegionModelChecker<storm::RationalFunction>(
        env, data.model, storm::api::createTask<storm::RationalFunction>(data.formulas[0], true), regionEngine, true, true, false,
        storm::api::MonotonicitySetting(true));
    auto regionChecker = storm::api::initializeRegionModelChecker<storm::RationalFunction>(
        env, data.model, storm::api::createTask<storm::RationalFunction>(data.formulas[0], true), regionEngine, true, true, false,
        storm::api::MonotonicitySetting(false));

    // start testing
    for (auto const& regionString : regionStrings) {
        auto region = storm::api::parseRegion<storm::RationalFunction>(regionString, data.modelParameters);
        auto expectedResult = regionChecker->analyzeRegion(env, region, storm::modelchecker::RegionResultHypothesis::Unknown, true);
        EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(env, region, storm::modelchecker::RegionResultHypothesis::Unknown, true));
    }
}

template<typename ValueType>
void checkSimpleMonotonicity(storm::Environment const& env, storm::modelchecker::RegionCheckEngine regionEngine, MonotonicityTestData data,
                             std::vector<std::string> const& regionStrings, bool assertDoneBuilding = false, bool printMatrix = false) {
    if (printMatrix) {
        data.model->getTransitionMatrix().printAsMatlabMatrix(std::cout);
    }
    storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType> monHelper(data.model, data.formulas, {});
    auto order = monHelper.checkMonotonicityInBuild(std::cout).begin()->first;
    if (assertDoneBuilding) {
        ASSERT_TRUE(order->getDoneBuilding());
    }

    // Modelcheckers
    auto regionCheckerMon = storm::api::initializeRegionModelChecker<storm::RationalFunction>(
        env, data.model, storm::api::createTask<storm::RationalFunction>(data.formulas[0], true), regionEngine, true, true, false,
        storm::api::MonotonicitySetting(true));
    auto regionChecker = storm::api::initializeRegionModelChecker<storm::RationalFunction>(
        env, data.model, storm::api::createTask<storm::RationalFunction>(data.formulas[0], true), regionEngine, true, true, false,
        storm::api::MonotonicitySetting(false));

    // Start testing
    for (auto const& regionString : regionStrings) {
        auto region = storm::api::parseRegion<storm::RationalFunction>(regionString, data.modelParameters);
        monHelper.createLocalMonotonicityResult(order, region);
        EXPECT_EQ(regionChecker->analyzeRegion(env, region, storm::modelchecker::RegionResultHypothesis::Unknown, true),
                  regionCheckerMon->analyzeRegion(env, region, storm::modelchecker::RegionResultHypothesis::Unknown, true));
    }
}

typedef ::testing::Types<DoubleSVIEnvironment, RationalPiEnvironment> TestingTypes;

TYPED_TEST_SUITE(SparseDtmcParameterLiftingMonotonicityTest, TestingTypes, );

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_LEQ) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm", "P<=0.84 [F s=5 ]", "", true, data);
    checkBrpMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data),
                                    {"0.7<=pL<=0.9,0.75<=pK<=0.95", "0.4<=pL<=0.65,0.75<=pK<=0.95", "0.1<=pL<=0.73,0.2<=pK<=0.715"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_GEQ) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm", "P>=0.84 [F s=5 ]", "", true, data);
    checkBrpMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data),
                                    {"0.1<=pL<=0.73,0.2<=pK<=0.715", "0.4<=pL<=0.65,0.75<=pK<=0.95", "0.7<=pL<=0.9,0.75<=pK<=0.95"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_LEQ_Incr) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2_mon_incr.pm", "P<=0.84 [F s=5 ]", "", true, data);
    checkBrpMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data),
                                    {"0.7<=pL<=0.9,0.75<=pK<=0.95", "0.4<=pL<=0.65,0.75<=pK<=0.95", "0.1<=pL<=0.73,0.2<=pK<=0.715"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_GEQ_Incr) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2_mon_incr.pm", "P>=0.84 [F s=5 ]", "", true, data);
    checkBrpMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data),
                                    {"0.1<=pL<=0.73,0.2<=pK<=0.715", "0.4<=pL<=0.65,0.75<=pK<=0.95", "0.7<=pL<=0.9,0.75<=pK<=0.95"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Parametric_Die_Mon) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/parametric_die_2.pm", "P <=0.5 [F s=7 & d=2 ]", "", true, data);
    checkSimpleMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data),
                                       {"0.1<=p<=0.2,0.8<=q<=0.9", "0.1<=p<=0.9,0.1<=q<=0.9", "0.8<=p<=0.9,0.1<=q<=0.2"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Simple1_Mon) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/simple1.pm", "P<0.75 [F s=3 ]", "", false, data);
    checkSimpleMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data), {"0.4<=p<=0.6", "0.1<=p<=0.9", "0.05<=p<=0.1"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Casestudy1_Mon) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy1.pm", "P<0.5 [F s=3 ]", "", false, data);
    checkSimpleMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data), {"0.1<=p<=0.5", "0.4<=p<=0.8", "0.7<=p<=0.9"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Casestudy2_Mon) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy2.pm", "P<0.5 [F s=4 ]", "", false, data);
    checkSimpleMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data), {"0.1<=p<=0.4", "0.4<=p<=0.9", "0.8<=p<=0.9"});
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Casestudy3_Mon) {
    typedef typename TestFixture::ValueType ValueType;
    MonotonicityTestData data;
    buildMonotonicityModel(STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy3.pm", "P<0.5 [F s=3 ]", "", false, data);
    checkSimpleMonotonicity<ValueType>(this->env(), TypeParam::regionEngine, std::move(data), {"0.6<=p<=0.9", "0.3<=p<=0.7", "0.1<=p<=0.4"}, true, true);
}
}  // namespace
