#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/parser/JaniParser.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ModelType.h"
#include "storm/storage/jani/Property.h"

TEST(JaniParser, DieExampleTest) {
    std::string testInput = R"({
	"jani-version": 1,
	"name": "die.jani",
	"type": "dtmc",
	"features": [ "derived-operators" ],
	"variables": [
		{
			"name": "s",
			"type": {
				"base": "int",
				"kind": "bounded",
				"lower-bound": 0,
				"upper-bound": 7
			},
			"initial-value": 0
		},
		{
			"name": "d",
			"type": {
				"base": "int",
				"kind": "bounded",
				"lower-bound": 0,
				"upper-bound": 6
			},
			"initial-value": 0
		}
	],
	"properties": [
		{
			"name": "Probability to throw a six",
			"expression": {
				"op": "filter",
				"fun": "max",
				"states": { "op": "initial" },
				"values": {
					"op": "Pmin",
					"exp": {
						"op": "U",
						"left": true,
						"right": {
							"op": "∧",
							"left": {
								"op": "=",
								"left": "s",
								"right": 7
							},
							"right": {
								"op": "=",
								"left": "d",
								"right": 6
							}
						}
					}
				}
			}
		},
		{
			"name": "Expected number of coin flips",
			"expression": {
				"op": "filter",
				"fun": "max",
				"states": { "op": "initial" },
				"values": {
					"op": "Emin",
					"accumulate": [ "steps" ],
					"exp": 1,
					"reach": {
						"op": "=",
						"left": "s",
						"right": 7
					}
				}
			}
		}
	],
	"automata": [
		{
			"name": "die",
			"locations": [{ "name": "l" }],
			"initial-locations": ["l"],
			"edges": [
				{
					"location": "l",
					"guard": {
						"exp": {
							"op": "=",
							"left": "s",
							"right": 0
						}
					},
					"destinations": [
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 1
								}
							]
						},
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 2
								}
							]
						}
					]
				},
				{
					"location": "l",
					"guard": {
						"exp": {
							"left": "s",
							"op": "=",
							"right": 1
						}
					},
					"destinations": [
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 3
								}
							]
						},
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 4
								}
							]
						}
					]
				},
				{
					"location": "l",
					"guard": {
						"exp": {
							"left": "s",
							"op": "=",
							"right": 2
						}
					},
					"destinations": [
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 5
								}
							]
						},
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 6
								}
							]
						}
					]
				},
				{
					"location": "l",
					"guard": {
						"exp": {
							"left": "s",
							"op": "=",
							"right": 3
						}
					},
					"destinations": [
						{
							"location": "l",
							"probability": { "exp": 0.5 }, 
							"assignments": [
								{
									"ref": "s",
									"value": 1
								}
							]
						},
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 7
								},
								{
									"ref": "d",
									"value": 1
								}
							]
						}
					]
				},
				{
					"location": "l",
					"guard": {
						"exp": {
							"left": "s",
							"op": "=",
							"right": 4
						}
					},
					"destinations": [
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 7
								},
								{
									"ref": "d",
									"value": 2
								}
							]
						},
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 7
								},
								{
									"ref": "d",
									"value": 3
								}
							]
						}
					]
				},
				{
					"location": "l",
					"guard": {
						"exp": {
							"left": "s",
							"op": "=",
							"right": 5
						}
					},
					"destinations": [
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 7
								},
								{
									"ref": "d",
									"value": 4
								}
							]
						},
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 7
								},
								{
									"ref": "d",
									"value": 5
								}
							]
						}
					]
				},
				{
					"location": "l",
					"guard": {
						"exp": {
							"left": "s",
							"op": "=",
							"right": 6
						}
					},
					"destinations": [
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 2
								}
							]
						},
						{
							"location": "l",
							"probability": { "exp": 0.5 },
							"assignments": [
								{
									"ref": "s",
									"value": 7
								},
								{
									"ref": "d",
									"value": 6
								}
							]
						}
					]
				},
				{
					"location": "l",
					"guard": {
						"exp": {
							"left": "s",
							"op": "=",
							"right": 7
						}
					},
					"destinations": [
						{
							"location": "l",
							"assignments": [
								{
									"ref": "s",
									"value": 7
								}
							]
						}
					]
				}
			]
			
		}
	], 
	"system": {
		"elements": [ { "automaton": "die" } ]
	}
    })";

    std::pair<storm::jani::Model, std::vector<storm::jani::Property>> result;
    EXPECT_NO_THROW(result = storm::api::parseJaniModelFromString(testInput));
    EXPECT_EQ(storm::jani::ModelType::DTMC, result.first.getModelType());
    EXPECT_TRUE(result.first.hasGlobalVariable("s"));
    EXPECT_EQ(1ul, result.first.getNumberOfAutomata());
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
