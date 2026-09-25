#include <ranges>

#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/generator/PrismNextStateGenerator.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/sparse/ValuationTransformer.h"
#include "storm/storage/valuations/ValuationDescriptionBuilder.h"
#include "storm/storage/valuations/Valuations.h"
#include "storm/storage/valuations/ValuationsStorage.h"

TEST(ValuationTest, StateValuationConstruction) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");
    storm::generator::NextStateGeneratorOptions generatorOptions;
    generatorOptions.setBuildStateValuations();
    generatorOptions.setBuildAllLabels();
    auto builder = storm::builder::ExplicitModelBuilder<double>(program, generatorOptions);
    std::shared_ptr<storm::models::sparse::Model<double>> model = builder.build();
    ASSERT_TRUE(model->hasStateValuations());
    auto const& sv = model->getStateValuations();
    ASSERT_EQ(sv.getNumberOfEntities(), model->getNumberOfStates());
    ASSERT_TRUE(sv.getManager().hasVariable("s"));
    ASSERT_TRUE(sv.getManager().hasVariable("d"));
    auto const s = sv.getManager().getVariable("s");
    auto const d = sv.getManager().getVariable("d");
    auto const vars = sv.getAllVariables();
    ASSERT_EQ(2, vars.size());
    ASSERT_TRUE(vars.contains(s));
    ASSERT_TRUE(vars.contains(d));
    // reading values at sinit
    uint64_t const sinit = *model->getInitialStates().begin();
    ASSERT_TRUE(sv.entityHasVariable(sinit, s));
    ASSERT_TRUE(sv.entityHasVariable(sinit, d));
    EXPECT_EQ(0, sv.getInt64Value(sinit, s));
    EXPECT_EQ(0, sv.getOptionalInt64Value(sinit, s).value());
    EXPECT_EQ(0, sv.getOptionalInt64Value(sinit, d).value());
    // reading json at sinit
    auto js = sv.toJson(sinit);
    EXPECT_EQ(2, js.size());
    EXPECT_TRUE(js.contains("s"));
    EXPECT_TRUE(js.contains("d"));
    EXPECT_EQ(0, js["s"].get<int64_t>());
    EXPECT_EQ(0, js["d"].get<int64_t>());
    // reading values at "three" state
    ASSERT_TRUE(model->getStateLabeling().containsLabel("three"));
    ASSERT_TRUE(model->getStates("three").hasUniqueSetBit());
    uint64_t const three = *model->getStates("three").begin();
    EXPECT_EQ(7, sv.getInt64Value(three, s));
    EXPECT_EQ(3, sv.getInt64Value(three, d));
    // reading all values for d
    auto dValues = sv.getInt64Values(d);
    ASSERT_EQ(sv.getNumberOfEntities(), dValues.size());
    EXPECT_EQ(3, dValues[three]);
    int64_t const sum = std::accumulate(dValues.begin(), dValues.end(), 0ll);
    EXPECT_EQ(1 + 2 + 3 + 4 + 5 + 6, sum);
}

