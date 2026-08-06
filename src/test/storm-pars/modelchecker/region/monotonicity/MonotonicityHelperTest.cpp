#include "storm-config.h"
#include "test/storm_gtest.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/util/stringparser.h>
#pragma clang diagnostic pop

#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/storm.h"

class MonotonicityHelperTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(MonotonicityHelperTest, Derivative_checker) {
    // Create the region
    typename storm::storage::ParameterRegion<storm::RationalFunction>::Valuation lowerBoundaries;
    typename storm::storage::ParameterRegion<storm::RationalFunction>::Valuation upperBoundaries;
    auto region = storm::storage::ParameterRegion<storm::RationalFunction>(std::move(lowerBoundaries), std::move(upperBoundaries));

    // Derivative 0
    auto constFunction = storm::RationalFunction(0);
    auto constFunctionRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(constFunction, region);
    EXPECT_TRUE(constFunctionRes.first);
    EXPECT_TRUE(constFunctionRes.second);

    // Derivative 5
    constFunction = storm::RationalFunction(5);
    constFunctionRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(constFunction, region);
    EXPECT_TRUE(constFunctionRes.first);
    EXPECT_FALSE(constFunctionRes.second);

    // Derivative -4
    constFunction = storm::RationalFunction(storm::RationalFunction(1) - constFunction);
    constFunctionRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(constFunction, region);
    EXPECT_FALSE(constFunctionRes.first);
    EXPECT_TRUE(constFunctionRes.second);

    std::shared_ptr<storm::RawPolynomialCache> cache = std::make_shared<storm::RawPolynomialCache>();
    carl::StringParser parser;
    parser.setVariables({"p", "q"});

    // Create the region
    auto functionP = storm::RationalFunction(storm::Polynomial(parser.template parseMultivariatePolynomial<storm::RationalFunctionCoefficient>("p"), cache));
    auto functionQ = storm::RationalFunction(storm::Polynomial(parser.template parseMultivariatePolynomial<storm::RationalFunctionCoefficient>("q"), cache));

    auto varsP = functionP.gatherVariables();
    auto varsQ = functionQ.gatherVariables();
    storm::utility::parametric::Valuation<storm::RationalFunction> lowerBoundaries2;
    storm::utility::parametric::Valuation<storm::RationalFunction> upperBoundaries2;
    for (auto var : varsP) {
        typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType lb =
            storm::utility::convertNumber<typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType>(0 + 0.000001);
        typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType ub =
            storm::utility::convertNumber<typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType>(1 - 0.000001);
        lowerBoundaries2.emplace(std::make_pair(var, lb));
        upperBoundaries2.emplace(std::make_pair(var, ub));
    }
    for (auto var : varsQ) {
        typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType lb =
            storm::utility::convertNumber<typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType>(0 + 0.000001);
        typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType ub =
            storm::utility::convertNumber<typename storm::storage::ParameterRegion<storm::RationalFunction>::CoefficientType>(1 - 0.000001);
        lowerBoundaries2.emplace(std::make_pair(var, lb));
        upperBoundaries2.emplace(std::make_pair(var, ub));
    }
    region = storm::storage::ParameterRegion<storm::RationalFunction>(std::move(lowerBoundaries2), std::move(upperBoundaries2));

    // Derivative p
    auto function = functionP;
    auto functionRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(function, region);
    EXPECT_TRUE(functionRes.first);
    EXPECT_FALSE(functionRes.second);

    // Derivative 1-p
    auto functionDecr = storm::RationalFunction(storm::RationalFunction(1) - function);
    auto functionDecrRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(functionDecr, region);
    EXPECT_TRUE(functionDecrRes.first);
    EXPECT_FALSE(functionDecrRes.second);

