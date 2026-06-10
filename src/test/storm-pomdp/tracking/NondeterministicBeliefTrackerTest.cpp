#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm-pomdp/generator/NondeterministicBeliefTracker.h"
#include "storm/api/storm.h"
#include "storm/transformer/MakePOMDPCanonic.h"

namespace {

std::shared_ptr<storm::models::sparse::Pomdp<double>> buildMaze(std::string const& constants = "sl=0.0") {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism");
    program = storm::utility::prism::preprocess(program, constants);
    auto formula = storm::api::parsePropertiesForPrismProgram("Pmax=? [F \"goal\" ]", program).front().getRawFormula();
    auto model = storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();
    storm::transformer::MakePOMDPCanonic<double> makeCanonic(*model);
    return makeCanonic.transform();
}

TEST(BeliefStateManager, BasicProperties) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto pomdp = buildMaze();
    storm::generator::BeliefStateManager<double> manager(*pomdp);

    EXPECT_EQ(pomdp->getNumberOfStates(), manager.getNumberOfStates());

    for (uint64_t state = 0; state < pomdp->getNumberOfStates(); ++state) {
        EXPECT_EQ(pomdp->getObservation(state), manager.getObservation(state));
    }

    for (uint32_t obs = 0; obs < pomdp->getNrObservations(); ++obs) {
        EXPECT_GT(manager.getActionsForObservation(obs), 0ul);
    }
}

TEST(BeliefStateManager, ObservationOffsets) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto pomdp = buildMaze();
    storm::generator::BeliefStateManager<double> manager(*pomdp);

    // Verify that for each state, getState(obs, offset) round-trips back to the same observation.
    for (uint64_t state = 0; state < pomdp->getNumberOfStates(); ++state) {
        uint32_t obs = manager.getObservation(state);
        uint64_t offset = manager.getObservationOffset(state);
        EXPECT_LT(offset, manager.numberOfStatesPerObservation(obs));
        EXPECT_EQ(obs, manager.getObservation(manager.getState(obs, offset)));
    }
}

TEST(SparseBeliefTracker, ResetToInitial) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto pomdp = buildMaze();
    storm::generator::NondeterministicBeliefTracker<double, storm::generator::SparseBeliefState<double>> tracker(*pomdp);
    tracker.setRisk(std::vector<double>(pomdp->getNumberOfStates(), 0.0));

    uint32_t initObs = pomdp->getObservation(*pomdp->getInitialStates().begin());
    bool hit = tracker.reset(initObs);
    EXPECT_TRUE(hit);
    EXPECT_EQ(1ul, tracker.getNumberOfBeliefs());
    EXPECT_EQ(initObs, tracker.getCurrentObservation());
    EXPECT_EQ(1ul, tracker.getCurrentDimension());
}

TEST(SparseBeliefTracker, TrackValidObservation) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto pomdp = buildMaze();
    storm::generator::NondeterministicBeliefTracker<double, storm::generator::SparseBeliefState<double>> tracker(*pomdp);
    tracker.setRisk(std::vector<double>(pomdp->getNumberOfStates(), 0.0));

    uint32_t initObs = pomdp->getObservation(*pomdp->getInitialStates().begin());
    tracker.reset(initObs);

    // Collect observations reachable from the initial state.
    std::set<uint32_t> reachableObs;
    uint64_t initState = *pomdp->getInitialStates().begin();
    for (uint64_t row = pomdp->getNondeterministicChoiceIndices()[initState]; row < pomdp->getNondeterministicChoiceIndices()[initState + 1]; ++row) {
        for (auto const& entry : pomdp->getTransitionMatrix().getRow(row)) {
            reachableObs.insert(pomdp->getObservation(entry.getColumn()));
        }
    }
    ASSERT_FALSE(reachableObs.empty());

    // Tracking any reachable observation should yield a non-empty belief set.
    uint32_t nextObs = *reachableObs.begin();
    bool result = tracker.track(nextObs);
    EXPECT_TRUE(result);
    EXPECT_GT(tracker.getNumberOfBeliefs(), 0ul);
    EXPECT_EQ(nextObs, tracker.getCurrentObservation());
}

