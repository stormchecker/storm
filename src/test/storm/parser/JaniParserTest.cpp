#include "storm-config.h"

#include <fstream>
#include <iterator>
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/parser/JaniParser.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/io/file.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ModelType.h"
#include "storm/storage/jani/Property.h"

TEST(JaniParser, DieExampleTest) {
    std::ifstream file;
    storm::io::openFile(STORM_TEST_RESOURCES_DIR "/dtmc/die_janiproperties.jani", file);
    std::string const testInput{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    ASSERT_FALSE(testInput.empty());

    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModelFromString(testInput));
    EXPECT_EQ(storm::jani::ModelType::DTMC, result.first.getModelType());
    EXPECT_TRUE(result.first.hasGlobalVariable("s"));
    EXPECT_EQ(1ul, result.first.getNumberOfAutomata());

    ASSERT_EQ(12ul, result.second.size());

    // Parse from file vs parse from string
    EXPECT_EQ(storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/die_janiproperties.jani").second.size(), result.second.size());

    auto const& conjunction = result.second[2].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    ASSERT_TRUE(conjunction.isBinaryBooleanPathFormula());

    auto const& disjunction = result.second[3].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    ASSERT_TRUE(disjunction.isBinaryBooleanPathFormula());

    auto const& negation = result.second[4].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    ASSERT_TRUE(negation.isUnaryBooleanPathFormula());

    auto const& implication = result.second[5].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    ASSERT_TRUE(implication.isBinaryBooleanPathFormula());

    auto const& stateConjunction = *result.second[6].getRawFormula();
    ASSERT_TRUE(stateConjunction.isBinaryBooleanStateFormula());

    auto const& weakUntil = result.second[7].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    ASSERT_TRUE(weakUntil.isWeakUntilFormula());

    auto const& release = result.second[8].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    ASSERT_TRUE(release.isReleaseFormula());
}

TEST(JaniParser, DieExampleFragmentTest) {
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/die_janiproperties.jani"));
    ASSERT_EQ(12ul, result.second.size());

    auto const& weakUntil = result.second[7].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    auto const& release = result.second[8].getRawFormula()->asProbabilityOperatorFormula().getSubformula();
    auto const& until = result.second[9].getRawFormula()->asProbabilityOperatorFormula().getSubformula();

    // Weak until
    ASSERT_TRUE(weakUntil.isWeakUntilFormula());
    EXPECT_FALSE(weakUntil.isInFragment(storm::logic::pctl()));
    EXPECT_TRUE(weakUntil.isInFragment(storm::logic::pctlstar()));

    // Release
    ASSERT_TRUE(release.isReleaseFormula());
    EXPECT_FALSE(release.isInFragment(storm::logic::pctl()));
    EXPECT_TRUE(release.isInFragment(storm::logic::pctlstar()));

    // Until
    ASSERT_TRUE(until.isUntilFormula());
    EXPECT_TRUE(until.isInFragment(storm::logic::pctl()));
    EXPECT_TRUE(until.isInFragment(storm::logic::pctlstar()));
}

TEST(JaniParser, DieArrayExampleTest) {
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/die_array.jani"));
    EXPECT_EQ(storm::jani::ModelType::DTMC, result.first.getModelType());
    EXPECT_TRUE(result.first.containsArrayVariables());
    EXPECT_TRUE(result.first.hasGlobalVariable("sd"));
    EXPECT_EQ(1ul, result.first.getNumberOfAutomata());
}

TEST(JaniParser, FTWCTest) {
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/ma/ftwc.jani"));
    EXPECT_EQ(storm::jani::ModelType::MA, result.first.getModelType());
    EXPECT_TRUE(result.first.containsArrayVariables());
    EXPECT_TRUE(result.first.hasGlobalVariable("workstations_up"));
    EXPECT_TRUE(result.first.hasGlobalVariable("workstations_up"));
    EXPECT_EQ(6ul, result.first.getNumberOfAutomata());
    EXPECT_TRUE(result.first.getAutomaton("Switch").hasVariable("id"));
    EXPECT_TRUE(result.first.getAutomaton("Switch_1").hasVariable("id"));
    EXPECT_TRUE(result.first.getAutomaton("Workstation").hasVariable("id"));
    EXPECT_TRUE(result.first.getAutomaton("Workstation_1").hasVariable("id"));
    EXPECT_TRUE(result.first.getAutomaton(1).hasVariable("id"));
}

TEST(JaniParser, DieArrayNestedExampleTest) {
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/die_array_nested.jani"));
    EXPECT_EQ(storm::jani::ModelType::DTMC, result.first.getModelType());
    EXPECT_TRUE(result.first.containsArrayVariables());
    EXPECT_TRUE(result.first.hasGlobalVariable("sd"));
    EXPECT_EQ(1ul, result.first.getNumberOfAutomata());
}

TEST(JaniParser, UnassignedVariablesTest) {
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/mdp/unassigned-variables.jani"));
    EXPECT_EQ(storm::jani::ModelType::MDP, result.first.getModelType());
    EXPECT_TRUE(result.first.hasConstant("c"));
    EXPECT_EQ(2ul, result.first.getNumberOfAutomata());
}

TEST(JaniParser, TrigonometryAndTranscendentalNumbersTest) {
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/dtmc/test_trigonometry.jani"));
    auto& model = result.first;
    auto& properties = result.second;
    EXPECT_EQ(storm::jani::ModelType::DTMC, model.getModelType());
    EXPECT_EQ(model.getNumberOfAutomata(), 1U);
    EXPECT_EQ(properties.size(), 2U);
    EXPECT_NO_THROW(model.substitute({}, true));
}

TEST(JaniParser, MultiobjectiveTest) {
    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModel(STORM_TEST_RESOURCES_DIR "/mdp/multiobj_consensus2_3_2.jani"));
    auto& model = result.first;
    EXPECT_EQ(storm::jani::ModelType::MDP, model.getModelType());
    auto& properties = result.second;
    ASSERT_EQ(properties.size(), 6U);
    for (size_t i = 0; i < properties.size(); ++i) {
        auto const& f = properties[i].getRawFormula();
        ASSERT_TRUE(f->isMultiObjectiveFormula()) << "Property #" << i << ": " << *f;
        // First three formulas are tradeoff, last three are lexicographic
        EXPECT_EQ(i < 3, f->asMultiObjectiveFormula().isTradeoff()) << "Property #" << i << ": " << *f;
        EXPECT_EQ(i >= 3, f->asMultiObjectiveFormula().isLexicographic()) << "Property #" << i << ": " << *f;
        // Formulas #0 and #3 have Boolean results
        EXPECT_EQ(i == 0 || i == 3, f->asMultiObjectiveFormula().hasQualitativeResult()) << "Property #" << i << ": " << *f;
        // Formulas #2 and #5 have multi-dimensional results
        EXPECT_EQ(i == 2 || i == 5, f->asMultiObjectiveFormula().hasMultiDimensionalResult()) << "Property #" << i << ": " << *f;
    }
}
