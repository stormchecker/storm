#include "storm-config.h"
#include "test/storm_gtest.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

#include "storm-pars/api/export.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/analysis/GraphConditions.h"
#include "storm/api/storm.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/generator/NextStateGenerator.h"
#include "storm/io/file.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/storage/jani/Property.h"
#include "storm/utility/constants.h"

namespace {

/*!
 * Builds the parametric DTMC at the given file for the given formula.
 */
std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> buildParametricDtmc(std::string const& programFile, std::string const& formulaAsString) {
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    EXPECT_EQ(1ul, formulas.size());
    storm::generator::NextStateGeneratorOptions options(*formulas.front());
    return storm::builder::ExplicitModelBuilder<storm::RationalFunction>(program, options).build()->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
}

/*!
 * Collects the string representations of the given constraints in the collector's discovery order.
 */
std::vector<std::string> asStringsInDiscoveryOrder(storm::analysis::ConstraintCollector::ConstraintSet const& constraints) {
    std::vector<std::string> result;
    for (auto const& entry : constraints) {
        result.push_back(entry.toString());
    }
    return result;
}

/*!
 * Collects the string representations of the given constraints, sorted. Use this only for assertions that must not
 * depend on the order, such as the set of collected constraints. Assertions about the order itself have to use
 * asStringsInDiscoveryOrder, since sorting here would hide any deviation from that order.
 */
std::vector<std::string> asSortedStrings(storm::analysis::ConstraintCollector::ConstraintSet const& constraints) {
    auto result = asStringsInDiscoveryOrder(constraints);
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace

class GraphConditionsTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        storm::clearRFVariablePool();
    }

    void TearDown() override {
        storm::clearRFVariablePool();
    }
};

TEST_F(GraphConditionsTest, PolynomialDenominators) {
    auto dtmc = buildParametricDtmc(STORM_TEST_RESOURCES_DIR "/pdtmc/only_p.pm", "P=? [F s=1]");

    storm::analysis::ConstraintCollector collector(*dtmc);

    // The only parameter must be reported.
    std::vector<storm::RationalFunctionVariable> variables(collector.getVariables().begin(), collector.getVariables().end());
    ASSERT_EQ(1ul, variables.size());
    EXPECT_EQ("p", variables.front().name());

    // The transitions are the symbolic values p and 1 - p, and both denominators are positive constants. We
    // therefore expect the constraints 0 <= p <= 1 and 0 <= 1 - p <= 1. Note that the row sums to 1 identically,
    // so no row-sum constraint is collected.
    //
    // Relations are normalized so that the polynomial never occurs on the greater side, and the upper bound is
    // phrased as a relation of the difference to zero. Consequently, the constraints derived from p and from
    // 1 - p coincide pairwise and are collected only once each.
    std::vector<std::string> expectedWellformed{"((-1 + p) <= 0)", "(-(p) <= 0)"};
    EXPECT_EQ(expectedWellformed, asSortedStrings(collector.getWellformedConstraints()));

    // Both transitions must stay non-zero, which for p and 1 - p amounts to p != 0 and 1 - p != 0.
    std::vector<std::string> expectedGraphPreserving{"((1 - p) != 0)", "(p != 0)"};
    EXPECT_EQ(expectedGraphPreserving, asSortedStrings(collector.getGraphPreservingConstraints()));
}

TEST_F(GraphConditionsTest, SymbolicDenominators) {
    auto dtmc = buildParametricDtmc(STORM_TEST_RESOURCES_DIR "/pdtmc/only_rational_denominator.pm", "P=? [F s=2]");

    storm::analysis::ConstraintCollector collector(*dtmc);

    std::vector<storm::RationalFunctionVariable> variables(collector.getVariables().begin(), collector.getVariables().end());
    ASSERT_EQ(2ul, variables.size());

    // Here both denominators are the symbolic value 1 - p + q, so non-negativity cannot be reduced to a sign
    // constraint on the nominator. Instead, the denominator must be constrained to be non-zero, and the sign of
    // each nominator must follow the sign of the denominator.
    std::vector<std::string> expectedWellformed{"((((-1 + p) - q) < 0) ? ((-1 + p) <= 0) : ((1 - p) <= 0))", "((((-1 + p) - q) < 0) ? (-(q) <= 0) : (q <= 0))",
                                                "(((1 - p) + q) != 0)"};
    EXPECT_EQ(expectedWellformed, asSortedStrings(collector.getWellformedConstraints()));

    std::vector<std::string> expectedGraphPreserving{"((1 - p) != 0)", "(q != 0)"};
    EXPECT_EQ(expectedGraphPreserving, asSortedStrings(collector.getGraphPreservingConstraints()));
}

