#include "storm-config.h"
#include "test/storm_gtest.h"

#ifdef STORM_HAVE_CVC5
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/solver/Cvc5SmtSolver.h"
#include "storm/storage/expressions/OperatorType.h"

TEST(Cvc5SmtSolver, CheckSat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");

    storm::solver::Cvc5SmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Expression exprDeMorgan = storm::expressions::iff(!(x && y), !x || !y);

    ASSERT_NO_THROW(s.add(exprDeMorgan));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.reset());

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");

    storm::expressions::Expression exprFormula = a >= manager->integer(0) && a < manager->integer(5) && b > manager->integer(7) && c == (a * b) && b + a > c;

    ASSERT_NO_THROW(s.add(exprFormula));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.reset());
}

TEST(Cvc5SmtSolver, CheckUnsat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");

    storm::solver::Cvc5SmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Expression exprDeMorgan = storm::expressions::iff(!(x && y), !x || !y);

    ASSERT_NO_THROW(s.add(!exprDeMorgan));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    ASSERT_NO_THROW(s.reset());

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");

    storm::expressions::Expression exprFormula =
        a >= manager->rational(2) && a < manager->integer(5) && b > manager->integer(7) && c == (a + b + manager->integer(1)) && b + a > c;

    ASSERT_NO_THROW(s.add(exprFormula));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    ASSERT_NO_THROW(s.reset());
}

TEST(Cvc5SmtSolver, Backtracking) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Expression expr1 = manager->boolean(true);
    storm::expressions::Expression expr2 = manager->boolean(false);
    storm::expressions::Expression expr3 = manager->boolean(false);

    ASSERT_NO_THROW(s.add(expr1));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.push());
    ASSERT_NO_THROW(s.add(expr2));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    ASSERT_NO_THROW(s.pop());
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.push());
    ASSERT_NO_THROW(s.add(expr2));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    ASSERT_NO_THROW(s.push());
    ASSERT_NO_THROW(s.add(expr3));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    ASSERT_NO_THROW(s.pop(2));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.reset());

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");
    storm::expressions::Expression exprFormula =
        a >= manager->integer(0) && a < manager->integer(5) && b > manager->integer(7) && c == (a + b - manager->integer(1)) && b + a > c;
    storm::expressions::Expression exprFormula2 = c > a + b + manager->integer(1);

    ASSERT_NO_THROW(s.add(exprFormula));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.push());
    ASSERT_NO_THROW(s.add(exprFormula2));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    ASSERT_NO_THROW(s.pop());
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
}

TEST(Cvc5SmtSolver, Assumptions) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");
    storm::expressions::Expression exprFormula =
        a >= manager->integer(0) && a < manager->integer(5) && b > manager->integer(7) && c == a + b - manager->integer(1) && b + a > c;
    storm::expressions::Variable f2 = manager->declareBooleanVariable("f2");
    storm::expressions::Expression exprFormula2 = storm::expressions::implies(f2, c > a + b + manager->integer(1));

    ASSERT_NO_THROW(s.add(exprFormula));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.add(exprFormula2));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(result = s.checkWithAssumptions({f2}));
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(result = s.checkWithAssumptions({!f2}));
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
}

TEST(Cvc5SmtSolver, GenerateModel) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result;

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");
    storm::expressions::Expression exprFormula =
        a > manager->integer(0) && a < manager->integer(5) && b > manager->integer(7) && c == a + b - manager->integer(1) && b + a > c;

    s.add(exprFormula);
    result = s.check();
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    std::shared_ptr<storm::solver::SmtSolver::ModelReference> model = s.getModel();
    int64_t aEval = model->getIntegerValue(a);
    int64_t bEval = model->getIntegerValue(b);
    int64_t cEval = model->getIntegerValue(c);
    ASSERT_TRUE(cEval == aEval + bEval - 1);
}

TEST(Cvc5SmtSolver, GenerateValuation) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result;

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareBooleanVariable("b");
    storm::expressions::Expression exprFormula = a > manager->integer(0) && a < manager->integer(5) && b;

    s.add(exprFormula);
    result = s.check();
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);

    storm::expressions::SimpleValuation valuation = s.getModelAsValuation();
    ASSERT_NO_THROW(ASSERT_TRUE(valuation.getIntegerValue(a) > 0));
    ASSERT_NO_THROW(ASSERT_TRUE(valuation.getIntegerValue(a) < 5));
    ASSERT_NO_THROW(ASSERT_TRUE(valuation.getBooleanValue(b)));
}

