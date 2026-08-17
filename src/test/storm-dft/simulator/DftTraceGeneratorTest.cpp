#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/generator/DftNextStateGenerator.h"
#include "storm-dft/simulator/DFTTraceSimulator.h"
#include "storm-dft/utility/SymmetryFinder.h"
#include "storm-parsers/api/storm-parsers.h"

namespace {

// Configurations for DFT traces
struct DftTracesConfig {
    bool useDC;
    bool useSR;
};

class NoOptimizationsConfig {
   public:
    typedef double ValueType;

    static DftTracesConfig createConfig() {
        return DftTracesConfig{false, false};
    }
};

class DontCareConfig {
   public:
    typedef double ValueType;

    static DftTracesConfig createConfig() {
        return DftTracesConfig{true, false};
    }
};

class SymmetryReductionConfig {
   public:
    typedef double ValueType;

    static DftTracesConfig createConfig() {
        return DftTracesConfig{false, true};
    }
};

class AllOptimizationsConfig {
   public:
    typedef double ValueType;

    static DftTracesConfig createConfig() {
        return DftTracesConfig{true, true};
    }
};

// General base class for testing of generating DFT traces.
template<typename TestType>
class DftTraceGeneratorTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;

    DftTraceGeneratorTest() : config(TestType::createConfig()) {}

    DftTracesConfig const& getConfig() const {
        return config;
    }

    std::pair<std::shared_ptr<storm::dft::storage::DFT<double>>, storm::dft::storage::DFTStateGenerationInfo> prepareDFT(std::string const& file) {
        // Load, build and prepare DFT
        std::shared_ptr<storm::dft::storage::DFT<double>> dft =
            storm::dft::api::prepareForMarkovAnalysis<double>(*(storm::dft::api::loadDFTGalileoFile<double>(file)));
        EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);

        // Compute relevant events
        std::vector<std::string> relevantNames;
        if (!config.useDC) {
            relevantNames.push_back("all");
        }
        storm::dft::utility::RelevantEvents relevantEvents = storm::dft::api::computeRelevantEvents({}, relevantNames);
        dft->setRelevantEvents(relevantEvents, false);

        // Find symmetries
        storm::dft::storage::DftSymmetries symmetries;
        if (config.useSR) {
            symmetries = storm::dft::utility::SymmetryFinder<double>::findSymmetries(*dft);
        }
        storm::dft::storage::DFTStateGenerationInfo stateGenerationInfo(dft->buildStateGenerationInfo(symmetries));
        return std::make_pair(dft, stateGenerationInfo);
    }

   private:
    DftTracesConfig config;
};

typedef ::testing::Types<NoOptimizationsConfig, DontCareConfig, SymmetryReductionConfig, AllOptimizationsConfig> TestingTypes;

TYPED_TEST_SUITE(DftTraceGeneratorTest, TestingTypes, );

TYPED_TEST(DftTraceGeneratorTest, And) {
    auto pair = this->prepareDFT(STORM_TEST_RESOURCES_DIR "/dft/and.dft");
    auto dft = pair.first;
    EXPECT_EQ(this->getConfig().useSR && this->getConfig().useDC, pair.second.hasSymmetries());
    storm::dft::generator::DftNextStateGenerator<double> generator(*dft, pair.second);

    // Start with initial state
    auto state = generator.createInitialState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));

    bool changed = state->orderBySymmetry();
    EXPECT_FALSE(changed);

    // Let C fail
    auto iterFailable = state->getFailableElements().begin();
    ASSERT_NE(iterFailable, state->getFailableElements().end());
    ++iterFailable;
    ASSERT_NE(iterFailable, state->getFailableElements().end());
    EXPECT_FALSE(iterFailable.isFailureDueToDependency());

    auto nextBE = iterFailable.asBE(*dft);
    EXPECT_EQ(nextBE->name(), "C");
    state = generator.createSuccessorState(state, nextBE);
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));
    changed = state->orderBySymmetry();
    EXPECT_EQ(this->getConfig().useSR && this->getConfig().useDC, changed);
    if (this->getConfig().useSR && this->getConfig().useDC) {
        EXPECT_TRUE(state->hasFailed(0));
    } else {
        EXPECT_TRUE(state->hasFailed(1));
    }

    // Let B fail
    iterFailable = state->getFailableElements().begin();
    ASSERT_NE(iterFailable, state->getFailableElements().end());
    EXPECT_FALSE(iterFailable.isFailureDueToDependency());

    nextBE = iterFailable.asBE(*dft);
    if (this->getConfig().useSR && this->getConfig().useDC) {
        // TODO: Apply symmetry to failable elements as well
        return;
        EXPECT_EQ(nextBE->name(), "C");
    } else {
        EXPECT_EQ(nextBE->name(), "B");
    }
    state = generator.createSuccessorState(state, nextBE);
    changed = state->orderBySymmetry();
    EXPECT_FALSE(changed);
    EXPECT_TRUE(state->hasFailed(dft->getTopLevelIndex()));
}

