#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/ExpressionCreator.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/ExprtkExpressionEvaluator.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/storage/expressions/SimpleValuation.h"

TEST(ExpressionEvaluation, NaiveEvaluation) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x;
    storm::expressions::Variable y;
    storm::expressions::Variable z;
    ASSERT_NO_THROW(x = manager->declareBooleanVariable("x"));
    ASSERT_NO_THROW(y = manager->declareIntegerVariable("y"));
    ASSERT_NO_THROW(z = manager->declareRationalVariable("z"));

    storm::expressions::SimpleValuation eval(manager);

    storm::expressions::Expression iteExpression = storm::expressions::ite(x, y + z, manager->integer(3) * z);

    eval.setRationalValue(z, 5.5);
    eval.setBooleanValue(x, true);
    for (int_fast64_t i = 0; i < 1000; ++i) {
        eval.setIntegerValue(y, 3 + i);
        EXPECT_NEAR(8.5 + i, iteExpression.evaluateAsDouble(&eval), 1e-6);
    }

    eval.setBooleanValue(x, false);
    for (int_fast64_t i = 0; i < 1000; ++i) {
        double zValue = i / static_cast<double>(10);
        eval.setRationalValue(z, zValue);
        EXPECT_NEAR(3 * zValue, iteExpression.evaluateAsDouble(&eval), 1e-6);
    }
}

TEST(ExpressionEvaluation, ExprTkEvaluation) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x;
    storm::expressions::Variable y;
    storm::expressions::Variable z;
    ASSERT_NO_THROW(x = manager->declareBooleanVariable("x"));
    ASSERT_NO_THROW(y = manager->declareIntegerVariable("y"));
    ASSERT_NO_THROW(z = manager->declareRationalVariable("z"));

    storm::expressions::Expression iteExpression = storm::expressions::ite(x, y + z, manager->integer(3) * z);
    storm::expressions::ExprtkExpressionEvaluator eval(*manager);

    eval.setRationalValue(z, 5.5);
    eval.setBooleanValue(x, true);
    for (int_fast64_t i = 0; i < 1000; ++i) {
        eval.setIntegerValue(y, 3 + i);
        EXPECT_NEAR(8.5 + i, eval.asRational(iteExpression), 1e-6);
    }

    eval.setBooleanValue(x, false);
    for (int_fast64_t i = 0; i < 1000; ++i) {
        double zValue = i / static_cast<double>(10);
        eval.setRationalValue(z, zValue);
        EXPECT_NEAR(3 * zValue, eval.asRational(iteExpression), 1e-6);
    }
}

TEST(ExpressionEvaluation, NegativeModulo) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::parser::ExpressionCreator creator(*manager);
    std::unordered_map<std::string, storm::expressions::Expression> mapping;
    creator.setIdentifierMapping(mapping);

    storm::expressions::Expression n = manager->integer(-1);
    storm::expressions::Expression mod = manager->integer(4);

    bool pass = true;
    storm::expressions::Expression expr = creator.createPowerModuloLogarithmExpression(n, storm::expressions::OperatorType::Modulo, mod, pass);

    auto positiveModulo = [](int a, int b) { return a >= 0 ? a % b : (a % b) + b; };

    int expectedInt = positiveModulo(-1, 4);
    double expectedDouble(expectedInt);

    storm::expressions::ExprtkExpressionEvaluator evaluator(*manager);
    int result1 = evaluator.asInt(expr);
    int result2 = expr.evaluateAsInt();
    double result3 = evaluator.asRational(expr);
    double result4 = expr.evaluateAsDouble();

    EXPECT_EQ(result1, expectedInt);
    EXPECT_EQ(result2, expectedInt);
    EXPECT_NEAR(result3, expectedDouble, 1e-6);
    EXPECT_NEAR(result4, expectedDouble, 1e-6);
}
