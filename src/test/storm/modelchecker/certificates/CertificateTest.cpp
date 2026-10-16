#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/CertificateModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/certificates/CertificateAlgorithm.h"
#include "storm/modelchecker/certificates/ReachabilityProbabilityCertificate.h"
#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitCertificateCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace {

// P(F "target") is 12/17 in state 0. States 3 and 4 are the target and a sink, respectively.
std::string const dtmcWithSink = R"(
dtmc

module main
    s : [0..4];
    [] s=0 -> 0.5 : (s'=1) + 0.5 : (s'=2);
    [] s=1 -> 0.3 : (s'=0) + 0.7 : (s'=3);
    [] s=2 -> 0.5 : (s'=3) + 0.5 : (s'=4);
    [] s=3 -> 1 : (s'=3);
    [] s=4 -> 1 : (s'=4);
endmodule

// All states are initial State ids then correspond to the values of s.
init
    true
endinit

label "target" = s=3;
)";

// The expected number of steps to the target is 40/17 in state 0.
std::string const dtmcWithoutSink = R"(
dtmc

module main
    s : [0..3];
    [] s=0 -> 0.5 : (s'=1) + 0.5 : (s'=2);
    [] s=1 -> 0.3 : (s'=0) + 0.7 : (s'=3);
    [] s=2 -> 1 : (s'=3);
    [] s=3 -> 1 : (s'=3);
endmodule

// All states are initial State ids then correspond to the values of s.
init
    true
endinit

label "target" = s=3;

rewards "steps"
    s<3 : 1;
endrewards
)";

// Pmax(F "target") = 1 and Pmin(F "target") = 1/5 in state 0.
// Rmin(F "target") = 3 in state 0, but Rmax(F "target") is infinite there since the sink can be reached.
std::string const mdp = R"(
mdp

module main
    s : [0..4];
    [a] s=0 -> 0.5 : (s'=1) + 0.5 : (s'=0);
    [b] s=0 -> 0.2 : (s'=2) + 0.8 : (s'=4);
    [a] s=1 -> 1 : (s'=3);
    [b] s=1 -> 0.5 : (s'=3) + 0.5 : (s'=4);
    [] s=2 -> 1 : (s'=3);
    [] s=3 -> 1 : (s'=3);
    [] s=4 -> 1 : (s'=4);
endmodule

// All states are initial State ids then correspond to the values of s.
init
    true
endinit

label "target" = s=3;

rewards "steps"
    s<3 : 1;
endrewards
)";

struct CertifiedBounds {
    double lower{-std::numeric_limits<double>::infinity()};
    double upper{std::numeric_limits<double>::infinity()};
};

double parseNumber(std::string const& token) {
    if (token == "inf") {
        return std::numeric_limits<double>::infinity();
    } else if (token == "-inf") {
        return -std::numeric_limits<double>::infinity();
    }
    // Rational numbers are exported as fractions
    auto const slash = token.find('/');
    if (slash != std::string::npos) {
        return std::stod(token.substr(0, slash)) / std::stod(token.substr(slash + 1));
    }
    return std::stod(token);
}

/*!
 * Reads the bounds of the given state from the summary of the certificate, which starts with "<description>: [lower, upper]".
 */
template<typename ValueType>
CertifiedBounds getBoundsFromSummary(storm::modelchecker::Certificate<ValueType> const& certificate, uint64_t numStates, uint64_t state) {
    storm::storage::BitVector relevantStates(numStates, false);
    relevantStates.set(state);
    std::string const summary = certificate.summaryString(relevantStates);
    auto const open = summary.find('[');
    auto const comma = summary.find(", ", open);
    auto const close = summary.find(']', comma);
    EXPECT_TRUE(open != std::string::npos && comma != std::string::npos && close != std::string::npos) << summary;
    return CertifiedBounds{parseNumber(summary.substr(open + 1, comma - open - 1)), parseNumber(summary.substr(comma + 2, close - comma - 2))};
}

class DoubleFpIIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.modelchecker().certificate().setAlgorithm(storm::CertificateAlgorithm::FpII);
        return env;
    }
};

class DoubleFpIITopologicalEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        auto env = DoubleFpIIEnvironment::createEnvironment();
        env.modelchecker().certificate().setUseTopological(true);
        return env;
    }
};

class RationalExPIEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.modelchecker().certificate().setAlgorithm(storm::CertificateAlgorithm::ExPI);
        return env;
    }
};

class RationalExPITopologicalEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static storm::Environment createEnvironment() {
        auto env = RationalExPIEnvironment::createEnvironment();
        env.modelchecker().certificate().setUseTopological(true);
        return env;
    }
};

template<typename TestType>
class CertificateTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    CertificateTest() : _environment(TestType::createEnvironment()) {}

    storm::Environment const& env() const {
        return _environment;
    }

    template<typename ModelType, typename ModelCheckerType>
    std::unique_ptr<storm::modelchecker::CheckResult> checkWithCertificate(std::string const& modelDescription, std::string const& formulaString,
                                                                           std::shared_ptr<ModelType>& model,
                                                                           storm::Environment const* customEnvironment = nullptr) const {
        storm::prism::Program program = storm::parser::PrismParser::parseFromString(modelDescription, "testfile");
        auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
        model = storm::api::buildSparseModel<ValueType>(program, formulas)->template as<ModelType>();
        ModelCheckerType checker(*model);
        storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formulas[0], true);
        task.setProduceCertificate(true);
        return checker.check(customEnvironment ? *customEnvironment : env(), task);
    }

    /*!
     * Checks that the result is a valid certificate whose bounds for state 0 enclose the expected value.
     */
    void checkCertificate(storm::modelchecker::CheckResult& result, storm::models::sparse::Model<ValueType> const& model, double expectedValue) const {
        ASSERT_TRUE(result.isExplicitCertificateCheckResult());
        EXPECT_TRUE(result.isExplicit());
        EXPECT_TRUE(result.isResultForAllStates());
        auto& certificateResult = result.template asExplicitCertificateCheckResult<ValueType>();
        EXPECT_TRUE(certificateResult.checkValidity(model));
        auto const bounds = getBoundsFromSummary(certificateResult.getCertificate(), model.getNumberOfStates(), 0);
        // The summary shows a limited number of digits.
        double const tolerance = 1e-5;
        if (std::isinf(expectedValue)) {
            EXPECT_TRUE(std::isinf(bounds.lower) && std::isinf(bounds.upper)) << "[" << bounds.lower << ", " << bounds.upper << "]";
        } else {
            EXPECT_LE(bounds.lower, expectedValue + tolerance);
            EXPECT_GE(bounds.upper, expectedValue - tolerance);
            EXPECT_LE(bounds.upper - bounds.lower, tolerance);
        }
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<DoubleFpIIEnvironment, DoubleFpIITopologicalEnvironment, RationalExPIEnvironment, RationalExPITopologicalEnvironment> TestingTypes;

TYPED_TEST_SUITE(CertificateTest, TestingTypes, );

TYPED_TEST(CertificateTest, DtmcReachabilityProbability) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    auto result =
        this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(dtmcWithSink, "P=? [F \"target\"]", model);
    this->checkCertificate(*result, *model, 12.0 / 17.0);
}

TYPED_TEST(CertificateTest, DtmcConstrainedReachabilityProbability) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    // Staying in the states 0 and 1 and moving to the target through state 1 only: v0 = 0.5 * v1, v1 = 0.3 * v0 + 0.7.
    auto result =
        this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(dtmcWithSink, "P=? [s<2 U s=3]", model);
    this->checkCertificate(*result, *model, 0.35 / 0.85);
}

TYPED_TEST(CertificateTest, DtmcReachabilityReward) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    auto result = this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(
        dtmcWithoutSink, "R{\"steps\"}=? [F \"target\"]", model);
    this->checkCertificate(*result, *model, 40.0 / 17.0);
}

TYPED_TEST(CertificateTest, DtmcReachabilityRewardInfinite) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    auto result = this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(
        // The state 4 is a sink, so the expected reward is infinite in all states that can reach it.
        std::string(dtmcWithSink) + "\nrewards \"steps\"\n    s<3 : 1;\nendrewards\n", "R{\"steps\"}=? [F \"target\"]", model);
    this->checkCertificate(*result, *model, std::numeric_limits<double>::infinity());
}

