#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/storm.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/generator/JaniNextStateGenerator.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingMemento.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Model.h"

namespace {

storm::jani::Model getJaniModelFromPrism(std::string const& pathInTestResourcesDir, bool prismCompatability = false) {
    storm::storage::SymbolicModelDescription modelDescription =
        storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/" + pathInTestResourcesDir, prismCompatability);
    auto m = modelDescription.toJani().preprocess().asJaniModel();
    auto unsupportedFeatures = m.restrictToFeatures(storm::generator::JaniNextStateGenerator<double>::getSupportedJaniFeatures());
    EXPECT_TRUE(unsupportedFeatures.empty()) << "Model '" << pathInTestResourcesDir << "' uses unsupported feature(s) " << unsupportedFeatures.toString();
    return m;
}

class ExplicitJaniModelBuilderTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(ExplicitJaniModelBuilderTest, Dtmc) {
    auto janiModel = getJaniModelFromPrism("/dtmc/die.pm");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/die_array.jani").first;
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/die_array_nested.jani").first;
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/dtmc/brp-16-2.pm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(677ul, model->getNumberOfStates());
    EXPECT_EQ(867ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/dtmc/crowds-5-5.pm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(8607ul, model->getNumberOfStates());
    EXPECT_EQ(15113ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/dtmc/leader-3-5.pm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(273ul, model->getNumberOfStates());
    EXPECT_EQ(397ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/dtmc/nand-5-2.pm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(1728ul, model->getNumberOfStates());
    EXPECT_EQ(2505ul, model->getNumberOfTransitions());

    janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/test_trigonometry.jani").first;
    auto constants = storm::storage::parseConstantDefinitionString(janiModel.getManager(), "step_size_rad=0.523599");  // step_size = 30 deg
    janiModel = janiModel.defineUndefinedConstants(constants);
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(5ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitJaniModelBuilderTest, pdtmc) {
    auto janiModel = getJaniModelFromPrism("/pdtmc/parametric_die.pm");
    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model =
        storm::builder::ExplicitModelBuilder<storm::RationalFunction>(janiModel).build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/pdtmc/die_array_nested.jani").first;
    janiModel.substituteConstantsFunctionsTranscendentals();
    model = storm::builder::ExplicitModelBuilder<storm::RationalFunction>(janiModel).build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/pdtmc/brp16_2.pm");
    model = storm::builder::ExplicitModelBuilder<storm::RationalFunction>(janiModel).build();
    EXPECT_EQ(677ul, model->getNumberOfStates());
    EXPECT_EQ(867ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitJaniModelBuilderTest, Ctmc) {
    auto janiModel = getJaniModelFromPrism("/ctmc/cluster2.sm", true);

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(276ul, model->getNumberOfStates());
    EXPECT_EQ(1120ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/ctmc/embedded2.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(3478ul, model->getNumberOfStates());
    EXPECT_EQ(14639ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/ctmc/polling2.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(22ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/ctmc/fms2.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(810ul, model->getNumberOfStates());
    EXPECT_EQ(3699ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/ctmc/tandem5.sm", true);
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(66ul, model->getNumberOfStates());
    EXPECT_EQ(189ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitJaniModelBuilderTest, Mdp) {
    auto janiModel = getJaniModelFromPrism("/mdp/two_dice.nm");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(169ul, model->getNumberOfStates());
    EXPECT_EQ(436ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/mdp/leader3.nm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(364ul, model->getNumberOfStates());
    EXPECT_EQ(654ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/mdp/coin2-2.nm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(272ul, model->getNumberOfStates());
    EXPECT_EQ(492ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/mdp/csma2-2.nm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(1038ul, model->getNumberOfStates());
    EXPECT_EQ(1282ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/mdp/firewire3-0.5.nm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(4093ul, model->getNumberOfStates());
    EXPECT_EQ(5585ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/mdp/wlan0-2-2.nm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(37ul, model->getNumberOfStates());
    EXPECT_EQ(59ul, model->getNumberOfTransitions());

    janiModel = getJaniModelFromPrism("/mdp/sync.nm");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(24ul, model->getNumberOfTransitions());
    EXPECT_EQ(12ul, model->getNumberOfChoices());

    janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards.jani").first;
    storm::generator::NextStateGeneratorOptions options;
    options.setBuildChoiceLabels();
    model = storm::builder::ExplicitModelBuilder<double>(janiModel, options).build();
    EXPECT_EQ(2ul, model->getChoiceLabeling().getNumberOfLabels());

    janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards4.jani").first;
    options.setBuildChoiceLabels();
    model = storm::builder::ExplicitModelBuilder<double>(janiModel, options).build();
    EXPECT_EQ(2ul, model->getChoiceLabeling().getNumberOfLabels());
}

TEST_F(ExplicitJaniModelBuilderTest, Ma) {
    auto janiModel = getJaniModelFromPrism("/ma/simple.ma");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(4ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());

    janiModel = getJaniModelFromPrism("/ma/hybrid_states.ma");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(5ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());

    janiModel = getJaniModelFromPrism("/ma/stream2.ma");
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(14ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(7ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());

    janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/ma/ftwc.jani").first;
    auto constants = storm::storage::parseConstantDefinitionString(janiModel.getManager(), "N=2,TIME_BOUND=1");
    janiModel = janiModel.defineUndefinedConstants(constants);
    model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(1536ul, model->getNumberOfStates());
    EXPECT_EQ(6448ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(1530ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());
}

TEST_F(ExplicitJaniModelBuilderTest, FailComposition) {
    auto janiModel = getJaniModelFromPrism("/mdp/system_composition.nm");

    STORM_SILENT_ASSERT_THROW(storm::builder::ExplicitModelBuilder<double>(janiModel).build(), storm::exceptions::WrongFormatException);
}

TEST_F(ExplicitJaniModelBuilderTest, unassignedVariables) {
    storm::jani::Model janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/mdp/unassigned-variables.jani").first;
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(25ul, model->getNumberOfStates());
    EXPECT_EQ(81ul, model->getNumberOfTransitions());
}

TEST_F(ExplicitJaniModelBuilderTest, enumerateInitial) {
    storm::jani::Model janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/mdp/enumerate_init.jani").first;
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(janiModel).build();
    EXPECT_EQ(94ul, model->getNumberOfStates());
    EXPECT_EQ(145ul, model->getNumberOfTransitions());
    EXPECT_EQ(72ul, model->getInitialStates().getNumberOfSetBits());
}

TEST_F(ExplicitJaniModelBuilderTest, SynchronizationVectorOutputActionIndex) {
    auto janiModel = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/mdp/synchronization_vector_output_action_index.jani").first;

    storm::generator::JaniNextStateGenerator<double> generator(janiModel);
    std::deque<storm::generator::CompressedState> states;
    auto stateToIdCallback = [&states](storm::generator::CompressedState const& state) {
        for (uint32_t index = 0; index < states.size(); ++index) {
            if (states[index] == state) {
                return index;
            }
        }
        states.push_back(state);
        return static_cast<uint32_t>(states.size() - 1);
    };

    auto initialStates = generator.getInitialStates(stateToIdCallback);
    ASSERT_EQ(1ul, initialStates.size());
    generator.load(states[initialStates.front()]);

    auto behavior = generator.expand(stateToIdCallback);
    ASSERT_EQ(3ul, behavior.getNumberOfChoices());

    std::set<std::string> actionNames;
    for (auto const& choice : behavior.getChoices()) {
        actionNames.insert(janiModel.getAction(choice.getActionIndex()).getName());
    }
    EXPECT_EQ(std::set<std::string>({"d", "e", "f"}), actionNames);
}
}  // namespace
