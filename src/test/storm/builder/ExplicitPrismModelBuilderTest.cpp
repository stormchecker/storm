#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/generator/PrismNextStateGenerator.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/expressions/ExpressionManager.h"

class ExplicitPrismModelBuilderTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(ExplicitPrismModelBuilderTest, Dtmc) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(677ul, model->getNumberOfStates());
    EXPECT_EQ(867ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(8607ul, model->getNumberOfStates());
    EXPECT_EQ(15113ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(273ul, model->getNumberOfStates());
    EXPECT_EQ(397ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/nand-5-2.pm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(1728ul, model->getNumberOfStates());
    EXPECT_EQ(2505ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitPrismModelBuilderTest, Ctmc) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ctmc/cluster2.sm", true);

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(276ul, model->getNumberOfStates());
    EXPECT_EQ(1120ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ctmc/embedded2.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(3478ul, model->getNumberOfStates());
    EXPECT_EQ(14639ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ctmc/polling2.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(22ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ctmc/fms2.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(810ul, model->getNumberOfStates());
    EXPECT_EQ(3699ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ctmc/tandem5.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(66ul, model->getNumberOfStates());
    EXPECT_EQ(189ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitPrismModelBuilderTest, Mdp) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(169ul, model->getNumberOfStates());
    EXPECT_EQ(436ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(364ul, model->getNumberOfStates());
    EXPECT_EQ(654ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(272ul, model->getNumberOfStates());
    EXPECT_EQ(492ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/csma2-2.nm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(1038ul, model->getNumberOfStates());
    EXPECT_EQ(1282ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/firewire3-0.5.nm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(4093ul, model->getNumberOfStates());
    EXPECT_EQ(5585ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0-2-2.nm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(37ul, model->getNumberOfStates());
    EXPECT_EQ(59ul, model->getNumberOfTransitions());

    storm::storage::SymbolicModelDescription modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/unbounded.nm");
    program = modelDescription.preprocess("N=-7").asPrismProgram();
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(9ul, model->getNumberOfStates());
    EXPECT_EQ(9ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/enumerate_init.prism");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(36ul, model->getNumberOfStates());
    EXPECT_EQ(66ul, model->getNumberOfTransitions());
    EXPECT_EQ(36ul, model->getInitialStates().getNumberOfSetBits());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/sync.nm");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(24ul, model->getNumberOfTransitions());
    EXPECT_EQ(12ul, model->getNumberOfChoices());
}

TEST_F(ExplicitPrismModelBuilderTest, Ma) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/simple.ma");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(4ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/hybrid_states.ma");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(5ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/ma/stream2.ma");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(14ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(7ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());
}

TEST_F(ExplicitPrismModelBuilderTest, POMdp) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism");
    program = program.preprocess("slippery=0.4");
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(7ul, model->getNumberOfStates());
    EXPECT_EQ(17ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/globalvars.prism");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(3ul, model->getNumberOfStates());
    EXPECT_EQ(6ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism");
    program = program.preprocess("sl=0.4");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(15ul, model->getNumberOfStates());
    EXPECT_EQ(91ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/refuel.prism");
    program = program.preprocess("N=5");
    model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(126ul, model->getNumberOfStates());
    EXPECT_EQ(547ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitPrismModelBuilderTest, Imdp) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/imdp/robot.prism");
    program = program.preprocess("delta=0.2");
    std::shared_ptr<storm::models::sparse::Model<storm::Interval>> model = storm::builder::ExplicitModelBuilder<storm::Interval>(program).build();
    EXPECT_EQ(6ul, model->getNumberOfStates());
    EXPECT_EQ(17ul, model->getNumberOfTransitions());

    program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/imdp/coin2.prism");
    program = program.preprocess("K=2,bias1=0.1");
    model = storm::builder::ExplicitModelBuilder<storm::Interval>(program).build();
    EXPECT_EQ(272ul, model->getNumberOfStates());
    EXPECT_EQ(492ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitPrismModelBuilderTest, SMG) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/smg/action_labels.prism");
    program = program.preprocess("");
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    EXPECT_EQ(3ul, model->getNumberOfStates());
    EXPECT_EQ(3ul, model->getNumberOfChoices());
    EXPECT_EQ(6ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitPrismModelBuilderTest, FailComposition) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/system_composition.nm");

    STORM_SILENT_ASSERT_THROW(storm::builder::ExplicitModelBuilder<double>(program).build(), storm::exceptions::WrongFormatException);
}

TEST_F(ExplicitPrismModelBuilderTest, FailUnbounded) {
    storm::storage::SymbolicModelDescription modelDescription = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/unbounded.nm");
    storm::prism::Program program = modelDescription.preprocess("N=7").asPrismProgram();
    STORM_SILENT_ASSERT_THROW(storm::builder::ExplicitModelBuilder<double>(program).build(), storm::exceptions::WrongFormatException);
}

TEST_F(ExplicitPrismModelBuilderTest, ExportExplicitLookup) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");
    storm::generator::NextStateGeneratorOptions generatorOptions;
    generatorOptions.setBuildAllLabels();
    auto builder = storm::builder::ExplicitModelBuilder<double>(program, generatorOptions);
    std::shared_ptr<storm::models::sparse::Model<double>> model = builder.build();
    auto lookup = builder.exportExplicitStateLookup();
    auto svar = program.getModules()[0].getIntegerVariable("s").getExpressionVariable();
    auto dvar = program.getModules()[0].getIntegerVariable("d").getExpressionVariable();
    auto& manager = program.getManager();
    EXPECT_EQ(model->getNumberOfStates(), lookup.lookup({{svar, manager.integer(1)}, {dvar, manager.integer(2)}}));
    EXPECT_TRUE(model->getNumberOfStates() > lookup.lookup({{svar, manager.integer(7)}, {dvar, manager.integer(2)}}));
    EXPECT_EQ(1ul, model->getLabelsOfState(lookup.lookup({{svar, manager.integer(7)}, {dvar, manager.integer(2)}})).count("two"));
}

bool trivial_true_mask(storm::expressions::SimpleValuation const&, uint64_t) {
    return true;
}

bool trivial_false_mask(storm::expressions::SimpleValuation const&, uint64_t) {
    return false;
}

bool only_first_action_mask(storm::expressions::SimpleValuation const&, uint64_t actionIndex) {
    return actionIndex <= 1;
}

TEST_F(ExplicitPrismModelBuilderTest, CallbackActionMask) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/die_selection.nm");
    storm::generator::NextStateGeneratorOptions generatorOptions;
    generatorOptions.setBuildAllLabels();
    generatorOptions.setBuildChoiceLabels();
    std::shared_ptr<storm::generator::StateValuationFunctionMask<double>> mask_object =
        std::make_shared<storm::generator::StateValuationFunctionMask<double>>(trivial_true_mask);
    std::shared_ptr<storm::generator::PrismNextStateGenerator<double>> generator =
        std::make_shared<storm::generator::PrismNextStateGenerator<double>>(program, generatorOptions, mask_object);
    auto builder = storm::builder::ExplicitModelBuilder<double>(generator);

    std::shared_ptr<storm::models::sparse::Model<double>> model = builder.build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(48ul, model->getNumberOfTransitions());

    mask_object = std::make_shared<storm::generator::StateValuationFunctionMask<double>>(trivial_false_mask);
    generator = std::make_shared<storm::generator::PrismNextStateGenerator<double>>(program, generatorOptions, mask_object);
    builder = storm::builder::ExplicitModelBuilder<double>(generator);

    model = builder.build();
    EXPECT_EQ(1ul, model->getNumberOfStates());
    EXPECT_EQ(1ul, model->getNumberOfTransitions());

    mask_object = std::make_shared<storm::generator::StateValuationFunctionMask<double>>(only_first_action_mask);
    generator = std::make_shared<storm::generator::PrismNextStateGenerator<double>>(program, generatorOptions, mask_object);
    builder = storm::builder::ExplicitModelBuilder<double>(generator);

    model = builder.build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());
}
