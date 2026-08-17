#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/JaniParser.h"
#include "storm/api/storm.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/environment/Environment.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ModelFeatures.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/localeliminator/EliminateAction.h"
#include "storm/storage/jani/localeliminator/JaniLocalEliminator.h"
#include "storm/storage/jani/localeliminator/RebuildWithoutUnreachableAction.h"
#include "storm/storage/jani/localeliminator/UnfoldAction.h"

typedef storm::models::sparse::Dtmc<double> Dtmc;
typedef storm::models::sparse::Mdp<double> Mdp;
typedef storm::modelchecker::SparseDtmcPrctlModelChecker<Dtmc> DtmcModelChecker;
typedef storm::modelchecker::SparseMdpPrctlModelChecker<Dtmc> MdpModelChecker;

using storm::jani::JaniLocalEliminator;
using namespace storm::jani::elimination_actions;

// The tests are grouped into these groups
// * Input behaviour
// * Common functionality
// * Unfolding
// * Elimination
// * Rebuilding without unreachable locations
// Input behaviour tests whether the basic public interface of the eliminator works correctly, whereas
// common functionality mainly refers to internal functions that are used in multiple places.

// **************
// Helper functions
// **************

class JaniLocalElimination : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

// As many tests rely on building and checking a model (which requires quite a few lines of code), we use this common
// helper function to do the model checking. Apart from this, the tests don't share any helper functions.
void checkModel(storm::jani::Model model, std::vector<storm::jani::Property> properties,
                std::map<storm::expressions::Variable, storm::expressions::Expression> consts, double expectedValue) {
    model = model.defineUndefinedConstants(consts);
    properties[0] = properties[0].substitute(consts);

    auto formulae = storm::api::extractFormulasFromProperties(properties);
    storm::builder::BuilderOptions options(formulae, model);
    options.setBuildAllLabels(true);
    auto explicitModel = storm::api::buildSparseModel<double>(model, options)->template as<Dtmc>();

    auto task = storm::modelchecker::CheckTask<>(*(formulae[0]), true);
    storm::Environment env;
    auto checkResult = storm::api::verifyWithSparseEngine<double>(env, explicitModel, task);
    auto quantResult = checkResult->asExplicitQuantitativeCheckResult<double>();

    auto initialStates = explicitModel->getInitialStates();
    EXPECT_EQ(1u, initialStates.getNumberOfSetBits());
    for (auto state = initialStates.begin(); state != initialStates.end(); ++state) {
        EXPECT_NEAR(expectedValue, quantResult[*state], storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
    }
}

// *******************************
// Tests for input behaviour:
// *******************************

// This test verifies that an error is produced if the type of property is not supported and that no error is thrown
// when a supported property type is provided.
TEST_F(JaniLocalElimination, PropertyTypeTest) {
    // Load a model (the model contains two variables x and y, but doesn't do anything with them).
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/do_nothing.jani", storm::jani::getAllKnownModelFeatures(), boost::none).first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());

    {
        // This should fail because we only support F and R as top-level operators
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("S=? [ true ]");
        auto property = storm::jani::Property("steady_state", formula, std::set<storm::expressions::Variable>());
        STORM_SILENT_EXPECT_THROW(JaniLocalEliminator(model, property), storm::exceptions::NotSupportedException);
    }
    {
        // This should fail because we only support reachability formulas
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [G x=1]");
        auto property = storm::jani::Property("generally", formula, std::set<storm::expressions::Variable>());
        STORM_SILENT_EXPECT_THROW(JaniLocalEliminator(model, property), storm::exceptions::NotSupportedException);
    }
    {
        // This should fail because we only support reachability formulas (this test is mainly there because of the
        // similarity between eventually and until formulas)
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [x=1 U y=1]");
        auto property = storm::jani::Property("until", formula, std::set<storm::expressions::Variable>());
        STORM_SILENT_EXPECT_THROW(JaniLocalEliminator(model, property), storm::exceptions::NotSupportedException);
    }
    {
        // This should succeed because reachability probabilities are supported
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [ F x=1 ]");
        auto property = storm::jani::Property("reachability", formula, std::set<storm::expressions::Variable>());
        EXPECT_NO_THROW(JaniLocalEliminator(model, property));
    }
    {
        // This should succeed because reachability probabilities are supported
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("R=? [ F x=1 ]");
        auto property = storm::jani::Property("reward_reachability", formula, std::set<storm::expressions::Variable>());
        EXPECT_NO_THROW(JaniLocalEliminator(model, property));
    }
}

// This test verifies that an error is given if no properties are provided.
TEST_F(JaniLocalElimination, NoPropertiesTest) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/do_nothing.jani", storm::jani::getAllKnownModelFeatures(), boost::none).first;
    std::vector<storm::jani::Property> empty_properties;
    STORM_SILENT_ASSERT_THROW(JaniLocalEliminator(model, empty_properties), storm::exceptions::InvalidArgumentException);
}

