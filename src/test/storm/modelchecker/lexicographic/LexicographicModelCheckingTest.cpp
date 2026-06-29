#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/lexicographic/LexicographicModelChecking.h"
#include "storm/modelchecker/results/LexicographicCheckResult.h"

TEST(LexicographicModelCheckingTest, prob_sched1) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
#ifndef STORM_HAVE_SPOT
    GTEST_SKIP() << "Spot not available.";
#endif
    typedef double ValueType;
    std::string formulasString = "multilex(Pmax=? [GF y=2], Pmax=? [GF y=1], Pmax=? [GF y=3]);";
    std::string pathToPrismFile = STORM_TEST_RESOURCES_DIR "/mdp/prob_sched.prism";
    std::pair<std::shared_ptr<storm::models::sparse::Mdp<ValueType>>, std::vector<std::shared_ptr<storm::logic::Formula const>>> modelFormulas;
    storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
    program = storm::utility::prism::preprocess(program, "");
    modelFormulas.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    modelFormulas.first = storm::api::buildSparseModel<ValueType>(program, modelFormulas.second)->template as<storm::models::sparse::Mdp<ValueType>>();

    auto mdp = std::move(modelFormulas.first);
    std::vector<storm::modelchecker::CheckTask<storm::logic::MultiObjectiveFormula, ValueType>> tasks;
    for (auto const& f : modelFormulas.second) {
        tasks.emplace_back(f->asMultiObjectiveFormula());
        tasks.back().setProduceSchedulers(true);
    }

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);
    storm::Environment env;
    {
        tasks[0].setOnlyInitialStatesRelevant(true);
        auto result = checker.checkLexObjectiveFormula(env, tasks[0]);
        ASSERT_TRUE(result->isLexicographicCheckResult());
        auto& lexResult = result->asLexicographicCheckResult<double>().getInitialStateValue();
        EXPECT_NEAR(1.0, lexResult[0], storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
        EXPECT_NEAR(0.5, lexResult[1], storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
        EXPECT_NEAR(0, lexResult[2], storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
    }
}

TEST(LexicographicModelCheckingTest, prob_sched2) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
#ifndef STORM_HAVE_SPOT
    GTEST_SKIP() << "Spot not available.";
#endif
    typedef double ValueType;
    std::string formulasString = "multilex(Pmax=? [GF y=1], Pmax=? [GF y=2], Pmax=? [GF y=3]);";
    std::string pathToPrismFile = STORM_TEST_RESOURCES_DIR "/mdp/prob_sched.prism";
    std::pair<std::shared_ptr<storm::models::sparse::Mdp<ValueType>>, std::vector<std::shared_ptr<storm::logic::Formula const>>> modelFormulas;
    storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
    program = storm::utility::prism::preprocess(program, "");
    modelFormulas.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    modelFormulas.first = storm::api::buildSparseModel<ValueType>(program, modelFormulas.second)->template as<storm::models::sparse::Mdp<ValueType>>();

    auto mdp = std::move(modelFormulas.first);
    std::vector<storm::modelchecker::CheckTask<storm::logic::MultiObjectiveFormula, ValueType>> tasks;
    for (auto const& f : modelFormulas.second) {
        tasks.emplace_back(f->asMultiObjectiveFormula());
        tasks.back().setProduceSchedulers(true);
    }

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);
    storm::Environment env;
    {
        tasks[0].setOnlyInitialStatesRelevant(true);
        auto result = checker.checkLexObjectiveFormula(env, tasks[0]);
        ASSERT_TRUE(result->isLexicographicCheckResult());
        auto const& lexResult = result->asLexicographicCheckResult<double>().getInitialStateValue();
        EXPECT_NEAR(0.5, lexResult[0], storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
        EXPECT_NEAR(1, lexResult[1], storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
        EXPECT_NEAR(0, lexResult[2], storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
    }
}
