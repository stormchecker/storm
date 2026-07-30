#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/api/storm-pars.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/storm.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/graph.h"

class MonotonicityCheckerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(MonotonicityCheckerTest, Simple1_larger_region) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/simple1.pm";
    std::string formulaAsString = "P=? [F s=3 ]";
    std::string constantsAsString = "";

    // model
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

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // For order extender
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<storm::RationalFunction>> propositionalChecker(*model);
    storm::storage::BitVector phiStates;
    storm::storage::BitVector psiStates;
    phiStates = storm::storage::BitVector(model->getTransitionMatrix().getRowCount(), true);
    storm::logic::EventuallyFormula formula = formulas[0]->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula();
    psiStates =
        propositionalChecker.check(formula.getSubformula())->template asExplicitQualitativeCheckResult<storm::RationalFunction>().getTruthValuesVector();
    // Get the maybeStates
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(model->getBackwardTransitions(), phiStates, psiStates);
    storm::storage::BitVector topStates = statesWithProbability01.second;
    storm::storage::BitVector bottomStates = statesWithProbability01.first;
    // OrderExtender
    storm::storage::SparseMatrix<storm::RationalFunction> matrix = model->getTransitionMatrix();
    auto orderExtender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, matrix);
    // Order
    auto order = std::get<0>(orderExtender.toOrder(region, nullptr));
    // monchecker
    auto monChecker = new storm::analysis::MonotonicityChecker<storm::RationalFunction>(model->getTransitionMatrix());

    // start testing
    auto var = modelParameters.begin();
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr, monChecker->checkLocalMonotonicity(order, 1, *var, region));
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Decr, monChecker->checkLocalMonotonicity(order, 2, *var, region));
}

TEST_F(MonotonicityCheckerTest, Simple1_small_region) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/simple1.pm";
    std::string formulaAsString = "P=? [F s=3 ]";
    std::string constantsAsString = "";

    // model
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

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.51<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // For order extender
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<storm::RationalFunction>> propositionalChecker(*model);
    storm::storage::BitVector phiStates;
    storm::storage::BitVector psiStates;
    phiStates = storm::storage::BitVector(model->getTransitionMatrix().getRowCount(), true);
    storm::logic::EventuallyFormula formula = formulas[0]->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula();
    psiStates =
        propositionalChecker.check(formula.getSubformula())->template asExplicitQualitativeCheckResult<storm::RationalFunction>().getTruthValuesVector();
    // Get the maybeStates
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(model->getBackwardTransitions(), phiStates, psiStates);
    storm::storage::BitVector topStates = statesWithProbability01.second;
    storm::storage::BitVector bottomStates = statesWithProbability01.first;
    // OrderExtender
    storm::storage::SparseMatrix<storm::RationalFunction> matrix = model->getTransitionMatrix();
    auto orderExtender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, matrix);
    // Order
    auto order = std::get<0>(orderExtender.toOrder(region, nullptr));
    // monchecker
    auto monChecker = new storm::analysis::MonotonicityChecker<storm::RationalFunction>(model->getTransitionMatrix());

    // start testing
    auto var = modelParameters.begin();
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr, monChecker->checkLocalMonotonicity(order, 0, *var, region));
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr, monChecker->checkLocalMonotonicity(order, 1, *var, region));
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Decr, monChecker->checkLocalMonotonicity(order, 2, *var, region));
}

TEST_F(MonotonicityCheckerTest, Casestudy1) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy1.pm";
    std::string formulaAsString = "P=? [F s=3 ]";
    std::string constantsAsString = "";

    // model
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

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // For order extender
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<storm::RationalFunction>> propositionalChecker(*model);
    storm::storage::BitVector phiStates;
    storm::storage::BitVector psiStates;
    phiStates = storm::storage::BitVector(model->getTransitionMatrix().getRowCount(), true);
    storm::logic::EventuallyFormula formula = formulas[0]->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula();
    psiStates =
        propositionalChecker.check(formula.getSubformula())->template asExplicitQualitativeCheckResult<storm::RationalFunction>().getTruthValuesVector();
    // Get the maybeStates
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(model->getBackwardTransitions(), phiStates, psiStates);
    storm::storage::BitVector topStates = statesWithProbability01.second;
    storm::storage::BitVector bottomStates = statesWithProbability01.first;
    // OrderExtender
    storm::storage::SparseMatrix<storm::RationalFunction> matrix = model->getTransitionMatrix();
    auto orderExtender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, matrix);
    // Order
    auto res = orderExtender.extendOrder(nullptr, region);
    auto order = std::get<0>(res);
    ASSERT_TRUE(order->getDoneBuilding());

    // monchecker
    auto monChecker = new storm::analysis::MonotonicityChecker<storm::RationalFunction>(matrix);

    // start testing
    auto var = modelParameters.begin();
    for (uint_fast64_t i = 0; i < 3; i++) {
        EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr,
                  monChecker->checkLocalMonotonicity(order, i, *var, region));
    }
}