// This test verifies that the model is flattened if it has more than one automaton and that the user is informed
// of this.
TEST_F(JaniLocalElimination, FlatteningTest) {
    auto modelAndProps =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/two_modules.jani", storm::jani::getAllKnownModelFeatures(), boost::none);
    auto eliminator = JaniLocalEliminator(modelAndProps.first, modelAndProps.second);
    eliminator.eliminate();
    auto result = eliminator.getResult();
    EXPECT_EQ(1u, result.getNumberOfAutomata());
}

// *******************************
// Tests for common functionality:
// *******************************

// This test verifies that missing guard completion works correctly. It should not alter the behaviour of the model
// and ensure that every location has an outgoing edge for every possible case.
TEST_F(JaniLocalElimination, MissingGuardCompletion) {
    // In this model, s_1 has a missing guard (the case x=0 is not covered). Thus, if we eliminate s_1 without
    // taking additional precautions, this missing guard will propagate to s_0, which causes an incorrect result.
    // If we activate the addMissingGuards option of the eliminator, this will be avoided.

    auto modelAndProps =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/missing_guard.jani", storm::jani::getAllKnownModelFeatures(), boost::none);
    auto eliminator = JaniLocalEliminator(modelAndProps.first, modelAndProps.second, true);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "s"));
    eliminator.scheduler.addAction(std::make_unique<EliminateAction>("main", "l_s_1"));
    eliminator.eliminate();
    auto result = eliminator.getResult();
    EXPECT_EQ(1u, result.getNumberOfAutomata());
    checkModel(result, modelAndProps.second, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 0.5);
}

// This test verifies that locations with loops are correctly identified.
TEST_F(JaniLocalElimination, LoopDetection) {
    auto model = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/loops.jani", storm::jani::getAllKnownModelFeatures(), boost::none).first;

    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F x=1]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    // TODO: To access the internals, I replicate some of the internal behaviour of the JaniLocalEliminator. This should probably be done in a neater way by
    // exposing the relevant bits publicly.

    auto session = JaniLocalEliminator::Session(model, property);
    UnfoldAction unfoldAction("main", "s");
    unfoldAction.doAction(session);

    EXPECT_FALSE(session.hasLoops("main", "l_s_0"));
    EXPECT_TRUE(session.hasLoops("main", "l_s_1"));
    EXPECT_TRUE(session.hasLoops("main", "l_s_2"));
    EXPECT_FALSE(session.hasLoops("main", "l_s_3"));
    EXPECT_TRUE(session.hasLoops("main", "l_s_4"));
    EXPECT_TRUE(session.hasLoops("main", "l_s_5"));
    EXPECT_FALSE(session.hasLoops("main", "l_s_6"));
}

// This test verifies that locations that can potentially satisfy the property are correctly identified.
TEST_F(JaniLocalElimination, IsPartOfPropComputation) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/three_variables.jani", storm::jani::getAllKnownModelFeatures(), boost::none)
            .first;

    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F (x=1 & (y>3 | z<2))]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    // TODO: To access the internals, I replicate some of the internal behaviour of the JaniLocalEliminator. This should probably be done in a neater way by
    // exposing the relevant bits publicly.

    auto session = JaniLocalEliminator::Session(model, property);
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l"));

    UnfoldAction unfoldActionX("main", "x");
    unfoldActionX.doAction(session);

    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_0"));
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1"));
    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_2"));

    UnfoldAction unfoldActionY("main", "y");
    unfoldActionY.doAction(session);

    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1_y_4"));
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1_y_3"));
    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_2_y_4"));

    UnfoldAction unfoldActionZ("main", "z");
    unfoldActionZ.doAction(session);

    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_0_y_2_z_3"));
    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_0_y_2_z_1"));
    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_0_y_5_z_3"));
    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_0_y_5_z_1"));
    EXPECT_FALSE(session.computeIsPartOfProp("main", "l_x_1_y_2_z_3"));
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1_y_2_z_1"));
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1_y_5_z_3"));
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1_y_5_z_1"));
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1_y_4_z_1"));
    EXPECT_TRUE(session.computeIsPartOfProp("main", "l_x_1_y_5_z_0"));
}