TEST(ValuationTest, StateValuationTransformation) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");
    storm::generator::NextStateGeneratorOptions generatorOptions;
    generatorOptions.setBuildStateValuations();
    auto builder = storm::builder::ExplicitModelBuilder<double>(program, generatorOptions);
    std::shared_ptr<storm::models::sparse::Model<double>> model = builder.build();
    ASSERT_TRUE(model->hasStateValuations());
    auto const& sv = model->getStateValuations();
    storm::storage::sparse::ValuationTransformer notransformer(sv);
    auto newsv = notransformer.build(true);
    ASSERT_EQ(newsv.getNumberOfEntities(), sv.getNumberOfEntities());
    storm::storage::sparse::ValuationTransformer transformer(sv);
    auto const svar = program.getManager().getVariable("s");
    auto const dvar = program.getManager().getVariable("d");
    auto const sgt3Var = program.getManager().declareBooleanVariable("sGT3");
    auto const alwaysTrueVar = program.getManager().declareBooleanVariable("alwaysTrue");
    auto const alwaysFalseVar = program.getManager().declareBooleanVariable("alwaysFalse");
    transformer.addExpression(sgt3Var, svar.getExpression() > program.getManager().integer(3));
    transformer.addExpression(alwaysTrueVar, svar.getExpression() == svar.getExpression());
    transformer.addExpression(alwaysFalseVar, dvar.getExpression() < dvar.getExpression());
    newsv = transformer.build(true);
    auto const vars = newsv.getAllVariables();
    ASSERT_EQ(5, vars.size());
    ASSERT_TRUE(vars.contains(svar));
    ASSERT_TRUE(vars.contains(dvar));
    ASSERT_TRUE(vars.contains(sgt3Var));
    ASSERT_TRUE(vars.contains(alwaysTrueVar));
    ASSERT_TRUE(vars.contains(alwaysFalseVar));
    uint64_t const sinit = *model->getInitialStates().begin();
    EXPECT_EQ(0, newsv.getInt64Value(sinit, svar));
    EXPECT_EQ(0, newsv.getInt64Value(sinit, dvar));
    EXPECT_FALSE(newsv.getBooleanValue(sinit, sgt3Var));
    EXPECT_TRUE(newsv.getBooleanValue(sinit, alwaysTrueVar));
    EXPECT_FALSE(newsv.getBooleanValue(sinit, alwaysFalseVar));

    for (uint64_t state = 0; state < newsv.getNumberOfEntities(); ++state) {
        ASSERT_TRUE(newsv.getBooleanValue(state, alwaysTrueVar));
        ASSERT_FALSE(newsv.getBooleanValue(state, alwaysFalseVar));
        ASSERT_EQ(sv.getInt64Value(state, svar), newsv.getInt64Value(state, svar));
        ASSERT_EQ(newsv.getBooleanValue(state, sgt3Var), newsv.getInt64Value(state, svar) > 3);
    }
}

TEST(ValuationTest, Valuations2classes) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const b = manager->declareBooleanVariable("b");
    auto const i = manager->declareIntegerVariable("i");
    auto const s = manager->declareStringVariable("s");
    auto const r = manager->declareRationalVariable("r");
    std::vector<storm::storage::sparse::ValuationClassDescription> classes;
    {
        storm::storage::sparse::ValuationDescriptionBuilder builder1(manager);
        builder1.addBooleanVariable(b, true);                 // 1 + 1 bit (optional
        builder1.addIntegerVariable(i, -4, 12);               // 17 different values, therefore 5 bits
        builder1.addStringVariable(s, true);                  // 64 + 1 bits (optional)
        builder1.addRationalVariable(r, 166);                 // 166 bits
        classes.push_back(builder1.buildClassDescription());  // adds 2 padding bits to fill a whole number of bytes
        EXPECT_EQ(2 + 5 + 65 + 166 + 2, classes.back().sizeInBits());
        EXPECT_TRUE(classes.back().hasStringVariable());

        storm::storage::sparse::ValuationDescriptionBuilder builder2(manager);
        builder2.addDoubleVariable(r);                        // 64 bits
        builder2.addBooleanVariable(b);                       // 1 bit
        builder2.addIntegerVariable(i, -10, -7);              // 4 different values, therefore  2 bits
        classes.push_back(builder2.buildClassDescription());  // adds 5 padding bits to fill a whole number of bytes
        EXPECT_EQ(64 + 1 + 2 + 5, classes.back().sizeInBits());
        EXPECT_FALSE(classes.back().hasStringVariable());
    }
    storm::storage::sparse::ValuationsStorage valuations(classes, {manager, manager});
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

