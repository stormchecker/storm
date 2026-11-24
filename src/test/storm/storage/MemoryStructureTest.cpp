#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/models/sparse/Mdp.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/memorystructure/MemoryStructure.h"
#include "storm/storage/memorystructure/MemoryStructureBuilder.h"
#include "storm/storage/memorystructure/SparseModelMemoryProduct.h"

namespace {
std::shared_ptr<storm::models::sparse::Mdp<double>> buildTinyMdp() {
    // 3-state chain: 0 -> 1 -> 2 (self-loop)
    // State 0 is initial, state 2 is a "goal" state.
    uint64_t numStates = 3;
    storm::storage::SparseMatrixBuilder<double> builder(numStates, numStates, numStates, true, true, numStates);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, 1.0);
    builder.newRowGroup(1);
    builder.addNextValue(1, 2, 1.0);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, 1.0);
    auto matrix = builder.build();

    storm::models::sparse::StateLabeling labeling(numStates);
    labeling.addLabel("init");
    labeling.addLabel("goal");
    labeling.addLabelToState("init", 0);
    labeling.addLabelToState("goal", 2);

    return std::make_shared<storm::models::sparse::Mdp<double>>(std::move(matrix), std::move(labeling));
}

storm::storage::MemoryStructure buildGoalMemoryStructure(storm::models::sparse::Mdp<double> const& mdp) {
    storm::storage::MemoryStructureBuilder<double> builder(2, mdp, false);
    storm::storage::BitVector goalStates(mdp.getNumberOfStates(), false);
    goalStates.set(2, true);
    storm::storage::BitVector allStates(mdp.getNumberOfStates(), true);
    builder.setTransition(0, 0, ~goalStates);
    builder.setTransition(0, 1, goalStates);
    builder.setTransition(1, 1, allStates);
    return builder.build();
}
}  // namespace

TEST(MemoryStructure, OnlyInitialStatesRelevantDefault) {
    auto mdp = buildTinyMdp();
    storm::storage::MemoryStructureBuilder<double> builder(1, *mdp, true);
    storm::storage::BitVector allStates(3, true);
    builder.setTransition(0, 0, allStates);
    auto mem = builder.build();

    EXPECT_TRUE(mem.isOnlyInitialStatesRelevantSet());
    EXPECT_EQ(1u, mem.getInitialMemoryStates().size());
}

TEST(MemoryStructure, OnlyInitialStatesRelevantFalse) {
    auto mdp = buildTinyMdp();
    storm::storage::MemoryStructureBuilder<double> builder(2, *mdp, false);
    storm::storage::BitVector allStates(3, true);
    builder.setTransition(0, 0, allStates);
    builder.setTransition(1, 1, allStates);
    auto mem = builder.build();

    EXPECT_FALSE(mem.isOnlyInitialStatesRelevantSet());
    EXPECT_EQ(3u, mem.getInitialMemoryStates().size());
}

TEST(MemoryStructure, ProductPropagatesOnlyInitialStatesRelevant) {
    auto mdp = buildTinyMdp();
    auto mem1 = buildGoalMemoryStructure(*mdp);
    auto mem2 = buildGoalMemoryStructure(*mdp);

    EXPECT_FALSE(mem1.isOnlyInitialStatesRelevantSet());
    EXPECT_FALSE(mem2.isOnlyInitialStatesRelevantSet());

    auto prod = mem1.product(mem2);
    EXPECT_FALSE(prod.isOnlyInitialStatesRelevantSet()) << "product of two non-initial-only structures should remain non-initial-only";
    EXPECT_EQ(3u, prod.getInitialMemoryStates().size());
}

TEST(MemoryStructure, ProductOfTrivialMemoryStructuresIsTrivial) {
    auto mdp = buildTinyMdp();
    auto mem1 = storm::storage::MemoryStructureBuilder<double>::buildTrivialMemoryStructure(*mdp);
    auto mem2 = storm::storage::MemoryStructureBuilder<double>::buildTrivialMemoryStructure(*mdp);

    EXPECT_TRUE(mem1.isOnlyInitialStatesRelevantSet());
    EXPECT_TRUE(mem2.isOnlyInitialStatesRelevantSet());

    auto prod = mem1.product(mem2);
    EXPECT_TRUE(prod.isOnlyInitialStatesRelevantSet()) << "product of two initial-only structures should remain initial-only";
    EXPECT_EQ(1u, prod.getInitialMemoryStates().size());
}

TEST(MemoryStructure, ProductModelWithOnlyInitialStatesRelevantFalse) {
    auto mdp = buildTinyMdp();
    auto mem1 = buildGoalMemoryStructure(*mdp);
    auto mem2 = buildGoalMemoryStructure(*mdp);
    auto prodMem = mem1.product(mem2);

    ASSERT_FALSE(prodMem.isOnlyInitialStatesRelevantSet());

    // This must not fire the STORM_LOG_ASSERT in SparseModelMemoryProduct::initialize().
    auto productType = prodMem.product(*mdp);
    auto productModel = productType.build();

    EXPECT_EQ(4u, productModel->getNumberOfStates());
    EXPECT_EQ(3u, productModel->getInitialStates().getNumberOfSetBits());
}

TEST(MemoryStructure, ProductModelWithOnlyInitialStatesRelevantTrue) {
    auto mdp = buildTinyMdp();
    storm::storage::MemoryStructureBuilder<double> builder(2, *mdp, true);
    storm::storage::BitVector goalStates(3, false);
    goalStates.set(2, true);
    storm::storage::BitVector allStates(3, true);
    builder.setTransition(0, 0, ~goalStates);
    builder.setTransition(0, 1, goalStates);
    builder.setTransition(1, 1, allStates);
    builder.setInitialMemoryState(0, 0);  // only initial model state (0) gets a memory state
    auto mem = builder.build();

    ASSERT_TRUE(mem.isOnlyInitialStatesRelevantSet());
    EXPECT_EQ(1u, mem.getInitialMemoryStates().size());

    auto productType = mem.product(*mdp);
    auto productModel = productType.build();

    // Only 1 initial product state (one initial model state * one initial memory state).
    EXPECT_EQ(1u, productModel->getInitialStates().getNumberOfSetBits());
}

TEST(MemoryStructure, StateLabelingRespectsOnlyInitialStatesRelevantFalse) {
    auto mdp = buildTinyMdp();
    auto mem = buildGoalMemoryStructure(*mdp);
    ASSERT_FALSE(mem.isOnlyInitialStatesRelevantSet());

    auto productType = mem.product(*mdp);
    auto productModel = productType.build();

    // The "init" label should mark exactly one product state per model state.
    auto const& initLabeling = productModel->getStateLabeling();
    std::set<std::string> labels = initLabeling.getLabels();
    ASSERT_NE(labels.end(), labels.find("init"));

    storm::storage::BitVector initStates = initLabeling.getStates("init");
    EXPECT_EQ(3u, initStates.getNumberOfSetBits());
}
