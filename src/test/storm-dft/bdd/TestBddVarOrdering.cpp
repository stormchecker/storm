#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/parser/BEOrderParser.h"
#include "storm-dft/transformations/SftToBddTransformator.h"
#include "storm/environment/Environment.h"

namespace {

TEST(TestBddVarOrdering, VariableOrdering) {
#ifdef STORM_HAVE_SYLVAN
    auto dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft");
    storm::Environment env;
    auto manager{std::make_shared<storm::dft::storage::SylvanBddManager>(env)};
    auto transformator{std::make_shared<storm::dft::transformations::SftToBddTransformator<double>>(dft, manager)};

    // Use default variable ordering x1, x2, x3, x4
    auto bdd = transformator->transformTopLevel();
    EXPECT_EQ("x1", manager->getName(0));
    EXPECT_EQ("x2", manager->getName(1));
    EXPECT_EQ("x3", manager->getName(2));
    EXPECT_EQ("x4", manager->getName(3));
    EXPECT_EQ(5ul, bdd.NodeCount());

    // Set different variable ordering x2, x4, x1, x3
    std::vector<size_t> beOrder;
    beOrder.push_back(dft->getIndex("x2"));
    beOrder.push_back(dft->getIndex("x4"));
    beOrder.push_back(dft->getIndex("x1"));
    beOrder.push_back(dft->getIndex("x3"));
    dft->setBEOrder(beOrder);
    manager = std::make_shared<storm::dft::storage::SylvanBddManager>(env);
    transformator = std::make_shared<storm::dft::transformations::SftToBddTransformator<double>>(dft, manager);

    bdd = transformator->transformTopLevel();
    EXPECT_EQ("x2", manager->getName(0));
    EXPECT_EQ("x4", manager->getName(1));
    EXPECT_EQ("x1", manager->getName(2));
    EXPECT_EQ("x3", manager->getName(3));
    EXPECT_EQ(7ul, bdd.NodeCount());
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

TEST(TestBddVarOrdering, OrderParser) {
#ifdef STORM_HAVE_SYLVAN
    auto dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft");

    // Load variable ordering
    auto beOrder = storm::dft::parser::BEOrderParser<double>::parseBEOrder(STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest_vars.txt", *dft);
    EXPECT_EQ(4ul, beOrder.size());
    EXPECT_EQ("x2", dft->getElement(beOrder.at(0))->name());
    EXPECT_EQ("x4", dft->getElement(beOrder.at(1))->name());
    EXPECT_EQ("x1", dft->getElement(beOrder.at(2))->name());
    EXPECT_EQ("x3", dft->getElement(beOrder.at(3))->name());
    dft->setBEOrder(beOrder);

    storm::Environment env;
    auto manager = std::make_shared<storm::dft::storage::SylvanBddManager>(env);
    auto transformator = std::make_shared<storm::dft::transformations::SftToBddTransformator<double>>(dft, manager);

    auto bdd = transformator->transformTopLevel();
    EXPECT_EQ("x2", manager->getName(0));
    EXPECT_EQ("x4", manager->getName(1));
    EXPECT_EQ("x1", manager->getName(2));
    EXPECT_EQ("x3", manager->getName(3));
    EXPECT_EQ(7ul, bdd.NodeCount());
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

}  // namespace
