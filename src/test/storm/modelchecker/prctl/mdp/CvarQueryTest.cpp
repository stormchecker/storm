#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/constants.h"

#include <set>

namespace {
template<typename ValueType>
std::shared_ptr<storm::models::sparse::Mdp<ValueType>> buildCvarModel(std::string const& modelPath, std::string const& propertyString, double alpha) {
    storm::prism::Program program = storm::api::parseProgram(modelPath);
    auto properties = storm::api::parsePropertiesForPrismProgram(propertyString, program);
    std::vector<storm::jani::Property> cvarProperties = {storm::api::createCvarProperty(properties.front(), alpha)};
    auto formulas = storm::api::extractFormulasFromProperties(cvarProperties);
    return storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
}

std::shared_ptr<storm::logic::Formula const> buildCvarFormula(std::string const& modelPath, std::string const& propertyString, double alpha) {
    auto properties = storm::api::parsePropertiesForPrismProgram(propertyString, storm::api::parseProgram(modelPath));
    return storm::api::createCvarProperty(properties.front(), alpha).getRawFormula();
}

template<typename ValueType>
ValueType checkInitialStateValue(std::shared_ptr<storm::models::sparse::Mdp<ValueType> > const& mdp,
                                 std::shared_ptr<storm::logic::Formula const> const& formula) {
    storm::Environment env;
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType> > checker(*mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);
    auto result = checker.check(env, task);
    return result->template asExplicitQuantitativeCheckResult<ValueType>().getMax();
}

TEST(CvarQueryTest, SimpleMdp) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif

    double alpha = 0.75;
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm";

    auto maxMdp = buildCvarModel<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    auto maxFormula = buildCvarFormula(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    double maxValue = checkInitialStateValue(maxMdp, maxFormula);
    EXPECT_NEAR(maxValue, 2.0, 1e-10);

    auto minMdp = buildCvarModel<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    auto minFormula = buildCvarFormula(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    double minValue = checkInitialStateValue(minMdp, minFormula);
    EXPECT_NEAR(minValue, 5.0 / 3.0, 1e-10);
}

TEST(CvarQueryTest, ReachableBadMecIsPreprocessedToZeroTerminalReward) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif

    double alpha = 0.5;
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_bad_mec_mdp.nm";

    auto maxMdp = buildCvarModel<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    auto maxFormula = buildCvarFormula(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    double maxValue = checkInitialStateValue(maxMdp, maxFormula);
    EXPECT_NEAR(maxValue, 0.0, 1e-10);

    auto minMdp = buildCvarModel<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    auto minFormula = buildCvarFormula(modelPath, "R{\"term\"}min=? [ F \"target\" ];", alpha);
    double minValue = checkInitialStateValue(minMdp, minFormula);
    EXPECT_NEAR(minValue, 0.0, 1e-10);
}

TEST(CvarQueryTest, RejectsNonAbsorbingOriginalTargetStates) {
    double alpha = 0.5;
    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_nonabsorbing_target_mdp.nm";

    auto mdp = buildCvarModel<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);
    auto formula = buildCvarFormula(modelPath, "R{\"term\"}max=? [ F \"target\" ];", alpha);

    storm::Environment env;
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> checker(*mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, double> task(*formula, true);
    STORM_SILENT_EXPECT_THROW(checker.check(env, task), storm::exceptions::InvalidPropertyException);
}

TEST(CvarQueryTest, BranchingTradeoffMdp) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif

    std::string modelPath = STORM_TEST_RESOURCES_DIR "/mdp/cvar_branching_tradeoff_mdp.nm";

    auto maxHalfMdp = buildCvarModel<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", 0.5);
    auto maxHalfFormula = buildCvarFormula(modelPath, "R{\"term\"}max=? [ F \"target\" ];", 0.5);
    EXPECT_NEAR(checkInitialStateValue(maxHalfMdp, maxHalfFormula), 7.0, 1e-10);

    auto maxThreeQuarterMdp = buildCvarModel<double>(modelPath, "R{\"term\"}max=? [ F \"target\" ];", 0.75);
    auto maxThreeQuarterFormula = buildCvarFormula(modelPath, "R{\"term\"}max=? [ F \"target\" ];", 0.75);
    EXPECT_NEAR(checkInitialStateValue(maxThreeQuarterMdp, maxThreeQuarterFormula), 8.0, 1e-10);

    auto minHalfMdp = buildCvarModel<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", 0.5);
    auto minHalfFormula = buildCvarFormula(modelPath, "R{\"term\"}min=? [ F \"target\" ];", 0.5);
    EXPECT_NEAR(checkInitialStateValue(minHalfMdp, minHalfFormula), 0.0, 1e-10);

    // this requires randomization of the strategy
    auto minThreeQuarterMdp = buildCvarModel<double>(modelPath, "R{\"term\"}min=? [ F \"target\" ];", 0.75);
    auto minThreeQuarterFormula = buildCvarFormula(modelPath, "R{\"term\"}min=? [ F \"target\" ];", 0.75);
    EXPECT_NEAR(checkInitialStateValue(minThreeQuarterMdp, minThreeQuarterFormula), 14.0 / 3.0, 1e-10);
}
} // namespace