TEST(ValuationTest, ValuationsSingleClass) {
    // Tests point-access via readValue / writeValue, entityHasVariable, getAllVariables, and resize
    // on a simple single-class layout with non-optional variables only.
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const b = manager->declareBooleanVariable("b");
    auto const i = manager->declareIntegerVariable("i");
    auto const d = manager->declareRationalVariable("d");

    storm::storage::sparse::ValuationDescriptionBuilder builder(manager);
    builder.addBooleanVariable(b);         // 1 bit
    builder.addIntegerVariable(i, -5, 5);  // 11 values → 4 bits
    builder.addDoubleVariable(d);          // 64 bits
    auto const desc = builder.buildClassDescription();
    EXPECT_EQ(1 + 4 + 64 + 3, desc.sizeInBits());

    storm::storage::sparse::ValuationsStorage valuations(desc, manager);
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
        if (e == 3) {
            continue;  // entity 3 was overwritten above
        }
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

TEST(ValuationTest, ValuationsSelectEntities) {
    // Tests selectEntities (BitVector and vector<uint64_t> overloads) on a single-class
    // layout. Verifies that the selection preserves values in the correct order and that the
    // original Valuations object is left unchanged.
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const b = manager->declareBooleanVariable("b");
    auto const i = manager->declareIntegerVariable("i");

    storm::storage::sparse::ValuationDescriptionBuilder builder(manager);
    builder.addBooleanVariable(b);        // 1 bit
    builder.addIntegerVariable(i, 0, 7);  // 8 values → 3 bits
    auto const desc = builder.buildClassDescription();

    storm::storage::sparse::ValuationsStorage valuations(desc, manager);

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

TEST(ValuationTest, RejectsNonCompliantDoubleOrStringSize) {
    // Double and String fields must always be exactly their default size (64 bits) per the UMB spec
    // (see storm::umb::validation::validateTypeDeclaration).
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const d = manager->declareRationalVariable("d");
    storm::storage::sparse::ValuationDescriptionBuilder doubleBuilder(manager);
    storm::storage::sparse::ValuationClassDescription::Variable const badDouble{
        .name = "d", .isOptional = std::nullopt, .type = {storm::umb::Type::Double, 128}, .lower = {}, .upper = {}, .offset = {}};
    EXPECT_THROW(doubleBuilder.addVariable(badDouble), storm::exceptions::WrongFormatException);

    auto const s = manager->declareStringVariable("s");
    storm::storage::sparse::ValuationDescriptionBuilder stringBuilder(manager);
    storm::storage::sparse::ValuationClassDescription::Variable const badString{
        .name = "s", .isOptional = std::nullopt, .type = {storm::umb::Type::String, 128}, .lower = {}, .upper = {}, .offset = {}};
    EXPECT_THROW(stringBuilder.addVariable(badString), storm::exceptions::WrongFormatException);

    // A compliant (default-size) Double field must still be accepted.
    storm::storage::sparse::ValuationDescriptionBuilder okBuilder(manager);
    storm::storage::sparse::ValuationClassDescription::Variable const okDouble{
        .name = "d", .isOptional = std::nullopt, .type = {storm::umb::Type::Double, std::nullopt}, .lower = {}, .upper = {}, .offset = {}};
    EXPECT_NO_THROW(okBuilder.addVariable(okDouble));
}

TEST(ValuationTest, OverwriteValues) {
    // Overwriting values must not affect neighboring variables (variables are not byte-aligned and span multiple bytes)
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const a = manager->declareIntegerVariable("a");
    auto const b = manager->declareIntegerVariable("b");
    auto const c = manager->declareIntegerVariable("c");
    storm::storage::sparse::ValuationDescriptionBuilder descBuilder(manager);
    descBuilder.addIntegerVariable(a, 0, 7);       // 3 bits
    descBuilder.addIntegerVariable(b, 0, 100000);  // 17 bits, crosses byte boundaries
    descBuilder.addIntegerVariable(c, 0, 1000);    // 10 bits
    storm::storage::sparse::ValuationsStorage storage(descBuilder.buildClassDescription(), manager);
    storage.resize(1);
    for (int64_t round = 0; round < 3; ++round) {
        int64_t const av = round == 0 ? 7 : (round == 1 ? 0 : 5);
        int64_t const bv = round == 0 ? 99999 : (round == 1 ? 1 : 65537);
        int64_t const cv = round == 0 ? 1000 : (round == 1 ? 0 : 513);
        storage.writeValue<int64_t>(0, a, av);
        storage.writeValue<int64_t>(0, b, bv);
        storage.writeValue<int64_t>(0, c, cv);
        EXPECT_EQ(av, storage.readValue<int64_t>(0, a));
        EXPECT_EQ(bv, storage.readValue<int64_t>(0, b));
        EXPECT_EQ(cv, storage.readValue<int64_t>(0, c));
    }
}

TEST(ValuationTest, ResizeInitializesAllNewEntitiesWithDefaults) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const i = manager->declareIntegerVariable("i");
    auto const r = manager->declareRationalVariable("r");
    auto const b = manager->declareBooleanVariable("b");
    storm::storage::sparse::ValuationDescriptionBuilder descBuilder(manager);
    // The default value of i is its upper bound -3, whose bit representation is not zero
    descBuilder.addVariable(
        {.name = "i", .isOptional = std::nullopt, .type = {storm::umb::Type::Int, 8}, .lower = std::nullopt, .upper = -3, .offset = std::nullopt});
    // The default value of r is 0/1 (and not 0/0)
    descBuilder.addRationalVariable(r, 16);
    descBuilder.addBooleanVariable(b);
    storm::storage::sparse::ValuationsStorage storage(descBuilder.buildClassDescription(), manager);

    storage.resize(5);
    ASSERT_EQ(5u, storage.size());
    for (uint64_t entity = 0; entity < 5; ++entity) {
        EXPECT_EQ(-3, storage.readValue<int64_t>(entity, i)) << "entity " << entity;
        EXPECT_EQ(storm::RationalNumber(0), storage.readValue<storm::RationalNumber>(entity, r)) << "entity " << entity;
        EXPECT_FALSE(storage.readValue<bool>(entity, b)) << "entity " << entity;
    }
    // Growing an already non-empty storage keeps the existing entities
    storage.writeValue<int64_t>(2, i, -7);
    storage.resize(8);
    ASSERT_EQ(8u, storage.size());
    EXPECT_EQ(-7, storage.readValue<int64_t>(2, i));
    for (uint64_t entity : {0, 1, 3, 4, 5, 6, 7}) {
        EXPECT_EQ(-3, storage.readValue<int64_t>(entity, i)) << "entity " << entity;
        EXPECT_EQ(storm::RationalNumber(0), storage.readValue<storm::RationalNumber>(entity, r)) << "entity " << entity;
    }
    // Shrinking
    storage.resize(3);
    EXPECT_EQ(3u, storage.size());
    EXPECT_EQ(-7, storage.readValue<int64_t>(2, i));
}

TEST(ValuationTest, ResizeMultipleClasses) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const a = manager->declareIntegerVariable("a");
    auto const b = manager->declareIntegerVariable("b");
    std::vector<storm::storage::sparse::ValuationClassDescription> classes;
    {
        storm::storage::sparse::ValuationDescriptionBuilder builder(manager);
        builder.addIntegerVariable(a, 0, 7);  // 3 bits, padded to one byte
        classes.push_back(builder.buildClassDescription());
    }
    {
        storm::storage::sparse::ValuationDescriptionBuilder builder(manager);
        builder.addIntegerVariable(b, -20000, 20000);  // 16 bits
        classes.push_back(builder.buildClassDescription());
    }
    storm::storage::sparse::ValuationsStorage storage(classes, {manager});
    ASSERT_EQ(2u, storage.numClasses());

    storage.resize(3, 0);
    storage.resize(7, 1);
    ASSERT_EQ(7u, storage.size());
    for (uint64_t entity = 0; entity < 7; ++entity) {
        EXPECT_EQ(entity < 3 ? 0u : 1u, storage.getClassOfEntity(entity)) << "entity " << entity;
    }
    // Make sure that we can access all entities independently of each other (in particular the last one)
    for (uint64_t entity = 0; entity < 3; ++entity) {
        storage.writeValue<int64_t>(entity, a, entity + 1);
    }
    for (uint64_t entity = 3; entity < 7; ++entity) {
        storage.writeValue<int64_t>(entity, b, -static_cast<int64_t>(entity) * 1000);
    }
    for (uint64_t entity = 0; entity < 3; ++entity) {
        EXPECT_EQ(static_cast<int64_t>(entity + 1), storage.readValue<int64_t>(entity, a));
    }
    for (uint64_t entity = 3; entity < 7; ++entity) {
        EXPECT_EQ(-static_cast<int64_t>(entity) * 1000, storage.readValue<int64_t>(entity, b));
    }
    EXPECT_EQ(3u + 4u * 2u, storage.getRawUmbData().valuations->size());
}

