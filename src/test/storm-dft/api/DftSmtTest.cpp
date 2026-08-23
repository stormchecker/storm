#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/modelchecker/DFTASFChecker.h"
#include "storm-dft/utility/FDEPConflictFinder.h"
#include "storm-dft/utility/FailureBoundFinder.h"

namespace {
class DftSmt : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(DftSmt, AndTest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/and.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    storm::dft::modelchecker::DFTASFChecker smtChecker(*dft);
    smtChecker.convert();
    smtChecker.toSolver();
    EXPECT_EQ(smtChecker.checkTleNeverFailed(), storm::solver::SmtSolver::CheckResult::Unsat);
}

TEST_F(DftSmt, PandTest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/pand.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    storm::dft::modelchecker::DFTASFChecker smtChecker(*dft);
    smtChecker.convert();
    smtChecker.toSolver();
    EXPECT_EQ(smtChecker.checkTleNeverFailed(), storm::solver::SmtSolver::CheckResult::Sat);
}

TEST_F(DftSmt, SpareTest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/spare_two_modules.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    storm::dft::modelchecker::DFTASFChecker smtChecker(*dft);
    smtChecker.convert();
    smtChecker.toSolver();
    EXPECT_EQ(smtChecker.checkTleFailsWithLeq(2), storm::solver::SmtSolver::CheckResult::Unsat);
    EXPECT_EQ(smtChecker.checkTleFailsWithEq(3), storm::solver::SmtSolver::CheckResult::Sat);
}

TEST_F(DftSmt, BoundTest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/spare5.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    storm::dft::modelchecker::DFTASFChecker smtChecker(*dft);
    smtChecker.convert();
    smtChecker.toSolver();
    EXPECT_EQ(storm::dft::utility::FailureBoundFinder::getLeastFailureBound(*dft, true, 30), uint64_t(2));
    EXPECT_EQ(storm::dft::utility::FailureBoundFinder::getAlwaysFailedBound(*dft, true, 30), uint64_t(4));
}

TEST_F(DftSmt, FDEPBoundTest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/fdep_bound.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft, false).first);
    storm::dft::modelchecker::DFTASFChecker smtChecker(*dft);
    smtChecker.convert();
    smtChecker.toSolver();
    EXPECT_EQ(storm::dft::utility::FailureBoundFinder::getLeastFailureBound(*dft, true, 30), uint64_t(1));
    EXPECT_EQ(storm::dft::utility::FailureBoundFinder::getAlwaysFailedBound(*dft, true, 30), uint64_t(5));
}

TEST_F(DftSmt, FDEPConflictTest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft =
        storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/spare_conflict_test.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    std::vector<bool> true_vector(10, true);

    EXPECT_EQ(storm::dft::utility::FDEPConflictFinder<double>::getDynamicBehavior(*dft), true_vector);
    EXPECT_TRUE(storm::dft::utility::FDEPConflictFinder<double>::getDependencyConflicts(*dft, true).empty());
}

TEST_F(DftSmt, FDEPConflictSPARETest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft =
        storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/spare_conflict_test.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    std::vector<bool> true_vector(10, true);

    EXPECT_EQ(storm::dft::utility::FDEPConflictFinder<double>::getDynamicBehavior(*dft), true_vector);
    EXPECT_TRUE(storm::dft::utility::FDEPConflictFinder<double>::getDependencyConflicts(*dft, true).empty());
}

TEST_F(DftSmt, FDEPConflictSEQTest) {
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/seq_conflict_test.dft");
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    std::vector<bool> expected_dynamic_vector(dft->nrElements(), true);
    expected_dynamic_vector.at(dft->getTopLevelIndex()) = false;

    EXPECT_EQ(storm::dft::utility::FDEPConflictFinder<double>::getDynamicBehavior(*dft), expected_dynamic_vector);
    EXPECT_EQ(storm::dft::utility::FDEPConflictFinder<double>::getDependencyConflicts(*dft, true).size(), uint64_t(3));
}
}  // namespace
