#include "storm-config.h"
#include "test/storm_gtest.h"

#ifdef STORM_HAVE_CVC5
#include "storm/adapters/Cvc5ExpressionAdapter.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/IntegerLiteralExpression.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/storage/expressions/RationalLiteralExpression.h"
#include "storm/storage/expressions/VariableExpression.h"

namespace {
/*!
 * Checks that the given storm expression is equivalent to the given cvc5 term. For this, we assert the negation of the
 * equivalence and check that the solver considers it unsatisfiable.
 */
void assertEquivalent(storm::expressions::Expression const& expression, cvc5::Term const& term, cvc5::TermManager& termManager, cvc5::Solver& solver,
                      storm::adapters::Cvc5ExpressionAdapter& adapter) {
    // Note that the variables occurring in the term have to be the ones that the adapter uses. Two constants with the
    // same name are distinct variables for CVC5.
    cvc5::Term equivalence = termManager.mkTerm(cvc5::Kind::EQUAL, {adapter.translateExpression(expression), term});
    solver.assertFormula(termManager.mkTerm(cvc5::Kind::NOT, {equivalence}));
    ASSERT_TRUE(solver.checkSat().isUnsat()) << "Not equivalent: " << expression << " (" << adapter.translateExpression(expression) << ") vs " << term;
    solver.resetAssertions();
}
}  // namespace