TEST(ValuationTest, WriteRejectsValuesThatDoNotFitIntoStoredType) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const s = manager->declareIntegerVariable("s");
    auto const u = manager->declareIntegerVariable("u");
    storm::storage::sparse::ValuationDescriptionBuilder descBuilder(manager);
    // Signed and unsigned 4 bit variables without declared bounds
    descBuilder.addVariable({.name = "s", .isOptional = std::nullopt, .type = {storm::umb::Type::Int, 4}, .lower = {}, .upper = {}, .offset = {}});
    descBuilder.addVariable({.name = "u", .isOptional = std::nullopt, .type = {storm::umb::Type::Uint, 4}, .lower = {}, .upper = {}, .offset = {}});
    storm::storage::sparse::ValuationsStorage storage(descBuilder.buildClassDescription(), manager);
    storage.resize(1);
    using storm::exceptions::OutOfRangeException;
    using Integer = storm::storage::sparse::ValuationsStorage::Integer;

    for (int64_t v : std::views::iota(-8, 8)) {
        storage.writeValue<int64_t>(0, s, v);
        EXPECT_EQ(v, storage.readValue<int64_t>(0, s));
    }
    for (int64_t v : {-9, -100, 8, 15, 16}) {
        STORM_SILENT_EXPECT_THROW(storage.writeValue<int64_t>(0, s, v), OutOfRangeException);
        STORM_SILENT_EXPECT_THROW(storage.writeValue<Integer>(0, s, Integer(static_cast<long>(v))), OutOfRangeException);
    }
    // The failed writes did not modify the value
    storage.writeValue<int64_t>(0, s, -8);
    STORM_SILENT_EXPECT_THROW(storage.writeValue<int64_t>(0, s, 8), OutOfRangeException);
    EXPECT_EQ(-8, storage.readValue<int64_t>(0, s));

    for (int64_t v : std::views::iota(0, 16)) {
        storage.writeValue<int64_t>(0, u, v);
        EXPECT_EQ(v, storage.readValue<int64_t>(0, u));
    }
    for (int64_t v : {-1, -16, 16, 17}) {
        STORM_SILENT_EXPECT_THROW(storage.writeValue<int64_t>(0, u, v), OutOfRangeException);
    }
    STORM_SILENT_EXPECT_THROW(storage.writeValue<Integer>(0, u, Integer(-1)), OutOfRangeException);
}