TEST_F(GraphConditionsTest, ConstraintsAreDeduplicated) {
    auto dtmc = buildParametricDtmc(STORM_TEST_RESOURCES_DIR "/pdtmc/only_p.pm", "P=? [F s=1]");

    storm::analysis::ConstraintCollector collector(*dtmc);

    // The same constraint must not be collected twice, even though it can be derived from several transitions.
    // The parametric die model has seven transitions guarded by the same two parameters.
    auto dtmcWithManyTransitions = buildParametricDtmc(STORM_TEST_RESOURCES_DIR "/pdtmc/parametric_die_2.pm", "P=? [F s=7]");
    storm::analysis::ConstraintCollector dieCollector(*dtmcWithManyTransitions);

    for (auto const& constraints : {asSortedStrings(collector.getWellformedConstraints()), asSortedStrings(collector.getGraphPreservingConstraints()),
                                    asSortedStrings(dieCollector.getWellformedConstraints()), asSortedStrings(dieCollector.getGraphPreservingConstraints())}) {
        std::set<std::string> distinct(constraints.begin(), constraints.end());
        EXPECT_EQ(constraints.size(), distinct.size());
    }
}

TEST_F(GraphConditionsTest, ConstraintsAreOrderedDeterministically) {
    auto dtmc = buildParametricDtmc(STORM_TEST_RESOURCES_DIR "/pdtmc/only_p.pm", "P=? [F s=1]");

    // Collecting the constraints repeatedly from equal models must yield the same order.
    storm::analysis::ConstraintCollector firstCollector(*dtmc);
    storm::analysis::ConstraintCollector secondCollector(*dtmc);

    EXPECT_EQ(asStringsInDiscoveryOrder(firstCollector.getWellformedConstraints()), asStringsInDiscoveryOrder(secondCollector.getWellformedConstraints()));
    EXPECT_EQ(asStringsInDiscoveryOrder(firstCollector.getGraphPreservingConstraints()),
              asStringsInDiscoveryOrder(secondCollector.getGraphPreservingConstraints()));

    // The constraints are collected in the order in which the model traversal discovers them, which is not their
    // lexicographic order. Pinning the expected order keeps this test from passing vacuously.
    std::vector<std::string> expectedWellformed{"(-(p) <= 0)", "((-1 + p) <= 0)"};
    std::vector<std::string> expectedGraphPreserving{"(p != 0)", "((1 - p) != 0)"};

    EXPECT_EQ(expectedWellformed, asStringsInDiscoveryOrder(firstCollector.getWellformedConstraints()));
    EXPECT_EQ(expectedGraphPreserving, asStringsInDiscoveryOrder(firstCollector.getGraphPreservingConstraints()));

    // Guard the guard: if the discovery order ever happened to coincide with the lexicographic one, the assertions
    // above could no longer detect a collector that reorders or sorts its constraints.
    auto sortedWellformed = asSortedStrings(firstCollector.getWellformedConstraints());
    EXPECT_NE(expectedWellformed, sortedWellformed);
}

TEST_F(GraphConditionsTest, ExportToFile) {
    auto dtmc = buildParametricDtmc(STORM_TEST_RESOURCES_DIR "/pdtmc/only_rational_denominator.pm", "P=? [F s=2]");

    storm::analysis::ConstraintCollector collector(*dtmc);

    // Use one of the model's symbolic transition values as the solution function.
    storm::RationalFunction resultValue = storm::utility::zero<storm::RationalFunction>();
    for (auto const& entry : dtmc->getTransitionMatrix()) {
        if (!storm::utility::isConstant(entry.getValue())) {
            resultValue = entry.getValue();
            break;
        }
    }

    std::string temporaryFile = "graph-conditions-export.txt";
    storm::pars::api::exportParametricResultToFile<storm::RationalFunction>(resultValue, collector, temporaryFile);

    std::ifstream input(temporaryFile);
    ASSERT_TRUE(input.good());
    std::stringstream buffer;
    buffer << input.rdbuf();
    std::string content = buffer.str();
    storm::io::closeFile(input);

    // The export must mention the parameters, the result, and both kinds of constraints.
    EXPECT_NE(std::string::npos, content.find("$Parameters: p; q; \n"));
    EXPECT_NE(std::string::npos, content.find("$Result: "));
    EXPECT_NE(std::string::npos, content.find("$Well-formed Constraints: \n"));
    EXPECT_NE(std::string::npos, content.find("$Graph-preserving Constraints: \n"));

    // The export must list exactly the collected constraints. The collector returns them in the order in which it
    // discovered them, so both sides are sorted before comparing.
    std::vector<std::string> wellformedLines;
    std::string currentSection;
    std::istringstream contentStream(content);
    std::string line;
    while (std::getline(contentStream, line)) {
        if (line == "$Well-formed Constraints: ") {
            currentSection = "wellformed";
        } else if (line == "$Graph-preserving Constraints: ") {
            currentSection = "graphPreserving";
        } else if (!line.empty() && currentSection == "wellformed") {
            wellformedLines.push_back(line);
        }
    }
    std::sort(wellformedLines.begin(), wellformedLines.end());
    EXPECT_EQ(asSortedStrings(collector.getWellformedConstraints()), wellformedLines);

    std::remove(temporaryFile.c_str());
}
