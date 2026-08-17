#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/generator/DftNextStateGenerator.h"
#include "storm-dft/simulator/DFTTraceSimulator.h"
#include "storm-dft/storage/DftSymmetries.h"

namespace {

// Helper functions
std::pair<double, double> simulateDft(std::string const& file, double timebound, size_t noRuns) {
    // Load, build and prepare DFT
    std::shared_ptr<storm::dft::storage::DFT<double>> dft =
        storm::dft::api::prepareForMarkovAnalysis<double>(*(storm::dft::api::loadDFTGalileoFile<double>(file)));
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);

    // Set relevant events
    storm::dft::utility::RelevantEvents relevantEvents = storm::dft::api::computeRelevantEvents({}, {});
    dft->setRelevantEvents(relevantEvents, false);

    // Find symmetries
    storm::dft::storage::DftSymmetries symmetries;
    storm::dft::storage::DFTStateGenerationInfo stateGenerationInfo(dft->buildStateGenerationInfo(symmetries));

    // Init random number generator
    // storm::utility::setLogLevel(l3pp::LogLevel::TRACE);
    boost::mt19937 gen(5u);
    storm::dft::simulator::DFTTraceSimulator<double> simulator(*dft, stateGenerationInfo, gen);

    size_t count = 0;
    size_t invalid = 0;
    storm::dft::simulator::SimulationTraceResult res;
    for (size_t i = 0; i < noRuns; ++i) {
        res = simulator.simulateCompleteTrace(timebound);
        if (res == storm::dft::simulator::SimulationTraceResult::SUCCESSFUL) {
            ++count;
        } else if (res == storm::dft::simulator::SimulationTraceResult::INVALID) {
            // Discard invalid traces
            ++invalid;
        }
    }
    return std::make_pair(count, invalid);
}

double simulateDftProb(std::string const& file, double timebound, size_t noRuns) {
    size_t count;
    size_t invalid;
    std::tie(count, invalid) = simulateDft(file, timebound, noRuns);
    EXPECT_EQ(invalid, 0ul);
    return (double)count / noRuns;
}

TEST(DftSimulatorTest, AndUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/and.dft", 2, 10000);
    EXPECT_NEAR(result, 0.3995764009, 0.01);
}

TEST(DftSimulatorTest, OrUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/or.dft", 1, 10000);
    EXPECT_NEAR(result, 0.6321205588, 0.01);
}

