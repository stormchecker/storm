#include "storm-config.h"
#include "test/storm_gtest.h"

#ifdef STORM_HAVE_MATHSAT
#include <mathsat.h>
#include "storm/adapters/MathsatExpressionAdapter.h"
#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/expressions/Expressions.h"
#include "storm/expressions/OperatorType.h"
#include "storm/settings/SettingsManager.h"

TEST(MathsatExpressionAdapter, StormToMathsatBasic) {
    msat_config config = msat_create_config();
    msat_env env = msat_create_env(config);
    ASSERT_FALSE(MSAT_ERROR_ENV(env));
    msat_destroy_config(config);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::MathsatExpressionAdapter adapter(*manager, env);

    storm::expressions::Expression exprTrue = manager->boolean(true);
    msat_term msatTrue = msat_make_true(env);
    msat_term conjecture;
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, msatTrue, adapter.translateExpression(exprTrue))));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    storm::expressions::Expression exprFalse = manager->boolean(false);
    msat_term msatFalse = msat_make_false(env);
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprFalse), msatFalse)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");
    storm::expressions::Expression exprConjunction = x && y;
    msat_term msatConjunction = msat_make_and(env, msat_make_constant(env, msat_declare_function(env, "x", msat_get_bool_type(env))),
                                              msat_make_constant(env, msat_declare_function(env, "y", msat_get_bool_type(env))));

    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprConjunction), msatConjunction)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    storm::expressions::Expression exprNor = !(x || y);
    msat_term msatNor = msat_make_not(env, msat_make_or(env, msat_make_constant(env, msat_declare_function(env, "x", msat_get_bool_type(env))),
                                                        msat_make_constant(env, msat_declare_function(env, "y", msat_get_bool_type(env)))));
    ASSERT_NO_THROW(adapter.translateExpression(exprNor));  // Variables already created in adapter.
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprNor), msatNor)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);
}

TEST(MathsatExpressionAdapter, StormToMathsatInteger) {
    msat_config config = msat_create_config();
    msat_env env = msat_create_env(config);
    ASSERT_FALSE(MSAT_ERROR_ENV(env));
    msat_destroy_config(config);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::MathsatExpressionAdapter adapter(*manager, env);

    storm::expressions::Variable x = manager->declareIntegerVariable("x");
    storm::expressions::Variable y = manager->declareIntegerVariable("y");

    storm::expressions::Expression exprAdd = x + y < -y;
    msat_decl xDecl = msat_declare_function(env, "x", msat_get_integer_type(env));
    msat_term xVar = msat_make_constant(env, xDecl);
    msat_decl yDecl = msat_declare_function(env, "y", msat_get_integer_type(env));
    msat_term yVar = msat_make_constant(env, yDecl);
    msat_term minusY = msat_make_times(env, msat_make_number(env, "-1"), yVar);
    msat_term msatAdd = msat_make_plus(env, xVar, yVar);
    msat_term msatLess = msat_make_and(env, msat_make_leq(env, msatAdd, minusY), msat_make_not(env, msat_make_equal(env, msatAdd, minusY)));
    msat_term conjecture;
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprAdd), msatLess)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    storm::expressions::Expression exprMult = !(x * y == y);
    msat_term msatTimes = msat_make_times(env, xVar, yVar);
    msat_term msatNotEqual = msat_make_not(env, msat_make_equal(env, msatTimes, yVar));
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprMult), msatNotEqual)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);
}