TEST(Cvc5SmtSolver, AllSat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");
    storm::expressions::Variable z = manager->declareBooleanVariable("z");
    storm::expressions::Expression exprFormula1 = storm::expressions::implies(x, a > manager->integer(5));
    storm::expressions::Expression exprFormula2 = storm::expressions::implies(y, a < manager->integer(5));
    storm::expressions::Expression exprFormula3 = storm::expressions::implies(z, b < manager->integer(5));

    s.add(exprFormula1);
    s.add(exprFormula2);
    s.add(exprFormula3);

    std::vector<storm::expressions::SimpleValuation> valuations = s.allSat({x, y});

    ASSERT_TRUE(valuations.size() == 3);
    for (uint64_t i = 0; i < valuations.size(); ++i) {
        ASSERT_FALSE(valuations[i].getBooleanValue(x) && valuations[i].getBooleanValue(y));

        for (uint64_t j = i + 1; j < valuations.size(); ++j) {
            ASSERT_TRUE((valuations[i].getBooleanValue(x) != valuations[j].getBooleanValue(x)) ||
                        (valuations[i].getBooleanValue(y) != valuations[j].getBooleanValue(y)));
        }
    }
}

TEST(Cvc5SmtSolver, AddNotCurrentModel) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);

    storm::expressions::Variable x = manager->declareBooleanVariable("x");

    // The formula is a tautology, so that both values of x constitute a model.
    s.add(x || !x);

    std::vector<storm::expressions::SimpleValuation> valuations;
    while (s.check() == storm::solver::SmtSolver::CheckResult::Sat) {
        valuations.push_back(s.getModelAsValuation());
        s.addNotCurrentModel();
    }

    ASSERT_EQ(2ull, valuations.size());
    ASSERT_NE(valuations[0].getBooleanValue(x), valuations[1].getBooleanValue(x));
    ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Unsat, s.check());
}

TEST(Cvc5SmtSolver, ModelReferenceDescribesItsOwnModel) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareBooleanVariable("b");
    s.add(a == manager->integer(42) && b);

    ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Sat, s.check());
    std::shared_ptr<storm::solver::SmtSolver::ModelReference> model = s.getModel();

    // The string representation must describe the model that the reference holds.
    std::string modelString = model->toString();
    EXPECT_NE(modelString.find("a"), std::string::npos);
    EXPECT_NE(modelString.find("b"), std::string::npos);
    EXPECT_NE(modelString.find("42"), std::string::npos);

    // The reference keeps describing that model, even after the solver has moved on.
    s.addNotCurrentModel();
    ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Unsat, s.check());
    EXPECT_EQ(modelString, model->toString());
    EXPECT_EQ(42, model->getIntegerValue(a));
    EXPECT_TRUE(model->getBooleanValue(b));
}

TEST(Cvc5SmtSolver, ModelReferenceRejectsValuesOfTheWrongType) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareBooleanVariable("b");
    storm::expressions::Variable c = manager->declareRationalVariable("c");
    s.add(a == manager->integer(42) && b && c == manager->rational(0.5));
    ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Sat, s.check());
    std::shared_ptr<storm::solver::SmtSolver::ModelReference> model = s.getModel();

    // The values are stored per type, so reading a variable with the wrong getter would access an unrelated
    // entry instead of reporting the mistake.
    EXPECT_THROW(model->getBooleanValue(a), storm::exceptions::InvalidTypeException);
    EXPECT_THROW(model->getBooleanValue(c), storm::exceptions::InvalidTypeException);
    EXPECT_THROW(model->getIntegerValue(b), storm::exceptions::InvalidTypeException);
    EXPECT_THROW(model->getIntegerValue(c), storm::exceptions::InvalidTypeException);
    EXPECT_THROW(model->getRationalValue(a), storm::exceptions::InvalidTypeException);
    EXPECT_THROW(model->getRationalValue(b), storm::exceptions::InvalidTypeException);

    // The matching getters still work.
    EXPECT_EQ(42, model->getIntegerValue(a));
    EXPECT_TRUE(model->getBooleanValue(b));
    EXPECT_DOUBLE_EQ(0.5, model->getRationalValue(c));
}

TEST(Cvc5SmtSolver, ModelReferenceReadsBitVectorVariableAsInteger) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);

    // A bitvector is an integer type in Storm, and IterativePolicySearch reads the value of such a scheduler
    // variable with the integer getter of a model.
    storm::expressions::Variable bv = manager->declareBitVectorVariable("bv", 8);
    s.add(bv == manager->integer(3));
    ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Sat, s.check());
    std::shared_ptr<storm::solver::SmtSolver::ModelReference> model = s.getModel();

    EXPECT_EQ(3, model->getIntegerValue(bv));
    EXPECT_THROW(model->getBooleanValue(bv), storm::exceptions::InvalidTypeException);
}

