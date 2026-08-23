#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/analysis.h"
#include "storm-dft/api/io.h"
#include "storm-dft/api/transformation.h"
#include "storm-dft/simulator/DFTTraceSimulator.h"
#include "storm-dft/simulator/ImportanceFunction.h"

namespace {

std::pair<std::shared_ptr<storm::dft::storage::DFT<double>>, storm::dft::storage::DFTStateGenerationInfo> prepareDFT(std::string const& file) {
    // Load, build and prepare DFT
    std::shared_ptr<storm::dft::storage::DFT<double>> dft =
        storm::dft::api::prepareForMarkovAnalysis<double>(*(storm::dft::api::loadDFTGalileoFile<double>(file)));
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);

    // Compute relevant events
    storm::dft::utility::RelevantEvents relevantEvents = storm::dft::api::computeRelevantEvents({}, {"all"});
    dft->setRelevantEvents(relevantEvents, false);

    storm::dft::storage::DFTStateGenerationInfo stateGenerationInfo(dft->buildStateGenerationInfo(storm::dft::storage::DftSymmetries()));
    return std::make_pair(dft, stateGenerationInfo);
}

TEST(ImportanceFunctionTest, RandomStepsAnd) {
    auto pair = prepareDFT(STORM_TEST_RESOURCES_DIR "/dft/and.dft");
    auto dft = pair.first;

    // Init random number generator
    boost::mt19937 gen(5u);
    storm::dft::simulator::DFTTraceSimulator<double> simulator(*dft, pair.second, gen);

    // Init importance function
    storm::dft::simulator::BECountImportanceFunction<double> imp = storm::dft::simulator::BECountImportanceFunction<double>(*dft);
    auto range = imp.getImportanceRange();
    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 2);

    auto state = simulator.getCurrentState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));
    EXPECT_EQ(imp.getImportance(state), 0);

    // First random step
    storm::dft::simulator::SimulationStepResult res = simulator.randomStep();
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
#if BOOST_VERSION > 106400
    // Older Boost versions yield different value
    EXPECT_NEAR(simulator.getCurrentTime(), 0.522079, 1e-6);
#endif
    state = simulator.getCurrentState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));
    EXPECT_EQ(imp.getImportance(state), 1);

    res = simulator.randomStep();
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
#if BOOST_VERSION > 106400
    // Older Boost versions yield different value
    EXPECT_NEAR(simulator.getCurrentTime(), 0.522079 + 0.9497214, 1e-6);
#endif
    state = simulator.getCurrentState();
    EXPECT_TRUE(state->hasFailed(dft->getTopLevelIndex()));
    EXPECT_EQ(imp.getImportance(state), 2);
}

}  // namespace