// This test verifies that the initial location is correctly identified.
TEST_F(JaniLocalElimination, IsInitialDetection) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/initial_locations.jani", storm::jani::getAllKnownModelFeatures(), boost::none)
            .first;

    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F x=1]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto session = JaniLocalEliminator::Session(model, property);

    EXPECT_TRUE(session.isPossiblyInitial("main", "l"));

    UnfoldAction unfoldActionX("main", "x");
    unfoldActionX.doAction(session);

    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_0"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_1"));
    EXPECT_TRUE(session.isPossiblyInitial("main", "l_x_2"));

    UnfoldAction unfoldActionY("main", "y");
    unfoldActionY.doAction(session);

    EXPECT_TRUE(session.isPossiblyInitial("main", "l_x_2_y_-1"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_2_y_0"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_0_y_0"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_0_y_-1"));

    UnfoldAction unfoldActionZ("main", "z");
    unfoldActionZ.doAction(session);

    EXPECT_TRUE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_0"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_1"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_2_y_0_z_0"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_1_y_-1_z_0"));

    UnfoldAction unfoldActionA("main", "a");
    unfoldActionA.doAction(session);

    EXPECT_TRUE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_0_a_true"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_0_a_false"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_0_y_-1_z_0_a_true"));

    UnfoldAction unfoldActionB("main", "b");
    unfoldActionB.doAction(session);

    EXPECT_TRUE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_0_a_true_b_false"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_0_a_true_b_true"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_2_y_0_z_0_a_true_b_true"));

    UnfoldAction unfoldActionC("main", "c");
    unfoldActionC.doAction(session);

    EXPECT_TRUE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_0_a_true_b_false_c_false"));
    EXPECT_FALSE(session.isPossiblyInitial("main", "l_x_2_y_-1_z_0_a_true_b_false_c_true"));
}

// This test verifies that the eliminable locations are correctly identified.
TEST_F(JaniLocalElimination, IsEliminableDetection) {
    auto model = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/uneliminable_locations.jani", storm::jani::getAllKnownModelFeatures(),
                                            boost::none)
                     .first;

    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F s=4]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto session = JaniLocalEliminator::Session(model, property);

    EXPECT_FALSE(session.isEliminable("main", "l"));

    UnfoldAction unfoldActionX("main", "s");
    unfoldActionX.doAction(session);

    EXPECT_FALSE(session.isEliminable("main", "l_s_0"));
    EXPECT_FALSE(session.isEliminable("main", "l_s_1"));
    EXPECT_TRUE(session.isEliminable("main", "l_s_2"));
    EXPECT_TRUE(session.isEliminable("main", "l_s_3"));
    EXPECT_FALSE(session.isEliminable("main", "l_s_4"));
    EXPECT_FALSE(session.isEliminable("main", "l_s_5"));
}

// This test verifies that the set of variables that make up the property is correctly identified.
TEST_F(JaniLocalElimination, IsVariablePartOfProperty) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/do_nothing.jani", storm::jani::getAllKnownModelFeatures(), boost::none).first;
    {
        storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F x=1]");
        auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());
        auto session = JaniLocalEliminator::Session(model, property);
        EXPECT_TRUE(session.isVariablePartOfProperty("x"));
        EXPECT_FALSE(session.isVariablePartOfProperty("y"));
    }
    {
        storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
        std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F x=1 & y=1]");
        auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());
        auto session = JaniLocalEliminator::Session(model, property);
        EXPECT_TRUE(session.isVariablePartOfProperty("x"));
        EXPECT_TRUE(session.isVariablePartOfProperty("y"));
    }
}

// *******************
// Tests for unfolding
// *******************

// This test verifies that unfolding a bounded integer works correctly
TEST_F(JaniLocalElimination, UnfoldingBoundedInteger) {
    auto model = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/simple_bounded_integer_unfolding.jani",
                                            storm::jani::getAllKnownModelFeatures(), boost::none)
                     .first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F x=1]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "x"));
    eliminator.eliminate();
    auto result = eliminator.getResult();
    EXPECT_EQ(5u, result.getAutomaton(0).getNumberOfLocations());
    checkModel(result, {property}, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 0.25);
}

// This test verifies that unfolding a boolean works correctly
TEST_F(JaniLocalElimination, UnfoldingBoolean) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/simple_bool_unfolding.jani", storm::jani::getAllKnownModelFeatures(), boost::none)
            .first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F (!x)]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "x"));
    eliminator.eliminate();
    auto result = eliminator.getResult();
    EXPECT_EQ(2u, result.getAutomaton(0).getNumberOfLocations());
    checkModel(result, {property}, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 0.875);
}

// This test verifies that trying to unfold a variable that has already been unfolded produces an error
TEST_F(JaniLocalElimination, UnfoldingTwice) {
    auto model = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/simple_bounded_integer_unfolding.jani",
                                            storm::jani::getAllKnownModelFeatures(), boost::none)
                     .first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F x=1]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "x"));
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "x"));
    STORM_SILENT_EXPECT_THROW(eliminator.eliminate(), storm::exceptions::InvalidOperationException);
}