TEST(MathsatExpressionAdapter, StormToMathsatReal) {
    msat_config config = msat_create_config();
    msat_env env = msat_create_env(config);
    ASSERT_FALSE(MSAT_ERROR_ENV(env));
    msat_destroy_config(config);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::MathsatExpressionAdapter adapter(*manager, env);

    storm::expressions::Variable x = manager->declareRationalVariable("x");
    storm::expressions::Variable y = manager->declareRationalVariable("y");

    storm::expressions::Expression exprAdd = x + y < -y;
    msat_decl xDecl = msat_declare_function(env, "x", msat_get_rational_type(env));
    msat_term xVar = msat_make_constant(env, xDecl);
    msat_decl yDecl = msat_declare_function(env, "y", msat_get_rational_type(env));
    msat_term yVar = msat_make_constant(env, yDecl);
    msat_term minusY = msat_make_times(env, msat_make_number(env, "-1"), yVar);
    msat_term msatAdd = msat_make_plus(env, xVar, yVar);
    msat_term msatLess = msat_make_and(env, msat_make_leq(env, msatAdd, minusY), msat_make_not(env, msat_make_equal(env, msatAdd, minusY)));
    msat_term conjecture;
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprAdd), msatLess)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    storm::expressions::Expression exprMult = !(x * y == y);
    msat_term msatTimes = msat_make_times(env, xVar, yVar);
    msat_term msatNotEqual = msat_make_not(env, msat_make_equal(env, msatTimes, yVar));
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprMult), msatNotEqual)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);
}

TEST(MathsatExpressionAdapter, StormToMathsatPower) {
    msat_config config = msat_create_config();
    msat_env env = msat_create_env(config);
    ASSERT_FALSE(MSAT_ERROR_ENV(env));
    msat_destroy_config(config);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::MathsatExpressionAdapter adapter(*manager, env);

    storm::expressions::Variable x = manager->declareRationalVariable("x");
    msat_term xVar = msat_make_constant(env, msat_declare_function(env, "x", msat_get_rational_type(env)));
    msat_term msatSquare = msat_make_times(env, xVar, xVar);
    msat_term conjecture;

    // Integer exponent.
    storm::expressions::Expression exprIntPow = storm::expressions::pow(x, manager->integer(2));
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_equal(env, adapter.translateExpression(exprIntPow), msatSquare)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    // Rational-typed but integral exponent
    storm::expressions::Expression exprRationalPow = storm::expressions::pow(x, manager->rational(2));
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_equal(env, adapter.translateExpression(exprRationalPow), msatSquare)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    // Larger odd exponent to test exponentiation by squaring
    storm::expressions::Expression exprFifthPow = storm::expressions::pow(x, manager->integer(5));
    msat_term msatFifth = msat_make_times(env, msat_make_times(env, msatSquare, msatSquare), xVar);
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_equal(env, adapter.translateExpression(exprFifthPow), msatFifth)));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    // Exponent zero.
    storm::expressions::Expression exprZeroPow = storm::expressions::pow(x, manager->integer(0));
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_equal(env, adapter.translateExpression(exprZeroPow), msat_make_number(env, "1"))));
    msat_assert_formula(env, conjecture);
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    // Non-integral exponent is rejected.
    STORM_SILENT_ASSERT_THROW(adapter.translateExpression(storm::expressions::pow(x, manager->rational(0.5))),
                              storm::exceptions::ExpressionEvaluationException);
}

