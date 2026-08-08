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
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/graph.h"

class AssumptionCheckerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(AssumptionCheckerTest, Brp_no_bisimulation) {
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
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<storm::RationalFunction>>(*dtmc);
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();

    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(193ul, dtmc->getNumberOfStates());
    ASSERT_EQ(383ul, dtmc->getNumberOfTransitions());

    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= pK <= 0.00001, 0.00001 <= pL <= 0.99999", vars);

    auto checker = storm::analysis::AssumptionChecker<storm::RationalFunction, double>(dtmc->getTransitionMatrix());
    storm::storage::BitVector above(193);
    above.set(0);
    storm::storage::BitVector below(193);
    below.set(1);

    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(model->getTransitionMatrix(), options);

    auto statesSorted = storm::utility::graph::getTopologicalSort(model->getTransitionMatrix());
    auto dummyOrder = std::shared_ptr<storm::analysis::Order>(new storm::analysis::Order(above, below, 193, decomposition, statesSorted));

    storm::analysis::Assumption assumption{7, 5, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::UNKNOWN, checker.validateAssumption(assumption, dummyOrder, region));

    assumption = storm::analysis::Assumption{5, 7, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::UNKNOWN, checker.validateAssumption(assumption, dummyOrder, region));

    assumption = storm::analysis::Assumption{7, 5, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::UNKNOWN, checker.validateAssumption(assumption, dummyOrder, region));

    checker.initializeCheckingOnSamples(formulas[0], dtmc, region, 3);
    assumption = storm::analysis::Assumption{7, 5, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::UNKNOWN, checker.validateAssumption(assumption, dummyOrder, region));

    assumption = storm::analysis::Assumption{5, 7, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, dummyOrder, region));

    assumption = storm::analysis::Assumption{7, 5, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, dummyOrder, region));
}

TEST_F(AssumptionCheckerTest, Simple1) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/simple1.pm";
    std::string formulaAsString = "P=? [F s=3]";
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
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(5ul, dtmc->getNumberOfStates());
    ASSERT_EQ(8ul, dtmc->getNumberOfTransitions());

    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= p <= 0.99999", vars);

    auto checker = storm::analysis::AssumptionChecker<storm::RationalFunction, double>(dtmc->getTransitionMatrix());

    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(model->getTransitionMatrix(), options);

    storm::storage::BitVector above(5);
    above.set(3);
    storm::storage::BitVector below(5);
    below.set(4);
    auto statesSorted = storm::utility::graph::getTopologicalSort(model->getTransitionMatrix());
    auto order = std::shared_ptr<storm::analysis::Order>(new storm::analysis::Order(above, below, 5, decomposition, statesSorted));

    // Validating
    storm::analysis::Assumption assumption{1, 2, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{2, 1, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    region = storm::api::parseRegion<storm::RationalFunction>("0.51 <= p <= 0.99", vars);
    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{2, 1, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));
}

TEST_F(AssumptionCheckerTest, Casestudy1) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy1.pm";
    std::string formulaAsString = "P=? [F s=3]";
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
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(5ul, dtmc->getNumberOfStates());
    ASSERT_EQ(8ul, dtmc->getNumberOfTransitions());

    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= p <= 0.99999", vars);

    auto checker = storm::analysis::AssumptionChecker<storm::RationalFunction, double>(dtmc->getTransitionMatrix());

    storm::storage::BitVector above(5);
    above.set(3);
    storm::storage::BitVector below(5);
    below.set(4);

    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(model->getTransitionMatrix(), options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(model->getTransitionMatrix());
    auto order = std::shared_ptr<storm::analysis::Order>(new storm::analysis::Order(above, below, 5, decomposition, statesSorted));

    // Validating
    storm::analysis::Assumption assumption{1, 2, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{2, 1, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    checker.initializeCheckingOnSamples(formulas[0], dtmc, region, 3);
    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{2, 1, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));
}

TEST_F(AssumptionCheckerTest, Casestudy2) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy2.pm";
    std::string formulaAsString = "P=? [F s=4]";
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
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(6ul, dtmc->getNumberOfStates());
    ASSERT_EQ(12ul, dtmc->getNumberOfTransitions());

    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= p <= 0.99999", vars);

    auto checker = storm::analysis::AssumptionChecker<storm::RationalFunction, double>(dtmc->getTransitionMatrix());

    storm::storage::BitVector above(6);
    above.set(4);
    storm::storage::BitVector below(6);
    below.set(5);

    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(model->getTransitionMatrix(), options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(model->getTransitionMatrix());
    auto order = std::shared_ptr<storm::analysis::Order>(new storm::analysis::Order(above, below, 6, decomposition, statesSorted));
    order->add(3);

    // Checking on samples and validate
    storm::analysis::Assumption assumption{1, 2, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{2, 1, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));
}

TEST_F(AssumptionCheckerTest, Casestudy3) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/casestudy3.pm";
    std::string formulaAsString = "P=? [F s=3]";
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
    ASSERT_TRUE(simplifier.simplify(*(formulas[0])));
    model = simplifier.getSimplifiedModel();
    dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    ASSERT_EQ(5ul, dtmc->getNumberOfStates());
    ASSERT_EQ(8ul, dtmc->getNumberOfTransitions());

    // Create the region
    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);
    auto region = storm::api::parseRegion<storm::RationalFunction>("0.00001 <= p <= 0.4", vars);

    auto checker = storm::analysis::AssumptionChecker<storm::RationalFunction, double>(dtmc->getTransitionMatrix());

    // Order
    storm::storage::BitVector above(5);
    above.set(3);
    storm::storage::BitVector below(5);
    below.set(4);
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();
    auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<storm::RationalFunction>(model->getTransitionMatrix(), options);
    auto statesSorted = storm::utility::graph::getTopologicalSort(model->getTransitionMatrix());
    auto order = std::shared_ptr<storm::analysis::Order>(new storm::analysis::Order(above, below, 5, decomposition, statesSorted));

    storm::analysis::Assumption assumption{1, 2, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{2, 1, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    checker.initializeCheckingOnSamples(formulas[0], dtmc, region, 3);

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::VALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{2, 1, storm::expressions::RelationType::Greater};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));

    assumption = storm::analysis::Assumption{1, 2, storm::expressions::RelationType::Equal};
    EXPECT_EQ(storm::analysis::AssumptionStatus::INVALID, checker.validateAssumption(assumption, order, region));
}
