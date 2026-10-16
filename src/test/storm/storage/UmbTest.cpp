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
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/storage/umb/export/SparseModelToUmb.h"
#include "storm/storage/umb/export/UmbExport.h"
#include "storm/storage/umb/import/SparseModelFromUmb.h"
#include "storm/storage/umb/import/UmbImport.h"
#include "storm/storage/umb/model/StringEncoding.h"
#include "storm/storage/umb/model/UmbModel.h"
#include "storm/storage/umb/model/ValueEncoding.h"
#include "storm/storage/valuations/ValuationsStorage.h"
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
        generatorOptions.setBuildStateValuations(true);
        generatorOptions.setBuildObservationValuations(true);

        // build model from prism file
        storm::prism::Program program = storm::parser::PrismParser::parse(prismfile, true);
        program = program.preprocess(constants);
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
            auto assertEqualValuations = [](auto const& v, auto const& other_v) {
                ASSERT_EQ(v.size(), other_v.size());
                EXPECT_EQ(0, v.numStrings());
                EXPECT_EQ(0, other_v.numStrings());
                ASSERT_EQ(1, v.numClasses());
                ASSERT_EQ(1, other_v.numClasses());
                ASSERT_EQ(v.getClassDescription().sizeInBits(), other_v.getClassDescription().sizeInBits());
                storm::umb::UmbModel::Valuation data = v.getRawUmbData();
                storm::umb::UmbModel::Valuation other_data = other_v.getRawUmbData();
                EXPECT_EQ(data.valuations, other_data.valuations);
                EXPECT_EQ(data.stringMapping, other_data.stringMapping);
                EXPECT_EQ(data.strings == other_data.strings, true);
                EXPECT_EQ(data.valuationToClass, other_data.valuationToClass);
            };
            ASSERT_TRUE(model->hasStateValuations());
            ASSERT_TRUE(otherModelPtr->hasStateValuations());
            assertEqualValuations(model->getStateValuations().getStorage(), otherModelPtr->getStateValuations().getStorage());
            // POMDP specific things
            if (model->isPartiallyObservable()) {
                ASSERT_TRUE(model->isOfType(storm::models::ModelType::Pomdp));
                auto pomdp = model->template as<storm::models::sparse::Pomdp<ValueType>>();
                auto otherPomdp = otherModelPtr->template as<storm::models::sparse::Pomdp<ValueType>>();
                EXPECT_EQ(pomdp->getObservations(), otherPomdp->getObservations());
                ASSERT_TRUE(pomdp->hasObservationValuations());
                ASSERT_TRUE(otherPomdp->hasObservationValuations());
                assertEqualValuations(pomdp->getObservationValuations().getStorage(), otherPomdp->getObservationValuations().getStorage());
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
#ifndef STORM_HAVE_SMT_SOLVER
        GTEST_SKIP() << "No SMT solver available.";
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

/*!
 * UMB allows multiple branches with the same source choice and target state, whereas a sparse matrix has at most one entry per row and column.
 * Probabilities of such branches are summed up. Branch rewards of such branches must coincide.
 */
TEST(UmbTest, DuplicateBranches) {
    // DTMC with states 0, 1, 2 where 0 is initial and transitions 0 -> 1 (0.5), 0 -> 2 (0.5), 1 -> 1 (1), 2 -> 2 (1).
    // The reward model has transition rewards 3 for 0 -> 1 and 5 for 0 -> 2.
    storm::storage::SparseMatrixBuilder<double> transitionMatrixBuilder(3, 3, 4);
    transitionMatrixBuilder.addNextValue(0, 1, 0.5);
    transitionMatrixBuilder.addNextValue(0, 2, 0.5);
    transitionMatrixBuilder.addNextValue(1, 1, 1.0);
    transitionMatrixBuilder.addNextValue(2, 2, 1.0);
    storm::storage::SparseMatrixBuilder<double> rewardMatrixBuilder(3, 3, 2);
    rewardMatrixBuilder.addNextValue(0, 1, 3.0);
    rewardMatrixBuilder.addNextValue(0, 2, 5.0);
    storm::storage::BitVector initialStates(3, false);
    initialStates.set(0);
    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init", std::move(initialStates));
    std::optional<std::vector<double>> noStateRewards, noStateActionRewards;
    std::optional<storm::storage::SparseMatrix<double>> transitionRewards = rewardMatrixBuilder.build();
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<double>> rewardModels;
    rewardModels.emplace(
        "rew", storm::models::sparse::StandardRewardModel<double>(std::move(noStateRewards), std::move(noStateActionRewards), std::move(transitionRewards)));
    storm::storage::sparse::ModelComponents<double> components(transitionMatrixBuilder.build(), std::move(labeling), std::move(rewardModels));
    storm::models::sparse::Dtmc<double> const dtmc(std::move(components));
    storm::models::sparse::Model<double> const& model = dtmc;

    // Exports the model and splits the first branch (0 -> 1) into two branches with the same source and target.
    // The reward of the second part of the split branch is given as argument.
    auto createUmbWithDuplicateBranch = [&model](double rewardOfSecondPart) {
        auto umb = storm::umb::sparseModelToUmb(model, storm::umb::ExportOptions{});
        auto& targets = umb.branchToTarget.value();
        auto& probabilities = umb.branchToProbability.get<double>();
        auto& branchRewards = umb.rewards().value().begin()->second.branches.value().values.get<double>();
        EXPECT_EQ((std::vector<uint64_t>{1, 2, 1, 2}), targets);
        EXPECT_EQ((std::vector<double>{0.5, 0.5, 1.0, 1.0}), probabilities);
        EXPECT_EQ((std::vector<double>{3.0, 5.0, 0.0, 0.0}), branchRewards);
        targets.insert(targets.begin(), 1);
        probabilities[0] = 0.25;
        probabilities.insert(probabilities.begin(), 0.25);
        branchRewards.insert(branchRewards.begin(), 3.0);
        branchRewards[1] = rewardOfSecondPart;
        for (auto& branchIndex : umb.choiceToBranches.value()) {
            if (branchIndex > 0) {
                ++branchIndex;  // Shift the start of all choices except for the first one
            }
        }
        umb.index.transitionSystem.numBranches += 1;
        return umb;
    };

    // Probabilities are summed up and the rewards coincide
    {
        auto umb = createUmbWithDuplicateBranch(3.0);
        std::stringstream validationErrors;
        ASSERT_TRUE(umb.validate(validationErrors)) << validationErrors.str();
        auto importedModel = storm::umb::sparseModelFromUmb<double>(umb);
        ASSERT_TRUE(importedModel);
        EXPECT_EQ(3ul, importedModel->getNumberOfStates());
        EXPECT_EQ(3ul, importedModel->getNumberOfChoices());
        EXPECT_EQ(4ul, importedModel->getNumberOfTransitions());
        EXPECT_EQ(model.getTransitionMatrix(), importedModel->getTransitionMatrix());
        ASSERT_TRUE(importedModel->hasUniqueRewardModel());
        auto const& importedRewardModel = importedModel->getUniqueRewardModel();
        ASSERT_TRUE(importedRewardModel.hasTransitionRewards());
        EXPECT_EQ(model.getUniqueRewardModel().getTransitionRewardMatrix(), importedRewardModel.getTransitionRewardMatrix());
    }
    // Different rewards for the same transition can not be represented in a sparse model
    {
        auto umb = createUmbWithDuplicateBranch(4.0);
        STORM_SILENT_EXPECT_THROW(storm::umb::sparseModelFromUmb<double>(umb), storm::exceptions::NotSupportedException);
    }
}

namespace {
/*!
 * Creates an MDP with two states and three choices (two for state 0 and one for state 1) with the given choice labeling.
 */
storm::models::sparse::Mdp<double> createMdpWithChoiceLabeling(std::map<std::string, std::vector<uint64_t>> const& labels) {
    storm::storage::SparseMatrixBuilder<double> builder(3, 2, 4, true, true, 2);
    builder.newRowGroup(0);
    builder.addNextValue(0, 0, 0.5);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(1, 1, 1.0);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, 1.0);
    storm::storage::BitVector initialStates(2, false);
    initialStates.set(0);
    storm::models::sparse::StateLabeling stateLabeling(2);
    stateLabeling.addLabel("init", std::move(initialStates));
    storm::storage::sparse::ModelComponents<double> components(builder.build(), std::move(stateLabeling));
    components.choiceLabeling.emplace(3);
    for (auto const& [label, choices] : labels) {
        storm::storage::BitVector choiceBits(3, false);
        for (auto const choice : choices) {
            choiceBits.set(choice);
        }
        components.choiceLabeling->addLabel(label, std::move(choiceBits));
    }
    return storm::models::sparse::Mdp<double>(std::move(components));
}

std::vector<std::string> getChoiceActionStrings(storm::umb::UmbModel const& umb) {
    std::vector<std::string> result;
    for (std::string_view action : storm::umb::stringVectorView(umb.choiceActions->strings, umb.choiceActions->stringMapping)) {
        result.emplace_back(action);
    }
    return result;
}
}  // namespace

