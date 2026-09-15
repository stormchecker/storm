#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/analysis.h"
#include "storm-dft/api/io.h"
#include "storm-dft/api/transformation.h"
#include "storm-dft/environment/AnalysisEnvironment.h"
#include "storm-dft/environment/DftEnvironment.h"
#include "storm-dft/environment/ModelBuilderEnvironment.h"
#include "storm-dft/environment/TransformationEnvironment.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/properties.h"
#include "storm/storage/jani/Property.h"

namespace {

// Configurations for DFT approximation
class ApproxDepthConfig {
   public:
    typedef double ValueType;

    static storm::dft::DftEnvironment createEnvironment() {
        storm::dft::DftEnvironment env;
        env.modelBuilder().setUseSymmetryReduction(false);
        env.modelBuilder().setAllowDCForRelevantEvents(false);
        env.analysis().setUseModularisation(false);
        env.analysis().setApproximationHeuristic(storm::dft::builder::ApproximationHeuristic::DEPTH);
        env.transformation().setEliminateChains(false);
        return env;
    }
};

class ApproxProbabilityConfig {
   public:
    typedef double ValueType;

    static storm::dft::DftEnvironment createEnvironment() {
        storm::dft::DftEnvironment env;
        env.modelBuilder().setUseSymmetryReduction(false);
        env.modelBuilder().setAllowDCForRelevantEvents(false);
        env.analysis().setUseModularisation(false);
        env.analysis().setApproximationHeuristic(storm::dft::builder::ApproximationHeuristic::PROBABILITY);
        env.transformation().setEliminateChains(false);
        return env;
    }
};

class ApproxBoundDifferenceConfig {
   public:
    typedef double ValueType;

    static storm::dft::DftEnvironment createEnvironment() {
        storm::dft::DftEnvironment env;
        env.modelBuilder().setUseSymmetryReduction(false);
        env.modelBuilder().setAllowDCForRelevantEvents(false);
        env.analysis().setUseModularisation(false);
        env.analysis().setApproximationHeuristic(storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE);
        env.transformation().setEliminateChains(false);
        return env;
    }
};

// General base class for testing of DFT approximation
template<typename TestType>
class DftApproximationTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;

    DftApproximationTest() : _environment(TestType::createEnvironment()) {}

    std::pair<double, double> analyzeMTTF(std::string const& file, double errorBound) const {
        std::shared_ptr<storm::dft::storage::DFT<double>> dft =
            storm::dft::api::prepareForMarkovAnalysis<double>(*storm::dft::api::loadDFTGalileoFile<double>(file));
        EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
        std::string property = "T=? [F \"failed\"]";
        std::vector<std::shared_ptr<storm::logic::Formula const>> properties = storm::api::extractFormulasFromProperties(storm::api::parseProperties(property));
        storm::dft::DftEnvironment env = this->env();
        env.analysis().setApproximationError(errorBound);
        typename storm::dft::modelchecker::DFTModelChecker<double>::dft_results results =
            storm::dft::api::analyzeDFT<double>(env, *dft, properties, storm::dft::utility::RelevantEvents());
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
        storm::dft::DftEnvironment env = this->env();
        env.analysis().setApproximationError(errorBound);
        typename storm::dft::modelchecker::DFTModelChecker<double>::dft_results results =
            storm::dft::api::analyzeDFT<double>(env, *dft, properties, storm::dft::utility::RelevantEvents());
        return boost::get<storm::dft::modelchecker::DFTModelChecker<double>::approximation_result>(results[0]);
    }

    storm::dft::DftEnvironment const& env() const {
        return _environment;
    }

   private:
    storm::dft::DftEnvironment _environment;
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
