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
#include "storm/utility/solver.h"

namespace {

// The tags below select the SMT solver that the policy searches are tested with. The searches obtain their
// solver through SmtSolverFactory::create() without an environment, which always uses the compile-time
// default. Hence, a tag has to provide the concrete factory instead of an SMT solver type.
class DefaultEnvironment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::DefaultSmtSolverFactory>();
    }

    static std::string name() {
        return "Default";
    }

    static bool skip() {
#ifdef STORM_HAVE_SMT_SOLVER
        return false;
#else
        return true;
#endif
    }
};

#ifdef STORM_HAVE_MATHSAT
class MathsatEnvironment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();
    }

    static std::string name() {
        return "Mathsat";
    }

    static bool skip() {
        return false;
    }
};
#endif

#ifdef STORM_HAVE_Z3
class Z3Environment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
    }

    static std::string name() {
        return "Z3";
    }

    static bool skip() {
        return false;
    }
};
#endif

#ifdef STORM_HAVE_CVC5
class Cvc5Environment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::Cvc5SmtSolverFactory>();
    }

    static std::string name() {
        return "Cvc5";
    }

    static bool skip() {
        return false;
    }
};
#endif

// Puts the SMT solver into the test name, so that a failure identifies the backend that produced it.
class SmtSolverNameGenerator {
   public:
    template<typename ParamType>
    static std::string GetName(int) {
        return ParamType::name();
    }
};

}  // namespace

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

void oneshot_test(std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory, std::string const& path, std::string const& constants,
                  std::string formulaString, uint64_t lookahead) {
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
    storm::pomdp::OneShotPolicySearch<double> memlessSearch(*pomdp, targetStates, surelyNotAlmostSurelyReachTarget, smtSolverFactory);
    memlessSearch.analyzeForInitialStates(lookahead);
}

void iterativesearch_test(std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory, std::string const& path, std::string const& constants,
                          std::string formulaString, bool wr) {
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
    janicreator.verifySymbolic(storm::Environment(), initialOnly);
}

// The tests that do not use an SMT solver.
class QualitativeAnalysis : public ::testing::Test {
   public:
    void SetUp() override {
#ifndef STORM_HAVE_SMT_SOLVER
        GTEST_SKIP() << "No SMT solver available.";
#endif
    }
};

// The policy searches, which are tested once per available SMT solver.
template<typename TestType>
class QualitativeAnalysisSmt : public ::testing::Test {
   public:
    void SetUp() override {
        if (skipped()) {
            GTEST_SKIP() << "No SMT solver available.";
        }
    }

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory() const {
        return TestType::factory();
    }

    bool skipped() const {
        return TestType::skip();
    }
};

typedef ::testing::Types<DefaultEnvironment
#ifdef STORM_HAVE_MATHSAT
                         ,
                         MathsatEnvironment
#endif
#ifdef STORM_HAVE_Z3
                         ,
                         Z3Environment
#endif
#ifdef STORM_HAVE_CVC5
                         ,
                         Cvc5Environment
#endif
                         >
    QualitativeAnalysisSmtTypes;

TYPED_TEST_SUITE(QualitativeAnalysisSmt, QualitativeAnalysisSmtTypes, SmtSolverNameGenerator);

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

TYPED_TEST(QualitativeAnalysisSmt, OneShot_Simple) {
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", 5);
}

TYPED_TEST(QualitativeAnalysisSmt, OneShots_Maze) {
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", 5);
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", 30);
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", 30);
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", 5);
    oneshot_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", 5);
}

TYPED_TEST(QualitativeAnalysisSmt, Iterative_Simple) {
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", false);

    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.4", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/simple.prism", "slippery=0.0", "Pmax=? [F \"goal\" ]", true);
}

TYPED_TEST(QualitativeAnalysisSmt, Iterative_Maze) {
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", false);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", false);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", false);

    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [F \"goal\" ]", true);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.4", "Pmax=? [!\"bad\" U \"goal\" ]", true);
    iterativesearch_test(this->smtSolverFactory(), STORM_TEST_RESOURCES_DIR "/pomdp/maze2.prism", "sl=0.0", "Pmax=? [!\"bad\" U \"goal\"]", true);
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