    // Derivative 1-2p
    auto functionNonMonotonic = storm::RationalFunction(storm::RationalFunction(1) - storm::RationalFunction(2) * function);
    auto functionNonMonotonicRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(functionNonMonotonic, region);
    EXPECT_FALSE(functionNonMonotonicRes.first);
    EXPECT_FALSE(functionNonMonotonicRes.second);

    // Derivative -p
    functionDecr = storm::RationalFunction(storm::RationalFunction(0) - function);
    functionDecrRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(functionDecr, region);
    EXPECT_FALSE(functionDecrRes.first);
    EXPECT_TRUE(functionDecrRes.second);

    // Derivative p*q
    function = functionP * functionQ;
    functionRes = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>::checkDerivative(function, region);
    EXPECT_TRUE(functionRes.first);
    EXPECT_FALSE(functionRes.second);
}

TEST_F(MonotonicityHelperTest, Brp_with_bisimulation_no_samples) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [true U s=4 & i=N ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Apply bisimulation
    storm::storage::BisimulationType bisimType = storm::storage::BisimulationType::Strong;
    if (storm::settings::getModule<storm::settings::modules::BisimulationSettings>().isWeakBisimulationSet()) {
        bisimType = storm::storage::BisimulationType::Weak;
    }

    model = storm::api::performBisimulationMinimization<storm::RationalFunction>(model, formulas, bisimType)
                ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    ASSERT_EQ(99ul, model->getNumberOfStates());
    ASSERT_EQ(195ul, model->getNumberOfTransitions());

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=pL<=0.9, 0.1<=pK<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // Start testing
    storm::analysis::MonotonicityHelper<storm::RationalFunction, double> MonotonicityHelper =
        storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, true);
    // Check if correct result size
    auto result = MonotonicityHelper.checkMonotonicityInBuild(std::cout, false);
    EXPECT_EQ(1ul, result.size());

    // Check if the order and general monotonicity result is correct.
    auto order = result.begin()->first;
    auto monotonicityResult = result.begin()->second.first;
    EXPECT_TRUE(monotonicityResult->isDone());
    EXPECT_TRUE(monotonicityResult->existsMonotonicity());
    EXPECT_TRUE(monotonicityResult->isAllMonotonicity());
    auto assumptions = result.begin()->second.second;
    EXPECT_EQ(0ul, assumptions.size());

    // Check if result for each variable is correct
    auto monRes = monotonicityResult->getMonotonicityResult();
    for (auto entry : monRes) {
        EXPECT_EQ(storm::analysis::MonotonicityResult<storm::RationalFunctionVariable>::Monotonicity::Incr, entry.second);
    }
}

TEST_F(MonotonicityHelperTest, Brp_with_bisimulation_samples) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [true U s=4 & i=N ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Apply bisimulation
    storm::storage::BisimulationType bisimType = storm::storage::BisimulationType::Strong;
    if (storm::settings::getModule<storm::settings::modules::BisimulationSettings>().isWeakBisimulationSet()) {
        bisimType = storm::storage::BisimulationType::Weak;
    }

    model = storm::api::performBisimulationMinimization<storm::RationalFunction>(model, formulas, bisimType)
                ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    ASSERT_EQ(99ul, model->getNumberOfStates());
    ASSERT_EQ(195ul, model->getNumberOfTransitions());

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=pL<=0.9, 0.1<=pK<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // Start testing
    storm::analysis::MonotonicityHelper<storm::RationalFunction, double> MonotonicityHelper =
        storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, true, 50);
    // Check if correct result size
    auto result = MonotonicityHelper.checkMonotonicityInBuild(std::cout, false);
    EXPECT_EQ(1ul, result.size());

    // Check if the order and general monotonicity result is correct.
    auto order = result.begin()->first;
    auto monotonicityResult = result.begin()->second.first;
    EXPECT_TRUE(monotonicityResult->isDone());
    EXPECT_TRUE(monotonicityResult->existsMonotonicity());
    EXPECT_TRUE(monotonicityResult->isAllMonotonicity());
    auto assumptions = result.begin()->second.second;
    EXPECT_EQ(0ul, assumptions.size());

    // Check if result for each variable is correct
    auto monRes = monotonicityResult->getMonotonicityResult();
    for (auto entry : monRes) {
        EXPECT_EQ(storm::analysis::MonotonicityResult<storm::RationalFunctionVariable>::Monotonicity::Incr, entry.second);
    }
}

