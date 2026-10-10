#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/PolynomialToExpression.h"

namespace {

using Coeff = storm::RationalFunctionCoefficient;
using Variable = storm::RationalFunctionVariable;

// A single term of a polynomial: a rational coefficient given as a numerator and a denominator, multiplied by a
// product of variables, where a repeated variable stands for a power.
struct Term {
    std::int64_t numerator;
    std::int64_t denominator;
    std::vector<Variable> factors;
};

using Terms = std::vector<Term>;

// Creating a rational function variable always yields a new one, even for a name that is already in use. The tests
// need the same variable everywhere, so these are created only once.
Variable p() {
    static Variable const variable = storm::createRFVariable("p");
    return variable;
}

Variable q() {
    static Variable const variable = storm::createRFVariable("q");
    return variable;
}

// An integer as an exact rational number. The cast keeps the constructor call unambiguous.
storm::RationalNumber rational(std::int64_t value) {
    return storm::RationalNumber{static_cast<long>(value)};
}

// The coefficient of the given term as carl uses it.
Coeff coefficientOf(Term const& term) {
    return Coeff(term.numerator) / Coeff(term.denominator);
}

// Builds the polynomial described by the given terms. This constructs exactly the polynomial type that
// ConstraintCollector hands to polynomialToExpression, so no part of the value is lost on the way in.
storm::RawPolynomial buildPolynomial(Terms const& terms) {
    storm::RawPolynomial result;
    for (auto const& term : terms) {
        if (term.numerator == 0) {
            continue;
        }
        storm::RawPolynomial partial{coefficientOf(term)};
        for (auto const& factor : term.factors) {
            partial *= factor;
        }
        result += partial;
    }
    return result;
}

// Converts the given polynomial to an expression and evaluates that expression at the given values.
storm::RationalNumber evaluateConverted(storm::RawPolynomial const& polynomial, storm::RationalNumber const& pValue, storm::RationalNumber const& qValue) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto expression = storm::expressions::polynomialToExpression(polynomial, manager);

    storm::expressions::ExpressionEvaluator<storm::RationalNumber> evaluator(*manager);
    if (manager->hasVariable("p")) {
        evaluator.setRationalValue(manager->getVariable("p"), pValue);
    }
    if (manager->hasVariable("q")) {
        evaluator.setRationalValue(manager->getVariable("q"), qValue);
    }

    return evaluator.asRational(expression);
}

// Evaluates the terms described by the given list directly, without any conversion, which yields the value that the
// conversion has to reproduce.
storm::RationalNumber evaluateDirectly(Terms const& terms, storm::RationalNumber const& pValue, storm::RationalNumber const& qValue) {
    storm::RationalNumber result{0};
    for (auto const& term : terms) {
        if (term.numerator == 0) {
            continue;
        }
        storm::RationalNumber value = rational(term.numerator);
        value /= rational(term.denominator);
        for (auto const& factor : term.factors) {
            value *= factor == p() ? pValue : qValue;
        }
        result += value;
    }
    return result;
}

// The values at which the converted expressions are checked. Since the arithmetic is rational, every comparison is
// exact.
std::vector<std::pair<storm::RationalNumber, storm::RationalNumber>> const& sampleValues() {
    static std::vector<std::pair<storm::RationalNumber, storm::RationalNumber>> const values = {
        {storm::RationalNumber(0), storm::RationalNumber(0)},
        {storm::RationalNumber(1), storm::RationalNumber(1)},
        {storm::RationalNumber(-1), storm::RationalNumber(2)},
        {storm::RationalNumber(2), storm::RationalNumber(-3)},
        {storm::RationalNumber(1) / storm::RationalNumber(3), storm::RationalNumber(-5) / storm::RationalNumber(7)},
        {storm::RationalNumber(-4), storm::RationalNumber(1) / storm::RationalNumber(5)},
        {storm::RationalNumber(7) / storm::RationalNumber(2), storm::RationalNumber(-3) / storm::RationalNumber(4)},
    };
    return values;
}

// Checks that converting the polynomial described by the given terms to an expression preserves its value.
void expectValuesPreserved(Terms const& terms) {
    auto polynomial = buildPolynomial(terms);

    for (auto const& values : sampleValues()) {
        auto expected = evaluateDirectly(terms, values.first, values.second);
        auto actual = evaluateConverted(polynomial, values.first, values.second);

        EXPECT_EQ(expected, actual) << "at p = " << values.first << " and q = " << values.second;
    }
}

}  // namespace

TEST(PolynomialToExpressionTest, ConvertsPowers) {
    // Higher powers produce exponentiation nodes, which the affine models used by the graph condition tests never
    // produce.
    expectValuesPreserved({{1, 1, {p(), p()}}});
    expectValuesPreserved({{1, 1, {p(), p(), p()}}});
    expectValuesPreserved({{1, 1, {q(), q()}}});
    expectValuesPreserved({{1, 1, {p(), p(), q()}}});
    expectValuesPreserved({{2, 1, {p(), p()}}, {1, 1, {}}});
    expectValuesPreserved({{1, 1, {q(), q(), q(), q()}}});
}

TEST(PolynomialToExpressionTest, ConvertsMixedVariableProducts) {
    // A product of distinct variables must keep both factors.
    expectValuesPreserved({{1, 1, {p(), q()}}});
    expectValuesPreserved({{1, 1, {p(), q()}}, {1, 1, {p()}}, {1, 1, {q()}}});
    expectValuesPreserved({{2, 1, {p(), q()}}});
    expectValuesPreserved({{3, 1, {p(), q()}}, {2, 1, {p()}}, {1, 1, {q()}}});
    expectValuesPreserved({{1, 1, {p(), q()}}, {1, 1, {p(), p(), q()}}});
}

