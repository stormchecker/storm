#include "storm-config.h"
#include "test/storm_gtest.h"

#include <map>
#include <string>

#include "storm-parsers/parser/ValueParser.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/expressions/Expression.h"
#include "storm/expressions/ExpressionEvaluator.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/expressions/LinearCoefficientVisitor.h"
#include "storm/expressions/LinearityCheckVisitor.h"
#include "storm/expressions/RationalFunctionToExpression.h"
#include "storm/expressions/SimpleValuation.h"
#include "storm/expressions/ToRationalFunctionVisitor.h"

TEST(Expression, FactoryMethodTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    EXPECT_NO_THROW(manager->boolean(true));
    EXPECT_NO_THROW(manager->boolean(false));
    EXPECT_NO_THROW(manager->integer(3));
    EXPECT_NO_THROW(manager->rational(3.14));
    EXPECT_NO_THROW(manager->declareBooleanVariable("x"));
    EXPECT_NO_THROW(manager->declareIntegerVariable("y"));
    EXPECT_NO_THROW(manager->declareRationalVariable("z"));
}

TEST(Expression, AccessorTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Expression trueExpression;
    storm::expressions::Expression falseExpression;
    storm::expressions::Expression threeExpression;
    storm::expressions::Expression piExpression;
    storm::expressions::Expression boolVarExpression;
    storm::expressions::Expression intVarExpression;
    storm::expressions::Expression rationalVarExpression;

    ASSERT_NO_THROW(trueExpression = manager->boolean(true));
    ASSERT_NO_THROW(falseExpression = manager->boolean(false));
    ASSERT_NO_THROW(threeExpression = manager->integer(3));
    ASSERT_NO_THROW(piExpression = manager->rational(3.14));
    ASSERT_NO_THROW(boolVarExpression = manager->declareBooleanVariable("x"));
    ASSERT_NO_THROW(intVarExpression = manager->declareIntegerVariable("y"));
    ASSERT_NO_THROW(rationalVarExpression = manager->declareRationalVariable("z"));

    EXPECT_TRUE(trueExpression.hasBooleanType());
    EXPECT_TRUE(trueExpression.isLiteral());
    EXPECT_TRUE(trueExpression.isTrue());
    EXPECT_FALSE(trueExpression.isFalse());
    EXPECT_TRUE(trueExpression.getVariables() == std::set<storm::expressions::Variable>());

    EXPECT_TRUE(falseExpression.hasBooleanType());
    EXPECT_TRUE(falseExpression.isLiteral());
    EXPECT_FALSE(falseExpression.isTrue());
    EXPECT_TRUE(falseExpression.isFalse());
    EXPECT_TRUE(falseExpression.getVariables() == std::set<storm::expressions::Variable>());

    EXPECT_TRUE(threeExpression.hasIntegerType());
    EXPECT_TRUE(threeExpression.isLiteral());
    EXPECT_FALSE(threeExpression.isTrue());
    EXPECT_FALSE(threeExpression.isFalse());
    EXPECT_TRUE(threeExpression.getVariables() == std::set<storm::expressions::Variable>());

    EXPECT_TRUE(piExpression.hasRationalType());
    EXPECT_TRUE(piExpression.isLiteral());
    EXPECT_FALSE(piExpression.isTrue());
    EXPECT_FALSE(piExpression.isFalse());
    EXPECT_TRUE(piExpression.getVariables() == std::set<storm::expressions::Variable>());

    EXPECT_TRUE(boolVarExpression.hasBooleanType());
    EXPECT_FALSE(boolVarExpression.isLiteral());
    EXPECT_FALSE(boolVarExpression.isTrue());
    EXPECT_FALSE(boolVarExpression.isFalse());
    EXPECT_TRUE(boolVarExpression.getVariables() == std::set<storm::expressions::Variable>({manager->getVariable("x")}));

    EXPECT_TRUE(intVarExpression.hasIntegerType());
    EXPECT_FALSE(intVarExpression.isLiteral());
    EXPECT_FALSE(intVarExpression.isTrue());
    EXPECT_FALSE(intVarExpression.isFalse());
    EXPECT_TRUE(intVarExpression.getVariables() == std::set<storm::expressions::Variable>({manager->getVariable("y")}));

    EXPECT_TRUE(rationalVarExpression.hasRationalType());
    EXPECT_FALSE(rationalVarExpression.isLiteral());
    EXPECT_FALSE(rationalVarExpression.isTrue());
    EXPECT_FALSE(rationalVarExpression.isFalse());
    EXPECT_TRUE(rationalVarExpression.getVariables() == std::set<storm::expressions::Variable>({manager->getVariable("z")}));
}

