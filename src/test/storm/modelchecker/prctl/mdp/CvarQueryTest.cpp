#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/Environment.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/constants.h"

namespace {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Mdp<ValueType>> buildCvarModel(std::string const& propertyString, double alpha) {
    storm::prism::Program program = storm::api::parseProgram(STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm");
    auto properties = storm::api::parsePropertiesForPrismProgram(propertyString, program);
    std::vector<storm::jani::Property> cvarProperties = {storm::api::createCvarProperty(properties.front(), alpha)};
    auto formulas = storm::api::extractFormulasFromProperties(cvarProperties);
    return storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
}

template<typename ValueType>
ValueType checkInitialStateValue(std::shared_ptr<storm::models::sparse::Mdp<ValueType>> const& mdp,
                                 std::shared_ptr<storm::logic::Formula const> const& formula) {
    storm::Environment env;
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> task(*formula, true);
    auto result = checker.check(env, task);
    return result->template asExplicitQuantitativeCheckResult<ValueType>().getMax();
}

TEST(CvarQueryTest, SimpleMdp) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif

    double alpha = 0.75;

    auto maxMdp = buildCvarModel<double>("R{\"term\"}max=? [ F \"target\" ];", alpha);
    auto maxProperties = storm::api::parsePropertiesForPrismProgram("R{\"term\"}max=? [ F \"target\" ];",
                                                                    storm::api::parseProgram(STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm"));
    auto maxFormula = storm::api::createCvarProperty(maxProperties.front(), alpha).getRawFormula();
    double maxValue = checkInitialStateValue(maxMdp, maxFormula);
    EXPECT_NEAR(maxValue, 2.0, 1e-10);

    auto minMdp = buildCvarModel<double>("R{\"term\"}min=? [ F \"target\" ];", alpha);
    auto minProperties = storm::api::parsePropertiesForPrismProgram("R{\"term\"}min=? [ F \"target\" ];",
                                                                    storm::api::parseProgram(STORM_TEST_RESOURCES_DIR "/mdp/cvar_simple_mdp.nm"));
    auto minFormula = storm::api::createCvarProperty(minProperties.front(), alpha).getRawFormula();
    double minValue = checkInitialStateValue(minMdp, minFormula);
    EXPECT_NEAR(minValue, 5.0 / 3.0, 1e-10);
}

}  // namespace