TYPED_TEST(CertificateTest, DtmcReachabilityTime) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    auto result = this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(dtmcWithoutSink,
                                                                                                                              "T=? [F \"target\"]", model);
    this->checkCertificate(*result, *model, 40.0 / 17.0);
}

TYPED_TEST(CertificateTest, DtmcEmptyConstraintReachabilityProbability) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    // No state satisfies s>4, so the target can only be reached from within the target. This is different from not having a constraint.
    auto result =
        this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(dtmcWithSink, "P=? [s>4 U s=3]", model);
    this->checkCertificate(*result, *model, 0.0);
}

TYPED_TEST(CertificateTest, DtmcNegativeRewardsAreRejected) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    std::string const negativeRewards = dtmcWithoutSink + "\nrewards \"negative\"\n    s<3 : -1;\nendrewards\n";
    STORM_SILENT_EXPECT_THROW((this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(
                                  negativeRewards, "R{\"negative\"}=? [F \"target\"]", model)),
                              storm::exceptions::NotSupportedException);
}

TYPED_TEST(CertificateTest, SmoothIterationWithGammaOneIsRejected) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    for (auto const algorithm : {storm::CertificateAlgorithm::FpSmoothII, storm::CertificateAlgorithm::FpRoundII}) {
        storm::Environment env = this->env();
        env.modelchecker().certificate().setAlgorithm(algorithm);
        env.modelchecker().certificate().setGamma(1.0);
        STORM_SILENT_EXPECT_THROW((this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(
                                      dtmcWithSink, "P=? [F \"target\"]", model, &env)),
                                  storm::exceptions::InvalidEnvironmentException);
    }
}

TYPED_TEST(CertificateTest, CtmcReachability) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Ctmc<ValueType> ModelType;
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3, 3);
    builder.addNextValue(0, 1, storm::utility::convertNumber<ValueType>(2.0));
    builder.addNextValue(1, 2, storm::utility::convertNumber<ValueType>(4.0));
    builder.addNextValue(2, 2, storm::utility::convertNumber<ValueType>(1.0));  // The target is absorbing
    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("target");
    labeling.addLabelToState("target", 2);
    ModelType model(builder.build(3, 3), std::move(labeling));

    storm::parser::FormulaParser formulaParser(std::make_shared<storm::expressions::ExpressionManager>());
    storm::modelchecker::SparseCtmcCslModelChecker<ModelType> checker(model);
    for (auto const& [formulaString, expectedValue] : std::vector<std::pair<std::string, double>>{{"P=? [F \"target\"]", 1.0}, {"T=? [F \"target\"]", 0.75}}) {
        auto const formula = formulaParser.parseSingleFormulaFromString(formulaString);
        storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);
        task.setProduceCertificate(true);
        auto result = checker.check(this->env(), task);
        this->checkCertificate(*result, model, expectedValue);
    }
}

// Json export

template<typename ValueType>
double jsonToDouble(storm::json<ValueType> const& j) {
    if (j.is_string()) {
        return parseNumber(j.template get<std::string>());
    }
    if constexpr (std::is_same_v<ValueType, double>) {
        return j.template get<double>();
    }
    ADD_FAILURE() << "Unexpected json value " << j.dump();
    return 0.0;
}

