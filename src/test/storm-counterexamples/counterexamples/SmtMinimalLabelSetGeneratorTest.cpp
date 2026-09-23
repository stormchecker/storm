#include "storm-config.h"
#include "test/storm_gtest.h"

#include <optional>

#ifdef STORM_HAVE_Z3

#include "storm-counterexamples/api/counterexamples.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Model.h"

namespace {

/*!
 * Computes a minimal edge set counterexample for the given JANI file and property and returns the restricted model.
 */
std::optional<storm::jani::Model> computeJaniCounterexample(std::string const& janiFile, std::string const& formulaString) {
    storm::storage::SymbolicModelDescription modelDescription(storm::api::parseJaniModel(janiFile).first);
    auto properties = storm::api::parsePropertiesForSymbolicModelDescription(formulaString, modelDescription);
    auto formula = storm::api::extractFormulasFromProperties(properties).front();

    storm::builder::BuilderOptions options(*formula);
    options.setBuildChoiceOrigins(true);
    auto model = storm::api::buildSparseModel<double>(modelDescription, options);

    auto counterexample = storm::api::computeHighLevelCounterexampleMaxSmt(modelDescription, model, formula);
    auto highLevelCounterexample = std::dynamic_pointer_cast<storm::counterexamples::HighLevelCounterexample>(counterexample);
    if (highLevelCounterexample != nullptr && highLevelCounterexample->isJaniHighLevelCounterexample()) {
        return highLevelCounterexample->getModelDescription().asJaniModel();
    }
    ADD_FAILURE() << "Failed to compute a high-level counterexample for jani file " << janiFile << " and " << formulaString << ".";
    return std::nullopt;
}

uint64_t getNumberOfEdges(storm::jani::Model const& model) {
    uint64_t result = 0;
    for (auto const& automaton : model.getAutomata()) {
        result += automaton.getNumberOfEdges();
    }
    return result;
}

// The generator reasons symbolically about which edges can enable each other. Since the location of an automaton is
// not part of the variable valuation, that reasoning has to take the source and target locations of the edges into
// account. Models that keep their control flow in locations rather than in variables (as, e.g., models exported by
// Modest do) used to make the generator fail with a 'map::at: key not found' error.
TEST(SmtMinimalLabelSetGeneratorTest, DtmcWithMultipleLocations) {
    // The automaton moves from l0 either to l1 (probability 0.9) or to l2 (probability 0.1) and only reaches the
    // target state s=3 via l2. All guards are trivially true, so the location is the only thing distinguishing the
    // edges. The minimal counterexample consists of the initial edge and the edge in l2, but not the one in l1.
    auto result = computeJaniCounterexample(STORM_TEST_RESOURCES_DIR "/dtmc/locations_counterexample.jani", "P<0.05 [ F s=3 ]");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(1ull, result->getNumberOfAutomata());
    ASSERT_EQ(2ull, getNumberOfEdges(result.value()));

    storm::jani::Automaton const& automaton = result->getAutomaton(0);
    EXPECT_EQ(automaton.getLocationIndex("l0"), automaton.getEdge(0).getSourceLocationIndex());
    EXPECT_EQ(automaton.getLocationIndex("l2"), automaton.getEdge(1).getSourceLocationIndex());
}

TEST(SmtMinimalLabelSetGeneratorTest, MdpWithMultipleLocationsAndInitialLocations) {
    // Automaton A is as in the DTMC above, but its first edge synchronizes with automaton B, which has two initial
    // locations. As the property value is the maximum over the initial states, it suffices to keep the edge of B that
    // is enabled in one of the two initial locations. The edge of A in l1 is not needed.
    auto result = computeJaniCounterexample(STORM_TEST_RESOURCES_DIR "/mdp/locations_counterexample.jani", "P<0.5 [ F s=3 ]");
    ASSERT_TRUE(result.has_value());

    ASSERT_EQ(2ull, result->getNumberOfAutomata());
    EXPECT_EQ(2ull, result->getAutomaton("A").getNumberOfEdges());
    // Either edge of B yields a counterexample, so we only check how many of them are kept.
    EXPECT_EQ(1ull, result->getAutomaton("B").getNumberOfEdges());
}

}  // namespace

#endif
