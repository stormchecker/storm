#include "storm-config.h"
#include "storm/environment/Environment.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/parser/JaniParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/builder/DdJaniModelBuilder.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/jani/Compositions.h"

namespace {

class Cudd {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::CUDD;
};

class Sylvan {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::Sylvan;
};

template<typename TestType>
class DdJaniModelBuilderTest : public ::testing::Test {
   public:
    void SetUp() override {
        TestType::checkLibraryAvailable();
    }

    storm::jani::Model getJaniModelFromPrism(std::string const& pathInTestResourcesDir, bool prismCompatability = false) {
        storm::storage::SymbolicModelDescription modelDescription =
            storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/" + pathInTestResourcesDir, prismCompatability);
        auto m = modelDescription.toJani(true).preprocess().asJaniModel();
        auto unsupportedFeatures = m.restrictToFeatures(storm::builder::DdJaniModelBuilder<DdType, double>::getSupportedJaniFeatures());
        EXPECT_TRUE(unsupportedFeatures.empty()) << "Model '" << pathInTestResourcesDir << "' uses unsupported feature(s) " << unsupportedFeatures.toString();
        return m;
    }

    static const storm::dd::DdType DdType = TestType::DdType;
};

typedef ::testing::Types<Cudd, Sylvan> TestingTypes;
TYPED_TEST_SUITE(DdJaniModelBuilderTest, TestingTypes, );

TYPED_TEST(DdJaniModelBuilderTest, Dtmc) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::Environment env;
    auto janiModel = this->getJaniModelFromPrism("/dtmc/die.pm");