TEST(PolynomialToExpressionTest, ConvertsNonUnitRationalCoefficients) {
    // Coefficients other than 1 and -1 are emitted as rational literals.
    expectValuesPreserved({{1, 2, {p()}}});
    expectValuesPreserved({{1, 3, {p()}}, {5, 8, {}}});
    expectValuesPreserved({{1, 3, {p()}}, {5, 8, {p(), q()}}});
    expectValuesPreserved({{7, 2, {p(), p()}}});
    expectValuesPreserved({{-1, 2, {q()}}, {1, 4, {p()}}});
    expectValuesPreserved({{3, 4, {p()}}, {-7, 2, {q()}}});
    expectValuesPreserved({{1, 1, {p()}}, {1, 1, {q()}}, {-11, 6, {p(), q()}}});
}

TEST(PolynomialToExpressionTest, ConvertsZeroAndConstants) {
    // A polynomial with no terms at all is zero, which the conversion has to handle separately.
    EXPECT_TRUE(buildPolynomial(Terms{}).isZero());
    expectValuesPreserved(Terms{});

    // Terms that cancel out or vanish leave an empty polynomial behind as well.
    expectValuesPreserved({{1, 1, {p(), q()}}, {-1, 1, {p(), q()}}});
    expectValuesPreserved({{0, 1, {p()}}});
    expectValuesPreserved({{0, 1, {p(), p()}}, {0, 1, {q()}}});

    // A nonzero constant is a plain rational literal.
    expectValuesPreserved({{1, 1, {}}});
    expectValuesPreserved({{-1, 1, {}}});
    expectValuesPreserved({{5, 3, {}}});
    expectValuesPreserved({{5, 3, {}}, {1, 1, {p()}}, {-1, 2, {p(), q()}}});
}

TEST(PolynomialToExpressionTest, ConvertsNegativeCoefficients) {
    // Negative coefficients must keep their sign. The converter emits them as a subtraction from the accumulated
    // result, but that is not observable from the value alone, so only the sign is checked here.
    expectValuesPreserved({{1, 1, {p()}}, {-1, 1, {}}});
    expectValuesPreserved({{-1, 1, {p(), p()}}});
    expectValuesPreserved({{1, 2, {p()}}, {-3, 4, {q()}}});
    expectValuesPreserved({{-1, 1, {p(), q()}}, {1, 1, {p()}}, {1, 1, {q()}}});
    expectValuesPreserved({{-1, 1, {p(), p(), q()}}, {1, 3, {q()}}});
}

TEST(PolynomialToExpressionTest, ConvertsTermsIndependentlyOfTheirOrder) {
    // The converter accumulates terms, so a different order must yield the same value.
    expectValuesPreserved({{2, 1, {p()}}, {3, 1, {q()}}, {4, 1, {p(), q()}}});
    expectValuesPreserved({{4, 1, {p(), q()}}, {3, 1, {q()}}, {2, 1, {p()}}});
}

TEST(PolynomialToExpressionTest, DeclaresTheVariablesItNeeds) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto polynomial = buildPolynomial({{1, 1, {p(), p(), q()}}, {1, 2, {p()}}});

    // Every variable occurring in the polynomial is declared in the given manager, by name.
    EXPECT_FALSE(manager->hasVariable("p"));
    EXPECT_FALSE(manager->hasVariable("q"));

    auto expression = storm::expressions::polynomialToExpression(polynomial, manager);

    EXPECT_TRUE(manager->hasVariable("p"));
    EXPECT_TRUE(manager->hasVariable("q"));
    EXPECT_EQ(expression.getVariables().size(), 2ul);
}

TEST(PolynomialToExpressionTest, DoesNotDeclareUnusedVariables) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto polynomial = buildPolynomial({{1, 1, {p()}}});

    auto expression = storm::expressions::polynomialToExpression(polynomial, manager);

    EXPECT_TRUE(manager->hasVariable("p"));
    EXPECT_FALSE(manager->hasVariable("q"));
    EXPECT_EQ(expression.getVariables().size(), 1ul);
}

TEST(PolynomialToExpressionTest, ReusesPreviouslyDeclaredVariables) {
    // Converting into a manager that already knows the variables reuses them rather than redeclaring them.
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto declaredP = manager->declareRationalVariable("p");
    auto declaredQ = manager->declareRationalVariable("q");

    auto polynomial = buildPolynomial({{1, 1, {p(), p(), q()}}});
    auto expression = storm::expressions::polynomialToExpression(polynomial, manager);

    // The conversion resolves the variables through the manager, so these are the ones the expression refers to.
    EXPECT_TRUE(expression.containsVariable({declaredP, declaredQ}));
    EXPECT_EQ(expression.getVariables(), (std::set<storm::expressions::Variable>{declaredP, declaredQ}));
}

TEST(PolynomialToExpressionTest, ConvertsAPolynomialWithoutVariablesToAConstant) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto polynomial = buildPolynomial({{5, 3, {}}});

    auto expression = storm::expressions::polynomialToExpression(polynomial, manager);

    // Such a polynomial must not declare any variable and must convert to a constant literal.
    EXPECT_FALSE(manager->hasVariable("p"));
    EXPECT_FALSE(manager->hasVariable("q"));
    EXPECT_FALSE(expression.containsVariables());
    EXPECT_TRUE(expression.isLiteral());
}