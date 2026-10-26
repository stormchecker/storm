#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/analysis/IterativePolicySearch.h"
#include "storm-pomdp/analysis/JaniBeliefSupportMdpGenerator.h"
#include "storm-pomdp/analysis/OneShotPolicySearch.h"
#include "storm-pomdp/analysis/QualitativeAnalysisOnGraphs.h"
#include "storm/api/storm.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/environment/Environment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/transformer/MakePOMDPCanonic.h"
#include "test/SmtSolverTestUtils.h"

void graphalgorithm_test(std::string const& path, std::string const& constants, std::string formulaString) {
    storm::prism::Program program = storm::parser::PrismParser::parse(path);
    program = program.preprocess(constants);
    std::shared_ptr<storm::logic::Formula const> formula = storm::api::parsePropertiesForPrismProgram(formulaString, program).front().getRawFormula();
    std::shared_ptr<storm::models::sparse::Pomdp<double>> pomdp =
        storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();
    storm::transformer::MakePOMDPCanonic<double> makeCanonic(*pomdp);
    pomdp = makeCanonic.transform();

    // Run graph algorithm
    auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(*pomdp, *formula);
    storm::analysis::QualitativeAnalysisOnGraphs<double> qualitativeAnalysis(*pomdp);
    storm::storage::BitVector surelyNotAlmostSurelyReachTarget = qualitativeAnalysis.analyseProbSmaller1(formula->asProbabilityOperatorFormula());
    pomdp->getTransitionMatrix().makeRowGroupsAbsorbing(surelyNotAlmostSurelyReachTarget);
    storm::storage::BitVector targetStates = qualitativeAnalysis.analyseProb1(formula->asProbabilityOperatorFormula());
}

void oneshot_test(storm::solver::SmtSolverType smtSolverType, std::string const& path, std::string const& constants, std::string formulaString,
                  uint64_t lookahead) {
    storm::prism::Program program = storm::parser::PrismParser::parse(path);
    program = program.preprocess(constants);
    std::shared_ptr<storm::logic::Formula const> formula = storm::api::parsePropertiesForPrismProgram(formulaString, program).front().getRawFormula();
    std::shared_ptr<storm::models::sparse::Pomdp<double>> pomdp =
        storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();
    storm::transformer::MakePOMDPCanonic<double> makeCanonic(*pomdp);
    pomdp = makeCanonic.transform();

    // Run graph algorithm
    auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(*pomdp, *formula);
    storm::analysis::QualitativeAnalysisOnGraphs<double> qualitativeAnalysis(*pomdp);
    storm::storage::BitVector surelyNotAlmostSurelyReachTarget = qualitativeAnalysis.analyseProbSmaller1(formula->asProbabilityOperatorFormula());
    pomdp->getTransitionMatrix().makeRowGroupsAbsorbing(surelyNotAlmostSurelyReachTarget);
    storm::storage::BitVector targetStates = qualitativeAnalysis.analyseProb1(formula->asProbabilityOperatorFormula());
    storm::Environment env;
    env.solver().setSmtSolverType(smtSolverType);
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::SmtSolverFactory>();
    storm::pomdp::OneShotPolicySearch<double> memlessSearch(env, *pomdp, targetStates, surelyNotAlmostSurelyReachTarget, smtSolverFactory);
    memlessSearch.analyzeForInitialStates(lookahead);
}

void iterativesearch_test(storm::solver::SmtSolverType smtSolverType, std::string const& path, std::string const& constants, std::string formulaString,
                          bool wr) {
    storm::prism::Program program = storm::parser::PrismParser::parse(path);
    program = program.preprocess(constants);
    std::shared_ptr<storm::logic::Formula const> formula = storm::api::parsePropertiesForPrismProgram(formulaString, program).front().getRawFormula();
    std::shared_ptr<storm::models::sparse::Pomdp<double>> pomdp =
        storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();
    storm::transformer::MakePOMDPCanonic<double> makeCanonic(*pomdp);
    pomdp = makeCanonic.transform();

    // Run graph algorithm
    auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(*pomdp, *formula);
    storm::analysis::QualitativeAnalysisOnGraphs<double> qualitativeAnalysis(*pomdp);
    storm::storage::BitVector surelyNotAlmostSurelyReachTarget = qualitativeAnalysis.analyseProbSmaller1(formula->asProbabilityOperatorFormula());
    pomdp->getTransitionMatrix().makeRowGroupsAbsorbing(surelyNotAlmostSurelyReachTarget);
    storm::storage::BitVector targetStates = qualitativeAnalysis.analyseProb1(formula->asProbabilityOperatorFormula());

    storm::Environment env;
    env.solver().setSmtSolverType(smtSolverType);
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::SmtSolverFactory>();
    storm::pomdp::MemlessSearchOptions options;
    uint64_t lookahead = pomdp->getNumberOfStates();
    storm::pomdp::IterativePolicySearch<double> search(env, *pomdp, targetStates, surelyNotAlmostSurelyReachTarget, smtSolverFactory, options);
    if (wr) {
        search.computeWinningRegion(lookahead);
    } else {
        search.analyzeForInitialStates(lookahead);
    }
}