TEST(Cvc5ExpressionAdapter, StormToCvc5Boolean) {
    cvc5::TermManager termManager;
    cvc5::Solver solver(termManager);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Cvc5ExpressionAdapter adapter(*manager, solver);

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");

    cvc5::Term cvc5X = adapter.translateExpression(x);
    cvc5::Term cvc5Y = adapter.translateExpression(y);

    assertEquivalent(manager->boolean(true), termManager.mkBoolean(true), termManager, solver, adapter);
    assertEquivalent(manager->boolean(false), termManager.mkBoolean(false), termManager, solver, adapter);
    assertEquivalent(x && y, termManager.mkTerm(cvc5::Kind::AND, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(x || y, termManager.mkTerm(cvc5::Kind::OR, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(!(x && y), termManager.mkTerm(cvc5::Kind::NOT, {termManager.mkTerm(cvc5::Kind::AND, {cvc5X, cvc5Y})}), termManager, solver, adapter);
    assertEquivalent(storm::expressions::iff(x, y), termManager.mkTerm(cvc5::Kind::EQUAL, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(storm::expressions::implies(x, y), termManager.mkTerm(cvc5::Kind::IMPLIES, {cvc5X, cvc5Y}), termManager, solver, adapter);
}

TEST(Cvc5ExpressionAdapter, StormToCvc5Integer) {
    cvc5::TermManager termManager;
    cvc5::Solver solver(termManager);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Cvc5ExpressionAdapter adapter(*manager, solver);

    storm::expressions::Variable x = manager->declareIntegerVariable("x");
    storm::expressions::Variable y = manager->declareIntegerVariable("y");

    cvc5::Term cvc5X = adapter.translateExpression(x);
    cvc5::Term cvc5Y = adapter.translateExpression(y);

    assertEquivalent(manager->integer(0), termManager.mkInteger(0), termManager, solver, adapter);
    assertEquivalent(manager->integer(-13), termManager.mkInteger(-13), termManager, solver, adapter);
    assertEquivalent(x + y, termManager.mkTerm(cvc5::Kind::ADD, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(x - y, termManager.mkTerm(cvc5::Kind::SUB, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(-x, termManager.mkTerm(cvc5::Kind::NEG, {cvc5X}), termManager, solver, adapter);
    assertEquivalent(x * y, termManager.mkTerm(cvc5::Kind::MULT, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(x + y < -y,
                     termManager.mkTerm(cvc5::Kind::LT, {termManager.mkTerm(cvc5::Kind::ADD, {cvc5X, cvc5Y}), termManager.mkTerm(cvc5::Kind::NEG, {cvc5Y})}),
                     termManager, solver, adapter);
    assertEquivalent(storm::expressions::pow(x, manager->integer(2), true), termManager.mkTerm(cvc5::Kind::POW, {cvc5X, termManager.mkInteger(2)}), termManager,
                     solver, adapter);
}

TEST(Cvc5ExpressionAdapter, StormToCvc5Real) {
    cvc5::TermManager termManager;
    cvc5::Solver solver(termManager);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Cvc5ExpressionAdapter adapter(*manager, solver);

    storm::expressions::Variable x = manager->declareRationalVariable("x");
    storm::expressions::Variable y = manager->declareRationalVariable("y");

    cvc5::Term cvc5X = adapter.translateExpression(x);
    cvc5::Term cvc5Y = adapter.translateExpression(y);

    assertEquivalent(manager->rational(storm::RationalNumber(1, 2)), termManager.mkReal(1, 2), termManager, solver, adapter);
    assertEquivalent(manager->rational(storm::RationalNumber(-3, 4)), termManager.mkReal(-3, 4), termManager, solver, adapter);
    assertEquivalent(x + y, termManager.mkTerm(cvc5::Kind::ADD, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(x - y, termManager.mkTerm(cvc5::Kind::SUB, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(-x, termManager.mkTerm(cvc5::Kind::NEG, {cvc5X}), termManager, solver, adapter);
    assertEquivalent(x * y, termManager.mkTerm(cvc5::Kind::MULT, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(x / y, termManager.mkTerm(cvc5::Kind::DIVISION, {cvc5X, cvc5Y}), termManager, solver, adapter);
    assertEquivalent(storm::expressions::Expression(x) < storm::expressions::Expression(y), termManager.mkTerm(cvc5::Kind::LT, {cvc5X, cvc5Y}), termManager,
                     solver, adapter);
}

TEST(Cvc5ExpressionAdapter, StormToCvc5MixedNumericalTypes) {
    cvc5::TermManager termManager;
    cvc5::Solver solver(termManager);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Cvc5ExpressionAdapter adapter(*manager, solver);

    storm::expressions::Variable intVar = manager->declareIntegerVariable("intVar");
    storm::expressions::Variable realVar = manager->declareRationalVariable("realVar");

    cvc5::Term cvc5IntVar = adapter.translateExpression(intVar);
    cvc5::Term cvc5RealVar = adapter.translateExpression(realVar);
    cvc5::Term cvc5ToReal = termManager.mkTerm(cvc5::Kind::TO_REAL, {cvc5IntVar});

    // Dividing an integer by a rational requires the integer to be converted into a rational.
    assertEquivalent(intVar / realVar, termManager.mkTerm(cvc5::Kind::DIVISION, {cvc5ToReal, cvc5RealVar}), termManager, solver, adapter);

    // Dividing two integers yields a rational number, so both operands have to be converted.
    storm::expressions::Variable intVar2 = manager->declareIntegerVariable("intVar2");
    cvc5::Term cvc5IntVar2 = adapter.translateExpression(intVar2);
    assertEquivalent(intVar / intVar2, termManager.mkTerm(cvc5::Kind::DIVISION, {cvc5ToReal, termManager.mkTerm(cvc5::Kind::TO_REAL, {cvc5IntVar2})}),
                     termManager, solver, adapter);
}

TEST(Cvc5ExpressionAdapter, StormToCvc5FloorAndCeil) {
    cvc5::TermManager termManager;
    cvc5::Solver solver(termManager);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Cvc5ExpressionAdapter adapter(*manager, solver);

    storm::expressions::Variable x = manager->declareRationalVariable("x");
    cvc5::Term cvc5X = adapter.translateExpression(x);

    cvc5::Term cvc5Floor = adapter.translateExpression(storm::expressions::floor(x));
    cvc5::Term cvc5Ceil = adapter.translateExpression(storm::expressions::ceil(x));

    // The floor and the ceiling of a real number are integers, which we check by looking at the model.
    solver.assertFormula(termManager.mkTerm(cvc5::Kind::EQUAL, {cvc5X, termManager.mkReal(7, 3)}));
    solver.assertFormula(termManager.mkTerm(cvc5::Kind::EQUAL, {cvc5Floor, termManager.mkInteger(2)}));
    solver.assertFormula(termManager.mkTerm(cvc5::Kind::EQUAL, {cvc5Ceil, termManager.mkInteger(3)}));
    ASSERT_TRUE(solver.checkSat().isSat());

    // Storm's floor is CVC5's to_int, whereas the ceiling is the negation of the floor of the negation.
    assertEquivalent(storm::expressions::floor(x), termManager.mkTerm(cvc5::Kind::TO_INTEGER, {cvc5X}), termManager, solver, adapter);
    assertEquivalent(storm::expressions::ceil(x),
                     termManager.mkTerm(cvc5::Kind::NEG, {termManager.mkTerm(cvc5::Kind::TO_INTEGER, {termManager.mkTerm(cvc5::Kind::NEG, {cvc5X})})}),
                     termManager, solver, adapter);
}

TEST(Cvc5ExpressionAdapter, StormToCvc5IfThenElse) {
    cvc5::TermManager termManager;
    cvc5::Solver solver(termManager);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Cvc5ExpressionAdapter adapter(*manager, solver);

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareIntegerVariable("y");
    storm::expressions::Variable z = manager->declareIntegerVariable("z");

    cvc5::Term cvc5X = adapter.translateExpression(x);
    cvc5::Term cvc5Y = adapter.translateExpression(y);
    cvc5::Term cvc5Z = adapter.translateExpression(z);

    assertEquivalent(storm::expressions::ite(x, y, z), termManager.mkTerm(cvc5::Kind::ITE, {cvc5X, cvc5Y, cvc5Z}), termManager, solver, adapter);
}

TEST(Cvc5ExpressionAdapter, Cvc5ToStorm) {
    cvc5::TermManager termManager;
    cvc5::Solver solver(termManager);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Cvc5ExpressionAdapter adapter(*manager, solver);

    // Boolean constants can be translated back to storm.
    storm::expressions::Expression translatedTrue = adapter.translateExpression(termManager.mkBoolean(true));
    ASSERT_TRUE(translatedTrue.isTrue());
    ASSERT_TRUE(adapter.translateExpression(termManager.mkBoolean(false)).isFalse());

    // CVC5 prints negative integers in the SMT-LIB notation, i.e., as "(- 42)".
    auto const assertIntegerLiteral = [&adapter, &termManager](int_fast64_t value) {
        storm::expressions::Expression translated = adapter.translateExpression(termManager.mkInteger(value));
        auto const* integerLiteral = dynamic_cast<storm::expressions::IntegerLiteralExpression const*>(&translated.getBaseExpression());
        EXPECT_TRUE(integerLiteral != nullptr);
        if (integerLiteral != nullptr) {
            EXPECT_EQ(value, integerLiteral->getValue());
        }
    };
    assertIntegerLiteral(-42);
    assertIntegerLiteral(0);
    assertIntegerLiteral(42);

    // CVC5 prints non-integral rationals in the SMT-LIB notation, e.g., 1/2 is printed as "(/ 1 2)" and -3/4 as
    // "(/ (- 3) 4)", whereas integral rationals are printed in decimal notation.
    auto const assertRationalLiteral = [&adapter, &termManager](int numerator, int denominator) {
        storm::expressions::Expression translated = adapter.translateExpression(termManager.mkReal(numerator, denominator));
        auto const* rationalLiteral = dynamic_cast<storm::expressions::RationalLiteralExpression const*>(&translated.getBaseExpression());
        EXPECT_TRUE(rationalLiteral != nullptr);
        if (rationalLiteral != nullptr) {
            EXPECT_EQ(storm::RationalNumber(numerator, denominator), rationalLiteral->getValue());
        }
    };
    assertRationalLiteral(1, 2);
    assertRationalLiteral(-3, 4);
    assertRationalLiteral(7, 3);
    assertRationalLiteral(2, 1);

    // Free constants and the usual operators can be translated back to storm.
    storm::expressions::Variable xVar = manager->declareIntegerVariable("x");
    storm::expressions::Variable yVar = manager->declareIntegerVariable("y");
    cvc5::Term cvc5X = adapter.translateExpression(xVar);
    cvc5::Term cvc5Y = adapter.translateExpression(yVar);
    cvc5::Term cvc5Sum = termManager.mkTerm(cvc5::Kind::ADD, {cvc5X, cvc5Y});
    cvc5::Term cvc5Less = termManager.mkTerm(cvc5::Kind::LT, {cvc5Sum, termManager.mkInteger(0)});

    storm::expressions::Expression translatedSum = adapter.translateExpression(cvc5Sum);
    storm::expressions::Expression translatedLess = adapter.translateExpression(cvc5Less);

    ASSERT_EQ(storm::expressions::OperatorType::Plus, translatedSum.getOperator());
    ASSERT_TRUE(translatedSum.getOperand(0).isVariable());
    ASSERT_EQ("x", translatedSum.getOperand(0).getIdentifier());
    ASSERT_TRUE(translatedSum.getOperand(1).isVariable());
    ASSERT_EQ("y", translatedSum.getOperand(1).getIdentifier());

    ASSERT_EQ(storm::expressions::OperatorType::Less, translatedLess.getOperator());
    ASSERT_EQ(storm::expressions::OperatorType::Plus, translatedLess.getOperand(0).getOperator());
    auto const* zeroLiteral = dynamic_cast<storm::expressions::IntegerLiteralExpression const*>(&translatedLess.getOperand(1).getBaseExpression());
    ASSERT_TRUE(zeroLiteral != nullptr);
    ASSERT_EQ(0, zeroLiteral->getValue());
}
#endif
