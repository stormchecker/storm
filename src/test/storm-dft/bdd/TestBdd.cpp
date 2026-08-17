#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/adapters/SFTBDDPropertyFormulaAdapter.h"
#include "storm-dft/api/storm-dft.h"
#include "storm-dft/modelchecker/SFTBDDChecker.h"
#include "storm-dft/transformations/SftToBddTransformator.h"
#include "storm-dft/utility/MTTFHelper.h"
#include "storm-parsers/api/properties.h"
#include "storm/environment/Environment.h"

namespace {

struct SftTestData {
    std::string testname;
    std::string filepath;
    std::string bddHash;
    double probabilityAtTimeboundOne;
    double mttf;
    std::vector<double> birnbaum;
    std::vector<double> CIF;
    std::vector<double> DIF;
    std::vector<double> RAW;
    std::vector<double> RRW;

    friend std::ostream &operator<<(std::ostream &os, SftTestData const &data) {
        auto printVector = [&os](std::vector<double> const &arr) {
            os << ", {";
            for (auto const &i : arr) {
                os << i;
                // will leave trailing ", " but its simpler
                // and would still be a valid initializer
                os << ", ";
            }
            os << '}';
        };

        os << "{\"" << data.testname << '"';
        os << ", \"" << data.filepath << '"';
        os << ", \"" << data.bddHash << '"';
        os << ", " << data.probabilityAtTimeboundOne;
        os << ", " << data.mttf;
        printVector(data.birnbaum);
        printVector(data.CIF);
        printVector(data.DIF);
        printVector(data.RAW);
        printVector(data.RRW);
        os << '}';
        return os;
    }
};

class SftBddTest : public testing::TestWithParam<SftTestData> {
   protected:
    void SetUp() override {
#ifdef STORM_HAVE_SYLVAN
        auto const &param{TestWithParam::GetParam()};
        auto dft{storm::dft::api::loadDFTGalileoFile<double>(param.filepath)};
        storm::Environment env;
        checker = std::make_shared<storm::dft::modelchecker::SFTBDDChecker>(dft, std::make_shared<storm::dft::storage::SylvanBddManager>(env));
#else
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    std::shared_ptr<storm::dft::modelchecker::SFTBDDChecker> checker;
};

TEST_P(SftBddTest, bddHash) {
#ifdef STORM_HAVE_SYLVAN
    auto const &param{TestWithParam::GetParam()};
    EXPECT_EQ(checker->getTransformator()->transformTopLevel().GetShaHash(), param.bddHash);
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

TEST_P(SftBddTest, ProbabilityAtTimeOne) {
    auto const &param{TestWithParam::GetParam()};
    EXPECT_NEAR(checker->getProbabilityAtTimebound(1), param.probabilityAtTimeboundOne, 1e-6);
}

TEST_P(SftBddTest, MTTF) {
    auto const &param{TestWithParam::GetParam()};
    EXPECT_NEAR(storm::dft::utility::MTTFHelperProceeding(checker->getDFT()), param.mttf, 1e-5);
    EXPECT_NEAR(storm::dft::utility::MTTFHelperVariableChange(checker->getDFT()), param.mttf, 1e-5);
}

template<typename T1, typename T2>
void expectVectorNear(T1 const &v1, T2 const &v2, double const precision = 1e-6) {
    ASSERT_EQ(v1.size(), v2.size());
    for (size_t i{0}; i < v1.size(); ++i) {
        if (!std::isinf(v1[i])) {
            EXPECT_NEAR(v1[i], v2[i], precision);
        } else {
            EXPECT_EQ(v1[i], v2[i]);
        }
    }
}

TEST_P(SftBddTest, Birnbaum) {
    auto const &param{TestWithParam::GetParam()};
    expectVectorNear(checker->getAllBirnbaumFactorsAtTimebound(1), param.birnbaum);
}

TEST_P(SftBddTest, CIF) {
    auto const &param{TestWithParam::GetParam()};
    expectVectorNear(checker->getAllCIFsAtTimebound(1), param.CIF);
}

TEST_P(SftBddTest, DIF) {
    auto const &param{TestWithParam::GetParam()};
    expectVectorNear(checker->getAllDIFsAtTimebound(1), param.DIF);
}

TEST_P(SftBddTest, RAW) {
    auto const &param{TestWithParam::GetParam()};
    expectVectorNear(checker->getAllRAWsAtTimebound(1), param.RAW);
}

TEST_P(SftBddTest, RRW) {
    auto const &param{TestWithParam::GetParam()};
    expectVectorNear(checker->getAllRRWsAtTimebound(1), param.RRW);
}

std::vector<SftTestData> sftTestData{
    {
        "And",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/AndTest.dft",
        "07251c962e40a962c342b8673fd18b45a1461ebd917f83f6720aa106cf277f9f",
        0.25,
        2.164042561,
        {0.5, 0.5},
        {1, 1},
        {1, 1},
        {2, 2},
        {INFINITY, INFINITY},
    },
    {
        "Or",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/OrTest.dft",
        "c5cf2304417926961c3e1ce1d876fc2886ece1365fd946bfd3e1abd71401696d",
        0.75,
        0.7213475204,
        {0.5, 0.5},
        {0.3333333333, 0.3333333333},
        {0.666667, 0.666667},
        {1.333333333, 1.333333333},
        {1.5, 1.5},
    },
    {
        "AndOr",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft",
        "fc1e9a418e3c207e81ffa7fde7768f027b6996732c4216c1ed5de6861dbc86ae",
        0.5625,
        1.082021281,
        {0.375, 0.375, 0.375, 0.375},
        {0.3333333333, 0.3333333333, 0.3333333333, 0.3333333333},
        {0.666667, 0.666667, 0.666667, 0.666667},
        {1.333333333, 1.333333333, 1.333333333, 1.333333333},
        {1.5, 1.5, 1.5, 1.5},
    },
    {
        "Vot",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/VotTest.dft",
        "c005a8d6ad70cc497e1efa3733b0dc52e94c465572d3f1fc5de4983ddd178094",
        0.6875,
        0.8415721072,
        {0.375, 0.375, 0.375, 0.375},
        {0.27272727, 0.27272727, 0.27272727, 0.27272727},
        {0.636364, 0.636364, 0.636364, 0.636364},
        {1.27272727, 1.27272727, 1.27272727, 1.27272727},
        {1.375, 1.375, 1.375, 1.375},
    },
    {
        "Importance",
        STORM_TEST_RESOURCES_DIR "/dft/bdd/ImportanceTest.dft",
        "38221c1eb557dd6f15cf33faf61e18c4e426fab2fb909c56ac0d5f4ff0be499f",
        0.2655055433,
        1.977074913,
        {0.531011, 0.368041, 0.224763, 0.0596235, 0.0543206, 0.0810368, 0},
        {1, 0.693094, 0.153453, 0.112283, 0.09231, 0.192934, 0},
        {1, 0.846547, 0.306906, 0.556142, 0.501849, 0.703097, 0.5},
        {2, 1.693094106, 1.693094106, 1.112283035, 1.112283035, 1.112283035, 1},
        {INFINITY, 3.25832778, 1.18127, 1.126485175, 1.1016977, 1.23905588, 1},
    },
};
INSTANTIATE_TEST_SUITE_P(SFTs, SftBddTest, testing::ValuesIn(sftTestData), [](auto const &info) { return info.param.testname; });

TEST(TestBdd, AndOrRelevantEvents) {
#ifdef STORM_HAVE_SYLVAN
    auto dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft");
    storm::Environment env;
    auto manager = std::make_shared<storm::dft::storage::SylvanBddManager>(env);
    storm::dft::utility::RelevantEvents relevantEvents{"F", "F1", "F2", "x1"};
    storm::dft::transformations::SftToBddTransformator<double> transformer{dft, manager, relevantEvents};

    auto const result = transformer.transformRelevantEvents();

    EXPECT_EQ(result.size(), 4ul);

    EXPECT_EQ(result.at("F").GetShaHash(), "fc1e9a418e3c207e81ffa7fde7768f027b6996732c4216c1ed5de6861dbc86ae");
    EXPECT_EQ(result.at("F1").GetShaHash(), "c5cf2304417926961c3e1ce1d876fc2886ece1365fd946bfd3e1abd71401696d");
    EXPECT_EQ(result.at("F2").GetShaHash(), "a4f129fa27c6cd32625b088811d4b12f8059ae0547ee035c083deed9ef9d2c59");
    EXPECT_EQ(result.at("x1").GetShaHash(), "b0d991484e405a391b6d3d241fed9c00d4a2e5bf6f57300512394d819253893d");
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

TEST(TestBdd, AndOrRelevantEventsChecked) {
#ifdef STORM_HAVE_SYLVAN
    auto dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft");
    storm::Environment env;
    auto manager{std::make_shared<storm::dft::storage::SylvanBddManager>(env)};
    storm::dft::utility::RelevantEvents relevantEvents{"F", "F1", "F2", "x1"};
    auto transformator{std::make_shared<storm::dft::transformations::SftToBddTransformator<double>>(dft, manager, relevantEvents)};

    storm::dft::modelchecker::SFTBDDChecker checker{transformator};

    auto relevantEventsBdds = transformator->transformRelevantEvents();

    EXPECT_NEAR(checker.getProbabilityAtTimebound(relevantEventsBdds["F"], 1), 0.5625, 1e-6);

    EXPECT_NEAR(checker.getProbabilityAtTimebound(relevantEventsBdds["F1"], 1), 0.75, 1e-6);
    EXPECT_NEAR(checker.getProbabilityAtTimebound(relevantEventsBdds["F2"], 1), 0.75, 1e-6);

    EXPECT_NEAR(checker.getProbabilityAtTimebound(relevantEventsBdds["x1"], 1), 0.5, 1e-6);
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

TEST(TestBdd, AndOrFormulaFail) {
#ifdef STORM_HAVE_SYLVAN
    auto dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft");
    auto const props{storm::api::extractFormulasFromProperties(storm::api::parseProperties("P=? [F < 1 !\"F2_failed\"];"))};
    storm::Environment env;
    storm::dft::adapters::SFTBDDPropertyFormulaAdapter checker{dft, props, std::make_shared<storm::dft::storage::SylvanBddManager>(env)};

    STORM_SILENT_EXPECT_THROW(checker.check(), storm::exceptions::NotSupportedException);
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

TEST(TestBdd, AndOrFormula) {
#ifdef STORM_HAVE_SYLVAN
    auto dft = storm::dft::api::loadDFTGalileoFile<double>(STORM_TEST_RESOURCES_DIR "/dft/bdd/AndOrTest.dft");
    auto const props{
        storm::api::extractFormulasFromProperties(storm::api::parseProperties("P=? [F <= 1 \"failed\"];"
                                                                              "P=? [F <= 1 \"F_failed\"];"
                                                                              "P=? [F <= 1 \"F1_failed\" & \"F2_failed\"];"
                                                                              "P=? [F  = 1 !\"failed\"];"
                                                                              "P=? [F  = 1 !\"F1_failed\"];"
                                                                              "P=? [F  = 1 !\"F2_failed\"];"
                                                                              "P=? [F <= 1 \"F1_failed\"];"
                                                                              "P=? [F <= 1 \"F2_failed\"];"))};
    storm::Environment env;
    storm::dft::adapters::SFTBDDPropertyFormulaAdapter checker{dft, props, std::make_shared<storm::dft::storage::SylvanBddManager>(env)};

    auto const resultProbs{checker.check()};
    auto const result{checker.formulasToBdd()};

    EXPECT_EQ(result.size(), 8ul);

    EXPECT_EQ(resultProbs[0], resultProbs[1]);
    EXPECT_EQ(resultProbs[1], resultProbs[2]);
    EXPECT_NEAR(resultProbs[0], 0.5625, 1e-6);

    EXPECT_NEAR(resultProbs[3], 1 - resultProbs[0], 1e-6);

    EXPECT_EQ(resultProbs[6], resultProbs[7]);
    EXPECT_NEAR(resultProbs[6], 0.75, 1e-6);

    EXPECT_EQ(resultProbs[4], resultProbs[5]);
    EXPECT_NEAR(resultProbs[4], 1 - resultProbs[6], 1e-6);

    EXPECT_EQ(result[0], result[1]);
    EXPECT_EQ(result[1], result[2]);
    EXPECT_EQ(result[2], result[0]);

    EXPECT_EQ(result[0].GetBDD(), result[1].GetBDD());
    EXPECT_EQ(result[1].GetBDD(), result[2].GetBDD());
    EXPECT_EQ(result[2].GetBDD(), result[0].GetBDD());

    EXPECT_EQ(result[0].GetBDD(), (!result[3]).GetBDD());
    EXPECT_NE(result[0].GetBDD(), result[3].GetBDD());

    EXPECT_EQ(result[6].GetBDD(), (!result[4]).GetBDD());
    EXPECT_NE(result[6].GetBDD(), result[4].GetBDD());

    EXPECT_EQ(result[7].GetBDD(), (!result[5]).GetBDD());
    EXPECT_NE(result[7].GetBDD(), result[5].GetBDD());

    EXPECT_NE(result[3].GetBDD(), result[4].GetBDD());

    EXPECT_EQ(result[0].GetShaHash(), "fc1e9a418e3c207e81ffa7fde7768f027b6996732c4216c1ed5de6861dbc86ae");
    EXPECT_EQ(result[1].GetShaHash(), "fc1e9a418e3c207e81ffa7fde7768f027b6996732c4216c1ed5de6861dbc86ae");
    EXPECT_EQ(result[2].GetShaHash(), "fc1e9a418e3c207e81ffa7fde7768f027b6996732c4216c1ed5de6861dbc86ae");
    EXPECT_EQ(result[3].GetShaHash(), "fc1e9a418e3c207e81ffa7fde7768f027b6996732c4216c1ed5de6861dbc86ae");
    EXPECT_EQ(result[4].GetShaHash(), "c5cf2304417926961c3e1ce1d876fc2886ece1365fd946bfd3e1abd71401696d");
    EXPECT_EQ(result[5].GetShaHash(), "a4f129fa27c6cd32625b088811d4b12f8059ae0547ee035c083deed9ef9d2c59");
    EXPECT_EQ(result[6].GetShaHash(), "c5cf2304417926961c3e1ce1d876fc2886ece1365fd946bfd3e1abd71401696d");
    EXPECT_EQ(result[7].GetShaHash(), "a4f129fa27c6cd32625b088811d4b12f8059ae0547ee035c083deed9ef9d2c59");
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

}  // namespace
