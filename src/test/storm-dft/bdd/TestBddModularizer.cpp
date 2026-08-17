#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/modelchecker/DftModularizationChecker.h"

namespace {

struct ModularizerTestData {
    std::string testname;
    std::string filepath;
    double probabilityAtTimeboundOne;
};

class BddModularizerTest : public testing::TestWithParam<ModularizerTestData> {
   protected:
    void SetUp() override {
#ifdef STORM_HAVE_SYLVAN
        auto const &param{TestWithParam::GetParam()};
        auto dft{storm::dft::api::loadDFTGalileoFile<double>(param.filepath)};
        checker = std::make_shared<storm::dft::modelchecker::DftModularizationChecker<double>>(dft);
#else
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    std::shared_ptr<storm::dft::modelchecker::DftModularizationChecker<double>> checker;
};

TEST_P(BddModularizerTest, ProbabilityAtTimeOne) {
    auto const &param{TestWithParam::GetParam()};
    EXPECT_NEAR(checker->getProbabilityAtTimebound(1), param.probabilityAtTimeboundOne, 1e-6);
}

std::vector<ModularizerTestData> modularizerTestData{
    {
        "And",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/AndTest.dft",
        0.25,
    },
    {
        "Or",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/OrTest.dft",
        0.75,
    },
    {
        "AndOr",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft",
        0.5625,
    },
    {
        "Vot",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/VotTest.dft",
        0.6875,
    },
    {
        "Importance",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/ImportanceTest.dft",
        0.2655055433,
    },
    {
        "Spare",
        STORM_TEST_RESOURCES_DIR "/dft/spare5.dft",
        0.2017690905,
    },
    {
        "MCS",
        STORM_TEST_RESOURCES_DIR "/dft/mcs.dft",
        0.9984947969,
    },
};
INSTANTIATE_TEST_SUITE_P(BddModularizer, BddModularizerTest, testing::ValuesIn(modularizerTestData), [](auto const &info) { return info.param.testname; });

}  // namespace