TYPED_TEST(CertificateTest, JsonExportProbability) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    auto result =
        this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(dtmcWithSink, "P=? [F \"target\"]", model);
    ASSERT_TRUE(result->isExplicitCertificateCheckResult());
    storm::json<ValueType> const j = result->template asExplicitCertificateCheckResult<ValueType>().getCertificate().toJson();

    EXPECT_EQ("reachability-probability", j.at("kind").template get<std::string>());
    EXPECT_FALSE(j.contains("direction"));
    EXPECT_FALSE(j.contains("constraint"));
    ASSERT_EQ(1ull, j.at("target").at("states").size());
    EXPECT_EQ(3ull, j.at("target").at("states").at(0).template get<uint64_t>());
    EXPECT_NE(std::string::npos, j.at("target").at("label").template get<std::string>().find("target"));

    auto const& lowerValues = j.at("lower").at("values");
    auto const& upperValues = j.at("upper").at("values");
    ASSERT_EQ(5ull, lowerValues.size());
    ASSERT_EQ(5ull, upperValues.size());
    EXPECT_LE(jsonToDouble<ValueType>(lowerValues.at(0)), 12.0 / 17.0 + 1e-6);
    EXPECT_GE(jsonToDouble<ValueType>(upperValues.at(0)), 12.0 / 17.0 - 1e-6);
    EXPECT_NEAR(12.0 / 17.0, jsonToDouble<ValueType>(lowerValues.at(0)), 1e-6);
    EXPECT_EQ(1.0, jsonToDouble<ValueType>(lowerValues.at(3)));
    EXPECT_EQ(0.0, jsonToDouble<ValueType>(upperValues.at(4)));
    if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        // Exact values are exported as fractions
        EXPECT_EQ("12/17", lowerValues.at(0).template get<std::string>());
        EXPECT_EQ("31/34", lowerValues.at(1).template get<std::string>());
        EXPECT_EQ("1/2", upperValues.at(2).template get<std::string>());
        EXPECT_EQ("1", upperValues.at(3).template get<std::string>());
    }

    auto const& ranks = j.at("lower").at("ranks");
    ASSERT_EQ(5ull, ranks.size());
    EXPECT_EQ(2ull, ranks.at(0).template get<uint64_t>());
    EXPECT_EQ(1ull, ranks.at(1).template get<uint64_t>());
    EXPECT_EQ(1ull, ranks.at(2).template get<uint64_t>());
    EXPECT_EQ(0ull, ranks.at(3).template get<uint64_t>());
    EXPECT_EQ("inf", ranks.at(4).template get<std::string>());
    EXPECT_FALSE(j.at("upper").contains("ranks"));
}

TYPED_TEST(CertificateTest, JsonExportConstrainedProbability) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    auto result =
        this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(dtmcWithSink, "P=? [s<2 U s=3]", model);
    storm::json<ValueType> const j = result->template asExplicitCertificateCheckResult<ValueType>().getCertificate().toJson();
    ASSERT_TRUE(j.contains("constraint"));
    ASSERT_EQ(2ull, j.at("constraint").at("states").size());
    EXPECT_EQ(0ull, j.at("constraint").at("states").at(0).template get<uint64_t>());
    EXPECT_EQ(1ull, j.at("constraint").at("states").at(1).template get<uint64_t>());
    EXPECT_TRUE(j.at("constraint").contains("label"));
}

TYPED_TEST(CertificateTest, JsonExportReward) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    std::shared_ptr<ModelType> model;
    auto result = this->template checkWithCertificate<ModelType, storm::modelchecker::SparseDtmcPrctlModelChecker<ModelType>>(
        dtmcWithoutSink, "R{\"steps\"}=? [F \"target\"]", model);
    ASSERT_TRUE(result->isExplicitCertificateCheckResult());
    storm::json<ValueType> const j = result->template asExplicitCertificateCheckResult<ValueType>().getCertificate().toJson();

    EXPECT_EQ("reachability-reward", j.at("kind").template get<std::string>());
    EXPECT_FALSE(j.contains("direction"));
    EXPECT_EQ("steps", j.at("reward-model").template get<std::string>());
    ASSERT_EQ(4ull, j.at("choice-rewards").size());
    for (uint64_t choice = 0; choice < 3; ++choice) {
        EXPECT_EQ(1.0, jsonToDouble<ValueType>(j.at("choice-rewards").at(choice)));
    }
    EXPECT_EQ(0.0, jsonToDouble<ValueType>(j.at("choice-rewards").at(3)));
    ASSERT_EQ(1ull, j.at("target").at("states").size());

    ASSERT_EQ(4ull, j.at("lower").at("values").size());
    ASSERT_EQ(4ull, j.at("upper").at("values").size());
    EXPECT_NEAR(40.0 / 17.0, jsonToDouble<ValueType>(j.at("lower").at("values").at(0)), 1e-6);
    EXPECT_NEAR(40.0 / 17.0, jsonToDouble<ValueType>(j.at("upper").at("values").at(0)), 1e-6);
    if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        EXPECT_EQ("40/17", j.at("upper").at("values").at(0).template get<std::string>());
        EXPECT_EQ("29/17", j.at("upper").at("values").at(1).template get<std::string>());
    }
    // There are no states with infinite value, so the lower bound ranks are all infinite
    for (auto const& rank : j.at("lower").at("ranks")) {
        EXPECT_EQ("inf", rank.template get<std::string>());
    }
    auto const& upperRanks = j.at("upper").at("ranks");
    ASSERT_EQ(4ull, upperRanks.size());
    EXPECT_EQ(2ull, upperRanks.at(0).template get<uint64_t>());
    EXPECT_EQ(1ull, upperRanks.at(1).template get<uint64_t>());
    EXPECT_EQ(1ull, upperRanks.at(2).template get<uint64_t>());
    EXPECT_EQ(0ull, upperRanks.at(3).template get<uint64_t>());
}

