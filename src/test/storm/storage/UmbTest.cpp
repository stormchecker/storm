#include <filesystem>
#include <limits>
#include <random>
#include <span>
#include <vector>

#include "storm-config.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/storage/umb/export/SparseModelToUmb.h"
#include "storm/storage/umb/export/UmbExport.h"
#include "storm/storage/umb/import/SparseModelFromUmb.h"
#include "storm/storage/umb/import/UmbImport.h"
#include "storm/storage/umb/model/UmbModel.h"
#include "storm/storage/umb/model/ValueEncoding.h"
#include "storm/utility/constants.h"
#include "test/storm_gtest.h"
/*!
 *  Test round trip encoding and decoding of umb
 */
namespace {
class UmbRoundTripTest : public ::testing::Test {
   protected:
    std::filesystem::path umbFile;
    void removeUmbFile() {
        if (!umbFile.empty() && std::filesystem::exists(umbFile)) {
            std::error_code ec;
            std::filesystem::remove(umbFile);
            ASSERT_EQ(ec.value(), 0) << "Unable to remove temporary file " << umbFile << " for UMB round trip test: " << ec.message();
        }
    }

    void setUpUmbFileName() {
        std::error_code ec;
        auto tmp = std::filesystem::temp_directory_path(ec);
        ASSERT_EQ(ec.value(), 0) << "Unable to get temporary directory for UMB round trip test: " << ec.message();
        std::random_device rd;
        do {
            umbFile = tmp / std::filesystem::path("storm_umb_round_trip_test_" + std::to_string(rd()) + ".umb");
        } while (std::filesystem::exists(umbFile));
    }

    template<typename ValueType>
    void run(std::filesystem::path const& prismfile, std::string const& constants = "", storm::umb::ExportOptions const& exportOptions = {}) {
        setUpUmbFileName();
        // Set-up options structs
        storm::umb::ImportOptions importOptions;
        importOptions.buildChoiceLabeling = exportOptions.allowChoiceLabelingAsActions || exportOptions.allowChoiceOriginsAsActions;
        storm::generator::NextStateGeneratorOptions generatorOptions;
        generatorOptions.setBuildChoiceLabels(exportOptions.allowChoiceLabelingAsActions);
        generatorOptions.setBuildChoiceOrigins(exportOptions.allowChoiceOriginsAsActions);
        generatorOptions.setBuildAllRewardModels();
        generatorOptions.setBuildAllLabels();
        // TODO: valuations

        // build model from prism file
        storm::prism::Program program = storm::parser::PrismParser::parse(prismfile, true);
        program = storm::utility::prism::preprocess(program, constants);
        auto builder = storm::builder::ExplicitModelBuilder<ValueType>(program, generatorOptions);
        auto model = builder.build();

        auto assertEqualModel = [&model](auto const& otherModelPtr) {
            ASSERT_TRUE(otherModelPtr) << "No model.";
            EXPECT_EQ(model->getType(), otherModelPtr->getType());
            EXPECT_EQ(model->getNumberOfStates(), otherModelPtr->getNumberOfStates());
            EXPECT_EQ(model->getNumberOfChoices(), otherModelPtr->getNumberOfChoices());
            EXPECT_EQ(model->getNumberOfTransitions(), otherModelPtr->getNumberOfTransitions());
            if (!model->isOfType(storm::models::ModelType::Ctmc) || std::is_same_v<ValueType, storm::RationalNumber>) {
                // Round trip in CTMC case may yield slightly different probabilities because we translate between rates and probabilities
                EXPECT_EQ(model->getTransitionMatrix(), otherModelPtr->getTransitionMatrix());
            }
            EXPECT_EQ(model->getStateLabeling(), otherModelPtr->getStateLabeling());
            if (model->isNondeterministicModel() && model->hasChoiceLabeling()) {
                // This test does not work for deterministic models as we might fuse multiple (overlapping) choice labels together
                EXPECT_EQ(model->getChoiceLabeling(), otherModelPtr->getChoiceLabeling());
            }
            EXPECT_EQ(model->getNumberOfRewardModels(), otherModelPtr->getNumberOfRewardModels());
            for (auto const& [name, rewardModel] : model->getRewardModels()) {
                ASSERT_TRUE(otherModelPtr->hasRewardModel(name) || model->hasUniqueRewardModel()) << "Other model does not have reward model '" << name << "'.";
                auto const& otherRewardModel = model->hasUniqueRewardModel() ? otherModelPtr->getUniqueRewardModel() : otherModelPtr->getRewardModel(name);
                if (rewardModel.hasStateRewards()) {
                    ASSERT_TRUE(otherRewardModel.hasStateRewards());
                    EXPECT_EQ(rewardModel.getStateRewardVector(), otherRewardModel.getStateRewardVector());
                }
                if (rewardModel.hasStateActionRewards()) {
                    ASSERT_TRUE(otherRewardModel.hasStateActionRewards());
                    EXPECT_EQ(rewardModel.getStateActionRewardVector(), otherRewardModel.getStateActionRewardVector());
                }
                if (rewardModel.hasTransitionRewards()) {
                    ASSERT_TRUE(otherRewardModel.hasTransitionRewards());
                    EXPECT_EQ(rewardModel.getTransitionRewardMatrix(), otherRewardModel.getTransitionRewardMatrix());
                }
            }
        };

        // Short round trip: model -> umb -> model
        auto umb1 = storm::umb::sparseModelToUmb(*model, exportOptions);
        umb1.encodeRationals();
        std::stringstream validationErrors;
        ASSERT_TRUE(umb1.validate(validationErrors)) << validationErrors.str();
        validationErrors.clear();
        auto model1 = storm::umb::sparseModelFromUmb<ValueType>(umb1, importOptions);
        assertEqualModel(model1);

        // long round trip: model -> umb -> file -> umb -> model
        storm::umb::toArchive(umb1, umbFile, exportOptions);
        auto umb2 = storm::umb::importUmb(umbFile, importOptions);
        ASSERT_TRUE(umb2.validate(validationErrors)) << validationErrors.str();
        validationErrors.clear();
        auto model2 = storm::umb::sparseModelFromUmb<ValueType>(umb2, importOptions);
        assertEqualModel(model2);
        removeUmbFile();
    }

