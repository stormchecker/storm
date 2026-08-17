#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>

#ifdef STORM_HAVE_Z3
#include "storm/adapters/Z3ExpressionAdapter.h"
#include "storm/settings/SettingsManager.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/OperatorType.h"

TEST(Z3ExpressionAdapter, StormToZ3Basic) {
    z3::context ctx;
    z3::solver s(ctx);
    z3::expr conjecture = ctx.bool_val(false);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Z3ExpressionAdapter adapter(*manager, ctx);

    storm::expressions::Expression exprTrue = manager->boolean(true);
    z3::expr z3True = ctx.bool_val(true);
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprTrue), z3True)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();

    storm::expressions::Expression exprFalse = manager->boolean(false);
    z3::expr z3False = ctx.bool_val(false);
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprFalse), z3False)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");
    storm::expressions::Expression exprConjunction = x && y;
    z3::expr z3Conjunction = (ctx.bool_const("x") && ctx.bool_const("y"));

    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprConjunction), z3Conjunction)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();

    storm::expressions::Expression exprNor = !(x || y);
    z3::expr z3Nor = !(ctx.bool_const("x") || ctx.bool_const("y"));
    ASSERT_NO_THROW(adapter.translateExpression(exprNor));  // Variables already created in adapter.
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprNor), z3Nor)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();
}

TEST(Z3ExpressionAdapter, StormToZ3Integer) {
    z3::context ctx;
    z3::solver s(ctx);
    z3::expr conjecture = ctx.bool_val(false);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Z3ExpressionAdapter adapter(*manager, ctx);

    storm::expressions::Variable x = manager->declareIntegerVariable("x");
    storm::expressions::Variable y = manager->declareIntegerVariable("y");

    storm::expressions::Expression exprAdd = x + y < -y;
    z3::expr z3Add = (ctx.int_const("x") + ctx.int_const("y") < -ctx.int_const("y"));
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprAdd), z3Add)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();

    storm::expressions::Expression exprMult = !(x * y == y);
    z3::expr z3Mult = !(ctx.int_const("x") * ctx.int_const("y") == ctx.int_const("y"));
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprMult), z3Mult)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();
}

TEST(Z3ExpressionAdapter, StormToZ3Real) {
    z3::context ctx;
    z3::solver s(ctx);
    z3::expr conjecture = ctx.bool_val(false);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Z3ExpressionAdapter adapter(*manager, ctx);

    storm::expressions::Variable x = manager->declareRationalVariable("x");
    storm::expressions::Variable y = manager->declareRationalVariable("y");

    storm::expressions::Expression exprAdd = x + y < -y;
    z3::expr z3Add = (ctx.real_const("x") + ctx.real_const("y") < -ctx.real_const("y"));
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprAdd), z3Add)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();

    storm::expressions::Expression exprMult = !(x * y == y);
    z3::expr z3Mult = !(ctx.real_const("x") * ctx.real_const("y") == ctx.real_const("y"));
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprMult), z3Mult)));
    s.add(conjecture);
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();
}

TEST(Z3ExpressionAdapter, StormToZ3FloorCeil) {
    z3::context ctx;
    z3::solver s(ctx);
    z3::expr conjecture = ctx.bool_val(false);

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::adapters::Z3ExpressionAdapter adapter(*manager, ctx);

    storm::expressions::Variable d = manager->declareRationalVariable("d");
    storm::expressions::Variable i = manager->declareIntegerVariable("i");

    storm::expressions::Expression exprFloor = storm::expressions::floor(d) == i && d > manager->rational(4.1) && d < manager->rational(4.991);
    z3::expr z3Floor = ctx.int_val(4) == ctx.int_const("i");

    conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprFloor), z3Floor));
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprFloor), z3Floor)));
    s.add(conjecture);

    // It is not logically equivalent, so this should be satisfiable.
    ASSERT_TRUE(s.check() == z3::sat);
    s.reset();
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_implies(ctx, adapter.translateExpression(exprFloor), z3Floor)));
    s.add(conjecture);

    // However, the left part implies the right one, which is why this should be unsatisfiable.
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();

    storm::expressions::Expression exprCeil = storm::expressions::ceil(d) == i && d > manager->rational(4.1) && d < manager->rational(4.991);
    z3::expr z3Ceil = ctx.int_val(5) == ctx.int_const("i");

    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_iff(ctx, adapter.translateExpression(exprCeil), z3Ceil)));
    s.add(conjecture);
    // It is not logically equivalent, so this should be satisfiable.
    ASSERT_TRUE(s.check() == z3::sat);
    s.reset();
    ASSERT_NO_THROW(conjecture = !z3::expr(ctx, Z3_mk_implies(ctx, adapter.translateExpression(exprCeil), z3Ceil)));
    s.add(conjecture);

    // However, the left part implies the right one, which is why this should be unsatisfiable.
    ASSERT_TRUE(s.check() == z3::unsat);
    s.reset();
}

TEST(Z3ExpressionAdapter, Z3ToStormBasic) {
    z3::context ctx;

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    manager->declareBooleanVariable("x");
    manager->declareBooleanVariable("y");
    storm::adapters::Z3ExpressionAdapter adapter(*manager, ctx);

    z3::expr z3True = ctx.bool_val(true);
    storm::expressions::Expression exprTrue;
    exprTrue = adapter.translateExpression(z3True);
    ASSERT_TRUE(exprTrue.isTrue());

    z3::expr z3False = ctx.bool_val(false);
    storm::expressions::Expression exprFalse;
    exprFalse = adapter.translateExpression(z3False);
    ASSERT_TRUE(exprFalse.isFalse());

    z3::expr z3Conjunction = (ctx.bool_const("x") && ctx.bool_const("y"));
    storm::expressions::Expression exprConjunction;
    (exprConjunction = adapter.translateExpression(z3Conjunction));
    ASSERT_EQ(storm::expressions::OperatorType::And, exprConjunction.getOperator());
    ASSERT_TRUE(exprConjunction.getOperand(0).isVariable());
    ASSERT_EQ("x", exprConjunction.getOperand(0).getIdentifier());
    ASSERT_TRUE(exprConjunction.getOperand(1).isVariable());
    ASSERT_EQ("y", exprConjunction.getOperand(1).getIdentifier());

    z3::expr z3Nor = !(ctx.bool_const("x") || ctx.bool_const("y"));
    storm::expressions::Expression exprNor;
    (exprNor = adapter.translateExpression(z3Nor));
    ASSERT_EQ(storm::expressions::OperatorType::Not, exprNor.getOperator());
    ASSERT_EQ(storm::expressions::OperatorType::Or, exprNor.getOperand(0).getOperator());
    ASSERT_TRUE(exprNor.getOperand(0).getOperand(0).isVariable());
    ASSERT_EQ("x", exprNor.getOperand(0).getOperand(0).getIdentifier());
    ASSERT_TRUE(exprNor.getOperand(0).getOperand(1).isVariable());
    ASSERT_EQ("y", exprNor.getOperand(0).getOperand(1).getIdentifier());
}
#endif
