#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/io.h"
#include "storm-dft/transformations/DftTransformer.h"

namespace {

TEST(DftTransformerTest, UniqueConstantFailedTest) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/const_be_test.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> originalDft = storm::dft::api::loadDFTGalileoFile<double>(file);
    std::shared_ptr<storm::dft::storage::DFT<double>> transformedDft =
        storm::dft::transformations::DftTransformer<double>::transformUniqueFailedBE(*originalDft);

    // Count in original DFT
    auto bes = originalDft->getBasicElements();
    uint64_t constBeFailedCount = 0;
    uint64_t constBeFailsafeCount = 0;
    for (auto &be : bes) {
        if (be->beType() == storm::dft::storage::elements::BEType::CONSTANT) {
            if (be->canFail()) {
                ++constBeFailedCount;
            } else {
                ++constBeFailsafeCount;
            }
        }
    }
    EXPECT_EQ(3ul, constBeFailedCount);
    EXPECT_EQ(0ul, constBeFailsafeCount);

    // Count in transformed DFT
    bes = transformedDft->getBasicElements();
    constBeFailedCount = 0;
    constBeFailsafeCount = 0;
    for (auto &be : bes) {
        if (be->beType() == storm::dft::storage::elements::BEType::CONSTANT) {
            if (be->canFail()) {
                ++constBeFailedCount;
            } else {
                ++constBeFailsafeCount;
            }
        }
    }
    EXPECT_EQ(1ul, constBeFailedCount);
    EXPECT_EQ(3ul, constBeFailsafeCount);
}

TEST(DftTransformerTest, BinaryFDEPTest) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/fdep5.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> originalDft = storm::dft::api::loadDFTGalileoFile<double>(file);
    std::shared_ptr<storm::dft::storage::DFT<double>> transformedDft =
        storm::dft::transformations::DftTransformer<double>::transformBinaryDependencies(*originalDft);

    // Count in original DFT
    uint64_t dependencyCount = originalDft->getDependencies().size();
    EXPECT_EQ(1ul, dependencyCount);

    // Count in transformed DFT
    dependencyCount = transformedDft->getDependencies().size();
    EXPECT_EQ(2ul, dependencyCount);
}

TEST(DftTransformerTest, PDEPTransformTest) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/pdep4.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> originalDft = storm::dft::api::loadDFTGalileoFile<double>(file);
    std::shared_ptr<storm::dft::storage::DFT<double>> transformedDft =
        storm::dft::transformations::DftTransformer<double>::transformBinaryDependencies(*originalDft);

    // Count in original DFT
    uint64_t fdepCount = 0;
    uint64_t pdepCount = 0;
    for (auto depIndex : originalDft->getDependencies()) {
        auto dep = originalDft->getDependency(depIndex);
        if (dep->probability() == 1) {
            ++fdepCount;
        } else {
            ++pdepCount;
        }
    }
    EXPECT_EQ(1ul, pdepCount);
    EXPECT_EQ(0ul, fdepCount);
    EXPECT_EQ(3ul, originalDft->nrBasicElements());

    // Count in transformed DFT
    fdepCount = 0;
    pdepCount = 0;
    for (auto depIndex : transformedDft->getDependencies()) {
        auto dep = transformedDft->getDependency(depIndex);
        if (dep->probability() == 1) {
            ++fdepCount;
        } else {
            ++pdepCount;
        }
    }
    EXPECT_EQ(1ul, pdepCount);
    EXPECT_EQ(2ul, fdepCount);
    EXPECT_EQ(4ul, transformedDft->nrBasicElements());
}

}  // namespace