TEST_F(MonotonicityHelperTest, zeroconf) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/zeroconf4.pm";
    std::string formulaAsString = "P > 0.5 [ F s=5 ]";
    std::string constantsAsString = "n = 4";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    storm::storage::BisimulationType bisimType = storm::storage::BisimulationType::Strong;
    if (storm::settings::getModule<storm::settings::modules::BisimulationSettings>().isWeakBisimulationSet()) {
        bisimType = storm::storage::BisimulationType::Weak;
    }

    model = storm::api::performBisimulationMinimization<storm::RationalFunction>(model, formulas, bisimType)
                ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    ASSERT_EQ(7ul, model->getNumberOfStates());
    ASSERT_EQ(12ul, model->getNumberOfTransitions());

    // Create region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=pL<=0.9, 0.1<=pK<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // Start testing
    auto MonotonicityHelper = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, 50);
    // Check if correct result size
    auto result = MonotonicityHelper.checkMonotonicityInBuild(std::cout, false);
    EXPECT_EQ(1ul, result.size());

    // Check if the order and general monotonicity result is correct.
    auto order = result.begin()->first;
    auto monotonicityResult = result.begin()->second.first;
    EXPECT_TRUE(monotonicityResult->isDone());
    EXPECT_TRUE(monotonicityResult->existsMonotonicity());
    EXPECT_TRUE(monotonicityResult->isAllMonotonicity());
    // TODO @Jip we have 1 assumption instead of 0 here
    auto assumptions = result.begin()->second.second;
    EXPECT_EQ(0ul, assumptions.size());

    // Check if result for each variable is correct
    auto monRes = monotonicityResult->getMonotonicityResult();
    for (auto entry : monRes) {
        EXPECT_EQ(storm::analysis::MonotonicityResult<storm::RationalFunctionVariable>::Monotonicity::Incr, entry.second);
    }
}

TEST_F(MonotonicityHelperTest, Simple1) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/simple1.pm";
    std::string formulaAsString = "P > 0.5 [ F s=3 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    ASSERT_EQ(5ul, model->getNumberOfStates());
    ASSERT_EQ(8ul, model->getNumberOfTransitions());

    // Create region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.49", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // Start testing
    auto MonotonicityHelper = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, 10);

    // Check if correct result size
    auto result = MonotonicityHelper.checkMonotonicityInBuild(std::cout, false);
    EXPECT_EQ(1ul, result.size());

    // Check if the order and general monotonicity result is correct.
    auto order = result.begin()->first;
    auto monotonicityResult = result.begin()->second.first;
    EXPECT_TRUE(monotonicityResult->isDone());
    EXPECT_FALSE(monotonicityResult->existsMonotonicity());
    EXPECT_FALSE(monotonicityResult->isAllMonotonicity());
    auto assumptions = result.begin()->second.second;
    EXPECT_EQ(0ul, assumptions.size());

    // Check if result for each variable is correct
    auto monRes = monotonicityResult->getMonotonicityResult();
    for (auto entry : monRes) {
        EXPECT_EQ(storm::analysis::MonotonicityResult<storm::RationalFunctionVariable>::Monotonicity::Unknown, entry.second);
    }
}

