#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <vector>

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderExtender.h"
#include "storm-pars/transformer/SparseParametricModelSimplifier.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/bisimulation.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/graph.h"

class OrderExtenderTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(OrderExtenderTest, Brp_with_bisimulation_on_model) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [F s=4 & i=N ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Apply bisimulation
    storm::storage::BisimulationType bisimType = storm::storage::BisimulationType::Strong;

    model = storm::api::performBisimulationMinimization<storm::RationalFunction>(model, formulas, bisimType)
                ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(99ul, model->getNumberOfStates());
    ASSERT_EQ(195ul, model->getNumberOfTransitions());

    auto vars = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= pK <= 0.999999, 0.00001 <= pL <= 0.999999", vars);

    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(model, formulas[0]);
    auto monRes = new storm::analysis::MonotonicityResult<typename storm::analysis::OrderExtender<storm::RationalFunction, double>::VariableType>;
    auto criticalTuple = extender.toOrder(
        region,
        std::make_shared<storm::analysis::MonotonicityResult<typename storm::analysis::OrderExtender<storm::RationalFunction, double>::VariableType>>(*monRes));
    EXPECT_EQ(model->getNumberOfStates(), std::get<1>(criticalTuple));
    EXPECT_EQ(model->getNumberOfStates(), std::get<2>(criticalTuple));

    auto order = std::get<0>(criticalTuple);
    for (uint_fast64_t i = 0; i < model->getNumberOfStates(); ++i) {
        EXPECT_TRUE((order->contains(i)));
    }

    // Check on some nodes
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(5, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(94, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order->compare(7, 13));
}

TEST_F(OrderExtenderTest, Brp_without_bisimulation_on_model) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [F s=4 & i=N ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    ASSERT_EQ(193ul, model->getNumberOfStates());
    ASSERT_EQ(383ul, model->getNumberOfTransitions());

    auto vars = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= pK <= 0.999999, 0.00001 <= pL <= 0.999999", vars);

    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(model, formulas[0]);
    auto monRes = new storm::analysis::MonotonicityResult<typename storm::analysis::OrderExtender<storm::RationalFunction, double>::VariableType>;
    auto criticalTuple = extender.toOrder(
        region,
        std::make_shared<storm::analysis::MonotonicityResult<typename storm::analysis::OrderExtender<storm::RationalFunction, double>::VariableType>>(*monRes));
    EXPECT_EQ(183ul, std::get<1>(criticalTuple));
    EXPECT_EQ(186ul, std::get<2>(criticalTuple));
}

TEST_F(OrderExtenderTest, Brp_with_bisimulation_on_matrix) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [F s=4 & i=N ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Apply bisimulation
    storm::storage::BisimulationType bisimType = storm::storage::BisimulationType::Strong;

    model = storm::api::performBisimulationMinimization<storm::RationalFunction>(model, formulas, bisimType)
                ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(99ul, model->getNumberOfStates());
    ASSERT_EQ(195ul, model->getNumberOfTransitions());

    auto vars = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= pK <= 0.999999, 0.00001 <= pL <= 0.999999", vars);
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

    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, model->getTransitionMatrix());
    auto res = extender.extendOrder(nullptr, region);
    auto order = std::get<0>(res);
    EXPECT_EQ(order->getNumberOfAddedStates(), model->getNumberOfStates());
    EXPECT_TRUE(order->getDoneBuilding());

    // Check on some nodes
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(5, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(94, 5));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::UNKNOWN, order->compare(7, 13));
}

TEST_F(OrderExtenderTest, Brp_without_bisimulation_on_matrix) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [F s=4 & i=N ]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    ASSERT_EQ(193ul, model->getNumberOfStates());
    ASSERT_EQ(383ul, model->getNumberOfTransitions());

    auto vars = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= pK <= 0.999999, 0.00001 <= pL <= 0.999999", vars);
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

    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, model->getTransitionMatrix());
    auto res = extender.extendOrder(nullptr, region);
    auto order = std::get<0>(res);
    EXPECT_FALSE(order->getDoneBuilding());
}

TEST_F(OrderExtenderTest, simple1_on_model) {
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
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.51<=p<=0.9", modelParameters);

    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(model, formulas[0]);
    auto order = std::get<0>(extender.toOrder(region));
    EXPECT_EQ(5ul, order->getNumberOfAddedStates());
    EXPECT_TRUE(order->getDoneBuilding());

    // Check on all states
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(2, 4));
}

TEST_F(OrderExtenderTest, simple1_on_matrix) {
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
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.51 <= p <= 0.9", modelParameters);

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
    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, model->getTransitionMatrix());
    auto res = extender.extendOrder(nullptr, region);
    auto order = std::get<0>(res);
    EXPECT_EQ(order->getNumberOfAddedStates(), model->getNumberOfStates());
    EXPECT_TRUE(order->getDoneBuilding());

    // Check on all states, as this one automatically handles assumptions (if there is one valid) all are ABOVE
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(2, 4));
}

TEST_F(OrderExtenderTest, casestudy1_on_model) {
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
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.51<=p<=0.9", modelParameters);

    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(model, formulas[0]);
    auto order = std::get<0>(extender.toOrder(region));

    EXPECT_EQ(5ul, order->getNumberOfAddedStates());
    EXPECT_TRUE(order->getDoneBuilding());
    // Check on all states
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(2, 4));
}

TEST_F(OrderExtenderTest, casestudy1_on_matrix) {
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
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.51 <= p <= 0.9", modelParameters);

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
    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, model->getTransitionMatrix());
    auto res = extender.extendOrder(nullptr, region);
    auto order = std::get<0>(res);
    EXPECT_EQ(order->getNumberOfAddedStates(), model->getNumberOfStates());
    EXPECT_TRUE(order->getDoneBuilding());

    // Check on all states
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 1));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(3, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 0));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(1, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 2));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(0, 4));
    EXPECT_EQ(storm::analysis::Order::NodeComparison::ABOVE, order->compare(2, 4));
}

TEST_F(OrderExtenderTest, casestudy2_on_matrix) {
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
    auto simplifier = storm::transformer::SparseParametricModelSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create the region
    auto modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.1 <= p <= 0.2", modelParameters);

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
    auto extender = storm::analysis::OrderExtender<storm::RationalFunction, double>(topStates, bottomStates, model->getTransitionMatrix());
    auto res = extender.extendOrder(nullptr, region);
    EXPECT_TRUE(std::get<0>(res)->getDoneBuilding());
}