TYPED_TEST(CertificateTest, JsonExportMdpDirectionAndInfinity) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    {
        std::shared_ptr<ModelType> model;
        auto result =
            this->template checkWithCertificate<ModelType, storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>>(mdp, "Pmin=? [F \"target\"]", model);
        storm::json<ValueType> const j = result->template asExplicitCertificateCheckResult<ValueType>().getCertificate().toJson();
        EXPECT_EQ("min", j.at("direction").template get<std::string>());
    }
    {
        // The maximal expected reward is infinite in state 0
        std::shared_ptr<ModelType> model;
        auto result = this->template checkWithCertificate<ModelType, storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>>(
            mdp, "R{\"steps\"}max=? [F \"target\"]", model);
        storm::json<ValueType> const j = result->template asExplicitCertificateCheckResult<ValueType>().getCertificate().toJson();
        EXPECT_EQ("max", j.at("direction").template get<std::string>());
        EXPECT_EQ("inf", j.at("lower").at("values").at(0).template get<std::string>());
        EXPECT_EQ("inf", j.at("upper").at("values").at(0).template get<std::string>());
        // The rewards are given for each choice, not each state
        EXPECT_EQ(model->getNumberOfChoices(), j.at("choice-rewards").size());
    }
}

TYPED_TEST(CertificateTest, MdpReachabilityProbability) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    {
        std::shared_ptr<ModelType> model;
        auto result =
            this->template checkWithCertificate<ModelType, storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>>(mdp, "Pmax=? [F \"target\"]", model);
        this->checkCertificate(*result, *model, 1.0);
    }
    {
        std::shared_ptr<ModelType> model;
        auto result =
            this->template checkWithCertificate<ModelType, storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>>(mdp, "Pmin=? [F \"target\"]", model);
        this->checkCertificate(*result, *model, 0.2);
    }
}

TYPED_TEST(CertificateTest, MdpReachabilityReward) {
    typedef typename TestFixture::ValueType ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    {
        std::shared_ptr<ModelType> model;
        auto result = this->template checkWithCertificate<ModelType, storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>>(
            mdp, "R{\"steps\"}min=? [F \"target\"]", model);
        this->checkCertificate(*result, *model, 3.0);
    }
    {
        std::shared_ptr<ModelType> model;
        auto result = this->template checkWithCertificate<ModelType, storm::modelchecker::SparseMdpPrctlModelChecker<ModelType>>(
            mdp, "R{\"steps\"}max=? [F \"target\"]", model);
        this->checkCertificate(*result, *model, std::numeric_limits<double>::infinity());
    }
}

// The following tests build certificates by hand to make sure that the validity check detects invalid ones.
class HandMadeCertificateTest : public ::testing::Test {
   public:
    typedef storm::RationalNumber ValueType;

    void SetUp() override {
        storm::prism::Program program = storm::parser::PrismParser::parseFromString(dtmcWithSink, "testfile");
        model = storm::api::buildSparseModel<ValueType>(program, std::vector<std::shared_ptr<storm::logic::Formula const>>())
                    ->as<storm::models::sparse::Dtmc<ValueType>>();
        targetStates = storm::storage::BitVector(model->getNumberOfStates(), false);
        targetStates.set(3);
    }

    static std::vector<ValueType> rationals(std::vector<std::pair<int, int>> const& fractions) {
        std::vector<ValueType> result;
        for (auto const& [numerator, denominator] : fractions) {
            result.push_back(storm::utility::convertNumber<ValueType>(numerator) / storm::utility::convertNumber<ValueType>(denominator));
        }
        return result;
    }

    std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> model;
    storm::storage::BitVector targetStates;
};

