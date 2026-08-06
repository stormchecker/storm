#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/derivative/GradientDescentInstantiationSearcher.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-pars/utility/FeasibilitySynthesisTask.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/logic/Formulas.h"

using namespace storm::pars;

namespace {
/*class RationalGmmxxEnvironment {
   public:
    typedef storm::RationalFunction FunctionType;
    typedef storm::RationalNumber ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        return env;
    }
};*/
class DoubleGmmxxEnvironment {
   public:
    typedef storm::RationalFunction FunctionType;
    typedef double ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        return env;
    }
};
/*class RationalEigenEnvironment {
   public:
    typedef storm::RationalFunction FunctionType;
    typedef storm::RationalNumber ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        return env;
    }
};*/
class DoubleEigenEnvironment {
   public:
    typedef storm::RationalFunction FunctionType;
    typedef double ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        return env;
    }
};
class DoubleEigenTopologicalEnvironment {
   public:
    typedef storm::RationalFunction FunctionType;
    typedef double ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Topological);
        env.solver().topological().setUnderlyingEquationSolverType(storm::solver::EquationSolverType::Eigen);
        return env;
    }
};
template<typename TestType>
class GradientDescentInstantiationSearcherTest : public ::testing::Test {
   public:
    typedef typename TestType::FunctionType FunctionType;
    typedef typename TestType::ConstantType ConstantType;
    GradientDescentInstantiationSearcherTest() : _environment(TestType::createEnvironment()) {}
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

typedef ::testing::Types<
// The rational environments take ages... GD is just not made for rational arithmetic.
#ifdef STORM_HAVE_GMM
    DoubleGmmxxEnvironment, /*RationalGmmxxEnvironment,*/
#endif
    DoubleEigenEnvironment, DoubleEigenTopologicalEnvironment /*, RationalEigenEnvironment*/>
    TestingTypes;
}  // namespace

TYPED_TEST_SUITE(GradientDescentInstantiationSearcherTest, TestingTypes, );

TYPED_TEST(GradientDescentInstantiationSearcherTest, Simple) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/gradient1.pm";
    std::string formulaAsString = "P>=0.2499 [F s=2]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*dtmc);
    ASSERT_TRUE(simplifier.simplify(*formulas[0]));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);

    storm::derivative::GradientDescentInstantiationSearcher<typename TestFixture::FunctionType, typename TestFixture::ConstantType> checker(*dtmc);
    std::shared_ptr<storm::logic::Formula> formulaWithoutBounds = formulas[0]->clone();
    std::shared_ptr<storm::logic::Formula const> formulaNoBound = formulaWithoutBounds->asSharedPointer();
    std::shared_ptr<FeasibilitySynthesisTask> t = std::make_shared<FeasibilitySynthesisTask>(formulaNoBound);
    t->setBound(formulas[0]->asOperatorFormula().getBound());
    std::shared_ptr<FeasibilitySynthesisTask const> feasibilityTask = std::make_shared<FeasibilitySynthesisTask const>(std::move(*t));

    checker.setup(this->env(), feasibilityTask);
    typename TestFixture::ConstantType doubleInstantiation = checker.gradientDescent().second;
    ASSERT_NEAR(doubleInstantiation * 4, 1, 1e-6);
}