TYPED_TEST(DftTraceGeneratorTest, RandomStepsAnd) {
    auto pair = this->prepareDFT(STORM_TEST_RESOURCES_DIR "/dft/and.dft");
    auto dft = pair.first;
    EXPECT_EQ(this->getConfig().useSR && this->getConfig().useDC, pair.second.hasSymmetries());

    // Init random number generator
    boost::mt19937 gen(5u);
    storm::dft::simulator::DFTTraceSimulator<double> simulator(*dft, pair.second, gen);

    auto state = simulator.getCurrentState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));

    // First random step
    storm::dft::simulator::SimulationStepResult res = simulator.randomStep();
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
#if BOOST_VERSION > 106400
    // Older Boost versions yield different value
    EXPECT_NEAR(simulator.getCurrentTime(), 0.522079, 1e-6);
#endif
    state = simulator.getCurrentState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));

    res = simulator.randomStep();
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
#if BOOST_VERSION > 106400
    // Older Boost versions yield different value
    EXPECT_NEAR(simulator.getCurrentTime(), 0.522079 + 0.9497214, 1e-6);
#endif
    state = simulator.getCurrentState();
    EXPECT_TRUE(state->hasFailed(dft->getTopLevelIndex()));
}

TYPED_TEST(DftTraceGeneratorTest, Reset) {
    auto pair = this->prepareDFT(STORM_TEST_RESOURCES_DIR "/dft/and.dft");
    auto dft = pair.first;
    EXPECT_EQ(this->getConfig().useSR && this->getConfig().useDC, pair.second.hasSymmetries());

    // Init random number generator
    boost::mt19937 gen(5u);
    storm::dft::simulator::DFTTraceSimulator<double> simulator(*dft, pair.second, gen);

    auto state = simulator.getCurrentState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));

    // First random step
    storm::dft::simulator::SimulationStepResult res = simulator.randomStep();
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
#if BOOST_VERSION > 106400
    // Older Boost versions yield different value
    EXPECT_NEAR(simulator.getCurrentTime(), 0.522079, 1e-6);
#endif
    auto stateStep1 = simulator.getCurrentState();
    EXPECT_FALSE(stateStep1->hasFailed(dft->getTopLevelIndex()));

    res = simulator.randomStep();
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
#if BOOST_VERSION > 106400
    // Older Boost versions yield different value
    EXPECT_NEAR(simulator.getCurrentTime(), 0.522079 + 0.9497214, 1e-6);
#endif
    state = simulator.getCurrentState();
    EXPECT_TRUE(state->hasFailed(dft->getTopLevelIndex()));

    // Reset to previous state
    simulator.resetToState(stateStep1);
    state = simulator.getCurrentState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));

    res = simulator.randomStep();
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
#if BOOST_VERSION > 106400
    // Older Boost versions yield different value
    EXPECT_NEAR(simulator.getCurrentTime(), 0.522079 + 0.9497214 + 2.4686932, 1e-6);
#endif
    state = simulator.getCurrentState();
    EXPECT_TRUE(state->hasFailed(dft->getTopLevelIndex()));
}

TYPED_TEST(DftTraceGeneratorTest, Fdep) {
    auto pair = this->prepareDFT(STORM_TEST_RESOURCES_DIR "/dft/fdep.dft");
    auto dft = pair.first;

    // Init random number generator. Will not be important as we are choosing the steps deterministically.
    boost::mt19937 gen(5u);
    storm::dft::simulator::DFTTraceSimulator<double> simulator(*dft, pair.second, gen);

    // Start with initial state
    auto state = simulator.getCurrentState();
    EXPECT_FALSE(state->hasFailed(dft->getTopLevelIndex()));

    // Let B_Power fail
    auto iterFailable = state->getFailableElements().begin();
    ASSERT_NE(iterFailable, state->getFailableElements().end());
    ++iterFailable;
    ASSERT_NE(iterFailable, state->getFailableElements().end());
    ++iterFailable;
    ASSERT_NE(iterFailable, state->getFailableElements().end());

    EXPECT_FALSE(iterFailable.isFailureDueToDependency());
    auto nextBE = iterFailable.asBE(*dft);
    EXPECT_EQ(nextBE->name(), "B_Power");

    storm::dft::simulator::SimulationStepResult res = simulator.step(iterFailable);
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
    state = simulator.getCurrentState();
    EXPECT_TRUE(state->hasFailed(4));

    // Let B fail
    iterFailable = state->getFailableElements().begin();
    ASSERT_NE(iterFailable, state->getFailableElements().end());
    ++iterFailable;
    ASSERT_NE(iterFailable, state->getFailableElements().end());

    EXPECT_TRUE(iterFailable.isFailureDueToDependency());
    auto dependency = iterFailable.asDependency(*dft);
    EXPECT_EQ(dependency->dependentEvents().front()->name(), "B");

    res = simulator.step(iterFailable);
    EXPECT_EQ(res, storm::dft::simulator::SimulationStepResult::SUCCESSFUL);
    state = simulator.getCurrentState();
    EXPECT_TRUE(state->hasFailed(dft->getTopLevelIndex()));
}

}  // namespace