TEST(Expression, OperatorTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Expression trueExpression;
    storm::expressions::Expression falseExpression;
    storm::expressions::Expression threeExpression;
    storm::expressions::Expression piExpression;
    storm::expressions::Expression boolVarExpression;
    storm::expressions::Expression intVarExpression;
    storm::expressions::Expression rationalVarExpression;

    ASSERT_NO_THROW(trueExpression = manager->boolean(true));
    ASSERT_NO_THROW(falseExpression = manager->boolean(false));
    ASSERT_NO_THROW(threeExpression = manager->integer(3));
    ASSERT_NO_THROW(piExpression = manager->rational(3.14));
    ASSERT_NO_THROW(boolVarExpression = manager->declareBooleanVariable("x"));
    ASSERT_NO_THROW(intVarExpression = manager->declareIntegerVariable("y"));
    ASSERT_NO_THROW(rationalVarExpression = manager->declareRationalVariable("z"));

    storm::expressions::Expression tempExpression;

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::ite(trueExpression, falseExpression, piExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::ite(boolVarExpression, threeExpression, rationalVarExpression));
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::ite(boolVarExpression, threeExpression, intVarExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::ite(boolVarExpression, trueExpression, falseExpression));
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression + piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression + threeExpression);
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = threeExpression + piExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = rationalVarExpression + rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression - piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression - threeExpression);
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = threeExpression - piExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = rationalVarExpression - rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = -trueExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = -threeExpression);
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = -piExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = -rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression * piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression * threeExpression);
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = threeExpression * piExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = intVarExpression * intVarExpression);
    EXPECT_TRUE(tempExpression.hasIntegerType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression / piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression / threeExpression);
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = threeExpression / piExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = rationalVarExpression / intVarExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression && piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = trueExpression && falseExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = boolVarExpression && boolVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression || piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = trueExpression || falseExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = boolVarExpression || boolVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = !threeExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = !trueExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = !boolVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression == piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression == threeExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = intVarExpression == rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression != piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression != threeExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = intVarExpression != rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression > piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression > threeExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = intVarExpression > rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression >= piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression >= threeExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = intVarExpression >= rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression < piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression < threeExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = intVarExpression < rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression <= piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression <= threeExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = intVarExpression <= rationalVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression % piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = threeExpression % threeExpression);
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = threeExpression % piExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = piExpression % threeExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = piExpression % piExpression);
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::modulo(trueExpression, piExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::modulo(threeExpression, threeExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::modulo(threeExpression, piExpression));
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::modulo(piExpression, threeExpression));
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::modulo(piExpression, piExpression));
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::minimum(trueExpression, piExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::minimum(threeExpression, threeExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::minimum(intVarExpression, rationalVarExpression));
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::maximum(trueExpression, piExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::maximum(threeExpression, threeExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::maximum(intVarExpression, rationalVarExpression));
    EXPECT_TRUE(tempExpression.hasRationalType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::implies(trueExpression, piExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::implies(trueExpression, falseExpression));
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::implies(boolVarExpression, boolVarExpression));
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::iff(trueExpression, piExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::iff(trueExpression, falseExpression));
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::iff(boolVarExpression, boolVarExpression));
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = trueExpression != piExpression, storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = trueExpression != falseExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());
    ASSERT_NO_THROW(tempExpression = boolVarExpression != boolVarExpression);
    EXPECT_TRUE(tempExpression.hasBooleanType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::floor(trueExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::floor(threeExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::floor(rationalVarExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::ceil(trueExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::ceil(threeExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::ceil(rationalVarExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());

    STORM_SILENT_ASSERT_THROW(tempExpression = storm::expressions::pow(trueExpression, piExpression), storm::exceptions::InvalidTypeException);
    ASSERT_NO_THROW(tempExpression = storm::expressions::pow(threeExpression, threeExpression, true));
    EXPECT_TRUE(tempExpression.hasIntegerType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::pow(threeExpression, threeExpression, false));
    EXPECT_TRUE(tempExpression.hasRationalType());
    ASSERT_NO_THROW(tempExpression = storm::expressions::pow(intVarExpression, rationalVarExpression));
    EXPECT_TRUE(tempExpression.hasRationalType());

    ASSERT_NO_THROW(tempExpression = storm::expressions::maximum(threeExpression, threeExpression));
    EXPECT_TRUE(tempExpression.hasIntegerType());
}

TEST(Expression, SubstitutionTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Expression trueExpression;
    storm::expressions::Expression falseExpression;
    storm::expressions::Expression threeExpression;
    storm::expressions::Expression piExpression;
    storm::expressions::Expression boolVarExpression;
    storm::expressions::Expression intVarExpression;
    storm::expressions::Expression rationalVarExpression;

    ASSERT_NO_THROW(trueExpression = manager->boolean(true));
    ASSERT_NO_THROW(falseExpression = manager->boolean(false));
    ASSERT_NO_THROW(threeExpression = manager->integer(3));
    ASSERT_NO_THROW(piExpression = manager->rational(3.14));
    ASSERT_NO_THROW(boolVarExpression = manager->declareBooleanVariable("x"));
    ASSERT_NO_THROW(intVarExpression = manager->declareIntegerVariable("y"));
    ASSERT_NO_THROW(rationalVarExpression = manager->declareRationalVariable("z"));

    storm::expressions::Expression tempExpression;
    ASSERT_NO_THROW(tempExpression = (intVarExpression < threeExpression || boolVarExpression) && boolVarExpression);
    std::map<storm::expressions::Variable, storm::expressions::Expression> substution = {std::make_pair(manager->getVariable("y"), manager->rational(2.7)),
                                                                                         std::make_pair(manager->getVariable("x"), manager->boolean(true))};
    storm::expressions::Expression substitutedExpression;
    ASSERT_NO_THROW(substitutedExpression = tempExpression.substitute(substution));
    EXPECT_TRUE(substitutedExpression.simplify().isTrue());
}

TEST(Expression, SumAndProductTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::expressions::Variable xVar = manager->declareIntegerVariable("x");
    storm::expressions::Variable yVar = manager->declareIntegerVariable("y");
    storm::expressions::Variable zVar = manager->declareRationalVariable("z");
    storm::expressions::Expression x = xVar.getExpression();
    storm::expressions::Expression y = yVar.getExpression();
    storm::expressions::Expression z = zVar.getExpression();
    storm::expressions::Expression four = manager->integer(4);

    EXPECT_TRUE(storm::expressions::sum({x}).isVariable());
    EXPECT_TRUE(storm::expressions::product({x}).isVariable());

    storm::expressions::SimpleValuation valuation(manager);
    valuation.setIntegerValue(xVar, 2);
    valuation.setIntegerValue(yVar, 3);
    valuation.setRationalValue(zVar, 0.5);

    auto const intSum = storm::expressions::sum({x, y, four, x, y});
    auto const intProduct = storm::expressions::product({x, y, four, x, y});
    EXPECT_TRUE(intSum.hasIntegerType());
    EXPECT_TRUE(intProduct.hasIntegerType());
    EXPECT_EQ(14, intSum.evaluateAsInt(&valuation));
    EXPECT_EQ(144, intProduct.evaluateAsInt(&valuation));

    auto const mixedSum = storm::expressions::sum({x, y, four, z});
    auto const mixedProduct = storm::expressions::product({x, y, four, z});
    EXPECT_TRUE(mixedSum.hasRationalType());
    EXPECT_TRUE(mixedProduct.hasRationalType());
    EXPECT_NEAR(9.5, mixedSum.evaluateAsDouble(&valuation), 1e-12);
    EXPECT_NEAR(12.0, mixedProduct.evaluateAsDouble(&valuation), 1e-12);

    EXPECT_THROW(storm::expressions::sum({}), storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(storm::expressions::product({}), storm::exceptions::InvalidArgumentException);
}

TEST(Expression, SimplificationTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Expression trueExpression;
    storm::expressions::Expression falseExpression;
    storm::expressions::Expression threeExpression;
    storm::expressions::Expression intVarExpression;

    ASSERT_NO_THROW(trueExpression = manager->boolean(true));
    ASSERT_NO_THROW(falseExpression = manager->boolean(false));
    ASSERT_NO_THROW(threeExpression = manager->integer(3));
    ASSERT_NO_THROW(intVarExpression = manager->declareIntegerVariable("y"));

    storm::expressions::Expression tempExpression;
    storm::expressions::Expression simplifiedExpression;

    ASSERT_NO_THROW(tempExpression = trueExpression || intVarExpression > threeExpression);
    ASSERT_NO_THROW(simplifiedExpression = tempExpression.simplify());
    EXPECT_TRUE(simplifiedExpression.isTrue());

    ASSERT_NO_THROW(tempExpression = falseExpression && intVarExpression > threeExpression);
    ASSERT_NO_THROW(simplifiedExpression = tempExpression.simplify());
    EXPECT_TRUE(simplifiedExpression.isFalse());
}

TEST(Expression, SimpleEvaluationTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Expression trueExpression;
    storm::expressions::Expression falseExpression;
    storm::expressions::Expression threeExpression;
    storm::expressions::Expression piExpression;
    storm::expressions::Expression boolVarExpression;
    storm::expressions::Expression intVarExpression;
    storm::expressions::Expression rationalVarExpression;

    ASSERT_NO_THROW(trueExpression = manager->boolean(true));
    ASSERT_NO_THROW(falseExpression = manager->boolean(false));
    ASSERT_NO_THROW(threeExpression = manager->integer(3));
    ASSERT_NO_THROW(piExpression = manager->rational(3.14));
    ASSERT_NO_THROW(boolVarExpression = manager->declareBooleanVariable("x"));
    ASSERT_NO_THROW(intVarExpression = manager->declareIntegerVariable("y"));
    ASSERT_NO_THROW(rationalVarExpression = manager->declareRationalVariable("z"));

    storm::expressions::Expression tempExpression;

    ASSERT_NO_THROW(tempExpression = (intVarExpression < threeExpression || boolVarExpression) && boolVarExpression);

    ASSERT_NO_THROW(storm::expressions::SimpleValuation valuation(manager));
    storm::expressions::SimpleValuation valuation(manager);
    ASSERT_NO_THROW(valuation.setBooleanValue(manager->getVariable("x"), false));
    ASSERT_NO_THROW(valuation.setIntegerValue(manager->getVariable("y"), 0));
    ASSERT_NO_THROW(valuation.setRationalValue(manager->getVariable("z"), 0));

    STORM_SILENT_ASSERT_THROW(tempExpression.evaluateAsDouble(&valuation), storm::exceptions::InvalidTypeException);
    STORM_SILENT_ASSERT_THROW(tempExpression.evaluateAsInt(&valuation), storm::exceptions::InvalidTypeException);
    EXPECT_FALSE(tempExpression.evaluateAsBool(&valuation));
    ASSERT_NO_THROW(valuation.setIntegerValue(manager->getVariable("y"), 3));
    EXPECT_FALSE(tempExpression.evaluateAsBool(&valuation));

    ASSERT_NO_THROW(tempExpression = storm::expressions::ite(intVarExpression < threeExpression, trueExpression, falseExpression));
    STORM_SILENT_ASSERT_THROW(tempExpression.evaluateAsDouble(&valuation), storm::exceptions::InvalidTypeException);
    STORM_SILENT_ASSERT_THROW(tempExpression.evaluateAsInt(&valuation), storm::exceptions::InvalidTypeException);
    EXPECT_FALSE(tempExpression.evaluateAsBool(&valuation));
}

TEST(Expression, VisitorTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Expression threeExpression;
    storm::expressions::Expression piExpression;
    storm::expressions::Expression intVarExpression;
    storm::expressions::Expression rationalVarExpression;

    ASSERT_NO_THROW(threeExpression = manager->integer(3));
    ASSERT_NO_THROW(piExpression = manager->rational(3.14));
    ASSERT_NO_THROW(intVarExpression = manager->declareIntegerVariable("y"));
    ASSERT_NO_THROW(rationalVarExpression = manager->declareRationalVariable("z"));

    storm::expressions::Expression tempExpression = intVarExpression + rationalVarExpression * threeExpression;
    storm::expressions::LinearityCheckVisitor visitor;
    EXPECT_TRUE(visitor.check(tempExpression));
}

TEST(Expression, RationalFunctionToExpressionTest) {
    storm::parser::ValueParser<storm::RationalFunction> parser;
    parser.addParameter("p");
    parser.addParameter("q");
    auto rationalFunction = parser.parseValue("((5*p^(3))+(q*p*7)+2)/2");

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    auto transformer = storm::expressions::RationalFunctionToExpression<storm::RationalFunction>(manager);

    storm::expressions::Expression expr;
    EXPECT_NO_THROW(expr = transformer.toExpression(rationalFunction));
    auto base = storm::expressions::ExpressionEvaluator<storm::RationalFunction>(*manager);
    storm::expressions::ToRationalFunctionVisitor<storm::RationalFunction> visitor(base);
    ASSERT_NO_THROW(rationalFunction.simplify());
    storm::RationalFunction result;
    ASSERT_NO_THROW(result = visitor.toRationalFunction(expr));
    ASSERT_NO_THROW(result.simplify());
    EXPECT_EQ(rationalFunction.toString(), result.toString());

    rationalFunction = parser.parseValue("(5*((p^(3))+(q*p)))/2");
    transformer = storm::expressions::RationalFunctionToExpression<storm::RationalFunction>(manager);
    EXPECT_NO_THROW(expr = transformer.toExpression(rationalFunction));
    ASSERT_NO_THROW(rationalFunction.simplify());
    ASSERT_NO_THROW(result = visitor.toRationalFunction(expr));
    ASSERT_NO_THROW(result.simplify());
    EXPECT_EQ(rationalFunction.toString(), result.toString());
}

TEST(Expression, RationalFunctionToExpressionTest_no_params) {
    storm::parser::ValueParser<storm::RationalFunction> parser;
    parser.addParameter("p");
    auto rationalFunction1 = parser.parseValue("(p + 380)/3125");
    auto rationalFunction2 = parser.parseValue("(p)/3125");
    auto rationalFunction = rationalFunction1 - rationalFunction2;

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    auto transformer = storm::expressions::RationalFunctionToExpression<storm::RationalFunction>(manager);

    storm::expressions::Expression expr;
    EXPECT_NO_THROW(expr = transformer.toExpression(rationalFunction));
    auto base = storm::expressions::ExpressionEvaluator<storm::RationalFunction>(*manager);
    storm::expressions::ToRationalFunctionVisitor<storm::RationalFunction> visitor(base);
    ASSERT_NO_THROW(rationalFunction.simplify());
    storm::RationalFunction result;
    ASSERT_NO_THROW(result = visitor.toRationalFunction(expr));
    ASSERT_NO_THROW(result.simplify());
    EXPECT_EQ(rationalFunction.toString(), result.toString());
}

// The valuation checks the variables it is accessed with by assertions, which are disabled in release mode.
#ifndef NDEBUG
#define EXPECT_REJECTED(statement) EXPECT_DEATH_IF_SUPPORTED(statement, "")
#else
#define EXPECT_REJECTED(statement) (void)0
#endif

TEST(ExpressionDeathTest, SimpleValuationContainsOnlyItsSnapshotVariables) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareIntegerVariable("y");
    storm::expressions::Variable z = manager->declareRationalVariable("z");

    storm::expressions::SimpleValuation valuation(manager);
    valuation.setBooleanValue(x, true);
    valuation.setIntegerValue(y, 42);
    valuation.setRationalValue(z, 1.5);

    EXPECT_TRUE(valuation.contains(x));
    EXPECT_TRUE(valuation.contains(y));
    EXPECT_TRUE(valuation.contains(z));

    // Declare further variables after the snapshot has been taken.
    storm::expressions::Variable freshBoolean = manager->declareBooleanVariable("freshBoolean");
    storm::expressions::Variable freshInteger = manager->declareIntegerVariable("freshInteger");
    storm::expressions::Variable freshRational = manager->declareRationalVariable("freshRational");
    storm::expressions::Variable freshBitVector = manager->declareBitVectorVariable("freshBitVector", 8);

    EXPECT_FALSE(valuation.contains(freshBoolean));
    EXPECT_FALSE(valuation.contains(freshInteger));
    EXPECT_FALSE(valuation.contains(freshRational));
    EXPECT_FALSE(valuation.contains(freshBitVector));

    EXPECT_REJECTED(valuation.getBooleanValue(freshBoolean));
    EXPECT_REJECTED(valuation.setBooleanValue(freshBoolean, false));
    EXPECT_REJECTED(valuation.getIntegerValue(freshInteger));
    EXPECT_REJECTED(valuation.setIntegerValue(freshInteger, 1));
    EXPECT_REJECTED(valuation.getRationalValue(freshRational));
    EXPECT_REJECTED(valuation.setRationalValue(freshRational, 2.5));
    EXPECT_REJECTED(valuation.getBitVectorValue(freshBitVector));
    EXPECT_REJECTED(valuation.setBitVectorValue(freshBitVector, 1));

    // The variables the valuation was created for remain accessible.
    EXPECT_TRUE(valuation.getBooleanValue(x));
    EXPECT_EQ(42, valuation.getIntegerValue(y));
    EXPECT_DOUBLE_EQ(1.5, valuation.getRationalValue(z));

    // Printing must skip the variables that have no value in this snapshot.
    std::string printed;
    ASSERT_NO_THROW(printed = valuation.toString());
    EXPECT_NE(std::string::npos, printed.find("x=true"));
    EXPECT_EQ(std::string::npos, printed.find("freshBoolean"));
    EXPECT_NO_THROW(printed = valuation.toString(false));

    // Serialization must skip the variables that have no value in this snapshot.
    auto const serialized = valuation.toJson();
    EXPECT_TRUE(serialized.contains("x"));
    EXPECT_TRUE(serialized.contains("y"));
    EXPECT_TRUE(serialized.contains("z"));
    EXPECT_FALSE(serialized.contains("freshBoolean"));
    EXPECT_FALSE(serialized.contains("freshInteger"));
    EXPECT_FALSE(serialized.contains("freshRational"));
    EXPECT_FALSE(serialized.contains("freshBitVector"));
}

TEST(ExpressionDeathTest, SimpleValuationRejectsForeignVariables) {
    std::shared_ptr<storm::expressions::ExpressionManager> managerA(new storm::expressions::ExpressionManager());
    std::shared_ptr<storm::expressions::ExpressionManager> managerB(new storm::expressions::ExpressionManager());

    storm::expressions::Variable own = managerA->declareBooleanVariable("flag");
    storm::expressions::Variable foreign = managerB->declareBooleanVariable("flag");

    storm::expressions::SimpleValuation valuation(managerA);
    valuation.setBooleanValue(own, true);

    // Both variables have offset zero, so only the manager distinguishes them.
    ASSERT_EQ(own.getOffset(), foreign.getOffset());
    EXPECT_FALSE(valuation.contains(foreign));
    EXPECT_REJECTED(valuation.getBooleanValue(foreign));
    EXPECT_REJECTED(valuation.setBooleanValue(foreign, false));
    EXPECT_TRUE(valuation.getBooleanValue(own));
}

TEST(ExpressionDeathTest, SimpleValuationRejectsVariablesItDoesNotStore) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable flag = manager->declareBooleanVariable("flag");
    storm::expressions::Variable array = manager->declareArrayVariable("array", manager->getIntegerType());

    storm::expressions::SimpleValuation valuation(manager);
    valuation.setBooleanValue(flag, true);

    EXPECT_FALSE(valuation.contains(array));
    EXPECT_REJECTED(valuation.getBooleanValue(array));

    // Array variables share the offset space of no stored type, but they are never contained.
    ASSERT_NO_THROW(valuation.toString());
}

TEST(ExpressionDeathTest, SimpleValuationRejectsVariablesOfDefaultConstructedValuation) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    storm::expressions::Variable x = manager->declareBooleanVariable("x");

    storm::expressions::SimpleValuation valuation;
    EXPECT_FALSE(valuation.contains(x));
    EXPECT_REJECTED(valuation.getBooleanValue(x));
    EXPECT_REJECTED(valuation.setBooleanValue(x, true));
}
TEST(Expression, LinearCoefficientsTest) {
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto x = manager->declareRationalVariable("x");
    auto y = manager->declareRationalVariable("y");

    // 1/3 * x + (x - y) / 2 - 1 is linear with exact rational coefficients.
    auto linear = manager->rational(storm::utility::convertNumber<storm::RationalNumber>(1) / storm::utility::convertNumber<storm::RationalNumber>(3)) *
                      x.getExpression() +
                  (x.getExpression() - y.getExpression()) / manager->integer(2) - manager->integer(1);
    auto coefficients = storm::expressions::LinearCoefficientVisitor<storm::RationalNumber>().getLinearCoefficients(linear);
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(5) / storm::utility::convertNumber<storm::RationalNumber>(6),
              coefficients.getCoefficient(x));
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(-1) / storm::utility::convertNumber<storm::RationalNumber>(2),
              coefficients.getCoefficient(y));
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(-1), coefficients.getConstantPart());

    // Variables that cancel out are not part of the coefficients.
    auto cancelled = storm::expressions::LinearCoefficientVisitor<double>().getLinearCoefficients(x.getExpression() + y.getExpression() - x.getExpression());
    EXPECT_EQ(1ul, cancelled.size());

    // Non-linear expressions and divisions by zero are rejected.
    storm::expressions::LinearCoefficientVisitor<storm::RationalNumber> visitor;
    EXPECT_FALSE(visitor.tryGetLinearCoefficients(x.getExpression() * y.getExpression()).has_value());
    EXPECT_FALSE(visitor.tryGetLinearCoefficients(x.getExpression() / manager->integer(0)).has_value());
    EXPECT_FALSE(visitor.tryGetLinearCoefficients(x.getExpression() <= y.getExpression()).has_value());
    EXPECT_THROW(visitor.getLinearCoefficients(x.getExpression() * y.getExpression()), storm::exceptions::InvalidArgumentException);
}
