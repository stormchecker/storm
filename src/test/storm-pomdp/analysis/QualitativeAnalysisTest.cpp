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
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/transformer/MakePOMDPCanonic.h"

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

void oneshot_test(std::string const& path, std::string const& constants, std::string formulaString, uint64_t lookahead) {
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
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
    storm::pomdp::OneShotPolicySearch<double> memlessSearch(*pomdp, targetStates, surelyNotAlmostSurelyReachTarget, smtSolverFactory);
    memlessSearch.analyzeForInitialStates(lookahead);
}

void iterativesearch_test(std::string const& path, std::string const& constants, std::string formulaString, bool wr) {
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

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
    storm::pomdp::MemlessSearchOptions options;
    uint64_t lookahead = pomdp->getNumberOfStates();
    storm::pomdp::IterativePolicySearch<double> search(*pomdp, targetStates, surelyNotAlmostSurelyReachTarget, smtSolverFactory, options);
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
    janicreator.verifySymbolic(initialOnly);
}

class QualitativeAnalysis : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

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

TEST_F(QualitativeAnalysis, OneShot_Simple) {
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", 5);
}

TEST_F(QualitativeAnalysis, OneShots_Maze) {
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", 30);
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", 30);
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", 5);
    oneshot_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", 5);
}

TEST_F(QualitativeAnalysis, Iterative_Simple) {
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", false);

    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", true);
}

TEST_F(QualitativeAnalysis, Iterative_Maze) {
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", false);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", false);

    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", true);
    iterativesearch_test(STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", true);
}

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
