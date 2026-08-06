#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/generator/BeliefSupportTracker.h"
#include "storm/api/storm.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/transformer/MakePOMDPCanonic.h"

// TODO
// These tests depend on the interpretation of action and observation numbers and those may change.
// A more robust test would take the high-level actions and observations and track on those.

TEST(BeliefSupportTracking, Maze) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism");
    program = program.preprocess("sl=0.4");
    std::shared_ptr<storm::logic::Formula const> formula = storm::api::parsePropertiesForPrismProgram("Pmax=? [F \"goal\" ]", program).front().getRawFormula();
    std::shared_ptr<storm::models::sparse::Pomdp<double>> pomdp =
        storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();
    storm::transformer::MakePOMDPCanonic<double> makeCanonic(*pomdp);
    pomdp = makeCanonic.transform();

    storm::generator::BeliefSupportTracker<double> tracker(*pomdp);
    EXPECT_EQ(pomdp->getInitialStates(), tracker.getCurrentBeliefSupport());
    tracker.track(0, 0);
    auto beliefsup = tracker.getCurrentBeliefSupport();
    EXPECT_EQ(6ul, beliefsup.getNumberOfSetBits());
    tracker.track(0, 0);
    EXPECT_EQ(beliefsup, tracker.getCurrentBeliefSupport());
    tracker.track(0, 1);
    EXPECT_TRUE(tracker.getCurrentBeliefSupport().empty());
    tracker.reset();
    EXPECT_EQ(pomdp->getInitialStates(), tracker.getCurrentBeliefSupport());
    tracker.track(0, 0);
    EXPECT_EQ(beliefsup, tracker.getCurrentBeliefSupport());
    tracker.track(1, 0);
    EXPECT_EQ(beliefsup, tracker.getCurrentBeliefSupport());
    tracker.track(2, 1);
    EXPECT_EQ(1ul, tracker.getCurrentBeliefSupport().getNumberOfSetBits());
    tracker.track(3, 0);
    EXPECT_EQ(1ul, tracker.getCurrentBeliefSupport().getNumberOfSetBits());
    tracker.track(3, 0);
    EXPECT_EQ(2ul, tracker.getCurrentBeliefSupport().getNumberOfSetBits());
}