TEST_F(HandMadeCertificateTest, UpperBound) {
    // Every state that can reach the target is assigned the value 1, the sink the value 0.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> valid(std::nullopt, targetStates);
    valid.setUpperBoundsCertificate(rationals({{1, 1}, {1, 1}, {1, 1}, {1, 1}, {0, 1}}));
    EXPECT_TRUE(valid.checkValidity(*model));

    // State 2 reaches the target with probability 1/2 so 2/5 is too small.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> notInductive(std::nullopt, targetStates);
    notInductive.setUpperBoundsCertificate(rationals({{1, 1}, {1, 1}, {2, 5}, {1, 1}, {0, 1}}));
    EXPECT_FALSE(notInductive.checkValidity(*model));

    // Probabilities are non-negative, so upper bounds on them are as well (even if the value is inductive).
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> negative(std::nullopt, targetStates);
    negative.setUpperBoundsCertificate(rationals({{1, 1}, {1, 1}, {1, 1}, {1, 1}, {-1, 1}}));
    EXPECT_FALSE(negative.checkValidity(*model));

    // An empty constraint set is a valid constraint (and not the same as having no constraint): The target can only be reached from within the target.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> emptyConstraint(std::nullopt, targetStates,
                                                                                       storm::storage::BitVector(model->getNumberOfStates(), false));
    emptyConstraint.setUpperBoundsCertificate(rationals({{0, 1}, {0, 1}, {0, 1}, {1, 1}, {0, 1}}));
    EXPECT_TRUE(emptyConstraint.checkValidity(*model));

    // The upper bound for target states must be at least one.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> targetTooSmall(std::nullopt, targetStates);
    targetTooSmall.setUpperBoundsCertificate(rationals({{1, 1}, {1, 1}, {1, 1}, {1, 2}, {0, 1}}));
    EXPECT_FALSE(targetTooSmall.checkValidity(*model));
}

TEST_F(HandMadeCertificateTest, LowerBound) {
    auto const inf = storm::modelchecker::InfRank;
    // Lower bound 0 everywhere but in the target. Ranks need to decrease towards the target.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> valid(std::nullopt, targetStates);
    valid.setLowerBoundsCertificate(rationals({{0, 1}, {0, 1}, {0, 1}, {1, 1}, {0, 1}}), {2, 1, 1, 0, inf});
    EXPECT_TRUE(valid.checkValidity(*model));

    // State 1 reaches the target with probability 7/10 in one step, so 9/10 is not a lower bound that is inductive.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> notInductive(std::nullopt, targetStates);
    notInductive.setLowerBoundsCertificate(rationals({{0, 1}, {9, 10}, {0, 1}, {1, 1}, {0, 1}}), {2, 1, 1, 0, inf});
    EXPECT_FALSE(notInductive.checkValidity(*model));

    // Claiming that the sink (state 4) has a positive lower bound is wrong, which the infinite rank reveals.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> sinkWithPositiveBound(std::nullopt, targetStates);
    sinkWithPositiveBound.setLowerBoundsCertificate(rationals({{0, 1}, {0, 1}, {0, 1}, {1, 1}, {1, 10}}), {2, 1, 1, 0, inf});
    EXPECT_FALSE(sinkWithPositiveBound.checkValidity(*model));

    // Positive lower bounds require finite ranks.
    storm::modelchecker::ReachabilityProbabilityCertificate<ValueType> badRanks(std::nullopt, targetStates);
    badRanks.setLowerBoundsCertificate(rationals({{0, 1}, {7, 10}, {1, 2}, {1, 1}, {0, 1}}), {inf, inf, inf, 0, inf});
    EXPECT_FALSE(badRanks.checkValidity(*model));
}

TEST(CertificateAlgorithmTest, StringConversion) {
    for (auto const algorithm : {storm::CertificateAlgorithm::FpII, storm::CertificateAlgorithm::FpSmoothII, storm::CertificateAlgorithm::FpRoundII,
                                 storm::CertificateAlgorithm::ExPI}) {
        std::stringstream name;
        name << algorithm;
        EXPECT_EQ(algorithm, storm::certificateAlgorithmFromString(name.str()));
    }
    STORM_SILENT_EXPECT_THROW(storm::certificateAlgorithmFromString("unknown"), storm::exceptions::UnexpectedException);
}

}  // namespace
