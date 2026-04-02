#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/adapters/RationalNumberAdapter.h"

#include "storm-pomdp/generator/GenerateMonitorVerifier.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/sparse/ModelComponents.h"

namespace {

// Build a 3-state DTMC used as the system model (MC):
//   s0 (init): 0.6 -> s1, 0.4 -> s2
//   s1 ("a", "good"): self-loop
//   s2 ("b"):         self-loop
storm::models::sparse::Dtmc<double> buildSimpleMC() {
    storm::storage::SparseMatrixBuilder<double> builder(3, 3, 4, false, false);
    builder.addNextValue(0, 1, 0.6);
    builder.addNextValue(0, 2, 0.4);
    builder.addNextValue(1, 1, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto matrix = builder.build();

    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("a");
    labeling.addLabelToState("a", 1);
    labeling.addLabel("b");
    labeling.addLabelToState("b", 2);
    labeling.addLabel("good");
    labeling.addLabelToState("good", 1);

    storm::storage::sparse::ModelComponents<double> components(std::move(matrix), std::move(labeling));
    return storm::models::sparse::Dtmc<double>(std::move(components));
}

// Build a 3-state MDP used as the 2-step monitor:
//   m0 ("step0", init):         [a] -> m1,  [b] -> m1
//   m1 ("step1", "accepting"):  [a] -> m2,  [b] -> m2
//   m2 ("step2", "horizon"):    [a] -> m2   (self-loop; horizon overrides in product)
//
// The monitor accepts at step1 and then triggers a horizon reset at step2.
storm::models::sparse::Mdp<double> buildSimpleMonitor() {
    // 5 total choice rows across 3 row groups (states).
    storm::storage::SparseMatrixBuilder<double> builder(0, 0, 0, false, true);
    builder.newRowGroup(0);           // state 0: rows 0-1
    builder.addNextValue(0, 1, 1.0);  // [a] -> m1
    builder.addNextValue(1, 1, 1.0);  // [b] -> m1
    builder.newRowGroup(2);           // state 1: rows 2-3
    builder.addNextValue(2, 2, 1.0);  // [a] -> m2
    builder.addNextValue(3, 2, 1.0);  // [b] -> m2
    builder.newRowGroup(4);           // state 2: row 4
    builder.addNextValue(4, 2, 1.0);  // [a] -> m2 (self-loop)
    auto matrix = builder.build();

    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("step0");
    labeling.addLabelToState("step0", 0);
    labeling.addLabel("step1");
    labeling.addLabelToState("step1", 1);
    labeling.addLabel("accepting");
    labeling.addLabelToState("accepting", 1);
    labeling.addLabel("step2");
    labeling.addLabelToState("step2", 2);
    labeling.addLabel("horizon");
    labeling.addLabelToState("horizon", 2);

    // Choice labeling: 5 rows total.
    // "a": rows 0, 2, 4 ; "b": rows 1, 3
    storm::models::sparse::ChoiceLabeling choiceLabeling(5);
    storm::storage::BitVector aChoices(5);
    aChoices.set(0);
    aChoices.set(2);
    aChoices.set(4);
    choiceLabeling.addLabel("a", aChoices);
    storm::storage::BitVector bChoices(5);
    bChoices.set(1);
    bChoices.set(3);
    choiceLabeling.addLabel("b", bChoices);

    storm::storage::sparse::ModelComponents<double> components(std::move(matrix), std::move(labeling));
    components.choiceLabeling = std::move(choiceLabeling);
    return storm::models::sparse::Mdp<double>(std::move(components));
}

// Helper: create the generator with common settings and produce the product.
std::shared_ptr<storm::generator::MonitorVerifier<double>> makeProduct(storm::models::sparse::Dtmc<double> const& mc,
                                                                       storm::models::sparse::Mdp<double> const& monitor,
                                                                       storm::generator::GenerateMonitorVerifier<double>::Options options) {
    auto exprManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::generator::GenerateMonitorVerifier<double> gen(mc, monitor, exprManager, options);
    return gen.createProduct();
}

TEST(MonitorVerifier, ProductHasExpectedLabels) {
    auto mc = buildSimpleMC();
    auto monitor = buildSimpleMonitor();

    storm::generator::GenerateMonitorVerifier<double>::Options options;
    options.useRisk = false;
    options.useRejectionSampling = false;

    auto mv = makeProduct(mc, monitor, options);
    const auto& product = mv->getProduct();
    const auto& labeling = product.getStateLabeling();

    EXPECT_TRUE(labeling.containsLabel("init"));
    EXPECT_TRUE(labeling.containsLabel("goal"));
    EXPECT_TRUE(labeling.containsLabel("stop"));
    EXPECT_TRUE(labeling.containsLabel("condition"));
    EXPECT_TRUE(labeling.containsLabel("sink"));

    EXPECT_EQ(1ul, labeling.getStates("goal").getNumberOfSetBits());
    EXPECT_EQ(1ul, labeling.getStates("stop").getNumberOfSetBits());
    EXPECT_EQ(1ul, labeling.getStates("sink").getNumberOfSetBits());
    EXPECT_GE(labeling.getStates("init").getNumberOfSetBits(), 1ul);

    // "condition" must be exactly the union of "goal" and "stop".
    EXPECT_EQ(2ul, labeling.getStates("condition").getNumberOfSetBits());
    EXPECT_EQ(labeling.getStates("goal") | labeling.getStates("stop"), labeling.getStates("condition"));

    // The product POMDP must carry choice labeling.
    EXPECT_TRUE(product.hasChoiceLabeling());
}

TEST(MonitorVerifier, ObservationMapEntries) {
    auto mc = buildSimpleMC();
    auto monitor = buildSimpleMonitor();

    storm::generator::GenerateMonitorVerifier<double>::Options options;
    options.useRisk = false;
    options.useRejectionSampling = false;

    auto mv = makeProduct(mc, monitor, options);
    const auto& obsMap = mv->getObservationMap();

    // One entry per distinct (step, accepting) combination in the monitor.
    EXPECT_TRUE(obsMap.contains(std::make_pair(0u, false)));  // m0: step0, not accepting
    EXPECT_TRUE(obsMap.contains(std::make_pair(1u, true)));   // m1: step1, accepting
    EXPECT_TRUE(obsMap.contains(std::make_pair(2u, false)));  // m2: step2, not accepting (horizon)

    // All mapped observation values must be distinct.
    std::set<uint32_t> obsValues;
    for (const auto& [key, val] : obsMap) {
        obsValues.insert(val);
    }
    EXPECT_EQ(obsMap.size(), obsValues.size());
}

TEST(MonitorVerifier, RejectionSamplingHasNoSink) {
    auto mc = buildSimpleMC();
    auto monitor = buildSimpleMonitor();

    storm::generator::GenerateMonitorVerifier<double>::Options options;
    options.useRisk = false;
    options.useRejectionSampling = true;

    auto mv = makeProduct(mc, monitor, options);
    const auto& labeling = mv->getProduct().getStateLabeling();

    // With rejection sampling there is no dedicated sink state.
    EXPECT_FALSE(labeling.containsLabel("sink"));

    // But goal / stop / init must still be present.
    EXPECT_TRUE(labeling.containsLabel("goal"));
    EXPECT_TRUE(labeling.containsLabel("stop"));
    EXPECT_TRUE(labeling.containsLabel("init"));
}

TEST(MonitorVerifier, GoodLabelRoutesToGoalOrStop) {
    auto mc = buildSimpleMC();  // state 1 has "good", state 2 does not
    auto monitor = buildSimpleMonitor();

    storm::generator::GenerateMonitorVerifier<double>::Options options;
    options.useRisk = false;
    options.useRejectionSampling = false;

    auto mv = makeProduct(mc, monitor, options);
    const auto& product = mv->getProduct();
    const auto& labeling = product.getStateLabeling();
    const auto& transMatrix = product.getTransitionMatrix();

    uint64_t goalState = *labeling.getStates("goal").begin();
    uint64_t stopState = *labeling.getStates("stop").begin();

    // The observation assigned to (step=1, accepting=true).
    uint32_t acceptingObs = mv->getObservationMap().at(std::make_pair(1u, true));

    bool foundGoalTransition = false;
    bool foundStopTransition = false;

    for (uint64_t state = 0; state < product.getNumberOfStates(); ++state) {
        if (product.getObservation(state) != acceptingObs)
            continue;
        for (uint64_t row = transMatrix.getRowGroupIndices()[state]; row < transMatrix.getRowGroupIndices()[state + 1]; ++row) {
            if (product.getChoiceLabeling().getLabelsOfChoice(row).count("end") == 0)
                continue;
            for (const auto& entry : transMatrix.getRow(row)) {
                if (entry.getColumn() == goalState)
                    foundGoalTransition = true;
                if (entry.getColumn() == stopState)
                    foundStopTransition = true;
            }
        }
    }

    EXPECT_TRUE(foundGoalTransition) << "No 'end' transition to goal found among accepting product states";
    EXPECT_TRUE(foundStopTransition) << "No 'end' transition to stop found among accepting product states";
}

TEST(MonitorVerifier, WithRiskAlmostOneGoesToGoal) {
    auto mc = buildSimpleMC();
    auto monitor = buildSimpleMonitor();

    storm::generator::GenerateMonitorVerifier<double>::Options options;
    options.useRisk = true;
    options.useRejectionSampling = false;

    auto exprManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::generator::GenerateMonitorVerifier<double> gen(mc, monitor, exprManager, options);

    // s0=0.0, s1=1.0 (goes entirely to goal), s2=0.0 (goes entirely to stop)
    gen.setRisk({0.0, 1.0, 0.0});
    auto mv = gen.createProduct();

    const auto& product = mv->getProduct();
    const auto& labeling = product.getStateLabeling();
    const auto& transMatrix = product.getTransitionMatrix();

    uint64_t goalState = *labeling.getStates("goal").begin();
    uint64_t stopState = *labeling.getStates("stop").begin();
    uint32_t acceptingObs = mv->getObservationMap().at(std::make_pair(1u, true));

    bool foundGoalOnly = false;  // risk=1 accepting state -> only to goal
    bool foundStopOnly = false;  // risk=0 accepting state -> only to stop

    for (uint64_t state = 0; state < product.getNumberOfStates(); ++state) {
        if (product.getObservation(state) != acceptingObs)
            continue;
        for (uint64_t row = transMatrix.getRowGroupIndices()[state]; row < transMatrix.getRowGroupIndices()[state + 1]; ++row) {
            if (product.getChoiceLabeling().getLabelsOfChoice(row).count("end") == 0)
                continue;
            bool toGoal = false, toStop = false;
            for (const auto& entry : transMatrix.getRow(row)) {
                if (entry.getColumn() == goalState)
                    toGoal = true;
                if (entry.getColumn() == stopState)
                    toStop = true;
            }
            if (toGoal && !toStop)
                foundGoalOnly = true;
            if (!toGoal && toStop)
                foundStopOnly = true;
        }
    }

    EXPECT_TRUE(foundGoalOnly) << "Expected an accepting state with risk=1 to route entirely to goal";
    EXPECT_TRUE(foundStopOnly) << "Expected an accepting state with risk=0 to route entirely to stop";
}

TEST(MonitorVerifier, WithRiskIntermediateSplitsGoalStop) {
    auto mc = buildSimpleMC();
    auto monitor = buildSimpleMonitor();

    storm::generator::GenerateMonitorVerifier<double>::Options options;
    options.useRisk = true;
    options.useRejectionSampling = false;

    auto exprManager = std::make_shared<storm::expressions::ExpressionManager>();
    storm::generator::GenerateMonitorVerifier<double> gen(mc, monitor, exprManager, options);

    // s1 has risk 0.5 — the end-transition should go to both goal and stop.
    gen.setRisk({0.0, 0.5, 0.0});
    auto mv = gen.createProduct();

    const auto& product = mv->getProduct();
    const auto& labeling = product.getStateLabeling();
    const auto& transMatrix = product.getTransitionMatrix();

    uint64_t goalState = *labeling.getStates("goal").begin();
    uint64_t stopState = *labeling.getStates("stop").begin();
    uint32_t acceptingObs = mv->getObservationMap().at(std::make_pair(1u, true));

    bool foundSplit = false;

    for (uint64_t state = 0; state < product.getNumberOfStates(); ++state) {
        if (product.getObservation(state) != acceptingObs)
            continue;
        for (uint64_t row = transMatrix.getRowGroupIndices()[state]; row < transMatrix.getRowGroupIndices()[state + 1]; ++row) {
            if (product.getChoiceLabeling().getLabelsOfChoice(row).count("end") == 0)
                continue;
            bool toGoal = false, toStop = false;
            for (const auto& entry : transMatrix.getRow(row)) {
                if (entry.getColumn() == goalState)
                    toGoal = true;
                if (entry.getColumn() == stopState)
                    toStop = true;
            }
            if (toGoal && toStop) {
                foundSplit = true;
            }
        }
    }

    EXPECT_TRUE(foundSplit) << "Expected an accepting state with risk=0.5 to split between goal and stop";
}

}  // namespace
