#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/utility/SymmetryFinder.h"

namespace {

storm::dft::storage::DftSymmetries findSymmetries(std::string const& file) {
    // Load, and build DFT
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);
    // Find symmetries
    return storm::dft::utility::SymmetryFinder<double>::findSymmetries(*dft);
}

TEST(SymmetryTest, SymmetriesStaticFT) {
    auto symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/and.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0)[0].size(), 2ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/or.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0)[0].size(), 2ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/voting.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/voting2.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/voting3.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/voting4.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0)[0].size(), 2ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/voting5.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
}

TEST(SymmetryTest, SymmetriesDynamicFT) {
    auto symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/pand.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/por.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare2.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2).size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2)[0].size(), 2ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare3.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2).size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2)[0].size(), 3ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare4.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare5.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(1).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(1)[0].size(), 2ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare6.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare7.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/spare8.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/fdep.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/fdep2.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/fdep3.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/fdep4.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/fdep5.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(1).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(1)[0].size(), 2ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/fdep6.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/fdep7.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/pdep.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/pdep2.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/pdep3.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/pdep4.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/seq.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/seq2.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/seq3.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/seq4.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/seq5.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/seq6.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/mutex.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/mutex2.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/mutex3.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/mutex4.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/all_be_distributions.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 0ul);
    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/all_gates.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(21).size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(21)[0].size(), 4ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(32).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(32)[0].size(), 2ul);
}

TEST(SymmetryTest, SymmetricFT) {
    auto symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/symmetry6.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 5ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(12).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(12)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(10).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(10)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(14).size(), 5ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(14)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2).size(), 3ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2)[0].size(), 3ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/symmetry7.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 3ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(0)[0].size(), 3ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(5).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(5)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(7).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(7)[0].size(), 2ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft/pdep_symmetry.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2).size(), 4ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(6).size(), 9ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(6)[0].size(), 2ul);

    symmetries = findSymmetries(STORM_TEST_RESOURCES_DIR "/dft//hecs_2_2.dft");
    EXPECT_EQ(symmetries.nrSymmetries(), 4ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2).size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(2)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(12).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(12)[0].size(), 2ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(6).size(), 1ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(6)[0].size(), 5ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(18).size(), 19ul);
    EXPECT_EQ(symmetries.getSymmetryGroup(18)[0].size(), 2ul);
}

}  // namespace