    storm::builder::DdJaniModelBuilder<DdType, double> builder;
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = builder.build(env, janiModel);
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/dtmc/brp-16-2.pm");
    model = builder.build(env, janiModel);
    EXPECT_EQ(677ul, model->getNumberOfStates());
    EXPECT_EQ(867ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/dtmc/crowds-5-5.pm");
    model = builder.build(env, janiModel);
    EXPECT_EQ(8607ul, model->getNumberOfStates());
    EXPECT_EQ(15113ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/dtmc/leader-3-5.pm");
    model = builder.build(env, janiModel);
    EXPECT_EQ(273ul, model->getNumberOfStates());
    EXPECT_EQ(397ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/dtmc/nand-5-2.pm");
    model = builder.build(env, janiModel);
    EXPECT_EQ(1728ul, model->getNumberOfStates());
    EXPECT_EQ(2505ul, model->getNumberOfTransitions());
}

TYPED_TEST(DdJaniModelBuilderTest, Ctmc) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::Environment env;
    auto janiModel = this->getJaniModelFromPrism("/ctmc/cluster2.sm", true);
    storm::builder::DdJaniModelBuilder<DdType, double> builder;
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = builder.build(env, janiModel);
    EXPECT_EQ(276ul, model->getNumberOfStates());
    EXPECT_EQ(1120ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/ctmc/embedded2.sm", true);
    model = builder.build(env, janiModel);
    EXPECT_EQ(3478ul, model->getNumberOfStates());
    EXPECT_EQ(14639ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/ctmc/polling2.sm", true);
    model = builder.build(env, janiModel);
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(22ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/ctmc/fms2.sm", true);
    model = builder.build(env, janiModel);
    EXPECT_EQ(810ul, model->getNumberOfStates());
    EXPECT_EQ(3699ul, model->getNumberOfTransitions());

    janiModel = this->getJaniModelFromPrism("/ctmc/tandem5.sm", true);
    model = builder.build(env, janiModel);
    EXPECT_EQ(66ul, model->getNumberOfStates());
    EXPECT_EQ(189ul, model->getNumberOfTransitions());
}

TYPED_TEST(DdJaniModelBuilderTest, Mdp) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::Environment env;
    auto janiModel = this->getJaniModelFromPrism("/mdp/two_dice.nm");
    storm::builder::DdJaniModelBuilder<DdType, double> builder;
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = builder.build(env, janiModel);

    EXPECT_TRUE(model->getType() == storm::models::ModelType::Mdp);
    std::shared_ptr<storm::models::symbolic::Mdp<DdType>> mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();

    EXPECT_EQ(169ul, mdp->getNumberOfStates());
    EXPECT_EQ(436ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(254ul, mdp->getNumberOfChoices());

    janiModel = this->getJaniModelFromPrism("/mdp/leader3.nm");
    model = builder.build(env, janiModel);

    EXPECT_TRUE(model->getType() == storm::models::ModelType::Mdp);
    mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();

    EXPECT_EQ(364ul, mdp->getNumberOfStates());
    EXPECT_EQ(654ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(573ul, mdp->getNumberOfChoices());

    janiModel = this->getJaniModelFromPrism("/mdp/coin2-2.nm");
    model = builder.build(env, janiModel);

    EXPECT_TRUE(model->getType() == storm::models::ModelType::Mdp);
    mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();

    EXPECT_EQ(272ul, mdp->getNumberOfStates());
    EXPECT_EQ(492ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(400ul, mdp->getNumberOfChoices());

    janiModel = this->getJaniModelFromPrism("/mdp/csma2-2.nm");
    model = builder.build(env, janiModel);

    EXPECT_TRUE(model->getType() == storm::models::ModelType::Mdp);
    mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();

    EXPECT_EQ(1038ul, mdp->getNumberOfStates());
    EXPECT_EQ(1282ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(1054ul, mdp->getNumberOfChoices());

    janiModel = this->getJaniModelFromPrism("/mdp/firewire3-0.5.nm");
    model = builder.build(env, janiModel);

    EXPECT_TRUE(model->getType() == storm::models::ModelType::Mdp);
    mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();

    EXPECT_EQ(4093ul, mdp->getNumberOfStates());
    EXPECT_EQ(5585ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(5519ul, mdp->getNumberOfChoices());

    janiModel = this->getJaniModelFromPrism("/mdp/wlan0-2-2.nm");
    model = builder.build(env, janiModel);

    EXPECT_TRUE(model->getType() == storm::models::ModelType::Mdp);
    mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();

    EXPECT_EQ(37ul, mdp->getNumberOfStates());
    EXPECT_EQ(59ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(59ul, mdp->getNumberOfChoices());

    janiModel = this->getJaniModelFromPrism("/mdp/sync.nm");
    model = builder.build(env, janiModel);

    EXPECT_TRUE(model->getType() == storm::models::ModelType::Mdp);
    mdp = model->template as<storm::models::symbolic::Mdp<DdType>>();

    EXPECT_EQ(5ul, mdp->getNumberOfStates());
    EXPECT_EQ(24ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(12ul, mdp->getNumberOfChoices());
}

TYPED_TEST(DdJaniModelBuilderTest, SynchronizationVectors) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::Environment env;
    auto janiModel = this->getJaniModelFromPrism("/mdp/SmallPrismTest.nm");

    storm::builder::DdJaniModelBuilder<DdType, double> builder;

    // Start by checking the original composition.
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = builder.build(env, janiModel);
    EXPECT_EQ(7ul, model->getNumberOfStates());
    EXPECT_EQ(10ul, model->getNumberOfTransitions());

    // Now we tweak it's system composition to check whether synchronization vectors work.
    std::vector<std::shared_ptr<storm::jani::Composition>> automataCompositions;
    automataCompositions.push_back(std::make_shared<storm::jani::AutomatonComposition>("one"));
    automataCompositions.push_back(std::make_shared<storm::jani::AutomatonComposition>("two"));
    automataCompositions.push_back(std::make_shared<storm::jani::AutomatonComposition>("three"));

    // First, make all actions non-synchronizing.
    std::vector<storm::jani::SynchronizationVector> synchronizationVectors;

    std::vector<std::string> inputVector;
    inputVector.push_back("a");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back("c");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back("d");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back("b");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back("c");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back("c");
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();

    std::shared_ptr<storm::jani::Composition> newComposition = std::make_shared<storm::jani::ParallelComposition>(automataCompositions, synchronizationVectors);
    janiModel.setSystemComposition(newComposition);
    model = builder.build(env, janiModel);
    EXPECT_EQ(24ul, model->getNumberOfStates());
    EXPECT_EQ(48ul, model->getNumberOfTransitions());

    // Then, make only a, b and c synchronize.
    synchronizationVectors.clear();
    inputVector.clear();
    inputVector.push_back("a");
    inputVector.push_back("b");
    inputVector.push_back("c");
    synchronizationVectors.emplace_back(inputVector, "d");
    inputVector.clear();
    inputVector.push_back("c");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back("d");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back("c");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);

    newComposition = std::make_shared<storm::jani::ParallelComposition>(automataCompositions, synchronizationVectors);
    janiModel.setSystemComposition(newComposition);
    model = builder.build(env, janiModel);
    EXPECT_EQ(7ul, model->getNumberOfStates());
    EXPECT_EQ(10ul, model->getNumberOfTransitions());

    synchronizationVectors.clear();
    inputVector.clear();
    inputVector.push_back("a");
    inputVector.push_back("b");
    inputVector.push_back("c");
    synchronizationVectors.emplace_back(inputVector, "d");
    inputVector.clear();
    inputVector.push_back("c");
    inputVector.push_back("c");
    inputVector.push_back("a");
    synchronizationVectors.emplace_back(inputVector, "d");
    inputVector.clear();
    inputVector.push_back("d");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    newComposition = std::make_shared<storm::jani::ParallelComposition>(automataCompositions, synchronizationVectors);
    janiModel.setSystemComposition(newComposition);
    model = builder.build(env, janiModel);
    EXPECT_EQ(3ul, model->getNumberOfStates());
    EXPECT_EQ(3ul, model->getNumberOfTransitions());

    synchronizationVectors.clear();
    inputVector.clear();
    inputVector.push_back("a");
    inputVector.push_back("b");
    inputVector.push_back("c");
    synchronizationVectors.emplace_back(inputVector, "d");
    inputVector.clear();
    inputVector.push_back("c");
    inputVector.push_back("c");
    inputVector.push_back("a");
    synchronizationVectors.emplace_back(inputVector, "d");
    inputVector.clear();
    inputVector.push_back("d");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);
    inputVector.clear();
    inputVector.push_back("d");
    inputVector.push_back("c");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector, "b");
    newComposition = std::make_shared<storm::jani::ParallelComposition>(automataCompositions, synchronizationVectors);
    janiModel.setSystemComposition(newComposition);
    model = builder.build(env, janiModel);
    EXPECT_EQ(4ul, model->getNumberOfStates());
    EXPECT_EQ(5ul, model->getNumberOfTransitions());
}

TYPED_TEST(DdJaniModelBuilderTest, Composition) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::Environment env;
    auto janiModel = this->getJaniModelFromPrism("/mdp/system_composition.nm");

    storm::builder::DdJaniModelBuilder<DdType, double> builder;
    STORM_SILENT_EXPECT_THROW(std::shared_ptr<storm::models::symbolic::Model<DdType>> model = builder.build(env, janiModel),
                              storm::exceptions::WrongFormatException);

    janiModel = this->getJaniModelFromPrism("/mdp/system_composition2.nm");
    STORM_SILENT_EXPECT_THROW(std::shared_ptr<storm::models::symbolic::Model<DdType>> model = builder.build(env, janiModel),
                              storm::exceptions::WrongFormatException);
}

TYPED_TEST(DdJaniModelBuilderTest, InputEnabling) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    storm::Environment env;
    auto janiModel = this->getJaniModelFromPrism("/mdp/SmallPrismTest2.nm");

    storm::builder::DdJaniModelBuilder<DdType, double> builder;

    // Make some automaton compositions input-enabled.
    std::vector<std::shared_ptr<storm::jani::Composition>> automataCompositions;
    automataCompositions.push_back(std::make_shared<storm::jani::AutomatonComposition>("one"));
    automataCompositions.push_back(std::make_shared<storm::jani::AutomatonComposition>("two"));
    automataCompositions.push_back(std::make_shared<storm::jani::AutomatonComposition>("three", std::set<std::string>{"a"}));

    // Create the synchronization vectors.
    std::vector<storm::jani::SynchronizationVector> synchronizationVectors;
    std::vector<std::string> inputVector;
    inputVector.push_back("a");
    inputVector.push_back("b");
    inputVector.push_back("c");
    synchronizationVectors.emplace_back(inputVector, "d");
    inputVector.clear();
    inputVector.push_back("c");
    inputVector.push_back("c");
    inputVector.push_back("a");
    synchronizationVectors.emplace_back(inputVector, "d");
    inputVector.clear();
    inputVector.push_back("d");
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    inputVector.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
    synchronizationVectors.emplace_back(inputVector);

    std::shared_ptr<storm::jani::Composition> newComposition = std::make_shared<storm::jani::ParallelComposition>(automataCompositions, synchronizationVectors);
    janiModel.setSystemComposition(newComposition);
    std::shared_ptr<storm::models::symbolic::Model<DdType>> model = builder.build(env, janiModel);
    EXPECT_EQ(4ul, model->getNumberOfStates());
    EXPECT_EQ(5ul, model->getNumberOfTransitions());
}

}  // namespace