TEST(ValuationTest, FailedWriteDoesNotSetPresenceBitOfOptionalVariable) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const i = manager->declareIntegerVariable("i");
    storm::storage::sparse::ValuationDescriptionBuilder descBuilder(manager);
    descBuilder.addIntegerVariable(i, 0, 5, true);
    storm::storage::sparse::ValuationsStorage storage(descBuilder.buildClassDescription(), manager);
    storage.emplaceBack<true>([](auto, auto, auto&) { /* leave optional variable unset */ });
    auto isSet = [&storage, &i]() {
        bool result = false;
        storage.readCallback<std::nullopt_t, int64_t>(
            0, i, [&result](auto, auto, auto const& value) { result = !std::is_same_v<std::remove_cvref_t<decltype(value)>, std::nullopt_t>; });
        return result;
    };
    ASSERT_FALSE(isSet());
    // Out of the variable's bounds
    auto writeOutOfBounds = [&storage, &i]() { storage.writeCallback<false, true, int64_t>(0, i, [](auto, auto, auto& value) { value = 10; }); };
    STORM_SILENT_EXPECT_THROW(writeOutOfBounds(), storm::exceptions::OutOfRangeException);
    EXPECT_FALSE(isSet());
    // Successful write
    storage.writeCallback<false, true, int64_t>(0, i, [](auto, auto, auto& value) { value = 3; });
    ASSERT_TRUE(isSet());
    EXPECT_EQ(3, storage.readValue<int64_t>(0, i));
}
