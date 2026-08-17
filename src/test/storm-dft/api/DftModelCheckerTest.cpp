#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-parsers/api/storm-parsers.h"

namespace {

// Configurations for DFT analysis
struct DftAnalysisConfig {
    bool useSR;
    bool useMod;
    bool useDC;
};

class NoOptimizationsConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{false, false, false};
    }
};

class DontCareOnlyConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{false, false, true};
    }
};

class ModularisationOnlyConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{false, true, false};
    }
};

class SymmetryReductionOnlyConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{true, false, false};
    }
};

class ModularisationConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{false, true, true};
    }
};

class SymmetryReductionConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{true, false, true};
    }
};

class AllOptimizationsConfig {
   public:
    typedef double ValueType;

    static DftAnalysisConfig createConfig() {
        return DftAnalysisConfig{true, true, true};
    }
};

// General base class for testing of DFT model checking
template<typename TestType>
class DftModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;

    DftModelCheckerTest() : config(TestType::createConfig()) {}

    DftAnalysisConfig const& getConfig() const {
        return config;
    }

    double analyze(std::string const& file, std::string const& property) {
        // Load, build and prepare DFT
        std::shared_ptr<storm::dft::storage::DFT<double>> dft =
            storm::dft::api::prepareForMarkovAnalysis<double>(*(storm::dft::api::loadDFTGalileoFile<double>(file)));
        EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);

        // Create property
        std::vector<std::shared_ptr<storm::logic::Formula const>> properties = storm::api::extractFormulasFromProperties(storm::api::parseProperties(property));

        // Create relevant names
        std::vector<std::string> relevantNames;
        if (!config.useDC) {
            relevantNames.push_back("all");
        }
        storm::dft::utility::RelevantEvents relevantEvents = storm::dft::api::computeRelevantEvents(properties, relevantNames);

        // Perform model checking
        typename storm::dft::modelchecker::DFTModelChecker<double>::dft_results results =
            storm::dft::api::analyzeDFT<double>(*dft, properties, config.useSR, config.useMod, relevantEvents, false);
        return boost::get<double>(results[0]);
    }

    double analyzeMTTF(std::string const& file) {
        std::string property = "Tmin=? [F \"failed\"]";
        return analyze(file, property);
    }

    double analyzeReliability(std::string const& file, double bound) {
        std::string property = "Pmin=? [F<=" + std::to_string(bound) + " \"failed\"]";
        return analyze(file, property);
    }

    double analyzeReachability(std::string const& file) {
        std::string property = "Pmin=? [F \"failed\"]";
        return analyze(file, property);
    }

    double precision() {
        return 1e-12;
    }

    double precisionReliability() {
        return 1e-10;
    }

   private:
    DftAnalysisConfig config;
};

typedef ::testing::Types<NoOptimizationsConfig, DontCareOnlyConfig, ModularisationOnlyConfig, SymmetryReductionOnlyConfig, ModularisationConfig,
                         SymmetryReductionConfig, AllOptimizationsConfig>
    TestingTypes;

TYPED_TEST_SUITE(DftModelCheckerTest, TestingTypes, );

TYPED_TEST(DftModelCheckerTest, AndMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/and.dft");
    EXPECT_NEAR(result, 3, this->precision());
}

TYPED_TEST(DftModelCheckerTest, OrMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/or.dft");
    EXPECT_NEAR(result, 1, this->precision());
}

