#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/generator/PrismNextStateGenerator.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/sparse/ValuationTransformer.h"
#include "storm/storage/sparse/Valuations.h"

class StateValuationTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(StateValuationTest, StateValuationConstruction) {
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
    EXPECT_EQ(0, sv.getIntegerValue(sinit, s));
    EXPECT_EQ(0, sv.getOptionalIntegerValue(sinit, s).value());
    EXPECT_EQ(0, sv.getOptionalIntegerValue(sinit, d).value());
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
    EXPECT_EQ(7, sv.getIntegerValue(three, s));
    EXPECT_EQ(3, sv.getIntegerValue(three, d));
    // reading all values for d
    auto dValues = sv.getInt64Values(d);
    ASSERT_EQ(sv.getNumberOfEntities(), dValues.size());
    EXPECT_EQ(3, dValues[three]);
    int64_t const sum = std::accumulate(dValues.begin(), dValues.end(), 0ll);
    EXPECT_EQ(1 + 2 + 3 + 4 + 5 + 6, sum);
}

TEST_F(StateValuationTest, StateValuationTransformation) {
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
    EXPECT_EQ(0, newsv.getIntegerValue(sinit, svar));
    EXPECT_EQ(0, newsv.getIntegerValue(sinit, dvar));
    EXPECT_FALSE(newsv.getBooleanValue(sinit, sgt3Var));
    EXPECT_TRUE(newsv.getBooleanValue(sinit, alwaysTrueVar));
    EXPECT_FALSE(newsv.getBooleanValue(sinit, alwaysFalseVar));

    for (uint64_t state = 0; state < newsv.getNumberOfEntities(); ++state) {
        ASSERT_TRUE(newsv.getBooleanValue(state, alwaysTrueVar));
        ASSERT_FALSE(newsv.getBooleanValue(state, alwaysFalseVar));
        ASSERT_EQ(sv.getIntegerValue(state, svar), newsv.getIntegerValue(state, svar));
        ASSERT_EQ(newsv.getBooleanValue(state, sgt3Var), newsv.getIntegerValue(state, svar) > 3);
    }
}
