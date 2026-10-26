#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <string>

#include "storm-pars/analysis/ConstraintCollector.h"
#include "storm-pars/analysis/RegionConstraintChecker.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/properties.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/generator/NextStateGenerator.h"
#include "storm/languages/jani/Property.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/utility/constants.h"

namespace {

using Checker = storm::pars::analysis::RegionConstraintChecker;
using ConstraintSet = storm::pars::analysis::ConstraintCollector::ConstraintSet;
using Region = storm::pars::analysis::RationalFunctionRegion;
using CoefficientType = storm::pars::analysis::CoefficientType;

template<typename T>
CoefficientType coefficient(T const& value) {
    return storm::utility::convertNumber<CoefficientType>(value);
}

double asDouble(CoefficientType const& value) {
    return storm::utility::convertNumber<double>(value);
}

/*!
 * Retrieves the parametric variable with the given name, creating it if it does not exist yet.
 */
storm::RationalFunctionVariable variableByName(std::string const& name) {
    carl::VariablePool& pool = carl::VariablePool::getInstance();
    auto variable = pool.findVariableWithName(name);
    if (variable == storm::RationalFunctionVariable()) {
        variable = pool.getFreshPersistentVariable(name);
    }
    return variable;
}

/*!
 * Builds a rectangular region with the given lower and upper bounds.
 */
Region buildRegion(std::vector<std::pair<std::string, std::pair<double, double>>> const& bounds) {
    Region::Valuation lowerBoundaries;
    Region::Valuation upperBoundaries;
    for (auto const& [variable, boundsOfVariable] : bounds) {
        lowerBoundaries[variableByName(variable)] = coefficient(boundsOfVariable.first);
        upperBoundaries[variableByName(variable)] = coefficient(boundsOfVariable.second);
    }
    return Region(lowerBoundaries, upperBoundaries);
}

/*!
 * Retrieves a numerical variable expression, declaring the variable if necessary.
 */
storm::expressions::Expression variable(std::shared_ptr<storm::expressions::ExpressionManager> const& manager, std::string const& name) {
    manager->declareRationalVariable(name);
    return manager->getVariableExpression(name);
}

ConstraintSet buildConstraintSet(storm::expressions::Expression const& constraint) {
    return ConstraintSet{constraint};
}

}  // namespace

TEST(RegionConstraintCheckerTest, AffineConstraintHoldsThroughout) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    auto y = variable(manager, "y");

    // x + y - 3 <= 0 holds on the unit square, where the maximum of x + y is 2.
    auto result = Checker::checkConstraints(buildConstraintSet(x + y - manager->integer(3) <= manager->integer(0)),
                                            buildRegion({{"x", {0.0, 1.0}}, {"y", {0.0, 1.0}}}), "test");

    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat, result.verdict);
    EXPECT_FALSE(result.violatedConstraint.has_value());
    EXPECT_TRUE(result.undecidedConstraints.empty());
    EXPECT_FALSE(result.witness.has_value());
    EXPECT_TRUE(result.holdsThroughout());
}

TEST(RegionConstraintCheckerTest, AffineConstraintViolatedAtCorner) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    auto y = variable(manager, "y");

    // x + y - 1 <= 0 does not hold throughout the unit square; the corner (1, 1) is a witness.
    auto result = Checker::checkConstraints(buildConstraintSet(x + y - manager->integer(1) <= manager->integer(0)),
                                            buildRegion({{"x", {0.0, 1.0}}, {"y", {0.0, 1.0}}}), "test");

    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, result.verdict);
    EXPECT_FALSE(result.holdsThroughout());
    EXPECT_TRUE(result.violatedConstraint.has_value());
    ASSERT_TRUE(result.witness.has_value());
    EXPECT_NEAR(1.0, asDouble(result.witness->at(variableByName("x"))), 1e-9);
    EXPECT_NEAR(1.0, asDouble(result.witness->at(variableByName("y"))), 1e-9);
}

TEST(RegionConstraintCheckerTest, StrictInequalityOnBoundaryIsViolated) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    Region region = buildRegion({{"x", {0.0, 1.0}}});

    // x < 1 does not hold throughout [0, 1], because the maximum 1 is attained at the closed boundary.
    auto violated = Checker::checkConstraints(buildConstraintSet(x < manager->integer(1)), region, "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, violated.verdict);
    ASSERT_TRUE(violated.witness.has_value());
    EXPECT_NEAR(1.0, asDouble(violated.witness->at(variableByName("x"))), 1e-9);

    // In contrast, x <= 1 does hold throughout.
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat, Checker::checkConstraints(buildConstraintSet(x <= manager->integer(1)), region, "test").verdict);

    // x < 2 holds throughout.
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat, Checker::checkConstraints(buildConstraintSet(x < manager->integer(2)), region, "test").verdict);
}

