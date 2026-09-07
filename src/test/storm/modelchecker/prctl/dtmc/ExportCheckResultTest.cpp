#include "storm-config.h"
#include "test/storm_gtest.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/export.h"
#include "storm/api/properties.h"
#include "storm/environment/Environment.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/constants.h"

namespace {

// From state 1 the target is never reached, so the expected reward is infinite there and in state 0, which reaches
// state 1 with probability 1/2. States 2 and 3 have a finite expected reward of 1 and 0, respectively.
//
// Every state is initial: a program that pins the initial state down instead has its initial states enumerated by an
// SMT solver, which this test would then need a Storm built with one to run at all. Which states are initial makes no
// difference to what is checked below, since the check result and its export cover all of them either way.
std::string const modelDescription = R"(
dtmc

module main
    s : [0..3];
    [] s=0 -> 1/2 : (s'=1) + 1/2 : (s'=2);
    [] s=1 -> 1 : (s'=1);
    [] s=2 -> 1 : (s'=3);
    [] s=3 -> 1 : (s'=3);
endmodule

init
    true
endinit

label "target" = s=3;

rewards "steps"
    s<3 : 1;
endrewards
)";

std::filesystem::path getTemporaryFilePath() {
    std::error_code ec;
    auto tmpDir = std::filesystem::temp_directory_path(ec);
    EXPECT_EQ(0, ec.value()) << "Unable to get temporary directory for check result export test: " << ec.message();
    std::random_device rd;
    std::filesystem::path result;
    do {
        result = tmpDir / std::filesystem::path("storm_check_result_export_test_" + std::to_string(rd()) + ".json");
    } while (std::filesystem::exists(result));
    return result;
}

/*!
 * Checks an expected reward property whose value is infinite in some states, exports the result as JSON and reads the
 * export back. Infinite values must survive that round trip -- for the exact value type just as for double, where the
 * value type has an infinity of its own.
 */
template<typename ValueType>
void runExportTest(storm::Environment const& env) {
    storm::prism::Program program = storm::parser::PrismParser::parseFromString(modelDescription, "testfile");
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram("R{\"steps\"}=? [F \"target\"]", program));
    ASSERT_EQ(1ull, formulas.size());
    auto model = storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Dtmc<ValueType>>();
    ASSERT_EQ(4ull, model->getNumberOfStates());

    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> checker(*model);
    std::unique_ptr<storm::modelchecker::CheckResult> result = checker.check(env, *formulas[0]);
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    auto const& values = result->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector();
    ASSERT_EQ(4ull, values.size());

    // Two states cannot reach the target, so their expected reward is infinite. Without those, the export below would
    // not exercise anything.
    uint64_t numberOfInfiniteValues = 0;
    for (auto const& value : values) {
        if (storm::utility::isInfinity(value)) {
            ++numberOfInfiniteValues;
        }
    }
    ASSERT_EQ(2ull, numberOfInfiniteValues);

    auto const path = getTemporaryFilePath();
    ASSERT_NO_THROW(storm::api::exportCheckResultToJson<ValueType>(model, result, path.string()));
    std::ifstream exportedStream(path);
    ASSERT_TRUE(exportedStream.good());
    storm::json<double> exported;
    ASSERT_NO_THROW(exportedStream >> exported);
    exportedStream.close();
    std::filesystem::remove(path);

    ASSERT_TRUE(exported.is_array());
    ASSERT_EQ(values.size(), exported.size());
    for (auto const& entry : exported) {
        ASSERT_TRUE(entry.count("s") == 1 && entry.count("v") == 1);
        uint64_t const state = entry.at("s").template get<uint64_t>();
        ASSERT_LT(state, values.size());
        if (storm::utility::isInfinity(values[state])) {
            // JSON has no infinity of its own, so an infinite value is exported as this string.
            ASSERT_TRUE(entry.at("v").is_string()) << "State " << state << " has an infinite value, but was exported as " << entry.at("v").dump() << ".";
            EXPECT_EQ("inf", entry.at("v").template get<std::string>());
        } else {
            ASSERT_TRUE(entry.at("v").is_number()) << "State " << state << " has the finite value " << values[state] << ", but was exported as "
                                                   << entry.at("v").dump() << ".";
            EXPECT_EQ(storm::utility::convertNumber<double>(storm::utility::getFinite(values[state])), entry.at("v").template get<double>());
        }
    }
}
}  // namespace

/*!
 * No model checker hands out a negative infinity today, so this builds the check result directly. The export must not
 * fall back to the finite payload of an infinite value, which is a zero that means nothing.
 */
template<typename ValueType>
void runNegativeInfinityExportTest() {
    typedef storm::utility::ExtendedValueType<ValueType> ExtendedValueType;
    std::vector<ExtendedValueType> values{storm::utility::one<ExtendedValueType>(), storm::utility::negativeInfinity<ValueType>(),
                                          storm::utility::positiveInfinity<ValueType>()};
    storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType> result(std::move(values));

    storm::json<ValueType> exported;
    ASSERT_NO_THROW(exported = result.toJson());
    ASSERT_TRUE(exported.is_array());
    ASSERT_EQ(3ull, exported.size());

    EXPECT_TRUE(exported[0].at("v").is_number());
    // JSON has no infinity of its own, and the two of them must not be exported as the same thing.
    ASSERT_TRUE(exported[1].at("v").is_string()) << "The negative infinity was exported as " << exported[1].at("v").dump() << ".";
    EXPECT_EQ("-inf", exported[1].at("v").template get<std::string>());
    ASSERT_TRUE(exported[2].at("v").is_string()) << "The positive infinity was exported as " << exported[2].at("v").dump() << ".";
    EXPECT_EQ("inf", exported[2].at("v").template get<std::string>());
}

TEST(ExportCheckResultTest, NegativeInfinityDouble) {
    runNegativeInfinityExportTest<double>();
}

TEST(ExportCheckResultTest, NegativeInfinityExact) {
    runNegativeInfinityExportTest<storm::RationalNumber>();
}

/*!
 * A check result holds its values in the extended value type, so the test that decides whether to print the decimal
 * approximation of an exact value has to recognise that type too. It recognised only the plain one for a while, which
 * silently dropped the approximation from every exact result.
 */
TEST(ExportCheckResultTest, PrintsTheApproximationOfAnExactValue) {
    typedef storm::utility::ExtendedValueType<storm::RationalNumber> ExtendedValueType;
    storm::RationalNumber const third = storm::RationalNumber(1) / storm::RationalNumber(3);
    storm::modelchecker::ExplicitQuantitativeCheckResult<storm::RationalNumber> result(std::vector<ExtendedValueType>{ExtendedValueType(third)});

    std::stringstream out;
    result.writeToStream(out);
    EXPECT_NE(std::string::npos, out.str().find("1/3")) << "The exact value is missing from " << out.str() << ".";
    EXPECT_NE(std::string::npos, out.str().find("(approx.")) << "The approximation is missing from " << out.str() << ".";
}

/*!
 * The counterpart: a value type that is already decimal has nothing to approximate.
 */
TEST(ExportCheckResultTest, PrintsNoApproximationOfAnInexactValue) {
    storm::modelchecker::ExplicitQuantitativeCheckResult<double> result(std::vector<double>{0.25});

    std::stringstream out;
    result.writeToStream(out);
    EXPECT_EQ(std::string::npos, out.str().find("(approx.")) << "An approximation was printed for " << out.str() << ".";
}

TEST(ExportCheckResultTest, InfiniteRewardDouble) {
    storm::Environment env;
    runExportTest<double>(env);
}

TEST(ExportCheckResultTest, InfiniteRewardExact) {
    storm::Environment env;
    env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
    env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
    runExportTest<storm::RationalNumber>(env);
}
