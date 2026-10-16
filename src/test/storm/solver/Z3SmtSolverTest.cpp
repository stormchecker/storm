#include "storm-config.h"
#include "test/storm_gtest.h"

#ifdef STORM_HAVE_Z3
#include "storm/solver/Z3SmtSolver.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "storm/utility/SignalHandler.h"

TEST(Z3SmtSolver, CheckSat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");

    storm::solver::Z3SmtSolver s(*manager);
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

TEST(Z3SmtSolver, CheckUnsat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::expressions::Variable x = manager->declareBooleanVariable("x");
    storm::expressions::Variable y = manager->declareBooleanVariable("y");

    storm::solver::Z3SmtSolver s(*manager);
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

TEST(Z3SmtSolver, Backtracking) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Z3SmtSolver s(*manager);
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

TEST(Z3SmtSolver, Assumptions) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Z3SmtSolver s(*manager);
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

TEST(Z3SmtSolver, GenerateModel) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Z3SmtSolver s(*manager);
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

TEST(Z3SmtSolver, AllSat) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Z3SmtSolver s(*manager);

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

TEST(Z3SmtSolver, UnsatAssumptions) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::solver::Z3SmtSolver s(*manager);
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

namespace {

//! Builds the unsatisfiable pigeonhole principle PHP(numHoles+1, numHoles) as a hard propositional formula that keeps
//! z3 busy searching for a long time (instead of returning a result right away).
void buildHardPigeonholeFormula(storm::expressions::ExpressionManager& manager, storm::solver::Z3SmtSolver& solver, unsigned numHoles) {
    unsigned numPigeons = numHoles + 1;
    std::vector<std::vector<storm::expressions::Variable>> pigeon(numPigeons);
    for (unsigned i = 0; i < numPigeons; ++i) {
        for (unsigned j = 0; j < numHoles; ++j) {
            pigeon[i].push_back(manager.declareBooleanVariable("p_" + std::to_string(i) + "_" + std::to_string(j)));
        }
    }
    // Every pigeon sits in at least one hole.
    for (unsigned i = 0; i < numPigeons; ++i) {
        storm::expressions::Expression atLeastOneHole = manager.boolean(false);
        for (unsigned j = 0; j < numHoles; ++j) {
            atLeastOneHole = atLeastOneHole || pigeon[i][j];
        }
        solver.add(atLeastOneHole);
    }
    // No two pigeons share a hole.
    for (unsigned j = 0; j < numHoles; ++j) {
        for (unsigned i = 0; i < numPigeons; ++i) {
            for (unsigned k = i + 1; k < numPigeons; ++k) {
                solver.add(!pigeon[i][j] || !pigeon[k][j]);
            }
        }
    }
}

}  // namespace

// Regression test for https://github.com/stormchecker/storm/issues/74: while z3 is solving, a termination signal
// (e.g. Ctrl-C) must abort Storm. z3 installs its own SIGINT handler (the "ctrl_c" parameter) during check() by
// default, which quietly cancels the solve and makes check() return "unknown" instead of letting Storm react to the
// interrupt. Storm must disable this behavior.
TEST(Z3SmtSolver, SignalInterruptsRunningSolver) {
    constexpr int signalWaitTime = 1;

    pid_t subject = fork();
    ASSERT_GE(subject, 0);
    if (subject == 0) {
        // Child acting as "Storm".
        storm::utility::resources::installSignalHandler(signalWaitTime);

        std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
        storm::solver::Z3SmtSolver s(*manager);
        buildHardPigeonholeFormula(*manager, s, 12);
        s.check();

        // If we reach this point, the interrupt did not abort Storm (most likely because z3 swallowed it). Report
        // this to the parent through a distinct exit code.
        _exit(20);
    }

    // Give the child some time to actually start solving, then interrupt it.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ASSERT_EQ(kill(subject, SIGINT), 0);

    int status = 0;
    bool exited = false;
    for (int i = 0; i < (signalWaitTime + 10) * 100; ++i) {
        pid_t result = waitpid(subject, &status, WNOHANG);
        if (result == subject) {
            exited = true;
            break;
        }
        if (result == -1 && errno != EINTR) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!exited) {
        kill(subject, SIGKILL);
        while (waitpid(subject, &status, 0) == -1 && errno == EINTR) {
        }
    }

    ASSERT_TRUE(exited) << "Storm did not terminate while a z3 solver was running.";
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), SIGINT) << "The interrupt did not reach Storm's signal handler (z3 swallowed it).";
}

#endif