TEST(RegionConstraintCheckerTest, NegativeCoefficientIsHandled) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    Region region = buildRegion({{"x", {0.0, 1.0}}});

    // 3 - 2 * x >= 0 holds on [0, 1], as the minimum of 3 - 2x is attained at x = 1 and equals 1.
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat,
              Checker::checkConstraints(buildConstraintSet(manager->integer(3) - manager->integer(2) * x >= manager->integer(0)), region, "test").verdict);

    // In contrast, 1 - 2 * x >= 0 does not hold on [0, 1], because the form is -1 at x = 1.
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat,
              Checker::checkConstraints(buildConstraintSet(manager->integer(1) - manager->integer(2) * x >= manager->integer(0)), region, "test").verdict);

    // 1 - x >= 0 does not hold on [0, 2].
    auto violated = Checker::checkConstraints(buildConstraintSet(manager->integer(1) - x >= manager->integer(0)), buildRegion({{"x", {0.0, 2.0}}}), "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, violated.verdict);
    ASSERT_TRUE(violated.witness.has_value());
    EXPECT_NEAR(2.0, asDouble(violated.witness->at(variableByName("x"))), 1e-9);
}

TEST(RegionConstraintCheckerTest, NotEqualOverPositiveRegion) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");

    // x != 0 holds throughout a region that does not contain zero.
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat,
              Checker::checkConstraints(buildConstraintSet(x != manager->integer(0)), buildRegion({{"x", {1.0, 2.0}}}), "test").verdict);

    // x != 0 does not hold on a region containing zero; the witness attains exactly zero.
    auto violated = Checker::checkConstraints(buildConstraintSet(x != manager->integer(0)), buildRegion({{"x", {-1.0, 1.0}}}), "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, violated.verdict);
    ASSERT_TRUE(violated.witness.has_value());
    EXPECT_NEAR(0.0, asDouble(violated.witness->at(variableByName("x"))), 1e-9);
}

TEST(RegionConstraintCheckerTest, EqualityRequiresIdenticallyZero) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    Region region = buildRegion({{"x", {0.0, 1.0}}});

    // x == 0 does not hold throughout, although it holds at x = 0.
    auto violated = Checker::checkConstraints(buildConstraintSet(x == manager->integer(0)), region, "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, violated.verdict);
    ASSERT_TRUE(violated.witness.has_value());
    EXPECT_NEAR(1.0, asDouble(violated.witness->at(variableByName("x"))), 1e-9);

    // The identically vanishing form holds everywhere.
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat,
              Checker::checkConstraints(buildConstraintSet(x - x == manager->integer(0)), region, "test").verdict);
}

TEST(RegionConstraintCheckerTest, BooleanLiteralConstraintsAreDecided) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    Region region = buildRegion({{"x", {0.0, 1.0}}});

    // A constantly false constraint is violated at every point of the region and carries a witness.
    auto violated = Checker::checkConstraints(buildConstraintSet(manager->boolean(false)), region, "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, violated.verdict);
    EXPECT_TRUE(violated.violatedConstraint.has_value());
    ASSERT_TRUE(violated.witness.has_value());
    EXPECT_TRUE(violated.witness->count(variableByName("x")) > 0);

    // A constantly true constraint holds throughout.
    auto satisfied = Checker::checkConstraints(buildConstraintSet(manager->boolean(true)), region, "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat, satisfied.verdict);
    EXPECT_FALSE(satisfied.violatedConstraint.has_value());
}

TEST(RegionConstraintCheckerTest, CancellingCoefficientDoesNotRequireVariableInRegion) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");

    // x - x <= 0 is constantly zero: x cancels, so the region need not mention x.
    auto result = Checker::checkConstraints(buildConstraintSet(x - x <= manager->integer(0)), buildRegion({{"y", {0.0, 1.0}}}), "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat, result.verdict);
}

TEST(RegionConstraintCheckerTest, NonLinearConstraintIsUndecided) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    auto y = variable(manager, "y");
    Region region = buildRegion({{"x", {0.0, 1.0}}, {"y", {0.0, 1.0}}});

    // Non-linear constraints are not supported, even if they are violated (here at the corner (1, 1)).
    auto result = Checker::checkConstraints(buildConstraintSet(x * y <= manager->rational(0.25)), region, "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unknown, result.verdict);
    EXPECT_FALSE(result.undecidedConstraints.empty());
    EXPECT_FALSE(result.holdsThroughout());
}

TEST(RegionConstraintCheckerTest, IfThenElseIsUndecided) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    Region region = buildRegion({{"x", {0.0, 1.0}}});

    auto condition = x <= manager->rational(0.5);
    auto constraint = storm::expressions::ite(condition, x, manager->integer(1) - x) <= manager->integer(1);

    // An if-then-else constraint is piecewise and therefore not affine; we never claim that it holds.
    auto result = Checker::checkConstraints(buildConstraintSet(constraint), region, "test");
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unknown, result.verdict);
}