TEST_F(MonotonicityHelperTest, Casestudy1) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy1.pm";
    std::string formulaAsString = "P > 0.5 [ F s=3 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    ASSERT_EQ(5ul, model->getNumberOfStates());
    ASSERT_EQ(8ul, model->getNumberOfTransitions());

    auto MonotonicityHelper = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, 10);
    auto result = MonotonicityHelper.checkMonotonicityInBuild(std::cout, false);
    ASSERT_EQ(1ul, result.size());

    auto order = result.begin()->first;
    auto monotonicityResult = result.begin()->second.first;
    EXPECT_TRUE(monotonicityResult->isDone());
    EXPECT_TRUE(monotonicityResult->existsMonotonicity());
    EXPECT_TRUE(monotonicityResult->isAllMonotonicity());
    auto assumptions = result.begin()->second.second;
    EXPECT_EQ(0ul, assumptions.size());

    auto monRes = monotonicityResult->getMonotonicityResult();
    for (auto entry : monRes) {
        EXPECT_EQ(storm::analysis::MonotonicityResult<storm::RationalFunctionVariable>::Monotonicity::Incr, entry.second);
    }
}

TEST_F(MonotonicityHelperTest, CaseStudy2) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy2.pm";
    std::string formulaAsString = "P > 0.5 [ F s=4 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    ASSERT_EQ(6ul, model->getNumberOfStates());
    ASSERT_EQ(12ul, model->getNumberOfTransitions());

    // Start testing
    auto monotonicityHelper = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, 10);

    // Check if correct result size
    auto result = monotonicityHelper.checkMonotonicityInBuild(std::cout, false);
    EXPECT_EQ(1ul, result.size());
    EXPECT_FALSE(result.begin()->first->getDoneBuilding());
}

TEST_F(MonotonicityHelperTest, Casestudy3_not_monotone) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy3.pm";
    std::string formulaAsString = "P > 0.5 [ F s=3 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    ASSERT_EQ(5ul, model->getNumberOfStates());
    ASSERT_EQ(8ul, model->getNumberOfTransitions());

    auto MonotonicityHelper = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, 10);
    auto result = MonotonicityHelper.checkMonotonicityInBuild(std::cout, false);

    ASSERT_EQ(1ul, result.size());
    auto order = result.begin()->first;

    auto monotonicityResult = result.begin()->second.first;
    EXPECT_TRUE(monotonicityResult->isDone());
    EXPECT_FALSE(monotonicityResult->existsMonotonicity());
    EXPECT_FALSE(monotonicityResult->isAllMonotonicity());
    auto assumptions = result.begin()->second.second;
    EXPECT_EQ(0ul, assumptions.size());

    auto monRes = monotonicityResult->getMonotonicityResult();
    for (auto entry : monRes) {
        EXPECT_EQ(storm::analysis::MonotonicityResult<storm::RationalFunctionVariable>::Monotonicity::Unknown, entry.second);
    }
}

TEST_F(MonotonicityHelperTest, Casestudy3_monotone) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy3.pm";
    std::string formulaAsString = "P > 0.5 [ F s=3 ]";
    std::string constantsAsString = "";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.49", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    ASSERT_EQ(5ul, model->getNumberOfStates());
    ASSERT_EQ(8ul, model->getNumberOfTransitions());

    auto MonotonicityHelper = storm::analysis::MonotonicityHelper<storm::RationalFunction, double>(model, formulas, regions, 10);
    auto result = MonotonicityHelper.checkMonotonicityInBuild(std::cout, false);

    ASSERT_EQ(1ul, result.size());
    auto order = result.begin()->first;

    auto monotonicityResult = result.begin()->second.first;
    EXPECT_TRUE(monotonicityResult->isDone());
    EXPECT_TRUE(monotonicityResult->existsMonotonicity());
    EXPECT_TRUE(monotonicityResult->isAllMonotonicity());
    auto assumptions = result.begin()->second.second;
    EXPECT_EQ(0ul, assumptions.size());

    auto monRes = monotonicityResult->getMonotonicityResult();
    for (auto entry : monRes) {
        EXPECT_EQ(storm::analysis::MonotonicityResult<storm::RationalFunctionVariable>::Monotonicity::Incr, entry.second);
    }
}
