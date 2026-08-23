#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/io.h"
#include "storm-dft/utility/DftModularizer.h"

namespace {

TEST(DftModuleTest, Modularization) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/all_gates.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);

    storm::dft::utility::DftModularizer<double> modularizer;
    auto topModule = modularizer.computeModules(*dft);
    EXPECT_EQ(topModule.getRepresentative(), 37ul);
    EXPECT_TRUE(topModule.isStatic());
    EXPECT_FALSE(topModule.isFullyStatic());
    auto submodules = topModule.getSubModules();
    EXPECT_EQ(submodules.size(), 4ul);
    auto it = submodules.begin();
    EXPECT_EQ(it->getRepresentative(), 8ul);
    EXPECT_FALSE(it->isStatic());
    EXPECT_FALSE(it->isFullyStatic());
    EXPECT_EQ(it->getSubModules().size(), 3ul);
    ++it;
    EXPECT_EQ(it->getRepresentative(), 17ul);
    EXPECT_FALSE(it->isStatic());
    EXPECT_FALSE(it->isFullyStatic());
    EXPECT_EQ(it->getSubModules().size(), 3ul);
    ++it;
    EXPECT_EQ(it->getRepresentative(), 28ul);
    EXPECT_FALSE(it->isStatic());
    EXPECT_FALSE(it->isFullyStatic());
    EXPECT_EQ(it->getSubModules().size(), 6ul);
    ++it;
    EXPECT_EQ(it->getRepresentative(), 36ul);
    EXPECT_FALSE(it->isStatic());
    EXPECT_FALSE(it->isFullyStatic());
    EXPECT_EQ(it->getSubModules().size(), 7ul);
}

TEST(DftModuleTest, ModularizationCycle) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/fdep_cycle.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);

    storm::dft::utility::DftModularizer<double> modularizer;
    auto topModule = modularizer.computeModules(*dft);
    EXPECT_EQ(topModule.getRepresentative(), 2ul);
    EXPECT_EQ(topModule.getSubModules().size(), 2ul);
}

TEST(DftModuleTest, ModularizationOverlapping) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/modules2.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);

    storm::dft::utility::DftModularizer<double> modularizer;
    auto topModule = modularizer.computeModules(*dft);
    EXPECT_EQ(topModule.getRepresentative(), 13ul);
    auto submodules = topModule.getSubModules();
    EXPECT_EQ(submodules.size(), 2ul);
    EXPECT_TRUE(topModule.isStatic());
    EXPECT_FALSE(topModule.isFullyStatic());
    auto it = submodules.begin();
    // Submodule F1
    EXPECT_EQ(it->getRepresentative(), 5ul);
    EXPECT_EQ(it->getSubModules().size(), 3ul);
    EXPECT_TRUE(it->isStatic());
    EXPECT_TRUE(it->isFullyStatic());
    // Submodule F4
    ++it;
    EXPECT_EQ(it->getRepresentative(), 12ul);
    auto modulesF4 = it->getSubModules();
    EXPECT_EQ(modulesF4.size(), 2ul);
    EXPECT_FALSE(it->isStatic());
    EXPECT_FALSE(it->isFullyStatic());
    EXPECT_EQ(++it, submodules.end());
    // Submodule F5
    it = modulesF4.begin();
    EXPECT_EQ(it->getRepresentative(), 10ul);
    auto modulesF5 = it->getSubModules();
    EXPECT_EQ(modulesF5.size(), 2ul);
    EXPECT_FALSE(it->isStatic());
    EXPECT_FALSE(it->isFullyStatic());
    ++it;
    EXPECT_EQ(it->getRepresentative(), 11ul);
    EXPECT_TRUE(it->isSingleBE());
    EXPECT_EQ(++it, modulesF4.end());
    // Submodule F6
    it = modulesF5.begin();
    EXPECT_EQ(it->getRepresentative(), 8ul);
    auto modulesF6 = it->getSubModules();
    EXPECT_EQ(modulesF6.size(), 2ul);
    EXPECT_TRUE(it->isStatic());
    EXPECT_TRUE(it->isFullyStatic());
    ++it;
    EXPECT_EQ(it->getRepresentative(), 9ul);
    EXPECT_TRUE(it->isSingleBE());
    EXPECT_EQ(++it, modulesF5.end());
    // BE submodules of F6
    it = modulesF6.begin();
    EXPECT_EQ(it->getRepresentative(), 6ul);
    EXPECT_TRUE(it->isSingleBE());
    ++it;
    EXPECT_EQ(it->getRepresentative(), 7ul);
    EXPECT_TRUE(it->isSingleBE());
    EXPECT_EQ(++it, modulesF6.end());
}

}  // namespace
