#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/api/storm-pars.h"
#include "storm-pars/derivative/SparseDerivativeInstantiationModelChecker.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/logic/Formulas.h"

namespace {
class RationalGmmxxEnvironment {
   public:
    typedef storm::RationalFunction ValueType;
    typedef storm::RationalNumber ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        return env;
    }
};
class DoubleGmmxxEnvironment {
   public:
    typedef storm::RationalFunction ValueType;
    typedef double ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        return env;
    }
};
class RationalEigenEnvironment {
   public:
    typedef storm::RationalFunction ValueType;
    typedef storm::RationalNumber ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        return env;
    }
};
class DoubleEigenEnvironment {
   public:
    typedef storm::RationalFunction ValueType;
    typedef double ConstantType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        return env;
    }
};
template<typename TestType>
class SparseDerivativeInstantiationModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename TestType::ConstantType ConstantType;
    template<typename ValueType>
    using VariableType = typename storm::utility::parametric::VariableType<ValueType>::type;
    template<typename ValueType>
    using CoefficientType = typename storm::utility::parametric::CoefficientType<ValueType>::type;
    template<typename ValueType>
    using Instantiation = std::map<VariableType<storm::RationalFunction>, CoefficientType<storm::RationalFunction>>;
    template<typename ValueType>
    using ResultMap = std::map<VariableType<storm::RationalFunction>, ConstantType>;
    SparseDerivativeInstantiationModelCheckerTest() : _environment(TestType::createEnvironment()) {}
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
    void testModel(std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc,
                   std::vector<std::shared_ptr<const storm::logic::Formula>> formulas, storm::RationalFunction reachabilityFunction);

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<
#ifdef STORM_HAVE_GMM
    RationalGmmxxEnvironment, DoubleGmmxxEnvironment,
#endif
    RationalEigenEnvironment, DoubleEigenEnvironment>
    TestingTypes;
}  // namespace

TYPED_TEST_SUITE(SparseDerivativeInstantiationModelCheckerTest, TestingTypes, );

template<typename TestType>
void SparseDerivativeInstantiationModelCheckerTest<TestType>::testModel(std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc,
                                                                        std::vector<std::shared_ptr<const storm::logic::Formula>> formulas,
                                                                        storm::RationalFunction reachabilityFunction) {
    auto formulaWithoutBound = std::make_shared<storm::logic::ProbabilityOperatorFormula>(
        formulas[0]->asProbabilityOperatorFormula().getSubformula().asSharedPointer(), storm::logic::OperatorInformation(boost::none, boost::none));

    auto parameters = storm::models::sparse::getProbabilityParameters(*dtmc);
    storm::derivative::SparseDerivativeInstantiationModelChecker<storm::RationalFunction, typename TestType::ConstantType> derivativeModelChecker(*dtmc);

    std::map<VariableType<storm::RationalFunction>, storm::RationalFunction> derivatives;
    for (auto const& parameter : parameters) {
        derivatives[parameter] = reachabilityFunction.derivative(parameter);
    }

    // Generate test cases.
    std::vector<Instantiation<storm::RationalFunction>> testInstantiations;
    Instantiation<storm::RationalFunction> emptyInstantiation;
    testInstantiations.push_back(emptyInstantiation);
    for (auto const& param : parameters) {
        std::vector<Instantiation<storm::RationalFunction>> newInstantiations;
        for (auto point : testInstantiations) {
            for (typename TestType::ConstantType x = storm::utility::convertNumber<ConstantType>(1e-6); x <= 1;
                 x += (1 - storm::utility::convertNumber<ConstantType>(1e-6)) / 10) {
                std::map<VariableType<storm::RationalFunction>, CoefficientType<storm::RationalFunction>> newMap(point);
                newMap[param] = storm::utility::convertNumber<CoefficientType<storm::RationalFunction>>(x);
                newInstantiations.push_back(newMap);
            }
        }
        testInstantiations = newInstantiations;
    }

    // The test cases we are going to study. Left are the actual instantiations, right are the maps
    // for the results (which happen to share the same type).
    std::map<Instantiation<storm::RationalFunction>, ResultMap<storm::RationalFunction>> testCases;
    for (auto const& instantiation : testInstantiations) {
        ResultMap<storm::RationalFunction> resultMap;
        for (auto const& entry : instantiation) {
            auto parameter = entry.first;
            auto derivativeWrtParameter = derivatives[parameter];
            resultMap[parameter] = storm::utility::parametric::evaluate<typename TestType::ConstantType>(derivativeWrtParameter, instantiation);
        }
        testCases[instantiation] = resultMap;
    }

    auto checkTask = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formulaWithoutBound);
    derivativeModelChecker.specifyFormula(env(), checkTask);

    for (auto const& testCase : testCases) {
        Instantiation<ValueType> instantiation = testCase.first;
        for (auto const& position : instantiation) {
            auto parameter = position.first;
            auto parameterValue = position.second;
            auto expectedResult = testCase.second.at(parameter);

            auto derivative = derivativeModelChecker.check(env(), instantiation, parameter);
            ASSERT_NEAR(storm::utility::convertNumber<double>(derivative->getValueVector()[0]), storm::utility::convertNumber<double>(expectedResult), 1e-6)
                << instantiation;
        }
    }
}