TYPED_TEST(DftModelCheckerTest, VotingMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/voting.dft");
    EXPECT_NEAR(result, 5 / 3.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/voting2.dft");
    EXPECT_NEAR(result, 10 / 17.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/voting3.dft");
    EXPECT_NEAR(result, 2685 / 1547.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/voting4.dft");
    EXPECT_NEAR(result, 5 / 6.0, this->precision());
}

TYPED_TEST(DftModelCheckerTest, PandMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pand.dft");
    EXPECT_EQ(result, storm::utility::infinity<double>());
}

TYPED_TEST(DftModelCheckerTest, PorMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/por.dft");
    EXPECT_EQ(result, storm::utility::infinity<double>());
}

TYPED_TEST(DftModelCheckerTest, FdepMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep2.dft");
    EXPECT_NEAR(result, 2, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep3.dft");
    EXPECT_NEAR(result, 5 / 2.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep7.dft");
    EXPECT_NEAR(result, 5 / 12.0, this->precision());

    if (this->getConfig().useMod) {
        STORM_SILENT_EXPECT_THROW(this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep.dft"), storm::exceptions::NotSupportedException);
        STORM_SILENT_EXPECT_THROW(this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep4.dft"), storm::exceptions::NotSupportedException);
        STORM_SILENT_EXPECT_THROW(this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep5.dft"), storm::exceptions::NotSupportedException);
        STORM_SILENT_EXPECT_THROW(this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep6.dft"), storm::exceptions::NotSupportedException);
    } else {
        double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep.dft");
        EXPECT_NEAR(result, 2 / 3.0, this->precision());
        result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep4.dft");
        EXPECT_NEAR(result, 1, this->precision());
        result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep5.dft");
        EXPECT_NEAR(result, 3, this->precision());
        result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/fdep6.dft");
        EXPECT_NEAR(result, 9 / 56.0, this->precision());
    }
}

TYPED_TEST(DftModelCheckerTest, PdepMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pdep.dft");
    EXPECT_NEAR(result, 8 / 3.0, this->precision());
    if (this->getConfig().useMod && !this->getConfig().useDC) {
        STORM_SILENT_EXPECT_THROW(this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pdep2.dft"), storm::exceptions::NotSupportedException);
    } else {
        result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pdep2.dft");
        EXPECT_NEAR(result, 38 / 15.0, this->precision());
    }
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pdep3.dft");
    EXPECT_NEAR(result, 67 / 24.0, this->precision());

    if (this->getConfig().useMod) {
        STORM_SILENT_EXPECT_THROW(this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pdep4.dft"), storm::exceptions::NotSupportedException);
    } else {
        result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pdep4.dft");
        EXPECT_EQ(result, storm::utility::infinity<double>());
    }
}

TYPED_TEST(DftModelCheckerTest, SpareMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare.dft");
    EXPECT_NEAR(result, 46 / 13.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare2.dft");
    EXPECT_NEAR(result, 43 / 23.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare3.dft");
    EXPECT_NEAR(result, 14 / 11.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare4.dft");
    EXPECT_NEAR(result, 18836 / 3887.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare5.dft");
    EXPECT_NEAR(result, 8 / 3.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare6.dft");
    EXPECT_NEAR(result, 7 / 5.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare7.dft");
    EXPECT_NEAR(result, 551 / 150.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare8.dft");
    EXPECT_NEAR(result, 249 / 52.0, this->precision());  // DFTCalc has result of 4.33779 due to different semantics of nested spares
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare_dc.dft");
    EXPECT_NEAR(result, 78311 / 182700.0, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/modules3.dft");
    EXPECT_NEAR(result, 7 / 6.0, this->precision());
    // Spare gate is part of spare module. As a result it is not activated.
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/spare_contains_spare.dft");
    EXPECT_NEAR(result, 1, this->precision());
}

TYPED_TEST(DftModelCheckerTest, SeqMTTF) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq.dft");
    EXPECT_NEAR(result, 4, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq2.dft");
    EXPECT_NEAR(result, 6, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq3.dft");
    EXPECT_NEAR(result, 6, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq4.dft");
    EXPECT_NEAR(result, 6, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq5.dft");
    EXPECT_EQ(result, storm::utility::infinity<double>());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq6.dft");
    EXPECT_NEAR(result, 30000, this->precision());

    if (this->getConfig().useMod) {
        STORM_SILENT_EXPECT_THROW(this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq7.dft"), storm::exceptions::NotSupportedException);
    } else {
        result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq7.dft");
        EXPECT_EQ(result, storm::utility::infinity<double>());
    }
    result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/seq7.dft", 1.0);
    EXPECT_NEAR(result, 0.08, this->precision());
    result = this->analyzeReachability(STORM_TEST_RESOURCES_DIR "/dft/seq7.dft");
    EXPECT_NEAR(result, 0.08, this->precision());
    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/seq8.dft");
    EXPECT_NEAR(result, 11 / 8.0, this->precision());
}

TYPED_TEST(DftModelCheckerTest, Mutex) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/mutex.dft");
    EXPECT_NEAR(result, 1 / 2.0, this->precision());
    result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/mutex.dft", 1.0);
    EXPECT_NEAR(result, 0.8646647168, this->precisionReliability());
    result = this->analyzeReachability(STORM_TEST_RESOURCES_DIR "/dft/mutex.dft");
    EXPECT_NEAR(result, 1, this->precision());

    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/mutex2.dft");
    EXPECT_EQ(result, storm::utility::infinity<double>());
    result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/mutex2.dft", 1.0);
    EXPECT_NEAR(result, 0, this->precision());
    result = this->analyzeReachability(STORM_TEST_RESOURCES_DIR "/dft/mutex2.dft");
    EXPECT_NEAR(result, 0, this->precision());

    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/mutex3.dft");
    EXPECT_EQ(result, storm::utility::infinity<double>());
    result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/mutex3.dft", 1.0);
    EXPECT_NEAR(result, 0, this->precision());
    result = this->analyzeReachability(STORM_TEST_RESOURCES_DIR "/dft/mutex3.dft");
    EXPECT_NEAR(result, 0, this->precision());

    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/mutex4.dft");
    EXPECT_EQ(result, storm::utility::infinity<double>());
    result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/mutex4.dft", 1.0);
    EXPECT_NEAR(result, 0.5842978146, this->precisionReliability());
    result = this->analyzeReachability(STORM_TEST_RESOURCES_DIR "/dft/mutex4.dft");
    EXPECT_NEAR(result, 5 / 8.0, this->precision());
}

TYPED_TEST(DftModelCheckerTest, Symmetry) {
    double result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/symmetry6.dft");
    EXPECT_NEAR(result, 2804183 / 2042040.0, this->precision());
    result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/symmetry6.dft", 1.0);
    EXPECT_NEAR(result, 0.3421934224, this->precisionReliability());

    result = this->analyzeMTTF(STORM_TEST_RESOURCES_DIR "/dft/pdep_symmetry.dft");
    EXPECT_NEAR(result, 6553 / 5376.0, this->precision());
    result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/pdep_symmetry.dft", 1.0);
    EXPECT_NEAR(result, 0.4223514414, this->precisionReliability());
}

TYPED_TEST(DftModelCheckerTest, HecsReliability) {
    if (!this->getConfig().useDC) {
        // Skip configurations because it takes too long
        GTEST_SKIP();
        return;
    }
    double result = this->analyzeReliability(STORM_TEST_RESOURCES_DIR "/dft/hecs_2_2.dft", 1.0);
    EXPECT_NEAR(result, 0.00021997582, this->precisionReliability());
}
}  // namespace