TEST(MathsatExpressionAdapter, StormToMathsatFloorCeil) {
    msat_config config = msat_create_config();
    msat_env env = msat_create_env(config);
    ASSERT_FALSE(MSAT_ERROR_ENV(env));
    msat_destroy_config(config);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::MathsatExpressionAdapter adapter(*manager, env);

    storm::expressions::Variable d = manager->declareRationalVariable("d");
    storm::expressions::Variable i = manager->declareIntegerVariable("i");

    storm::expressions::Expression exprFloor = storm::expressions::floor(d) == i && d > manager->rational(4.1) && d < manager->rational(4.991);
    msat_decl iDecl = msat_declare_function(env, "i", msat_get_integer_type(env));
    msat_term iVar = msat_make_constant(env, iDecl);
    msat_term msatEqualsFour = msat_make_equal(env, msat_make_number(env, "4"), iVar);
    msat_term conjecture;
    msat_term translatedExpr = adapter.translateExpression(exprFloor);
    msat_term msatIff = msat_make_iff(env, translatedExpr, msatEqualsFour);
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msatIff));
    msat_assert_formula(env, conjecture);

    // It is not logically equivalent, so this should be satisfiable.
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_SAT);
    msat_reset_env(env);
    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_or(env, msat_make_not(env, adapter.translateExpression(exprFloor)), msatEqualsFour)));
    msat_assert_formula(env, conjecture);

    // However, the left part implies the right one, which is why this should be unsatisfiable.
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_UNSAT);
    msat_reset_env(env);

    storm::expressions::Expression exprCeil = storm::expressions::ceil(d) == i && d > manager->rational(4.1) && d < manager->rational(4.991);
    msat_term msatEqualsFive = msat_make_equal(env, msat_make_number(env, "5"), iVar);

    ASSERT_NO_THROW(conjecture = msat_make_not(env, msat_make_iff(env, adapter.translateExpression(exprFloor), msatEqualsFive)));
    msat_assert_formula(env, conjecture);

    // It is not logically equivalent, so this should be satisfiable.
    ASSERT_TRUE(msat_solve(env) == msat_result::MSAT_SAT);
    msat_reset_env(env);
    ASSERT_NO_THROW(conjecture = msat_make_or(env, msat_make_not(env, adapter.translateExpression(exprFloor)), msatEqualsFive));
    msat_assert_formula(env, conjecture);
    msat_reset_env(env);
}

TEST(MathsatExpressionAdapter, MathsatToStormBasic) {
    msat_config config = msat_create_config();
    msat_env env = msat_create_env(config);
    ASSERT_FALSE(MSAT_ERROR_ENV(env));
    msat_destroy_config(config);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    manager->declareBooleanVariable("x");
    manager->declareBooleanVariable("y");
    storm::adapters::MathsatExpressionAdapter adapter(*manager, env);

    msat_term msatTrue = msat_make_true(env);
    storm::expressions::Expression exprTrue;
    ASSERT_NO_THROW(exprTrue = adapter.translateExpression(msatTrue));
    ASSERT_TRUE(exprTrue.isTrue());

    msat_term msatFalse = msat_make_false(env);
    storm::expressions::Expression exprFalse;
    ASSERT_NO_THROW(exprTrue = adapter.translateExpression(msatFalse));
    ASSERT_TRUE(exprTrue.isFalse());

    msat_decl xDecl = msat_declare_function(env, "x", msat_get_bool_type(env));
    msat_term x = msat_make_constant(env, xDecl);
    msat_decl yDecl = msat_declare_function(env, "y", msat_get_bool_type(env));
    msat_term y = msat_make_constant(env, yDecl);
    msat_term msatConjunction = msat_make_and(env, x, y);
    storm::expressions::Expression exprConjunction;
    ASSERT_NO_THROW(exprConjunction = adapter.translateExpression(msatConjunction));
    ASSERT_EQ(storm::expressions::OperatorType::And, exprConjunction.getOperator());
    ASSERT_TRUE(exprConjunction.getOperand(0).isVariable());
    ASSERT_EQ("x", exprConjunction.getOperand(0).getIdentifier());
    ASSERT_TRUE(exprConjunction.getOperand(1).isVariable());
    ASSERT_EQ("y", exprConjunction.getOperand(1).getIdentifier());

    msat_term msatNor = msat_make_not(env, msat_make_or(env, x, y));
    storm::expressions::Expression exprNor;
    ASSERT_NO_THROW(exprNor = adapter.translateExpression(msatNor));
    ASSERT_EQ(storm::expressions::OperatorType::Not, exprNor.getOperator());
    ASSERT_EQ(storm::expressions::OperatorType::Or, exprNor.getOperand(0).getOperator());
    ASSERT_TRUE(exprNor.getOperand(0).getOperand(0).isVariable());
    ASSERT_EQ("x", exprNor.getOperand(0).getOperand(0).getIdentifier());
    ASSERT_TRUE(exprNor.getOperand(0).getOperand(1).isVariable());
    ASSERT_EQ("y", exprNor.getOperand(0).getOperand(1).getIdentifier());
}
#endif
