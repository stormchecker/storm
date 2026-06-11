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
#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/umb/export/SparseModelToUmb.h"
#include "storm/storage/umb/export/UmbExport.h"
#include "storm/storage/umb/import/SparseModelFromUmb.h"
#include "storm/storage/umb/import/UmbImport.h"
#include "storm/storage/umb/model/UmbModel.h"
#include "storm/storage/umb/model/Valuations.h"
#include "storm/storage/umb/model/ValueEncoding.h"
#include "storm/storage/umb/utility/ValuationDescriptionBuilder.h"
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
            assertEqualValuations(model->getStateValuations().getUmbValuations(), otherModelPtr->getStateValuations().getUmbValuations());
            // POMDP specific things
            if (model->isPartiallyObservable()) {
                ASSERT_TRUE(model->isOfType(storm::models::ModelType::Pomdp));
                auto pomdp = model->template as<storm::models::sparse::Pomdp<ValueType>>();
                auto otherPomdp = otherModelPtr->template as<storm::models::sparse::Pomdp<ValueType>>();
                EXPECT_EQ(pomdp->getObservations(), otherPomdp->getObservations());
                ASSERT_TRUE(pomdp->hasObservationValuations());
                ASSERT_TRUE(otherPomdp->hasObservationValuations());
                assertEqualValuations(pomdp->getObservationValuations().getUmbValuations(), otherPomdp->getObservationValuations().getUmbValuations());
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

TEST(UmbTest, Valuations) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const b = manager->declareBooleanVariable("b");
    auto const i = manager->declareIntegerVariable("i");
    auto const s = manager->declareStringVariable("s");
    auto const r = manager->declareRationalVariable("r");
    std::vector<storm::umb::ValuationClassDescription> classes;
    {
        storm::umb::ValuationDescriptionBuilder builder1(manager);
        builder1.addBooleanVariable(b, true);                 // 1 + 1 bit (optional
        builder1.addIntegerVariable(i, -4, 12);               // 17 different values, therefore 5 bits
        builder1.addStringVariable(s, true);                  // 64 + 1 bits (optional)
        builder1.addRationalVariable(r, 166);                 // 166 bits
        classes.push_back(builder1.buildClassDescription());  // adds 2 padding bits to fill a whole number of bytes
        EXPECT_EQ(2 + 5 + 65 + 166 + 2, classes.back().sizeInBits());
        EXPECT_TRUE(classes.back().hasStringVariable());

        storm::umb::ValuationDescriptionBuilder builder2(manager);
        builder2.addDoubleVariable(r);                        // 64 bits
        builder2.addBooleanVariable(b);                       // 1 bit
        builder2.addIntegerVariable(i, -10, -7);              // 4 different values, therefore  2 bits
        classes.push_back(builder2.buildClassDescription());  // adds 5 padding bits to fill a whole number of bytes
        EXPECT_EQ(64 + 1 + 2 + 5, classes.back().sizeInBits());
        EXPECT_FALSE(classes.back().hasStringVariable());
    }
    storm::umb::Valuations valuations(classes, {manager, manager});
    EXPECT_EQ(0, valuations.numStrings());
    EXPECT_EQ(2, valuations.numClasses());
    EXPECT_EQ(classes[0].sizeInBits(), valuations.getClassDescription(0).sizeInBits());
    EXPECT_EQ(classes[1].sizeInBits(), valuations.getClassDescription(1).sizeInBits());
    auto const vars = valuations.getAllVariables();
    EXPECT_EQ(4, vars.size());
    EXPECT_TRUE(vars.contains(b));
    EXPECT_TRUE(vars.contains(i));
    EXPECT_TRUE(vars.contains(s));
    EXPECT_TRUE(vars.contains(r));
    // Insert 200 entities with alternating classes and some non-trivial values
    std::vector<std::optional<bool>> b_values;
    std::vector<int64_t> i_values;
    std::vector<std::optional<std::string>> s_values;
    std::vector<storm::RationalNumber> r_values;
    for (uint64_t e = 0; e < 200; ++e) {
        if (e % 3 == 0) {
            // insert class 0
            valuations.emplaceBack<true>(0, [&](auto entity, auto const& var, auto& value) {
                using ValueType = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<ValueType, std::optional<bool>>) {
                    EXPECT_EQ(b, var);
                    if (entity % 2 == 0) {
                        value = entity % 4 == 0;
                    }
                    b_values.push_back(value);
                } else if constexpr (std::is_same_v<ValueType, int64_t>) {
                    EXPECT_EQ(i, var);
                    value = static_cast<int64_t>(entity) % 17 - 4;
                    i_values.push_back(value);
                } else if constexpr (std::is_same_v<ValueType, std::optional<std::string>>) {
                    EXPECT_EQ(s, var);
                    if (entity % 2 == 1) {
                        value = "str" + std::to_string(entity);
                    }
                    s_values.push_back(value);
                } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
                    EXPECT_EQ(r, var);
                    auto const uint64max = storm::utility::convertNumber<storm::RationalNumber, uint64_t>(std::numeric_limits<uint64_t>::max());
                    value = (uint64max + uint64max + storm::utility::convertNumber<storm::RationalNumber>(entity)) / (uint64max);
                    if (entity % 8 == 0) {
                        value = -value;
                    }
                    r_values.push_back(value);
                } else {
                    FAIL() << "Unexpected variable type " << typeid(ValueType).name() << " for variable " << var.getName();
                }
            });
        } else {
            // insert class 1
            valuations.emplaceBack<false, double, bool, int64_t>(1, [&](auto entity, auto const& var, auto& value) {
                using ValueType = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<ValueType, double>) {
                    EXPECT_EQ(r, var);
                    value = static_cast<double>(entity) / 3.0;
                    r_values.push_back(storm::utility::convertNumber<storm::RationalNumber>(value));
                } else if constexpr (std::is_same_v<ValueType, bool>) {
                    EXPECT_EQ(b, var);
                    value = entity % 2 == 0;
                    b_values.push_back(value);
                } else {
                    static_assert(std::is_same_v<ValueType, int64_t>);
                    EXPECT_EQ(i, var);
                    value = static_cast<int64_t>(entity) % 4 - 10;
                    i_values.push_back(value);
                }
            });
            s_values.push_back(std::nullopt);
        }
    }
    // Now check if reading back the values works correctly.
    ASSERT_EQ(200, b_values.size());
    ASSERT_EQ(200, i_values.size());
    ASSERT_EQ(200, s_values.size());
    ASSERT_EQ(200, r_values.size());
    valuations.readCallback<std::nullopt_t, bool, int64_t, std::string, storm::RationalNumber, double>([&](auto entity, auto const& var, auto const& value) {
        using ValueType = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<ValueType, std::nullopt_t>) {
            EXPECT_EQ(0, valuations.getClassOfEntity(entity));
            if (var == b) {
                EXPECT_FALSE(b_values[entity].has_value());
            } else {
                EXPECT_TRUE(var == s);
                EXPECT_FALSE(s_values[entity].has_value());
            }
        } else if constexpr (std::is_same_v<ValueType, bool>) {
            EXPECT_EQ(b, var);
            ASSERT_TRUE(b_values[entity].has_value());
            EXPECT_EQ(b_values[entity].value(), value);
        } else if constexpr (std::is_same_v<ValueType, int64_t>) {
            EXPECT_EQ(i, var);
            EXPECT_EQ(i_values[entity], value);
        } else if constexpr (std::is_same_v<ValueType, std::string>) {
            EXPECT_EQ(s, var);
            ASSERT_TRUE(s_values[entity].has_value());
            EXPECT_EQ(s_values[entity].value(), value);
        } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
            EXPECT_EQ(0, valuations.getClassOfEntity(entity));
            EXPECT_EQ(r, var);
            EXPECT_EQ(r_values[entity], value);
        } else {
            static_assert(std::is_same_v<ValueType, double>);
            EXPECT_EQ(1, valuations.getClassOfEntity(entity));
            EXPECT_EQ(r, var);
            EXPECT_EQ(storm::utility::convertNumber<double>(r_values[entity]), storm::utility::convertNumber<double>(value));
        }
    });
}