// A very simple DTMC
TYPED_TEST(SparseDerivativeInstantiationModelCheckerTest, Simple) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/gradient1.pm";
    std::string formulaAsString = "Pmax=? [F s=2]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // We have to create the dtmc and formulas here, because we need its parameters to create the polynomial
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*dtmc);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // The associated polynomial. In this case, it's p * (1 - p).
    carl::Variable varP = carl::VariablePool::getInstance().findVariableWithName("p");
    std::shared_ptr<storm::RawPolynomialCache> cache = std::make_shared<storm::RawPolynomialCache>();
    auto p = storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(varP), cache));
    storm::RationalFunction reachabilityFunction = p * (storm::RationalFunction(1) - p);

    this->testModel(dtmc, formulas, reachabilityFunction);
}

// A very simple DTMC with two parameters
TYPED_TEST(SparseDerivativeInstantiationModelCheckerTest, Simple2) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/gradient2.pm";
    std::string formulaAsString = "Pmax=? [F s=2]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // We have to create the dtmc and formulas here, because we need its parameters to create the polynomial
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*dtmc);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // The associated polynomial. In this case, it's p * (1 - q).
    carl::Variable varP = carl::VariablePool::getInstance().findVariableWithName("p");
    carl::Variable varQ = carl::VariablePool::getInstance().findVariableWithName("q");
    std::shared_ptr<storm::RawPolynomialCache> cache = std::make_shared<storm::RawPolynomialCache>();
    auto p = storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(varP), cache));
    auto q = storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(varQ), cache));
    storm::RationalFunction reachabilityFunction = p * (storm::RationalFunction(1) - q);

    this->testModel(dtmc, formulas, reachabilityFunction);
}

// The bounded retransmission protocol
TYPED_TEST(SparseDerivativeInstantiationModelCheckerTest, Brp162) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "Pmax=? [F s=4 & i=N ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // We have to create the dtmc and formulas here, because we need its parameters to create the polynomial
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*dtmc);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    carl::Variable pLVar = carl::VariablePool::getInstance().findVariableWithName("pL");
    carl::Variable pKVar = carl::VariablePool::getInstance().findVariableWithName("pK");
    std::shared_ptr<storm::RawPolynomialCache> cache = std::make_shared<storm::RawPolynomialCache>();
    auto pL = storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(pLVar), cache));
    auto pK = storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(pKVar), cache));

    // The term is ((pK)^16 * (pL)^16 * (pK^2*pL^2+(-3)*pK*pL+3)^16)/(1), so we're just going to create this here.
    auto firstTerm = pK * pK * pK * pK * pK * pK * pK * pK * pK * pK * pK * pK * pK * pK * pK * pK;
    auto secondTerm = pL * pL * pL * pL * pL * pL * pL * pL * pL * pL * pL * pL * pL * pL * pL * pL;
    auto thirdTermUnpowed = pK * pK * pL * pL + (-3) * pK * pL + 3;
    auto thirdTerm = thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed *
                     thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed * thirdTermUnpowed *
                     thirdTermUnpowed * thirdTermUnpowed;
    storm::RationalFunction reachabilityFunction = firstTerm * secondTerm * thirdTerm;

    this->testModel(dtmc, formulas, reachabilityFunction);
}