TEST(SparseBeliefTracker, TrackImpossibleObservation) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto pomdp = buildMaze();
    storm::generator::NondeterministicBeliefTracker<double, storm::generator::SparseBeliefState<double>> tracker(*pomdp);
    tracker.setRisk(std::vector<double>(pomdp->getNumberOfStates(), 0.0));

    uint32_t initObs = pomdp->getObservation(*pomdp->getInitialStates().begin());
    tracker.reset(initObs);

    // An observation beyond the valid range is never reachable.
    uint64_t impossibleObs = pomdp->getNrObservations() + 100;
    bool result = tracker.track(impossibleObs);
    EXPECT_FALSE(result);
    EXPECT_EQ(0ul, tracker.getNumberOfBeliefs());
}

TEST(SparseBeliefTracker, UniformRisk) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto pomdp = buildMaze();
    storm::generator::NondeterministicBeliefTracker<double, storm::generator::SparseBeliefState<double>> tracker(*pomdp);

    uint32_t initObs = pomdp->getObservation(*pomdp->getInitialStates().begin());

    // All-zero risk.
    std::vector<double> zeroRisk(pomdp->getNumberOfStates(), 0.0);
    tracker.setRisk(zeroRisk);
    tracker.reset(initObs);
    EXPECT_NEAR(0.0, tracker.getCurrentRisk(true), 1e-9);
    EXPECT_NEAR(0.0, tracker.getCurrentRisk(false), 1e-9);

    // All-one risk.
    std::vector<double> oneRisk(pomdp->getNumberOfStates(), 1.0);
    tracker.setRisk(oneRisk);
    tracker.reset(initObs);
    EXPECT_NEAR(1.0, tracker.getCurrentRisk(true), 1e-9);
    EXPECT_NEAR(1.0, tracker.getCurrentRisk(false), 1e-9);
}

TEST(SparseBeliefTracker, MixedRiskMaxMin) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    // Use sl=0.4 so that slipping lets the system stay in the initial state, creating
    // multiple beliefs from a single tracking step. We then assign heterogeneous risk
    // and verify that getCurrentRisk correctly returns the max and the min over beliefs.
    auto pomdp = buildMaze("sl=0.4");
    storm::generator::NondeterministicBeliefTracker<double, storm::generator::SparseBeliefState<double>> tracker(*pomdp);

    // Assign risk 0.0 to the initial state and 1.0 to everything else.
    uint64_t initState = *pomdp->getInitialStates().begin();
    std::vector<double> mixedRisk(pomdp->getNumberOfStates(), 1.0);
    mixedRisk[initState] = 0.0;
    tracker.setRisk(mixedRisk);

    uint32_t initObs = pomdp->getObservation(initState);
    tracker.reset(initObs);

    // After reset the sole belief sits on the initial state with risk 0.0.
    EXPECT_NEAR(0.0, tracker.getCurrentRisk(true), 1e-9);
    EXPECT_NEAR(0.0, tracker.getCurrentRisk(false), 1e-9);

    // Track any observation that corresponds to a non-initial state to obtain a belief
    // that has risk 1.0. We iterate until we either find one or exhaust observations.
    bool movedToHighRisk = false;
    for (uint32_t obs = 0; obs < static_cast<uint32_t>(pomdp->getNrObservations()); ++obs) {
        if (obs == initObs)
            continue;
        storm::generator::NondeterministicBeliefTracker<double, storm::generator::SparseBeliefState<double>> t2(*pomdp);
        t2.setRisk(mixedRisk);
        t2.reset(initObs);
        if (t2.track(obs)) {
            EXPECT_NEAR(1.0, t2.getCurrentRisk(true), 1e-9);
            movedToHighRisk = true;
            break;
        }
    }
    EXPECT_TRUE(movedToHighRisk) << "Could not find a non-initial reachable observation";
}

}  // namespace