TEST(UmbTest, ValuationsSingleClass) {
    // Tests point-access via readValue / writeValue, entityHasVariable, getAllVariables, and resize
    // on a simple single-class layout with non-optional variables only.
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const b = manager->declareBooleanVariable("b");
    auto const i = manager->declareIntegerVariable("i");
    auto const d = manager->declareRationalVariable("d");

    storm::umb::ValuationDescriptionBuilder builder(manager);
    builder.addBooleanVariable(b);         // 1 bit
    builder.addIntegerVariable(i, -5, 5);  // 11 values → 4 bits
    builder.addDoubleVariable(d);          // 64 bits
    auto const desc = builder.buildClassDescription();
    EXPECT_EQ(1 + 4 + 64 + 3, desc.sizeInBits());

    storm::umb::Valuations valuations(desc, manager);
    EXPECT_EQ(0u, valuations.size());
    EXPECT_EQ(1u, valuations.numClasses());

    // getAllVariables should list exactly the three declared variables
    auto const vars = valuations.getAllVariables();
    EXPECT_EQ(3u, vars.size());
    EXPECT_TRUE(vars.contains(b));
    EXPECT_TRUE(vars.contains(i));
    EXPECT_TRUE(vars.contains(d));

    // Insert 6 entities with distinct, easily checkable values
    std::vector<bool> bVals = {true, false, true, false, true, false};
    std::vector<int64_t> iVals = {-5, -3, 0, 2, 4, 5};
    std::vector<double> dVals = {0.0, 1.5, -2.25, 1e10, -1e-5, 3.14};

    for (uint64_t e = 0; e < 6; ++e) {
        valuations.emplaceBack<false, bool, int64_t, double>([&](auto entity, auto const& var, auto& value) {
            using ValueType = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<ValueType, bool>) {
                value = bVals[entity];
            } else if constexpr (std::is_same_v<ValueType, int64_t>) {
                value = iVals[entity];
            } else {
                static_assert(std::is_same_v<ValueType, double>);
                value = dVals[entity];
            }
        });
    }
    ASSERT_EQ(6u, valuations.size());

    // entityHasVariable: all variables belong to the single class, so always true
    for (uint64_t e = 0; e < 6; ++e) {
        EXPECT_TRUE(valuations.entityHasVariable(e, b));
        EXPECT_TRUE(valuations.entityHasVariable(e, i));
        EXPECT_TRUE(valuations.entityHasVariable(e, d));
    }

    // readValue round-trips
    for (uint64_t e = 0; e < 6; ++e) {
        EXPECT_EQ(bVals[e], valuations.readValue<bool>(e, b)) << " at entity " << e;
        EXPECT_EQ(iVals[e], valuations.readValue<int64_t>(e, i)) << " at entity " << e;
        EXPECT_EQ(dVals[e], valuations.readValue<double>(e, d)) << " at entity " << e;
    }

    // writeValue then readValue: overwrite entity 3 and verify neighbours are unaffected
    valuations.writeValue(3, b, true);
    valuations.writeValue(3, i, int64_t(-1));
    valuations.writeValue(3, d, 99.0);
    EXPECT_EQ(true, valuations.readValue<bool>(3, b));
    EXPECT_EQ(-1, valuations.readValue<int64_t>(3, i));
    EXPECT_EQ(99.0, valuations.readValue<double>(3, d));
    // neighbours untouched
    EXPECT_EQ(bVals[2], valuations.readValue<bool>(2, b));
    EXPECT_EQ(iVals[4], valuations.readValue<int64_t>(4, i));

    // resize: grow to 9 — new entities get default values and the first 6 stay intact
    valuations.resize(9);
    ASSERT_EQ(9u, valuations.size());
    for (uint64_t e = 0; e < 6; ++e) {
        if (e == 3)
            continue;  // entity 3 was overwritten above
        EXPECT_EQ(bVals[e], valuations.readValue<bool>(e, b)) << " at entity " << e;
        EXPECT_EQ(iVals[e], valuations.readValue<int64_t>(e, i)) << " at entity " << e;
        EXPECT_EQ(dVals[e], valuations.readValue<double>(e, d)) << " at entity " << e;
    }

    // resize: shrink back to 4
    valuations.resize(4);
    EXPECT_EQ(4u, valuations.size());
    EXPECT_EQ(bVals[0], valuations.readValue<bool>(0, b));
    EXPECT_EQ(iVals[1], valuations.readValue<int64_t>(1, i));
    EXPECT_EQ(dVals[2], valuations.readValue<double>(2, d));
}