TEST(Cvc5SmtSolver, ModelReferenceOutlivesItsSolver) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareBooleanVariable("b");
    storm::expressions::Variable c = manager->declareRationalVariable("c");

    std::shared_ptr<storm::solver::SmtSolver::ModelReference> model;
    {
        storm::solver::Cvc5SmtSolver s(*manager);
        s.add(a == manager->integer(7) && b && c == manager->rational(0.5));
        ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Sat, s.check());
        model = s.getModel();
    }

    // The solver (and its expression adapter) are gone, but the values are still readable.
    EXPECT_EQ(7, model->getIntegerValue(a));
    EXPECT_TRUE(model->getBooleanValue(b));
    EXPECT_DOUBLE_EQ(0.5, model->getRationalValue(c));
    EXPECT_NO_THROW(model->toString());
}

TEST(Cvc5SmtSolver, AddNotCurrentModelWithUnconstrainedVariable) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);

    storm::expressions::Variable x = manager->declareBooleanVariable("x");

    // The tautology does not mention y, so the model does not constrain it. Ruling out the current
    // model must not constrain y, otherwise models would be missed.
    storm::expressions::Variable y = manager->declareBooleanVariable("y");
    s.add(x || !x);

    std::vector<bool> yValues;
    while (s.check() == storm::solver::SmtSolver::CheckResult::Sat) {
        yValues.push_back(s.getModelAsValuation().getBooleanValue(y));
        s.addNotCurrentModel();
    }

    // x takes both values, and y is free to take both values in each case.
    ASSERT_EQ(4ull, yValues.size());
    EXPECT_NE(yValues[0], yValues[1]);
    EXPECT_NE(yValues[2], yValues[3]);
}

TEST(Cvc5SmtSolver, AddNotCurrentModelWithOnlyBitVectorVariables) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);

    // The bitvector is the only variable, so the blocking clause must not degenerate to "false".
    storm::expressions::Variable bv = manager->declareBitVectorVariable("bv", 8);
    s.add(bv == manager->integer(0) || bv == manager->integer(1));

    std::vector<int64_t> values;
    while (s.check() == storm::solver::SmtSolver::CheckResult::Sat) {
        values.push_back(s.getModelAsValuation().getIntegerValue(bv));
        s.addNotCurrentModel();
    }

    // Both models are found instead of the second check being unsatisfiable right away.
    ASSERT_EQ(2ull, values.size());
    EXPECT_NE(values[0], values[1]);
}

TEST(Cvc5SmtSolver, UnsatAssumptions) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Cvc5SmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareBooleanVariable("b");
    storm::expressions::Expression exprFormula = a >= manager->integer(0) && a < manager->integer(5);
    storm::expressions::Expression exprB = b;
    storm::expressions::Expression exprNotB = !b;

    // The formula is satisfiable on its own, but the two assumptions are contradictory.
    ASSERT_NO_THROW(s.add(exprFormula));
    ASSERT_NO_THROW(result = s.checkWithAssumptions({exprB, exprNotB}));
    ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Unsat, result);

    std::vector<storm::expressions::Expression> unsatAssumptions = s.getUnsatAssumptions();
    ASSERT_EQ(2ull, unsatAssumptions.size());

    auto const isAssumptionB = [](storm::expressions::Expression const& expression) { return expression.isVariable() && expression.getIdentifier() == "b"; };
    auto const isAssumptionNotB = [&isAssumptionB](storm::expressions::Expression const& expression) {
        return expression.getOperator() == storm::expressions::OperatorType::Not && isAssumptionB(expression.getOperand(0));
    };
    ASSERT_TRUE((isAssumptionB(unsatAssumptions[0]) && isAssumptionNotB(unsatAssumptions[1])) ||
                (isAssumptionNotB(unsatAssumptions[0]) && isAssumptionB(unsatAssumptions[1])));

    // A satisfiable check does not yield an unsatisfiable core.
    ASSERT_NO_THROW(result = s.checkWithAssumptions({exprB}));
    ASSERT_EQ(storm::solver::SmtSolver::CheckResult::Sat, result);
    ASSERT_ANY_THROW(s.getUnsatAssumptions());
}
#endif