TEST(RegionConstraintCheckerTest, MissingVariableIsRejected) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = variable(manager, "x");
    auto z = variable(manager, "z");

    // The constraint constrains z, but the region does not contain z, so no decision is possible.
    EXPECT_THROW(Checker::checkConstraints(buildConstraintSet(x + z <= manager->integer(1)), buildRegion({{"x", {0.0, 1.0}}}), "test"),
                 storm::exceptions::InvalidArgumentException);
}

TEST(RegionConstraintCheckerTest, RealModelConstraintsAreDecidedExactly) {
#ifndef STORM_HAVE_SMT_SOLVER
    GTEST_SKIP() << "No SMT solver available.";
#endif
    storm::clearRFVariablePool();

    storm::prism::Program program = storm::api::parseProgram(STORM_TEST_RESOURCES_DIR "/pdtmc/parametric_die_2.pm");
    program.checkValidity();
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram("P=? [F s=7]", program));
    EXPECT_EQ(1ul, formulas.size());
    storm::generator::NextStateGeneratorOptions options(*formulas.front());
    auto model =
        storm::builder::ExplicitModelBuilder<storm::RationalFunction>(program, options).build()->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    storm::pars::analysis::ConstraintCollector collector(*model);
    EXPECT_FALSE(collector.getWellformedConstraints().empty());
    EXPECT_FALSE(collector.getGraphPreservingConstraints().empty());
    Checker checker(collector);

    // The collected well-formedness constraints are p <= 1, q <= 1, p >= 0 and q >= 0, so the closed unit
    // square satisfies all of them. All constraints are linear, hence they are decided rather than undecided.
    auto unitSquare = buildRegion({{"p", {0.0, 1.0}}, {"q", {0.0, 1.0}}});
    auto wellformed = checker.checkWellformedness(unitSquare);
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Sat, wellformed.verdict);
    EXPECT_TRUE(wellformed.undecidedConstraints.empty());
    EXPECT_TRUE(wellformed.holdsThroughout());

    // The graph-preserving constraints are p != 0, p != 1, q != 0 and q != 1. Because the region is closed and
    // therefore contains its boundary, no rectangular region containing 0 or 1 can satisfy all of them. The
    // checker reports the violation and provides a witness, rather than pretending the model is graph
    // preserving.
    auto graphPreserving = checker.checkGraphPreservation(unitSquare);
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, graphPreserving.verdict);
    EXPECT_FALSE(graphPreserving.holdsThroughout());
    EXPECT_TRUE(graphPreserving.violatedConstraint.has_value());
    EXPECT_TRUE(graphPreserving.witness.has_value());

    // On a strictly interior region, both properties hold.
    auto interior = buildRegion({{"p", {0.1, 0.9}}, {"q", {0.1, 0.9}}});
    EXPECT_TRUE(checker.isWellDefined(interior));
    EXPECT_TRUE(checker.isGraphPreserving(interior));

    // A region outside [0, 1] satisfies p != 0, p != 1, q != 0 and q != 1, but is not well-formed. Hence it is not graph-preserving.
    auto outside = buildRegion({{"p", {2.0, 3.0}}, {"q", {2.0, 3.0}}});
    EXPECT_FALSE(checker.isWellDefined(outside));
    EXPECT_TRUE(Checker::checkConstraints(collector.getGraphPreservingConstraints(), outside, "test").holdsThroughout());
    auto outsideResult = checker.checkGraphPreservation(outside);
    EXPECT_EQ(storm::pars::analysis::ConstraintVerdict::Unsat, outsideResult.verdict);
    EXPECT_FALSE(checker.isGraphPreserving(outside));
    EXPECT_TRUE(outsideResult.violatedConstraint.has_value());
    EXPECT_TRUE(outsideResult.witness.has_value());
}

TEST(RegionConstraintCheckerTest, NonAffineModelConstraintsAreNeverClaimedToHold) {
#ifndef STORM_HAVE_SMT_SOLVER
    GTEST_SKIP() << "No SMT solver available.";
#endif
    storm::clearRFVariablePool();

    storm::prism::Program program = storm::api::parseProgram(STORM_TEST_RESOURCES_DIR "/pdtmc/only_rational_denominator.pm");
    program.checkValidity();
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram("P=? [F s=2]", program));
    if (formulas.size() != 1ul) {
        GTEST_SKIP() << "Could not extract a formula from the model.";
    }
    storm::generator::NextStateGeneratorOptions options(*formulas.front());
    auto model =
        storm::builder::ExplicitModelBuilder<storm::RationalFunction>(program, options).build()->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    storm::pars::analysis::ConstraintCollector collector(*model);
    Checker checker(collector);

    // This model yields piecewise, non-affine constraints. The checker must not claim that they hold.
    auto result = checker.checkWellformedness(buildRegion({{"p", {0.0, 1.0}}, {"q", {0.0, 1.0}}}));
    EXPECT_FALSE(result.undecidedConstraints.empty());
    EXPECT_FALSE(result.holdsThroughout());
}