TEST(UmbTest, ValuationsSelectEntities) {
    // Tests selectEntities (BitVector and vector<uint64_t> overloads) on a single-class
    // layout. Verifies that the selection preserves values in the correct order and that the
    // original Valuations object is left unchanged.
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const b = manager->declareBooleanVariable("b");
    auto const i = manager->declareIntegerVariable("i");

    storm::umb::ValuationDescriptionBuilder builder(manager);
    builder.addBooleanVariable(b);        // 1 bit
    builder.addIntegerVariable(i, 0, 7);  // 8 values → 3 bits
    auto const desc = builder.buildClassDescription();

    storm::umb::Valuations valuations(desc, manager);

    // Insert 8 entities: b = (entity % 2 == 0), i = entity
    for (uint64_t e = 0; e < 8; ++e) {
        valuations.emplaceBack<false, bool, int64_t>([e](auto /*entity*/, auto const& var, auto& value) {
            using ValueType = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<ValueType, bool>) {
                value = (e % 2 == 0);
            } else {
                static_assert(std::is_same_v<ValueType, int64_t>);
                value = static_cast<int64_t>(e);
            }
        });
    }
    ASSERT_EQ(8u, valuations.size());

    // --- BitVector selection: pick entities 1, 3, 5, 7 (odd indices) ---
    storm::storage::BitVector bvOdd(8, false);
    for (uint64_t e = 1; e < 8; e += 2) {
        bvOdd.set(e);
    }
    auto const selectedBv = valuations.selectEntities(bvOdd);
    ASSERT_EQ(4u, selectedBv.size());
    EXPECT_EQ(1u, selectedBv.numClasses());
    for (uint64_t sel = 0; sel < 4; ++sel) {
        uint64_t const origEntity = 2 * sel + 1;  // 1, 3, 5, 7
        EXPECT_EQ(false, selectedBv.readValue<bool>(sel, b)) << " selected entity " << sel;
        EXPECT_EQ(static_cast<int64_t>(origEntity), selectedBv.readValue<int64_t>(sel, i)) << " selected entity " << sel;
    }

    // --- vector<uint64_t> selection: pick entities {6, 0, 4} in that order ---
    std::vector<uint64_t> const indices = {6, 0, 4};
    auto const selectedVec = valuations.selectEntities(indices);
    ASSERT_EQ(3u, selectedVec.size());
    EXPECT_EQ(true, selectedVec.readValue<bool>(0, b));  // entity 6: even → true
    EXPECT_EQ(6, selectedVec.readValue<int64_t>(0, i));
    EXPECT_EQ(true, selectedVec.readValue<bool>(1, b));  // entity 0: even → true
    EXPECT_EQ(0, selectedVec.readValue<int64_t>(1, i));
    EXPECT_EQ(true, selectedVec.readValue<bool>(2, b));  // entity 4: even → true
    EXPECT_EQ(4, selectedVec.readValue<int64_t>(2, i));

    // Original must be unchanged
    ASSERT_EQ(8u, valuations.size());
    for (uint64_t e = 0; e < 8; ++e) {
        EXPECT_EQ(e % 2 == 0, valuations.readValue<bool>(e, b)) << " at original entity " << e;
        EXPECT_EQ(static_cast<int64_t>(e), valuations.readValue<int64_t>(e, i)) << " at original entity " << e;
    }
}