TEST(DftSimulatorTest, VotingUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/voting.dft", 1, 10000);
    EXPECT_NEAR(result, 0.4511883639, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/voting2.dft", 1, 10000);
    EXPECT_NEAR(result, 0.8173164759, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/voting3.dft", 1, 10000);
    EXPECT_NEAR(result, 0.3496529873, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/voting4.dft", 1, 10000);
#if BOOST_VERSION > 106400
    EXPECT_NEAR(result, 0.693568287, 0.01);
#else
    // Older Boost versions yield different value
    EXPECT_NEAR(result, 0.693568287, 0.015);
#endif
}

TEST(DftSimulatorTest, PandUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/pand.dft", 1, 10000);
    EXPECT_NEAR(result, 0.05434443602, 0.01);
}

TEST(DftSimulatorTest, PorUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/por.dft", 1, 10000);
    EXPECT_NEAR(result, 0.2753355179, 0.01);
}

TEST(DftSimulatorTest, FdepUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/fdep.dft", 1, 10000);
    EXPECT_NEAR(result, 0.7768698399, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/fdep2.dft", 1, 10000);
    EXPECT_NEAR(result, 0.3934693403, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/fdep3.dft", 1, 10000);
    EXPECT_NEAR(result, 0.329679954, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/fdep4.dft", 1, 10000);
    EXPECT_NEAR(result, 0.601280086, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/fdep5.dft", 1, 10000);
    EXPECT_NEAR(result, 0.1548181217, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/fdep6.dft", 1, 10000);
    EXPECT_NEAR(result, 0.9985116987, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/fdep7.dft", 1, 10000);
    EXPECT_NEAR(result, 0.9343760449, 0.01);
}

TEST(DftSimulatorTest, PdepUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/pdep.dft", 1, 10000);
    EXPECT_NEAR(result, 0.2017690905, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/pdep2.dft", 1, 10000);
    EXPECT_NEAR(result, 0.2401091405, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/pdep3.dft", 1, 10000);
    EXPECT_NEAR(result, 0.2259856274, 0.01);
    // Example pdep4 contains non-determinism which is not handled in simulation
    // result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/pdep4.dft", 1, 10000);
    // EXPECT_NEAR(result, 0.008122157897, 0.01);
}

TEST(DftSimulatorTest, SpareUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare.dft", 1, 10000);
    EXPECT_NEAR(result, 0.1118530638, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare2.dft", 1, 10000);
    EXPECT_NEAR(result, 0.2905027469, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare3.dft", 1, 10000);
    EXPECT_NEAR(result, 0.4660673246, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare4.dft", 1, 10000);
    EXPECT_NEAR(result, 0.01273070783, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare5.dft", 1, 10000);
    EXPECT_NEAR(result, 0.2017690905, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare6.dft", 1, 10000);
    EXPECT_NEAR(result, 0.4693712702, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare7.dft", 1, 10000);
    EXPECT_NEAR(result, 0.06108774525, 0.01);
    result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/spare8.dft", 1, 10000);
    EXPECT_NEAR(result, 0.02686144489, 0.01);
}

TEST(DftSimulatorTest, SeqUnreliability) {
    size_t count;
    size_t invalid;
    std::tie(count, invalid) = simulateDft(STORM_TEST_RESOURCES_DIR "/dft/seq.dft", 1, 10000);
    EXPECT_EQ(invalid, 0ul);
    double result = (double)count / 10000;
    EXPECT_NEAR(result, 0.09020401043, 0.01);
    std::tie(count, invalid) = simulateDft(STORM_TEST_RESOURCES_DIR "/dft/seq2.dft", 1, 10000);
    EXPECT_EQ(invalid, 0ul);
    result = (double)count / 10000;
    EXPECT_NEAR(result, 0.01438767797, 0.01);
    std::tie(count, invalid) = simulateDft(STORM_TEST_RESOURCES_DIR "/dft/seq3.dft", 1, 10000);
    EXPECT_EQ(invalid, 0ul);
    result = (double)count / 10000;
    EXPECT_NEAR(result, 0.01438767797, 0.01);
    std::tie(count, invalid) = simulateDft(STORM_TEST_RESOURCES_DIR "/dft/seq4.dft", 1, 10000);
    EXPECT_EQ(invalid, 0ul);
    result = (double)count / 10000;
    EXPECT_NEAR(result, 0.01438767797, 0.01);
    std::tie(count, invalid) = simulateDft(STORM_TEST_RESOURCES_DIR "/dft/seq5.dft", 1, 10000);
    EXPECT_EQ(invalid, 0ul);
    result = (double)count / 10000;
    EXPECT_EQ(result, 0);
    std::tie(count, invalid) = simulateDft(STORM_TEST_RESOURCES_DIR "/dft/seq6.dft", 1, 10000);
    EXPECT_EQ(invalid, 0ul);
    result = (double)count / 10000;
    EXPECT_NEAR(result, 2.499875004e-09, 0.01);
}

TEST(DftSimulatorTest, MutexUnreliability) {
    size_t count;
    size_t invalid;
    // Invalid states are currently not supported
    STORM_SILENT_EXPECT_THROW(simulateDft(STORM_TEST_RESOURCES_DIR "/dft/mutex.dft", 1, 10000), storm::exceptions::NotSupportedException);
    // EXPECT_GE(invalid, 0);
    // double result = (double) count / (10000 - invalid);
    // EXPECT_NEAR(result, 0.8646647168, 0.01);
    STORM_SILENT_EXPECT_THROW(simulateDft(STORM_TEST_RESOURCES_DIR "/dft/mutex2.dft", 1, 10000), storm::exceptions::NotSupportedException);
    // EXPECT_EQ(invalid, 10000);
    // EXPECT_EQ(count, 0);
    std::tie(count, invalid) = simulateDft(STORM_TEST_RESOURCES_DIR "/dft/mutex3.dft", 1, 10000);
    EXPECT_EQ(invalid, 0ul);
    EXPECT_EQ(count, 0ul);
}

TEST(DftSimulatorTest, SymmetryUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/symmetry6.dft", 1, 10000);
    EXPECT_NEAR(result, 0.3421934224, 0.01);
}

TEST(DftSimulatorTest, HecsUnreliability) {
    double result = simulateDftProb(STORM_TEST_RESOURCES_DIR "/dft/hecs_2_2.dft", 1, 10000);
    EXPECT_NEAR(result, 0.00021997582, 0.001);
}

}  // namespace
