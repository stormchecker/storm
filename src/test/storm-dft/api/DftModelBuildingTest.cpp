#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/io.h"
#include "storm-dft/api/transformation.h"
#include "storm-dft/builder/ExplicitDFTModelBuilder.h"
#include "storm-dft/utility/RelevantEvents.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/properties.h"
#include "storm/storage/jani/Property.h"

namespace {

TEST(DftModelBuildingTest, RelevantEvents) {
    // Initialize
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/dont_care.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    std::string property = "Tmin=? [F \"failed\"]";
    std::vector<std::shared_ptr<storm::logic::Formula const>> properties = storm::api::extractFormulasFromProperties(storm::api::parseProperties(property));
    storm::dft::storage::DftSymmetries symmetries;

    // Set relevant events (none)
    storm::dft::utility::RelevantEvents relevantEvents{};
    dft->setRelevantEvents(relevantEvents, false);
    // Build model
    storm::dft::builder::ExplicitDFTModelBuilder<double> builder(*dft, symmetries);
    builder.buildModel(0, 0.0);
    std::shared_ptr<storm::models::sparse::Model<double>> model = builder.getModel();
    EXPECT_EQ(8ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());

    // Set relevant events (all)
    relevantEvents = storm::dft::utility::RelevantEvents({"all"});
    dft->setRelevantEvents(relevantEvents, false);
    // Build model
    storm::dft::builder::ExplicitDFTModelBuilder<double> builder2(*dft, symmetries);
    builder2.buildModel(0, 0.0);
    model = builder2.getModel();
    EXPECT_EQ(512ul, model->getNumberOfStates());
    EXPECT_EQ(2305ul, model->getNumberOfTransitions());

    // Set relevant events (H)
    relevantEvents = storm::dft::utility::RelevantEvents({"H"});
    dft->setRelevantEvents(relevantEvents, false);
    // Build model
    storm::dft::builder::ExplicitDFTModelBuilder<double> builder3(*dft, symmetries);
    builder3.buildModel(0, 0.0);
    model = builder3.getModel();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(25ul, model->getNumberOfTransitions());

    // Set relevant events (H, I)
    relevantEvents = storm::dft::utility::RelevantEvents({"H", "I"});
    dft->setRelevantEvents(relevantEvents, false);
    // Build model
    storm::dft::builder::ExplicitDFTModelBuilder<double> builder4(*dft, symmetries);
    builder4.buildModel(0, 0.0);
    model = builder4.getModel();
    EXPECT_EQ(16ul, model->getNumberOfStates());
    EXPECT_EQ(33ul, model->getNumberOfTransitions());

    // Set relevant events (none)
    relevantEvents = storm::dft::utility::RelevantEvents{};
    dft->setRelevantEvents(relevantEvents, true);
    // Build model
    storm::dft::builder::ExplicitDFTModelBuilder<double> builder5(*dft, symmetries);
    builder5.buildModel(0, 0.0);
    model = builder5.getModel();
    EXPECT_EQ(8ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());

    // Set relevant events (all)
    relevantEvents = storm::dft::utility::RelevantEvents({"all"});
    dft->setRelevantEvents(relevantEvents, true);
    // Build model
    storm::dft::builder::ExplicitDFTModelBuilder<double> builder6(*dft, symmetries);
    builder6.buildModel(0, 0.0);
    model = builder6.getModel();
    EXPECT_EQ(8ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());

    // Set relevant events (H, I)
    relevantEvents = storm::dft::utility::RelevantEvents({"H", "I"});
    dft->setRelevantEvents(relevantEvents, true);
    // Build model
    storm::dft::builder::ExplicitDFTModelBuilder<double> builder7(*dft, symmetries);
    builder7.buildModel(0, 0.0);
    model = builder7.getModel();
    EXPECT_EQ(8ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());
}

}  // namespace
