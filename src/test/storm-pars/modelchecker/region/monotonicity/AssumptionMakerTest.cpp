#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/monotonicity/AssumptionChecker.h"
#include "storm-pars/modelchecker/region/monotonicity/AssumptionMaker.h"
#include "storm-pars/modelchecker/region/monotonicity/LocalMonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderExtender.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/graph.h"

class AssumptionMakerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(AssumptionMakerTest, Brp_without_bisimulation) {
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
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    model = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= pK <= 0.999999, 0.00001 <= pL <= 0.999999", vars);

    ASSERT_EQ(193ul, model->getNumberOfStates());
    ASSERT_EQ(383ul, model->getNumberOfTransitions());

    auto *extender = new storm::analysis::OrderExtender<storm::RationalFunction, double>(model, formulas[0]);
    auto criticalTuple = extender->toOrder(region, nullptr);
    ASSERT_EQ(183ul, std::get<1>(criticalTuple));
    ASSERT_EQ(186ul, std::get<2>(criticalTuple));

    auto assumptionMaker = storm::analysis::AssumptionMaker<storm::RationalFunction, double>(model->getTransitionMatrix());
    auto result = assumptionMaker.createAndCheckAssumptions(std::get<1>(criticalTuple), std::get<2>(criticalTuple), std::get<0>(criticalTuple), region);

    EXPECT_EQ(3ul, result.size());

    for (auto res : result) {
        EXPECT_EQ(storm::analysis::AssumptionStatus::UNKNOWN, res.second);
        EXPECT_EQ(true, res.first->getFirstOperand()->isVariable());
        EXPECT_EQ(true, res.first->getSecondOperand()->isVariable());
    }

    assumptionMaker.initializeCheckingOnSamples(formulas[0], model, region, 10);
    result = assumptionMaker.createAndCheckAssumptions(std::get<1>(criticalTuple), std::get<2>(criticalTuple), std::get<0>(criticalTuple), region);
    EXPECT_EQ(1ul, result.size());
    auto itr = result.begin();
    EXPECT_EQ(storm::analysis::AssumptionStatus::UNKNOWN, itr->second);
    EXPECT_EQ(true, itr->first->getFirstOperand()->isVariable());
    EXPECT_EQ(true, itr->first->getSecondOperand()->isVariable());
    EXPECT_EQ("186", itr->first->getFirstOperand()->asVariableExpression().getVariable().getName());
    EXPECT_EQ("183", itr->first->getSecondOperand()->asVariableExpression().getVariable().getName());
    EXPECT_EQ(storm::expressions::RelationType::Greater, itr->first->getRelationType());
}

TEST_F(AssumptionMakerTest, Simple1) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/simple1.pm";
    std::string formulaAsString = "P=? [F s=3 ]";
    std::string constantsAsString = "";

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= p <= 0.999999", vars);

    ASSERT_EQ(5ul, model->getNumberOfStates());
    ASSERT_EQ(8ul, model->getNumberOfTransitions());

    storm::storage::BitVector above(5);
    above.set(3);
    storm::storage::BitVector below(5);
    below.set(4);
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(model->getTransitionMatrix(), options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(model->getTransitionMatrix());
    auto order = std::shared_ptr<storm::analysis::Order>(new storm::analysis::Order(above, below, 5, decomposition, statesSorted));

    auto assumptionMaker = storm::analysis::AssumptionMaker<storm::RationalFunction, double>(model->getTransitionMatrix());
    auto result = assumptionMaker.createAndCheckAssumptions(1, 2, order, region);
    EXPECT_EQ(0ul, result.size());
    assumptionMaker.initializeCheckingOnSamples(formulas[0], model, region, 10);
    result = assumptionMaker.createAndCheckAssumptions(1, 2, order, region);
    EXPECT_EQ(0ul, result.size());

    region = storm::api::parseRegion<storm::RationalFunction>("0.500001 <= p <= 0.999999", vars);
    std::vector<std::vector<double>> samples;
    assumptionMaker.setSampleValues(samples);
    result = assumptionMaker.createAndCheckAssumptions(1, 2, order, region);
    EXPECT_EQ(1ul, result.size());
    auto itr = result.begin();
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, itr->second);
    EXPECT_EQ(true, itr->first->getFirstOperand()->isVariable());
    EXPECT_EQ(true, itr->first->getSecondOperand()->isVariable());
    EXPECT_EQ("1", itr->first->getFirstOperand()->asVariableExpression().getVariable().getName());
    EXPECT_EQ("2", itr->first->getSecondOperand()->asVariableExpression().getVariable().getName());
    EXPECT_EQ(storm::expressions::RelationType::Greater, itr->first->getRelationType());
}

TEST_F(AssumptionMakerTest, Casestudy1) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy1.pm";
    std::string formulaAsString = "P=? [F s=3 ]";
    std::string constantsAsString = "";

    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*model);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    model = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*model);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= p <= 0.999999", vars);

    ASSERT_EQ(5ul, model->getNumberOfStates());
    ASSERT_EQ(8ul, model->getNumberOfTransitions());

    storm::storage::BitVector above(5);
    above.set(3);
    storm::storage::BitVector below(5);
    below.set(4);

    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(model->getTransitionMatrix(), options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(model->getTransitionMatrix());
    auto order = std::shared_ptr<storm::analysis::Order>(new storm::analysis::Order(above, below, 5, decomposition, statesSorted));

    auto assumptionMaker = storm::analysis::AssumptionMaker<storm::RationalFunction, double>(model->getTransitionMatrix());
    auto result = assumptionMaker.createAndCheckAssumptions(1, 2, order, region);

    EXPECT_EQ(1ul, result.size());
    auto itr = result.begin();
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, itr->second);
    EXPECT_EQ(true, itr->first->getFirstOperand()->isVariable());
    EXPECT_EQ(true, itr->first->getSecondOperand()->isVariable());
    EXPECT_EQ("1", itr->first->getFirstOperand()->asVariableExpression().getVariable().getName());
    EXPECT_EQ("2", itr->first->getSecondOperand()->asVariableExpression().getVariable().getName());
    EXPECT_EQ(storm::expressions::RelationType::Greater, itr->first->getRelationType());
}
