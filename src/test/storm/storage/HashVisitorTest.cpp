#include "test/storm_gtest.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/storage/expressions/HashVisitor.h"

namespace {

class HashVisitorTest : public ::testing::Test {
   public:
    std::size_t hash(storm::expressions::Expression const& expression) {
        storm::expressions::HashVisitor visitor;
        return visitor.hash(expression);
    }
};

}  // namespace

TEST_F(HashVisitorTest, SyntacticallyEqualExpressionsHashEqually) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = manager->declareBooleanVariable("x");
    auto p = manager->declareRationalVariable("p");
    auto q = manager->declareRationalVariable("q");

    // Each of these pairs is syntactically equal, so the hashes must agree.
    EXPECT_EQ(hash(x && x), hash(x && x));
    EXPECT_EQ(hash((x && !x) || x), hash((x && !x) || x));
    EXPECT_EQ(hash(p <= q), hash(p <= q));
    EXPECT_EQ(hash(manager->integer(3) + manager->integer(4)), hash(manager->integer(3) + manager->integer(4)));
    EXPECT_EQ(hash(manager->rational(3.5)), hash(manager->rational(3.5)));
    EXPECT_EQ(hash(manager->boolean(true)), hash(manager->boolean(true)));

    // Syntactic equality is insensitive to expression identity: two separately built but
    // structurally identical expressions must still hash equally.
    auto left = (x && x);
    auto right = (x && x);
    ASSERT_TRUE(left.isSyntacticallyEqual(right));
    EXPECT_EQ(hash(left), hash(right));
}

TEST_F(HashVisitorTest, DifferentExpressionsHashDifferently) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = manager->declareBooleanVariable("x");
    auto y = manager->declareBooleanVariable("y");
    auto p = manager->declareRationalVariable("p");
    auto q = manager->declareRationalVariable("q");

    // Distinct variables.
    EXPECT_NE(hash(x), hash(y));

    // Distinct operators over the same operands.
    EXPECT_NE(hash(x && x), hash(x || x));

    // Same operator, different operand.
    EXPECT_NE(hash(x && x), hash(x && y));

    // Different literals of the same type.
    EXPECT_NE(hash(manager->integer(3)), hash(manager->integer(4)));
    EXPECT_NE(hash(manager->boolean(true)), hash(manager->boolean(false)));
    EXPECT_NE(hash(manager->rational(3.5)), hash(manager->rational(4.5)));

    // Different types that could otherwise coincide: an integer and a rational literal, and a
    // variable against a literal.
    EXPECT_NE(hash(manager->integer(2)), hash(manager->rational(2.0)));
    EXPECT_NE(hash(x), hash(manager->boolean(true)));

    // Same shape but a different relation type.
    EXPECT_NE(hash(p <= q), hash(p >= q));

    // The two arguments of a relation are not interchangeable, so the order matters.
    EXPECT_NE(hash(p <= q), hash(q <= p));
}

TEST_F(HashVisitorTest, NestedExpressionsPropagateChangesFromChildren) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = manager->declareBooleanVariable("x");
    auto y = manager->declareBooleanVariable("y");

    // A change deep inside must change the hash of the enclosing expression; otherwise a container
    // keyed on the hash would wrongly report two different expressions as equal.
    EXPECT_NE(hash((x && x) || (x && x)), hash((x && y) || (x && y)));
}

TEST_F(HashVisitorTest, PredicateExpressionsAreHashed) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = manager->declareBooleanVariable("x");
    auto y = manager->declareBooleanVariable("y");
    auto p = manager->declareRationalVariable("p");

    // Syntactically equal predicates must hash equally, also across distinct nodes.
    EXPECT_EQ(hash(storm::expressions::atLeastOneOf({x, y})), hash(storm::expressions::atLeastOneOf({x, y})));
    EXPECT_EQ(hash(storm::expressions::exactlyOneOf({x, y})), hash(storm::expressions::exactlyOneOf({x, y})));

    // The predicate type is part of the hash.
    EXPECT_NE(hash(storm::expressions::atLeastOneOf({x, y})), hash(storm::expressions::atMostOneOf({x, y})));
    EXPECT_NE(hash(storm::expressions::atLeastOneOf({x, y})), hash(storm::expressions::exactlyOneOf({x, y})));

    // The operands are part of the hash, and their order matters.
    EXPECT_NE(hash(storm::expressions::atLeastOneOf({x, y})), hash(storm::expressions::atLeastOneOf({y, x})));
    EXPECT_NE(hash(storm::expressions::atLeastOneOf({x, y})), hash(storm::expressions::atLeastOneOf({x, p})));

    // The arity is part of the hash.
    EXPECT_NE(hash(storm::expressions::atLeastOneOf({x})), hash(storm::expressions::atLeastOneOf({x, y})));

    // A predicate must not collide with a plain variable or literal of the same operands.
    EXPECT_NE(hash(storm::expressions::atLeastOneOf({x, y})), hash(x));
}

TEST_F(HashVisitorTest, PredicateExpressionsCompareSyntactically) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = manager->declareBooleanVariable("x");
    auto y = manager->declareBooleanVariable("y");

    // SyntacticalEqualityCheckVisitor must support predicates as well; otherwise the hash above is
    // unusable, because the two must agree.
    EXPECT_TRUE(storm::expressions::atLeastOneOf({x, y}).isSyntacticallyEqual(storm::expressions::atLeastOneOf({x, y})));
    EXPECT_FALSE(storm::expressions::atLeastOneOf({x, y}).isSyntacticallyEqual(storm::expressions::atMostOneOf({x, y})));
    EXPECT_FALSE(storm::expressions::atLeastOneOf({x, y}).isSyntacticallyEqual(storm::expressions::atLeastOneOf({y, x})));
    EXPECT_FALSE(storm::expressions::atLeastOneOf({x, y}).isSyntacticallyEqual(storm::expressions::atLeastOneOf({x})));
    EXPECT_FALSE(storm::expressions::atLeastOneOf({x, y}).isSyntacticallyEqual(x));

    // Predicates nested inside other expressions work too.
    EXPECT_TRUE((x && storm::expressions::atLeastOneOf({x, y})).isSyntacticallyEqual(x && storm::expressions::atLeastOneOf({x, y})));
}

TEST_F(HashVisitorTest, HashIsStableAcrossCalls) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto p = manager->declareRationalVariable("p");

    // The hash must not depend on the visitor's state, so that it can be used to size or partition
    // containers across separate traversals.
    auto expression = (p <= manager->integer(1));
    auto const first = hash(expression);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(first, hash(expression));
    }

    // A fresh visitor must agree with a reused one.
    storm::expressions::HashVisitor fresh;
    EXPECT_EQ(first, fresh.hash(expression));
}