TEST_F(MonotonicityCheckerTest, Casestudy2) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy2.pm";
    std::string formulaAsString = "P=? [F s=4 ]";
    std::string constantsAsString = "";

    // model
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

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.51<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // For order extender
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<storm::RationalFunction>> propositionalChecker(*model);
    storm::storage::BitVector phiStates;
    storm::storage::BitVector psiStates;
    phiStates = storm::storage::BitVector(model->getTransitionMatrix().getRowCount(), true);
    storm::logic::EventuallyFormula formula = formulas[0]->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula();
    psiStates =
        propositionalChecker.check(formula.getSubformula())->template asExplicitQualitativeCheckResult<storm::RationalFunction>().getTruthValuesVector();
    // Get the maybeStates
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(model->getBackwardTransitions(), phiStates, psiStates);
    storm::storage::BitVector topStates = statesWithProbability01.second;
    storm::storage::BitVector bottomStates = statesWithProbability01.first;
    // OrderExtender
    storm::storage::SparseMatrix<storm::RationalFunction> matrix = model->getTransitionMatrix();
    auto orderExtender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, matrix);
    // Order
    auto res = orderExtender.extendOrder(nullptr, region);
    auto order = std::get<0>(res);
    order->addRelation(1, 3);
    order->addRelation(3, 2);

    // monchecker
    auto monChecker = new storm::analysis::MonotonicityChecker<storm::RationalFunction>(matrix);

    // start testing
    auto var = modelParameters.begin();
    for (uint_fast64_t i = 0; i < 3; i++) {
        EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr,
                  monChecker->checkLocalMonotonicity(order, i, *var, region));
    }
}

TEST_F(MonotonicityCheckerTest, Casestudy3) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy3.pm";
    std::string formulaAsString = "P=? [F s=3 ]";
    std::string constantsAsString = "";

    // model
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

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1<=p<=0.9", modelParameters);
    std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> regions = {region};

    // For order extender
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<storm::RationalFunction>> propositionalChecker(*model);
    storm::storage::BitVector phiStates;
    storm::storage::BitVector psiStates;
    phiStates = storm::storage::BitVector(model->getTransitionMatrix().getRowCount(), true);
    storm::logic::EventuallyFormula formula = formulas[0]->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula();
    psiStates =
        propositionalChecker.check(formula.getSubformula())->template asExplicitQualitativeCheckResult<storm::RationalFunction>().getTruthValuesVector();
    // Get the maybeStates
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(model->getBackwardTransitions(), phiStates, psiStates);
    storm::storage::BitVector topStates = statesWithProbability01.second;
    storm::storage::BitVector bottomStates = statesWithProbability01.first;
    // OrderExtender
    storm::storage::SparseMatrix<storm::RationalFunction> matrix = model->getTransitionMatrix();
    auto orderExtender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, matrix);
    // Order
    auto res = orderExtender.extendOrder(nullptr, region);
    auto order = std::get<0>(res);
    ASSERT_TRUE(order->getDoneBuilding());

    // monchecker
    auto monChecker = new storm::analysis::MonotonicityChecker<storm::RationalFunction>(matrix);

    // start testing
    auto var = modelParameters.begin();
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Not, monChecker->checkLocalMonotonicity(order, 0, *var, region));
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr, monChecker->checkLocalMonotonicity(order, 1, *var, region));
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr, monChecker->checkLocalMonotonicity(order, 2, *var, region));

    region = storm::api::parseRegion<storm::RationalFunction>("0.51<=p<=0.9", modelParameters);
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Decr, monChecker->checkLocalMonotonicity(order, 0, *var, region));
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr, monChecker->checkLocalMonotonicity(order, 1, *var, region));
    EXPECT_EQ(storm::analysis::MonotonicityChecker<storm::RationalFunction>::Monotonicity::Incr, monChecker->checkLocalMonotonicity(order, 2, *var, region));
}