TYPED_TEST(GradientDescentInstantiationSearcherTest, Crowds) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/crowds3_5.pm";
    std::string formulaAsString = "P<=0.00000001 [F \"observe0Greater1\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*dtmc);
    ASSERT_TRUE(simplifier.simplify(*formulas[0]));
    model = simplifier.getSimplifiedModel();

    dtmc = storm::api::performBisimulationMinimization<storm::RationalFunction>(model, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    std::shared_ptr<storm::logic::Formula> formulaWithoutBounds = formulas[0]->clone();
    std::shared_ptr<storm::logic::Formula const> formulaNoBound = formulaWithoutBounds->asSharedPointer();
    std::shared_ptr<FeasibilitySynthesisTask> t = std::make_shared<FeasibilitySynthesisTask>(formulaNoBound);
    t->setBound(formulas[0]->asOperatorFormula().getBound());
    std::shared_ptr<FeasibilitySynthesisTask const> feasibilityTask = std::make_shared<FeasibilitySynthesisTask const>(std::move(*t));

    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);

    // First, test an ADAM instance. We will check that we have implemented ADAM correctly by comparing our results to results gathered by an ADAM
    // implementation in tensorflow :)
    storm::derivative::GradientDescentInstantiationSearcher<typename TestFixture::FunctionType, typename TestFixture::ConstantType> adamChecker(
        *dtmc, storm::derivative::GradientDescentMethod::ADAM, 0.01, 0.9, 0.999, 2, 1e-6, std::nullopt,
        storm::derivative::GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT, std::nullopt, true);
    adamChecker.setup(this->env(), feasibilityTask);
    auto doubleInstantiation = adamChecker.gradientDescent();
    auto walk = adamChecker.getVisualizationWalk();

    carl::Variable badCVar;
    carl::Variable pfVar;
    for (auto parameter : storm::models::sparse::getProbabilityParameters(*dtmc)) {
        if (parameter.name() == "badC") {
            badCVar = parameter;
        } else if (parameter.name() == "PF") {
            pfVar = parameter;
        }
    }
    std::shared_ptr<storm::RawPolynomialCache> cache = std::make_shared<storm::RawPolynomialCache>();
    auto const badC = storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(badCVar), cache));
    auto const pf = storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(pfVar), cache));

    const double badCValues[] = {0.5,
                                 0.49000033736228943,
                                 0.4799894690513611,
                                 0.4699603021144867,
                                 0.4599062204360962,
                                 0.4498208165168762,
                                 0.43969807028770447,
                                 0.4295324981212616,
                                 0.41931894421577454,
                                 0.4090527594089508,
                                 0.39872977137565613,
                                 0.3883463442325592,
                                 0.3778994083404541,
                                 0.3673863708972931,
                                 0.35680532455444336,
                                 0.3461548984050751,
                                 0.3354344069957733,
                                 0.3246437907218933,
                                 0.31378373503685,
                                 0.30285564064979553,
                                 0.2918616533279419,
                                 0.28080472350120544,
                                 0.2696886956691742,
                                 0.258518248796463,
                                 0.24729910492897034,
                                 0.23603792488574982,
                                 0.22474248707294464,
                                 0.21342173218727112,
                                 0.20208582282066345,
                                 0.1907462775707245,
                                 0.17941603064537048,
                                 0.16810956597328186,
                                 0.15684303641319275,
                                 0.14563430845737457,
                                 0.13450318574905396,
                                 0.1234714537858963,
                                 0.11256305128335953,
                                 0.10180411487817764,
                                 0.0912230908870697,
                                 0.080850750207901,
                                 0.07072017341852188};
    const double pfValues[] = {0.5,
                               0.49000218510627747,
                               0.47999337315559387,
                               0.46996763348579407,
                               0.45991936326026917,
                               0.4498434364795685,
                               0.4397353231906891,
                               0.4295912981033325,
                               0.4194081425666809,
                               0.4091835021972656,
                               0.3989158570766449,
                               0.3886045515537262,
                               0.3782498240470886,
                               0.3678528368473053,
                               0.3574157953262329,
                               0.3469419777393341,
                               0.33643582463264465,
                               0.32590293884277344,
                               0.3153502345085144,
                               0.30478596687316895,
                               0.2942197024822235,
                               0.28366249799728394,
                               0.2731269598007202,
                               0.26262718439102173,
                               0.25217896699905396,
                               0.24179960787296295,
                               0.2315080612897873,
                               0.2213248461484909,
                               0.2112719565629959,
                               0.20137275755405426,
                               0.1916518211364746,
                               0.18213464319705963,
                               0.17284737527370453,
                               0.1638164073228836,
                               0.15506798028945923,
                               0.14662761986255646,
                               0.13851958513259888,
                               0.13076619803905487,
                               0.12338729947805405,
                               0.11639954894781113,
                               0.10981585085391998};

    for (uint_fast64_t i = 0; i < 41; i++) {
        ASSERT_NEAR(storm::utility::convertNumber<double>(walk[i].position[badCVar]), badCValues[i], 1e-4);
        ASSERT_NEAR(storm::utility::convertNumber<double>(walk[i].position[pfVar]), pfValues[i], 1e-4);
    }

    ASSERT_NEAR(storm::utility::convertNumber<double>(doubleInstantiation.second), 0, 1e-6);

    // Same thing with RAdam
    storm::derivative::GradientDescentInstantiationSearcher<typename TestFixture::FunctionType, typename TestFixture::ConstantType> radamChecker(
        *dtmc, storm::derivative::GradientDescentMethod::RADAM, 0.01, 0.9, 0.999, 2, 1e-6, std::nullopt,
        storm::derivative::GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT, std::nullopt, true);
    radamChecker.setup(this->env(), feasibilityTask);
    auto radamInstantiation = radamChecker.gradientDescent();
    auto radamWalk = radamChecker.getVisualizationWalk();

    const double badCValuesRadam[] = {0.5,
                                      0.49060654640197754,
                                      0.48096320033073425,
                                      0.47105303406715393,
                                      0.4608582556247711,
                                      0.46068474650382996,
                                      0.4604234993457794,
                                      0.460092693567276,
                                      0.4597006142139435,
                                      0.4592539370059967,
                                      0.4587569832801819,
                                      0.45821380615234375,
                                      0.4576275646686554,
                                      0.4570005536079407,
                                      0.45633500814437866,
                                      0.4556325674057007,
                                      0.45489493012428284,
                                      0.454123318195343,
                                      0.4533190429210663,
                                      0.45248323678970337,
                                      0.45161670446395874,
                                      0.4507202208042145,
                                      0.44979462027549744,
                                      0.448840469121933,
                                      0.44785845279693604,
                                      0.4468490481376648,
                                      0.4458127021789551,
                                      0.4447498917579651,
                                      0.44366100430488586,
                                      0.4425463378429413,
                                      0.4414062798023224,
                                      0.4402410686016083,
                                      0.4390510618686676,
                                      0.43783634901046753,
                                      0.43659722805023193,
                                      0.43533384799957275,
                                      0.4340464472770691,
                                      0.4327350854873657,
                                      0.43139997124671936,
                                      0.43004119396209717,
                                      0.4286588430404663};

    const double pfValuesRadam[] = {0.5,
                                    0.4985547959804535,
                                    0.4970662295818329,
                                    0.4955315589904785,
                                    0.4939480423927307,
                                    0.4937744438648224,
                                    0.4935130178928375,
                                    0.4931819438934326,
                                    0.49278953671455383,
                                    0.492342472076416,
                                    0.49184510111808777,
                                    0.4913014769554138,
                                    0.49071478843688965,
                                    0.4900873303413391,
                                    0.48942139744758606,
                                    0.4887186288833618,
                                    0.48798075318336487,
                                    0.4872090220451355,
                                    0.48640477657318115,
                                    0.48556917905807495,
                                    0.48470309376716614,
                                    0.4838073253631592,
                                    0.48288270831108093,
                                    0.48192986845970154,
                                    0.4809495508670807,
                                    0.4799422323703766,
                                    0.47890838980674744,
                                    0.4778485894203186,
                                    0.4767632484436035,
                                    0.4756527245044708,
                                    0.47451743483543396,
                                    0.4733576774597168,
                                    0.4721738398075104,
                                    0.470966100692749,
                                    0.4697347581386566,
                                    0.46848005056381226,
                                    0.4672022759914398,
                                    0.46590155363082886,
                                    0.46457815170288086,
                                    0.46323221921920776,
                                    0.4618639349937439};

    for (uint_fast64_t i = 0; i < 41; i++) {
        ASSERT_NEAR(storm::utility::convertNumber<double>(radamWalk[i].position[badCVar]), badCValuesRadam[i], 1e-5);
        ASSERT_NEAR(storm::utility::convertNumber<double>(radamWalk[i].position[pfVar]), pfValuesRadam[i], 1e-5);
    }

    // Same thing with momentum
    storm::derivative::GradientDescentInstantiationSearcher<typename TestFixture::FunctionType, typename TestFixture::ConstantType> momentumChecker(
        *dtmc, storm::derivative::GradientDescentMethod::MOMENTUM, 0.001, 0.9, 0.999, 2, 1e-6, std::nullopt,
        storm::derivative::GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT, std::nullopt, true);
    momentumChecker.setup(this->env(), feasibilityTask);
    auto momentumInstantiation = momentumChecker.gradientDescent();
    auto momentumWalk = momentumChecker.getVisualizationWalk();

    const double badCValuesMomentum[] = {
        0.5 + 1e-6,          0.4990617036819458,  0.4972723126411438,  0.4947088360786438,  0.4914357662200928,  0.4875074326992035,  0.48296919465065,
        0.4778585731983185,  0.47220611572265625, 0.4660361409187317,  0.45936742424964905, 0.45221376419067383, 0.44458451867103577, 0.43648505210876465,
        0.42791709303855896, 0.41887906193733215, 0.4093664884567261,  0.3993722200393677,  0.3888867497444153,  0.37789851427078247, 0.3663942813873291,
        0.3543594181537628,  0.34177839756011963, 0.32863524556159973, 0.3149142265319824,  0.30060049891471863, 0.28568127751350403, 0.27014678716659546,
        0.253991961479187,   0.23721818625926971, 0.2198355346918106,  0.201865553855896,   0.18334446847438812, 0.16432702541351318, 0.1448907107114792,
        0.12514044344425201, 0.10521329939365387, 0.08528289198875427, 0.06556269526481628, 0.04630732536315918, 0.027810536324977875};
    const double pfValuesMomentum[] = {
        0.5 + 1e-6,          0.49985647201538086, 0.49958109855651855, 0.4991863965988159,  0.49868202209472656, 0.49807605147361755, 0.49737516045570374,
        0.49658480286598206, 0.4957093894481659,  0.49475228786468506, 0.4937160611152649,  0.49260255694389343, 0.4914129376411438,  0.49014779925346375,
        0.4888072907924652,  0.4873911142349243,  0.4858987331390381,  0.4843292832374573,  0.4826817214488983,  0.4809550344944,     0.47914814949035645,
        0.47726020216941833, 0.47529059648513794, 0.4732392132282257,  0.4711065888404846,  0.4688941240310669,  0.4666043817996979,  0.46424129605293274,
        0.46181055903434753, 0.45931991934776306, 0.4567795991897583,  0.4542025327682495,  0.4516047239303589,  0.4490053057670593,  0.44642651081085205,
        0.4438934028148651,  0.44143298268318176, 0.43907299637794495, 0.43684011697769165, 0.43475744128227234, 0.43284183740615845};

    for (uint_fast64_t i = 0; i < 41; i++) {
        ASSERT_NEAR(storm::utility::convertNumber<double>(momentumWalk[i].position[badCVar]), badCValuesMomentum[i], 1e-5);
        ASSERT_NEAR(storm::utility::convertNumber<double>(momentumWalk[i].position[pfVar]), pfValuesMomentum[i], 1e-5);
    }

    // Same thing with nesterov
    storm::derivative::GradientDescentInstantiationSearcher<typename TestFixture::FunctionType, typename TestFixture::ConstantType> nesterovChecker(
        *dtmc, storm::derivative::GradientDescentMethod::NESTEROV, 0.001, 0.9, 0.999, 2, 1e-6, std::nullopt,
        storm::derivative::GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT, std::nullopt, true);
    nesterovChecker.setup(this->env(), feasibilityTask);
    auto nesterovInstantiation = nesterovChecker.gradientDescent();
    auto nesterovWalk = nesterovChecker.getVisualizationWalk();

    const double badCValuesNesterov[] = {
        0.5 + 1e-6,          0.49821633100509644, 0.49565380811691284, 0.4923747181892395,  0.48843076825141907,  0.4838651120662689,  0.4787132740020752,
        0.473004013299942,   0.4667600393295288,  0.45999863743782043, 0.452732115983963,   0.44496846199035645,  0.43671154975891113, 0.4279615581035614,
        0.4187155067920685,  0.4089673161506653,  0.3987082839012146,  0.387927383184433,   0.376611590385437,    0.36474621295928955, 0.35231542587280273,
        0.33930283784866333, 0.3256921172142029,  0.3114677667617798,  0.2966162860393524,  0.2811274528503418,   0.2649959325790405,  0.24822339415550232,
        0.23082087934017181, 0.21281197667121887, 0.19423632323741913, 0.1751537024974823,  0.15564869344234467,  0.1358354091644287,  0.11586219072341919,
        0.09591540694236755, 0.07622162997722626, 0.05704677850008011, 0.03869114816188812, 0.021478772163391113, 0.005740571767091751};
    const double pfValuesNesterov[] = {
        0.5 + 1e-6,          0.49972638487815857, 0.4993318021297455,  0.4988263249397278,  0.4982176721096039,  0.4975121319293976,  0.4967147409915924,
        0.4958295524120331,  0.4948597252368927,  0.4938075542449951,  0.49267470836639404, 0.49146217107772827, 0.49017035961151123, 0.48879921436309814,
        0.48734840750694275, 0.48581716418266296, 0.48420456051826477, 0.4825096130371094,  0.48073118925094604, 0.4788684844970703,  0.47692081332206726,
        0.4748881161212921,  0.4727708697319031,  0.47057047486305237, 0.4682896137237549,  0.4659323990345001,  0.4635048806667328,  0.4610152840614319,
        0.4584745764732361,  0.45589667558670044, 0.45329880714416504, 0.45070162415504456, 0.4481291174888611,  0.44560813903808594, 0.44316741824150085,
        0.440836101770401,   0.43864157795906067, 0.4366067945957184,  0.4347473978996277,  0.433069109916687,   0.43156614899635315};

    for (uint_fast64_t i = 0; i < 41; i++) {
        ASSERT_NEAR(storm::utility::convertNumber<double>(nesterovWalk[i].position[badCVar]), badCValuesNesterov[i], 1e-5);
        ASSERT_NEAR(storm::utility::convertNumber<double>(nesterovWalk[i].position[pfVar]), pfValuesNesterov[i], 1e-5);
    }
}