/*!
 * Choices without a label are exported with the empty action string, which always has index 0.
 * In particular, this also holds if the choice labeling does not contain any label (then there is exactly one action).
 */
TEST(UmbTest, ChoiceLabelingExport) {
    struct TestCase {
        std::string description;
        std::map<std::string, std::vector<uint64_t>> labels;
        uint64_t expectedNumActions;
        std::vector<uint32_t> expectedChoiceToAction;
        std::vector<std::string> expectedActionStrings;
    };
    std::vector<TestCase> const testCases{{"no labels", {}, 1, {0, 0, 0}, {""}},
                                          {"label that is not assigned to any choice", {{"a", {}}}, 1, {0, 0, 0}, {""}},
                                          {"some choices unlabeled", {{"a", {0}}}, 2, {1, 0, 0}, {"", "a"}},
                                          {"all choices have the same label", {{"a", {0, 1, 2}}}, 1, {0, 0, 0}, {"a"}},
                                          {"all choices labeled with different labels", {{"a", {0}}, {"b", {1, 2}}}, 2, {0, 1, 1}, {"a", "b"}}};
    for (auto const& testCase : testCases) {
        SCOPED_TRACE(testCase.description);
        auto const mdp = createMdpWithChoiceLabeling(testCase.labels);
        storm::models::sparse::Model<double> const& model = mdp;
        auto umb = storm::umb::sparseModelToUmb(model, storm::umb::ExportOptions{});

        // The exported UMB model must be consistent
        std::stringstream validationErrors;
        EXPECT_TRUE(umb.validate(validationErrors)) << validationErrors.str();
        EXPECT_EQ(testCase.expectedNumActions, umb.index.transitionSystem.numChoiceActions);
        ASSERT_TRUE(umb.choiceActions.has_value());
        ASSERT_TRUE(umb.choiceActions->values.has_value());
        EXPECT_EQ(testCase.expectedChoiceToAction, umb.choiceActions->values.value());
        EXPECT_EQ(testCase.expectedActionStrings, getChoiceActionStrings(umb));

        // Importing the model yields the original choice labeling (choices with the empty action string are not labeled)
        auto imported = storm::umb::sparseModelFromUmb<double>(umb);
        ASSERT_TRUE(imported);
        ASSERT_TRUE(imported->hasChoiceLabeling());
        auto const& importedLabeling = imported->getChoiceLabeling();
        std::size_t numNonEmptyLabels = 0;
        for (auto const& [label, choices] : testCase.labels) {
            if (choices.empty()) {
                continue;  // labels that are assigned to no choice are not exported
            }
            ++numNonEmptyLabels;
            ASSERT_TRUE(importedLabeling.containsLabel(label));
            storm::storage::BitVector expectedChoices(3, false);
            for (auto const choice : choices) {
                expectedChoices.set(choice);
            }
            EXPECT_EQ(expectedChoices, importedLabeling.getChoices(label));
        }
        EXPECT_EQ(numNonEmptyLabels, importedLabeling.getNumberOfLabels());
    }
}