void symbolicbelsup_test(std::string const& path, std::string const& constants, std::string formulaString, bool wr) {
    storm::prism::Program program = storm::parser::PrismParser::parse(path);
    program = program.preprocess(constants);
    std::shared_ptr<storm::logic::Formula const> formula = storm::api::parsePropertiesForPrismProgram(formulaString, program).front().getRawFormula();
    std::shared_ptr<storm::models::sparse::Pomdp<double>> pomdp =
        storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();
    storm::transformer::MakePOMDPCanonic<double> makeCanonic(*pomdp);
    pomdp = makeCanonic.transform();

    // Run graph algorithm
    auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(*pomdp, *formula);
    storm::analysis::QualitativeAnalysisOnGraphs<double> qualitativeAnalysis(*pomdp);
    storm::storage::BitVector surelyNotAlmostSurelyReachTarget = qualitativeAnalysis.analyseProbSmaller1(formula->asProbabilityOperatorFormula());
    pomdp->getTransitionMatrix().makeRowGroupsAbsorbing(surelyNotAlmostSurelyReachTarget);
    storm::storage::BitVector targetStates = qualitativeAnalysis.analyseProb1(formula->asProbabilityOperatorFormula());

    storm::pomdp::qualitative::JaniBeliefSupportMdpGenerator<double> janicreator(*pomdp);
    janicreator.generate(targetStates, surelyNotAlmostSurelyReachTarget);
    bool initialOnly = !wr;
    janicreator.verifySymbolic(storm::Environment(), initialOnly);
}

class QualitativeAnalysis : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_SMT_SOLVER
        GTEST_SKIP() << "No SMT solver available.";
#endif
    }
};

// The policy searches, which are tested once per available SMT solver.
class QualitativeAnalysisSmt : public ::testing::TestWithParam<storm::solver::SmtSolverType> {};

namespace {

class RecordingSmtSolverFactory : public storm::utility::solver::Z3SmtSolverFactory {
   public:
    std::unique_ptr<storm::solver::SmtSolver> create(storm::expressions::ExpressionManager& manager) const override {
        ADD_FAILURE() << "Policy search must pass its environment to the SMT factory.";
        return Z3SmtSolverFactory::create(manager);
    }

    std::unique_ptr<storm::solver::SmtSolver> create(storm::Environment const& env, storm::expressions::ExpressionManager& manager) const override {
        environment = &env;
        return Z3SmtSolverFactory::create(env, manager);
    }

    mutable storm::Environment const* environment = nullptr;
};

}  // namespace

TEST_F(QualitativeAnalysis, PolicySearchUsesEnvironment) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/pomdp/simple_unit_rewards.prism").preprocess("slippery=0");
    auto formula = storm::api::parsePropertiesForPrismProgram("Pmax=? [F \"goal\"]", program).front().getRawFormula();
    auto pomdp = storm::api::buildSparseModel<double>(program, {formula})->as<storm::models::sparse::Pomdp<double>>();
    pomdp = storm::transformer::MakePOMDPCanonic<double>(*pomdp).transform();
    storm::analysis::QualitativeAnalysisOnGraphs<double> qualitativeAnalysis(*pomdp);
    auto sinkStates = qualitativeAnalysis.analyseProbSmaller1(formula->asProbabilityOperatorFormula());
    pomdp->getTransitionMatrix().makeRowGroupsAbsorbing(sinkStates);
    auto targetStates = qualitativeAnalysis.analyseProb1(formula->asProbabilityOperatorFormula());

    storm::Environment env;
    env.solver().setSmtSolverType(storm::solver::SmtSolverType::Mathsat);
    // An explicit factory can still choose Z3, even when the supplied environment selects another backend.
    for (bool iterative : {false, true}) {
        SCOPED_TRACE(iterative ? "iterative" : "one-shot");
        auto recordingFactory = std::make_shared<RecordingSmtSolverFactory>();
        std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory = recordingFactory;
        if (iterative) {
            storm::pomdp::IterativePolicySearch<double> search(env, *pomdp, targetStates, sinkStates, factory, storm::pomdp::MemlessSearchOptions());
            EXPECT_FALSE(search.analyzeForInitialStates(pomdp->getNumberOfStates()));
        } else {
            storm::pomdp::OneShotPolicySearch<double> search(env, *pomdp, targetStates, sinkStates, factory);
            EXPECT_FALSE(search.analyzeForInitialStates(pomdp->getNumberOfStates()));
        }
        EXPECT_EQ(recordingFactory->environment, &env);
    }
}

TEST_F(QualitativeAnalysis, GraphAlgorithm_Simple) {
    graphalgorithm_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]");
    graphalgorithm_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]");
}

TEST_F(QualitativeAnalysis, GraphAlgorithm_Maze) {
    graphalgorithm_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]");
    graphalgorithm_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]");
    graphalgorithm_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]");
    graphalgorithm_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]");
}

TEST_P(QualitativeAnalysisSmt, OneShot_Simple) {
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", 5);
}

TEST_P(QualitativeAnalysisSmt, OneShots_Maze) {
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", 30);
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", 30);
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", 5);
    oneshot_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", 5);
}

TEST_P(QualitativeAnalysisSmt, Iterative_Simple) {
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", false);

    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", true);
}

TEST_P(QualitativeAnalysisSmt, Iterative_Maze) {
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", false);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", false);

    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", true);
    iterativesearch_test(GetParam(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", true);
}

INSTANTIATE_TEST_SUITE_P(SmtSolvers, QualitativeAnalysisSmt, ::testing::ValuesIn(storm::test::smtSolversUnderTest()), storm::test::smtSolverName);
// The suite is empty if no SMT solver was compiled in.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QualitativeAnalysisSmt);

TEST_F(QualitativeAnalysis, SymbolicBelSup_Simple) {
#ifdef STORM_HAVE_SYLVAN
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", false);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", false);

    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", true);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", true);
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}

TEST_F(QualitativeAnalysis, SymbolicBelSup_Maze) {
#ifdef STORM_HAVE_SYLVAN
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", false);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", false);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", false);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", false);

    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", true);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", true);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", true);
    symbolicbelsup_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", true);
#else
    GTEST_SKIP() << "Library Sylvan not available.";
#endif
}