    virtual void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
#ifndef STORM_HAVE_LIBARCHIVE
        GTEST_SKIP() << "LibArchive not available.";
#endif
    }

    virtual void TearDown() override {
        removeUmbFile();
    }
};

TEST_F(UmbRoundTripTest, brp_dtmc) {
    storm::umb::ExportOptions options;
    run<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "", options);
    run<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "", options);
    run<storm::Interval>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "", options);
    run<storm::RationalInterval>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "", options);
    options.compression = storm::io::CompressionMode::Gzip;
    run<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "", options);
    options.compression = storm::io::CompressionMode::Xz;
    run<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "", options);
    options.compression = storm::io::CompressionMode::None;
    run<double>(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm", "", options);
}

TEST_F(UmbRoundTripTest, embedded_ctmc) {
    storm::umb::ExportOptions options;
    run<double>(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", "", options);
    run<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/ma/polling.ma", "N=3,Q=3", options);
    run<storm::Interval>(STORM_TEST_RESOURCES_DIR "/ma/polling.ma", "N=3,Q=3", options);
    run<storm::RationalInterval>(STORM_TEST_RESOURCES_DIR "/ma/polling.ma", "N=3,Q=3", options);
}

TEST_F(UmbRoundTripTest, firewire_mdp) {
    storm::umb::ExportOptions options;
    run<double>(STORM_TEST_RESOURCES_DIR "/mdp/firewire3-0.5.nm", "", options);
    run<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/mdp/firewire3-0.5.nm", "", options);
    run<storm::Interval>(STORM_TEST_RESOURCES_DIR "/mdp/firewire3-0.5.nm", "", options);
    run<storm::RationalInterval>(STORM_TEST_RESOURCES_DIR "/mdp/firewire3-0.5.nm", "", options);
    options.allowChoiceOriginsAsActions = true;
    options.allowChoiceLabelingAsActions = false;
    run<double>(STORM_TEST_RESOURCES_DIR "/mdp/firewire3-0.5.nm", "", options);
}

TEST_F(UmbRoundTripTest, polling_ma) {
    storm::umb::ExportOptions options;
    run<double>(STORM_TEST_RESOURCES_DIR "/ma/polling.ma", "N=3,Q=3", options);
    run<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/ma/polling.ma", "N=3,Q=3", options);
    run<storm::Interval>(STORM_TEST_RESOURCES_DIR "/ma/polling.ma", "N=3,Q=3", options);
    run<storm::RationalInterval>(STORM_TEST_RESOURCES_DIR "/ma/polling.ma", "N=3,Q=3", options);
}

TEST_F(UmbRoundTripTest, robot_imdp) {
    storm::umb::ExportOptions options;
    run<storm::Interval>(STORM_TEST_RESOURCES_DIR "/imdp/robot.prism", "delta=0.5", options);
    run<storm::RationalInterval>(STORM_TEST_RESOURCES_DIR "/imdp/robot.prism", "delta=0.5", options);
}

TEST_F(UmbRoundTripTest, maze_pomdp) {
    storm::umb::ExportOptions options;
    run<double>(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.5", options);
    run<storm::RationalNumber>(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.5", options);
    run<storm::Interval>(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.5", options);
    run<storm::RationalInterval>(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.5", options);
}

}  // namespace

TEST(UmbTest, RationalEncoding) {
    auto const one = storm::utility::one<storm::RationalNumber>();
    auto const int64max = storm::utility::convertNumber<storm::RationalNumber, int64_t>(std::numeric_limits<int64_t>::max());
    auto const int64min = storm::utility::convertNumber<storm::RationalNumber, int64_t>(std::numeric_limits<int64_t>::min());
    auto const uint64max = storm::utility::convertNumber<storm::RationalNumber, uint64_t>(std::numeric_limits<uint64_t>::max());

    std::vector<storm::RationalNumber> values(17);
    // The first 8 values are chosen such that they can be represented with two 64-bit numbers.
    values[0] = storm::utility::zero<storm::RationalNumber>();
    values[1] = -storm::utility::zero<storm::RationalNumber>();
    values[2] = one;
    values[3] = -one;
    values[4] = storm::utility::convertNumber<storm::RationalNumber, std::string>("123/456");
    values[5] = -storm::utility::convertNumber<storm::RationalNumber, std::string>("123/456");
    values[6] = int64max / uint64max;
    values[7] = int64min / uint64max;

    auto const simpleRationals = std::span<storm::RationalNumber>(values.data(), 8);
    ASSERT_EQ(128ull, storm::umb::ValueEncoding::getMinimalRationalSize(simpleRationals, false));
    ASSERT_EQ(128ull, storm::umb::ValueEncoding::getMinimalRationalSize(simpleRationals, true));
    auto encoded1 = storm::umb::ValueEncoding::createUint64FromRationalRange(simpleRationals, 128ull);
    auto decoded1 = storm::umb::ValueEncoding::uint64ToRationalRangeView(encoded1, 128ull);
    ASSERT_EQ(simpleRationals.size(), decoded1.size());
    for (size_t i = 0; i < simpleRationals.size(); ++i) {
        EXPECT_EQ(simpleRationals[i], decoded1[i]) << " at index " << i;
    }

    // The following values are chosen such that they are not representable with two 64-bit numbers.
    values[8] = int64max + one;
    values[9] = one / (uint64max + one);
    values[10] = int64min - one;
    values[11] = one / (int64min - one);
    values[12] = (int64min - one) / (uint64max + one);
    values[13] = storm::utility::convertNumber<storm::RationalNumber, std::string>(
        "949667607787274453086419753000949667607787274453086419753000949667607787274453086419753000949667607787274453086419753000949667607787274453086419753000"
        "9496676077872744530864197530009496676077872744530864197530009496676077872744530864197530/"
        "780116505469339517040847240228739241622101546262265311616467711470010820006007800398204693387501962318501358930877102188539546463329577703105788853954"
        "134811616465520508472358467546262155762385699576193087775947700108638258539546825539241654967");
    values[14] = one / values[13];
    values[15] = -values[13];
    values[16] = -values[14];

    ASSERT_EQ(1616ull, storm::umb::ValueEncoding::getMinimalRationalSize(values, false));
    ASSERT_EQ(1664ull, storm::umb::ValueEncoding::getMinimalRationalSize(values, true));
    auto encoded2 = storm::umb::ValueEncoding::createUint64FromRationalRange(values, 1664ull);
    auto decoded2 = storm::umb::ValueEncoding::uint64ToRationalRangeView(encoded2, 1664ull);

    ASSERT_EQ(values.size(), decoded2.size());
    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(values[i], decoded2[i]) << " at index " << i;
    }
}
