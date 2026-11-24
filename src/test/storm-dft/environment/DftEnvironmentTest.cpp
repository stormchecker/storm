#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/builder/DftExplorationHeuristic.h"
#include "storm-dft/environment/AnalysisEnvironment.h"
#include "storm-dft/environment/DftEnvironment.h"
#include "storm-dft/environment/ModelBuilderEnvironment.h"
#include "storm-dft/environment/TransformationEnvironment.h"

namespace {

TEST(DftEnvironmentTest, Defaults) {
    storm::dft::DftEnvironment env;

    // Analysis environment
    EXPECT_FALSE(env.analysis().isUseModularisation());
    EXPECT_FALSE(env.analysis().isSolveWithSMT());
    EXPECT_EQ(env.analysis().getChunksize(), 1ul);
    EXPECT_FALSE(env.analysis().isApproximationErrorSet());
    EXPECT_EQ(env.analysis().getApproximationHeuristic(), storm::dft::builder::ApproximationHeuristic::DEPTH);

    // Model builder environment
    EXPECT_TRUE(env.modelBuilder().isUseSymmetryReduction());
    EXPECT_FALSE(env.modelBuilder().isAllowDCForRelevantEvents());
    EXPECT_FALSE(env.modelBuilder().isAddLabelsClaiming());
    EXPECT_FALSE(env.modelBuilder().isMaxDepthSet());
    EXPECT_FALSE(env.modelBuilder().isTakeFirstDependency());
    EXPECT_FALSE(env.modelBuilder().isUniqueFailedBE());

    // Transformation environment
    EXPECT_FALSE(env.transformation().isUseBisimulation());
    EXPECT_FALSE(env.transformation().isEliminateChains());
    EXPECT_EQ(env.transformation().getLabelBehavior(), storm::transformer::EliminationLabelBehavior::KeepLabels);
}

TEST(DftEnvironmentTest, GetSet) {
    storm::dft::DftEnvironment env;
    env.modelBuilder().setUseSymmetryReduction(false);
    env.modelBuilder().setMaxDepth(3);
    env.analysis().setUseModularisation(true);
    env.analysis().setApproximationError(0.5);
    env.transformation().setUseBisimulation(true);
    env.transformation().setEliminateChains(true);
    env.transformation().setLabelBehavior(storm::transformer::EliminationLabelBehavior::MergeLabels);

    EXPECT_FALSE(env.modelBuilder().isUseSymmetryReduction());
    EXPECT_TRUE(env.modelBuilder().isMaxDepthSet());
    EXPECT_EQ(env.modelBuilder().getMaxDepth(), 3ul);
    EXPECT_TRUE(env.analysis().isUseModularisation());
    EXPECT_TRUE(env.analysis().isApproximationErrorSet());
    EXPECT_EQ(env.analysis().getApproximationError(), 0.5);
    EXPECT_TRUE(env.transformation().isUseBisimulation());
    EXPECT_TRUE(env.transformation().isEliminateChains());
    EXPECT_EQ(env.transformation().getLabelBehavior(), storm::transformer::EliminationLabelBehavior::MergeLabels);

    env.modelBuilder().unsetMaxDepth();
    EXPECT_FALSE(env.modelBuilder().isMaxDepthSet());
    env.analysis().unsetApproximationError();
    EXPECT_FALSE(env.analysis().isApproximationErrorSet());
}

TEST(DftEnvironmentTest, CopyIsIndependent) {
    storm::dft::DftEnvironment env;
    env.analysis().setUseModularisation(true);
    env.transformation().setUseBisimulation(true);

    storm::dft::DftEnvironment copy = env;
    copy.analysis().setUseModularisation(false);
    copy.transformation().setUseBisimulation(false);

    EXPECT_TRUE(env.analysis().isUseModularisation());
    EXPECT_FALSE(copy.analysis().isUseModularisation());
    EXPECT_TRUE(env.transformation().isUseBisimulation());
    EXPECT_FALSE(copy.transformation().isUseBisimulation());
}

}  // namespace
