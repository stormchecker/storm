#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/analysis.h"
#include "storm-dft/api/io.h"
#include "storm-dft/api/transformation.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/properties.h"
#include "storm/storage/jani/Property.h"

namespace {

// Configurations for DFT approximation
struct DftAnalysisConfig {
    storm::dft::builder::ApproximationHeuristic heuristic;
    bool useSR;
};

class ApproxDepthConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{storm::dft::builder::ApproximationHeuristic::DEPTH, false};
    }
};

class ApproxProbabilityConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{storm::dft::builder::ApproximationHeuristic::PROBABILITY, false};
    }
};

class ApproxBoundDifferenceConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE, false};
    }
};

// General base class for testing of DFT approximation
template<typename TestType>
class DftApproximationTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;

    DftApproximationTest() : config(TestType::createConfig()) {}

    DftApproximationTest const& getConfig() const {
        return config;
    }

    std::pair<double, double> analyzeMTTF(std::string const& file, double errorBound) const {
        std::shared_ptr<storm::dft::storage::DFT<double>> dft =
            storm::dft::api::prepareForMarkovAnalysis<double>(*storm::dft::api::loadDFTGalileoFile<double>(file));
        EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
        std::string property = "T=? [F \"failed\"]";
        std::vector<std::shared_ptr<storm::logic::Formula const>> properties = storm::api::extractFormulasFromProperties(storm::api::parseProperties(property));
        typename storm::dft::modelchecker::DFTModelChecker<double>::dft_results results = storm::dft::api::analyzeDFT<double>(
            *dft, properties, config.useSR, false, storm::dft::utility::RelevantEvents(), false, errorBound, config.heuristic, false);
        return boost::get<storm::dft::modelchecker::DFTModelChecker<double>::approximation_result>(results[0]);
    }

    std::pair<double, double> analyzeTimebound(std::string const& file, double timeBound, double errorBound) const {
        std::shared_ptr<storm::dft::storage::DFT<double>> dft =
            storm::dft::api::prepareForMarkovAnalysis<double>(*storm::dft::api::loadDFTGalileoFile<double>(file));
        EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
        std::stringstream propertyStream;
        propertyStream << "P=? [F<=" << timeBound << " \"failed\"]";
        std::vector<std::shared_ptr<storm::logic::Formula const>> properties =
            storm::api::extractFormulasFromProperties(storm::api::parseProperties(propertyStream.str()));
        typename storm::dft::modelchecker::DFTModelChecker<double>::dft_results results = storm::dft::api::analyzeDFT<double>(
            *dft, properties, config.useSR, false, storm::dft::utility::RelevantEvents(), false, errorBound, config.heuristic, false);
        return boost::get<storm::dft::modelchecker::DFTModelChecker<double>::approximation_result>(results[0]);
    }

   private:
    DftAnalysisConfig config;
};

typedef ::testing::Types<ApproxDepthConfig, ApproxProbabilityConfig, ApproxBoundDifferenceConfig> TestingTypes;

TYPED_TEST_SUITE(DftApproximationTest, TestingTypes, );

TYPED_TEST(DftApproximationTest, HecsMTTF) {
    double errorBound = 2;
    std::pair<double, double> approxResult = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/hecs_3_2_2_np.dft", errorBound);
    EXPECT_LE(approxResult.first, 417.9436693);
    EXPECT_GE(approxResult.second, 417.9436693);
    EXPECT_LE(2 * (approxResult.second - approxResult.first) / (approxResult.first + approxResult.second), errorBound);
    // Ensure results are not equal -> not exact values were computed
    EXPECT_GE(approxResult.second - approxResult.first, errorBound * approxResult.first / 10);
}

TYPED_TEST(DftApproximationTest, HecsTimebound) {
    // double errorBound = 0.01;
    double errorBound = 0.1;
    double timeBound = 100;
    std::pair<double, double> approxResult = this->analyzeTimebound(STORM_TEST_RESOURCES_DIR "/dft/hecs_3_2_2_np.dft", timeBound, errorBound);
    EXPECT_LE(approxResult.first, 0.0410018417);
    EXPECT_GE(approxResult.second, 0.0410018417);
    EXPECT_LE(approxResult.second - approxResult.first, errorBound);
    // Ensure results are not equal -> not exact values were computed
    EXPECT_GE(approxResult.second - approxResult.first, errorBound / 10);
}

}  // namespace
