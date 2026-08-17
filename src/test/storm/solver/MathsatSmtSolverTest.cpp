#include "storm-config.h"
#include "test/storm_gtest.h"

#ifdef STORM_HAVE_MATHSAT
#include "storm/solver/MathsatSmtSolver.h"

TEST(MathsatSmtSolver, CheckSat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");

    storm::solver::MathsatSmtSolver s(*manager);
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Expression exprDeMorgan = storm::expressions::iff(!(x && y), !x || !y);

    ASSERT_NO_THROW(s.add(exprDeMorgan));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.reset());

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");

    storm::expressions::Expression exprFormula =
        a >= manager->integer(0) && a < manager->integer(5) && b > manager->integer(7) && c == (a + b - manager->integer(1)) && b + a > c;

    ASSERT_NO_THROW(s.add(exprFormula));
    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Sat);
    ASSERT_NO_THROW(s.reset());
}

TEST(MathsatSmtSolver, CheckUnsat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");

    storm::solver::MathsatSmtSolver s(*manager);
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
}

TEST(MathsatSmtSolver, Backtracking) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::MathsatSmtSolver s(*manager);
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

TEST(MathsatSmtSolver, Assumptions) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::MathsatSmtSolver s(*manager);
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

TEST(MathsatSmtSolver, GenerateModel) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::MathsatSmtSolver s(*manager);
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
    int_fast64_t aEval = model->getIntegerValue(a);
    int_fast64_t bEval = model->getIntegerValue(b);
    int_fast64_t cEval = model->getIntegerValue(c);
    ASSERT_TRUE(cEval == aEval + bEval - 1);
}

TEST(MathsatSmtSolver, AllSat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::MathsatSmtSolver s(*manager);

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

TEST(MathsatSmtSolver, UnsatAssumptions) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::MathsatSmtSolver s(*manager, storm::solver::MathsatSmtSolver::Options(false, true, false));
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");
    storm::expressions::Expression exprFormula =
        a >= manager->integer(0) && a < manager->integer(5) && b > manager->integer(7) && c == a + b - manager->integer(1) && b + a > c;
    storm::expressions::Variable f2 = manager->declareBooleanVariable("f2");
    storm::expressions::Expression exprFormula2 = storm::expressions::implies(f2, c > a + b + manager->integer(1));

    s.add(exprFormula);
    s.add(exprFormula2);
    result = s.checkWithAssumptions({f2});
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
    std::vector<storm::expressions::Expression> unsatCore = s.getUnsatAssumptions();
    ASSERT_EQ(1ull, unsatCore.size());
    ASSERT_TRUE(unsatCore[0].isVariable());
    ASSERT_STREQ("f2", unsatCore[0].getIdentifier().c_str());
}

TEST(MathsatSmtSolver, InterpolationTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::MathsatSmtSolver s(*manager, storm::solver::MathsatSmtSolver::Options(false, false, true));
    storm::solver::SmtSolver::CheckResult result = storm::solver::SmtSolver::CheckResult::Unknown;

    storm::expressions::Variable a = manager->declareIntegerVariable("a");
    storm::expressions::Variable b = manager->declareIntegerVariable("b");
    storm::expressions::Variable c = manager->declareIntegerVariable("c");
    storm::expressions::Expression exprFormula = a > b;
    storm::expressions::Expression exprFormula2 = b > c;
    storm::expressions::Expression exprFormula3 = c > a;

    s.setInterpolationGroup(0);
    s.add(exprFormula);
    s.setInterpolationGroup(1);
    s.add(exprFormula2);
    s.setInterpolationGroup(2);
    s.add(exprFormula3);

    ASSERT_NO_THROW(result = s.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);

    storm::expressions::Expression interpol;
    ASSERT_NO_THROW(interpol = s.getInterpolant({0, 1}));

    storm::solver::MathsatSmtSolver s2(*manager);

    ASSERT_NO_THROW(s2.add(!storm::expressions::implies(exprFormula && exprFormula2, interpol)));
    ASSERT_NO_THROW(result = s2.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);

    ASSERT_NO_THROW(s2.reset());
    ASSERT_NO_THROW(s2.add(interpol && exprFormula3));
    ASSERT_NO_THROW(result = s2.check());
    ASSERT_TRUE(result == storm::solver::SmtSolver::CheckResult::Unsat);
}

#endif