// This test verifies that the sink isn't duplicated during unfolding and that the correct location is marked as sink
TEST_F(JaniLocalElimination, UnfoldingWithSink) {
    auto modelAndProps =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/missing_guard.jani", storm::jani::getAllKnownModelFeatures(), boost::none);
    auto eliminator = JaniLocalEliminator(modelAndProps.first, modelAndProps.second, true);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "s"));
    eliminator.eliminate();
    auto result = eliminator.getResult();
    EXPECT_EQ(5u, result.getAutomaton(0).getNumberOfLocations());
}

// This test verifies that whether a location can potentially satisfy the property is correctly propagated to the new
// locations during unfolding.
TEST_F(JaniLocalElimination, IsPartOfPropPropagationUnfolding) {
    // TODO
}

// *********************
// Tests for elimination
// *********************

// This test verifies that combining the two guards during elimination works correctly.
TEST_F(JaniLocalElimination, EliminationNewGuardTest) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/simple_guards.jani", storm::jani::getAllKnownModelFeatures(), boost::none).first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F c=2]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "c"));
    eliminator.scheduler.addAction(std::make_unique<EliminateAction>("main", "l_c_1"));
    eliminator.scheduler.addAction(std::make_unique<RebuildWithoutUnreachableAction>());
    eliminator.eliminate();
    auto result = eliminator.getResult();

    EXPECT_EQ(2u, result.getAutomaton(0).getNumberOfLocations());
    checkModel(result, {property}, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 1.0);
}

// This test verifies that combining the two probabilities during elimination works correctly.
TEST_F(JaniLocalElimination, EliminationNewProbabilityTest) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/simple_probabilities.jani", storm::jani::getAllKnownModelFeatures(), boost::none)
            .first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F c=2]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "c"));
    eliminator.scheduler.addAction(std::make_unique<EliminateAction>("main", "l_c_1"));
    eliminator.scheduler.addAction(std::make_unique<RebuildWithoutUnreachableAction>());
    eliminator.eliminate();
    auto result = eliminator.getResult();

    EXPECT_EQ(3u, result.getAutomaton(0).getNumberOfLocations());
    checkModel(result, {property}, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 1.0 / 12.0);
}

// This test verifies that generating a new assignment block from two existing ones works correctly.
TEST_F(JaniLocalElimination, EliminationNewUpdatesTest) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/simple_assignments.jani", storm::jani::getAllKnownModelFeatures(), boost::none)
            .first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F c=2&x=3&y=5&z=4&w=2]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "c"));
    eliminator.scheduler.addAction(std::make_unique<EliminateAction>("main", "l_c_1"));
    eliminator.scheduler.addAction(std::make_unique<RebuildWithoutUnreachableAction>());
    eliminator.eliminate();
    auto result = eliminator.getResult();

    EXPECT_EQ(2u, result.getAutomaton(0).getNumberOfLocations());
    EXPECT_EQ(4u, result.getAutomaton(0).getEdgesFromLocation(0).begin()->getDestination(0).getOrderedAssignments().getNumberOfAssignments());
    checkModel(result, {property}, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 1.0);
}

// This test tests the elimination process if multiple edges are incoming and outgoing from the location to be eliminated
TEST_F(JaniLocalElimination, EliminationMultipleEdges) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/incoming_and_outgoing.jani", storm::jani::getAllKnownModelFeatures(), boost::none)
            .first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F c=4]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "c"));
    eliminator.scheduler.addAction(std::make_unique<EliminateAction>("main", "l_c_3"));
    eliminator.scheduler.addAction(std::make_unique<RebuildWithoutUnreachableAction>());
    eliminator.eliminate();
    auto result = eliminator.getResult();

    EXPECT_EQ(6u, result.getAutomaton(0).getNumberOfLocations());
    checkModel(result, {property}, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 0.01845238095);
}

// This test verifies the behaviour if multiple destinations of a single edge point to the location to be eliminated.
TEST_F(JaniLocalElimination, EliminationMultiplicityTest) {
    auto model =
        storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/localeliminator/multiplicity.jani", storm::jani::getAllKnownModelFeatures(), boost::none).first;
    storm::parser::FormulaParser formulaParser(model.getExpressionManager().shared_from_this());
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("P=? [F c=2&x=3]");
    auto property = storm::jani::Property("prop", formula, std::set<storm::expressions::Variable>());

    auto eliminator = JaniLocalEliminator(model, property, false);
    eliminator.scheduler.addAction(std::make_unique<UnfoldAction>("main", "c"));
    eliminator.scheduler.addAction(std::make_unique<EliminateAction>("main", "l_c_1"));
    eliminator.scheduler.addAction(std::make_unique<RebuildWithoutUnreachableAction>());
    eliminator.eliminate();
    auto result = eliminator.getResult();

    EXPECT_EQ(2u, result.getAutomaton(0).getNumberOfLocations());
    checkModel(result, {property}, std::map<storm::expressions::Variable, storm::expressions::Expression>(), 2.0 / 15.0);
}
