#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"

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
    std::unique_ptr<storm::modelchecker::RegionModelChecker<storm::RationalFunction>> initializeRegionModelChecker(
        std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model, std::shared_ptr<const storm::logic::Formula> formula,
        bool useMonotonicity) {
        return storm::api::initializeRegionModelChecker(env(), model, storm::api::createTask<storm::RationalFunction>(formula, true), TestType::regionEngine,
                                                        true, true, false, storm::api::MonotonicitySetting(useMonotonicity));
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

typedef ::testing::Types<DoubleSVIEnvironment, RationalPiEnvironment> TestingTypes;

TYPED_TEST_SUITE(SparseDtmcParameterLiftingMonotonicityTest, TestingTypes, );

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_LEQ) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P<=0.84 [F s=5 ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

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

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Reachability order, as it is already done building we don't need to recreate the order for each region
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto monRes = monHelper->checkMonotonicityInBuild(std::cout);
    auto order = monRes.begin()->first;
    ASSERT_EQ(order->getNumberOfAddedStates(), model->getTransitionMatrix().getColumnCount());
    ASSERT_TRUE(order->getDoneBuilding());
    auto localMonRes =
        std::make_shared<storm::analysis::LocalMonotonicityResult<storm::RationalFunctionVariable>>(monRes.begin()->second.first, order->getNumberOfStates());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.7<=pL<=0.9,0.75<=pK<=0.95", modelParameters);
    auto expectedResult = regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.4<=pL<=0.65,0.75<=pK<=0.95", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=pL<=0.73,0.2<=pK<=0.715", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_GEQ) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P>=0.84 [F s=5 ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

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

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Reachability order, as it is already done building we don't need to recreate the order for each region
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto monRes = monHelper->checkMonotonicityInBuild(std::cout);
    auto order = monRes.begin()->first;
    ASSERT_EQ(order->getNumberOfAddedStates(), model->getTransitionMatrix().getColumnCount());
    ASSERT_TRUE(order->getDoneBuilding());
    auto localMonRes =
        std::make_shared<storm::analysis::LocalMonotonicityResult<storm::RationalFunctionVariable>>(monRes.begin()->second.first, order->getNumberOfStates());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=pL<=0.73,0.2<=pK<=0.715", modelParameters);
    auto expectedResult = regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.4<=pL<=0.65,0.75<=pK<=0.95", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.7<=pL<=0.9,0.75<=pK<=0.95", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_LEQ_Incr) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2_mon_incr.pm";
    std::string formulaAsString = "P<=0.84 [F s=5 ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

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

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Reachability order, as it is already done building we don't need to recreate the order for each region
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto monRes = monHelper->checkMonotonicityInBuild(std::cout);
    auto order = monRes.begin()->first;
    ASSERT_EQ(order->getNumberOfAddedStates(), model->getTransitionMatrix().getColumnCount());
    ASSERT_TRUE(order->getDoneBuilding());
    auto localMonRes =
        std::make_shared<storm::analysis::LocalMonotonicityResult<storm::RationalFunctionVariable>>(monRes.begin()->second.first, order->getNumberOfStates());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.7<=pL<=0.9,0.75<=pK<=0.95", modelParameters);
    auto expectedResult = regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.4<=pL<=0.65,0.75<=pK<=0.95", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=pL<=0.73,0.2<=pK<=0.715", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Brp_Prob_Mon_GEQ_Incr) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2_mon_incr.pm";
    std::string formulaAsString = "P>=0.84 [F s=5 ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

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

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Reachability order, as it is already done building we don't need to recreate the order for each region
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto monRes = monHelper->checkMonotonicityInBuild(std::cout);
    auto order = monRes.begin()->first;
    ASSERT_EQ(order->getNumberOfAddedStates(), model->getTransitionMatrix().getColumnCount());
    ASSERT_TRUE(order->getDoneBuilding());
    auto localMonRes =
        std::make_shared<storm::analysis::LocalMonotonicityResult<storm::RationalFunctionVariable>>(monRes.begin()->second.first, order->getNumberOfStates());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=pL<=0.73,0.2<=pK<=0.715", modelParameters);
    auto expectedResult = regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.4<=pL<=0.65,0.75<=pK<=0.95", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.7<=pL<=0.9,0.75<=pK<=0.95", modelParameters);
    expectedResult = regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true);
    EXPECT_EQ(expectedResult, regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Parametric_Die_Mon) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/parametric_die_2.pm";
    std::string formulaAsString = "P <=0.5 [F s=7 & d=2 ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

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

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // Start testing, localMonRes will remain the same
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.2,0.8<=q<=0.9", modelParameters);
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto order = monHelper->checkMonotonicityInBuild(std::cout).begin()->first;
    auto monRes = monHelper->createLocalMonotonicityResult(order, allSatRegion);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9,0.1<=q<=0.9", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.8<=p<=0.9,0.1<=q<=0.2", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Simple1_Mon) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/simple1.pm";
    std::string formulaAsString = "P<0.75 [F s=3 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // Start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.4<=p<=0.6", modelParameters);
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto order = monHelper->checkMonotonicityInBuild(std::cout).begin()->first;
    auto monRes = monHelper->createLocalMonotonicityResult(order, allSatRegion);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.05<=p<=0.1", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Casestudy1_Mon) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy1.pm";
    std::string formulaAsString = "P<0.5 [F s=3 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // Start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.5", modelParameters);
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto order = monHelper->checkMonotonicityInBuild(std::cout).begin()->first;
    auto monRes = monHelper->createLocalMonotonicityResult(order, allSatRegion);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.4<=p<=0.8", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.7<=p<=0.9", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Casestudy2_Mon) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy2.pm";
    std::string formulaAsString = "P<0.5 [F s=4 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    // Start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.4", modelParameters);
    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto order = monHelper->checkMonotonicityInBuild(std::cout).begin()->first;
    auto monRes = monHelper->createLocalMonotonicityResult(order, allSatRegion);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.4<=p<=0.9", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.8<=p<=0.9", modelParameters);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}

TYPED_TEST(SparseDtmcParameterLiftingMonotonicityTest, Casestudy3_Mon) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy3.pm";
    std::string formulaAsString = "P<0.5 [F s=3 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // Model parameters
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    // Modelcheckers
    auto regionCheckerMon = this->initializeRegionModelChecker(model, formulas[0], true);
    auto regionChecker = this->initializeRegionModelChecker(model, formulas[0], false);

    model->getTransitionMatrix().printAsMatlabMatrix(std::cout);

    auto monHelper = new storm::analysis::MonotonicityHelper<storm::RationalFunction, ValueType>(model, formulas, {});
    auto order = monHelper->checkMonotonicityInBuild(std::cout).begin()->first;
    ASSERT_TRUE(order->getDoneBuilding());

    // Start testing
    auto allSatRegion = storm::api::parseRegion<storm::RationalFunction>("0.6<=p<=0.9", modelParameters);
    auto monRes = monHelper->createLocalMonotonicityResult(order, allSatRegion);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allSatRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto exBothRegion = storm::api::parseRegion<storm::RationalFunction>("0.3<=p<=0.7", modelParameters);
    monRes = monHelper->createLocalMonotonicityResult(order, exBothRegion);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), exBothRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));

    auto allVioRegion = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.4", modelParameters);
    monRes = monHelper->createLocalMonotonicityResult(order, allVioRegion);
    EXPECT_EQ(regionChecker->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true),
              regionCheckerMon->analyzeRegion(this->env(), allVioRegion, storm::modelchecker::RegionResultHypothesis::Unknown, true));
}
}  // namespace
