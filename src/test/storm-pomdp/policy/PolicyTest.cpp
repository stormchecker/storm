#include "storm-config.h"
#include "test/storm_gtest.h"

#include <sstream>
#include <string>

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-pomdp/beliefs/policy/ApplyObservationBasedFSCToPomdp.h"
#include "storm-pomdp/beliefs/policy/ObservationBasedFiniteStateController.h"
#include "storm-pomdp/beliefs/policy/PolicyExtractor.h"
#include "storm-pomdp/transformer/MakeStateSetObservationClosed.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/scheduler/Scheduler.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/storage/valuations/ValuationDescriptionBuilder.h"
#include "storm/storage/valuations/Valuations.h"
#include "storm/storage/valuations/ValuationsStorage.h"
#include "storm/transformer/MakePOMDPCanonic.h"

namespace {

storm::models::sparse::Pomdp<double> buildSingleStatePomdp(bool withRewards = false, bool withChoiceLabels = false) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 0, 1.0);
    matrixBuilder.addNextValue(1, 0, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(1);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.observabilityClasses = std::vector<uint32_t>{0};
    if (withChoiceLabels) {
        storm::models::sparse::ChoiceLabeling choiceLabeling(2);
        choiceLabeling.addLabel("first");
        choiceLabeling.addLabelToChoice("first", 0);
        choiceLabeling.addLabel("second");
        choiceLabeling.addLabelToChoice("second", 1);
        components.choiceLabeling = std::move(choiceLabeling);
    }
    if (withRewards) {
        components.rewardModels.emplace("rew", storm::models::sparse::StandardRewardModel<double>(std::vector<double>{3.0}, std::vector<double>{2.0, 6.0}));
    }
    return storm::models::sparse::Pomdp<double>(std::move(components), true);
}

storm::models::sparse::Pomdp<double> buildSingleStatePomdpWithObservationValuations(bool reverseObservableOrder) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 0, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(1);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);

    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto const enabled = manager->declareBooleanVariable("enabled");
    auto const mode = manager->declareIntegerVariable("mode");
    storm::storage::sparse::ValuationDescriptionBuilder valuationBuilder(manager);
    if (reverseObservableOrder) {
        valuationBuilder.addIntegerVariable(mode, 0, 1);
        valuationBuilder.addBooleanVariable(enabled);
    } else {
        valuationBuilder.addBooleanVariable(enabled);
        valuationBuilder.addIntegerVariable(mode, 0, 1);
    }
    storm::storage::sparse::Valuations observationValuations(valuationBuilder.buildClassDescription(), manager);
    observationValuations.getStorage().resize(1);
    observationValuations.getStorage().writeValue(0, enabled, true);
    observationValuations.getStorage().writeValue(0, mode, int64_t{1});

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.observabilityClasses = std::vector<uint32_t>{0};
    components.observationValuations = std::move(observationValuations);
    return storm::models::sparse::Pomdp<double>(std::move(components), true);
}

storm::models::sparse::Mdp<double> buildBeliefMdpWithReorderedCutoffChoices(bool truncatedInitialState = false) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, 1.0);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 2, 1.0);
    matrixBuilder.addNextValue(2, 2, 1.0);
    matrixBuilder.newRowGroup(3);
    matrixBuilder.addNextValue(3, 2, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(3);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", truncatedInitialState ? 1 : 0);
    stateLabeling.addLabel("truncated");
    stateLabeling.addLabelToState("truncated", 1);

    storm::models::sparse::ChoiceLabeling choiceLabeling(4);
    choiceLabeling.addLabel("__sched_1");
    choiceLabeling.addLabelToChoice("__sched_1", 1);
    choiceLabeling.addLabel("__sched_0");
    choiceLabeling.addLabelToChoice("__sched_0", 2);

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.choiceLabeling = std::move(choiceLabeling);
    return storm::models::sparse::Mdp<double>(std::move(components));
}

storm::models::sparse::Mdp<double> buildSingleStateBeliefMdpWithUnlabelledSecondChoice() {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 0, 1.0);
    matrixBuilder.addNextValue(1, 0, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(1);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);
    stateLabeling.addLabel("truncated");

    storm::models::sparse::ChoiceLabeling choiceLabeling(2);
    choiceLabeling.addLabel("first");
    choiceLabeling.addLabelToChoice("first", 0);

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.choiceLabeling = std::move(choiceLabeling);
    return storm::models::sparse::Mdp<double>(std::move(components));
}

}  // namespace

TEST(ObservationBasedFiniteStateController, DeterminismTracksReplacedOutputs) {
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0);
    storm::storage::Distribution<double, uint64_t> distribution;
    distribution.addProbability(0, 0.5);
    distribution.addProbability(1, 0.5);

    fsc.addRandomisedActionTransition(0, 0, distribution, 0);
    EXPECT_FALSE(fsc.isDeterministic());

    fsc.addDeterministicActionTransition(0, 0, 1, 0);
    EXPECT_TRUE(fsc.isDeterministic());
}

TEST(ObservationBasedFiniteStateController, FormatsPartialSignatureMaps) {
    storm::pomdp::policy::ObservationSignature observationSignature;
    observationSignature.values.emplace("known-observable", std::string{"known-value"});
    std::optional observationSignatures{std::unordered_map<uint64_t, storm::pomdp::policy::ObservationSignature>{{0, observationSignature}}};
    std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{0, "known-action"}}}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0, observationSignatures, actionNames);
    fsc.addDeterministicActionTransition(0, 0, 0, 1);

    storm::storage::Distribution<double, uint64_t> distribution;
    distribution.addProbability(1, 1.0);
    fsc.addRandomisedActionTransition(1, 1, distribution, 0);

    auto const description = fsc.toString();
    EXPECT_NE(description.find("(0)[known-observable=known-value]"), std::string::npos);
    EXPECT_NE(description.find("1 ->"), std::string::npos);
    EXPECT_NE(description.find("(1) :"), std::string::npos);

    std::stringstream jsonOutput;
    fsc.writeJsonToStream(jsonOutput);
    auto const json = storm::json<double>::parse(jsonOutput.str());
    auto const& observable = json.at("nodes").at(0).at("transitions").at(0).at("observation").at("signature").at(0);
    EXPECT_EQ(observable.at("name").get<std::string>(), "known-observable");
    EXPECT_EQ(observable.at("type").get<std::string>(), "string");
    EXPECT_EQ(observable.at("value").get<std::string>(), "known-value");
}

TEST(ObservationBasedFiniteStateController, EscapesSignaturesAndActionNamesInDotOutput) {
    storm::pomdp::policy::ObservationSignature observationSignature;
    observationSignature.values.emplace("observation", std::string{"value\"\\\n"});
    std::optional observationSignatures{std::unordered_map<uint64_t, storm::pomdp::policy::ObservationSignature>{{0, observationSignature}}};
    std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{0, "action\"\\\n"}}}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0, observationSignatures, actionNames);
    fsc.addDeterministicActionTransition(0, 0, 0, 0);

    std::stringstream dotOutput;
    fsc.writeDotToStream(dotOutput);

    auto const dot = dotOutput.str();
    EXPECT_NE(dot.find("observation=value\\\"\\\\\\n"), std::string::npos);
    EXPECT_NE(dot.find("action\\\"\\\\\\n"), std::string::npos);
}

TEST(ApplyObservationBasedFiniteStateController, RejectsMultipleInitialStates) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 0, 1.0);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 1, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(2);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);
    stateLabeling.addLabelToState("init", 1);

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.observabilityClasses = std::vector<uint32_t>{0, 0};
    storm::models::sparse::Pomdp<double> pomdp(std::move(components), true);
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0);
    fsc.addDeterministicActionTransition(0, 0, 0, 0);

    EXPECT_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc), storm::exceptions::NotSupportedException);
    EXPECT_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc, true), storm::exceptions::NotSupportedException);
}

TEST(ApplyObservationBasedFiniteStateController, RejectsNoncanonicActionOrdering) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, 1.0);
    matrixBuilder.addNextValue(1, 0, 1.0);
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 0, 1.0);
    matrixBuilder.addNextValue(3, 1, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(2);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);
    storm::models::sparse::ChoiceLabeling choiceLabeling(4);
    choiceLabeling.addLabel("a");
    choiceLabeling.addLabelToChoice("a", 0);
    choiceLabeling.addLabelToChoice("a", 3);
    choiceLabeling.addLabel("b");
    choiceLabeling.addLabelToChoice("b", 1);
    choiceLabeling.addLabelToChoice("b", 2);

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.observabilityClasses = std::vector<uint32_t>{0, 0};
    components.choiceLabeling = std::move(choiceLabeling);
    storm::models::sparse::Pomdp<double> pomdp(std::move(components));
    ASSERT_FALSE(pomdp.isCanonic());

    std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{0, "a"}, {1, "b"}}}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> namedFsc(0, std::nullopt, actionNames);
    namedFsc.addDeterministicActionTransition(0, 0, 0, 0);
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> numericFsc(0);
    numericFsc.addDeterministicActionTransition(0, 0, 0, 0);
    for (bool dontCare : {false, true}) {
        SCOPED_TRACE(dontCare);
        EXPECT_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, namedFsc, dontCare), storm::exceptions::IllegalArgumentException);
        EXPECT_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, numericFsc, dontCare), storm::exceptions::IllegalArgumentException);
    }

    auto canonicPomdp = storm::transformer::MakePOMDPCanonic<double>(pomdp).transform();
    ASSERT_TRUE(canonicPomdp->isCanonic());
    auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(*canonicPomdp, namedFsc);
    ASSERT_EQ(inducedDtmc.getNumberOfStates(), 2);
    ASSERT_EQ(inducedDtmc.getNumberOfTransitions(), 2);
    for (uint64_t state : {0ul, 1ul}) {
        auto row = inducedDtmc.getTransitionMatrix().getRow(state);
        ASSERT_EQ(row.getNumberOfEntries(), 1);
        EXPECT_EQ(row.begin()->getColumn(), 1);
        EXPECT_DOUBLE_EQ(row.begin()->getValue(), 1.0);
    }
}

TEST(ApplyObservationBasedFiniteStateController, PreservesRewardsForRandomisedActions) {
    auto pomdp = buildSingleStatePomdp(true);
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0);
    storm::storage::Distribution<double, uint64_t> distribution;
    distribution.addProbability(0, 0.25);
    distribution.addProbability(1, 0.75);
    fsc.addRandomisedActionTransition(0, 0, distribution, 0);

    auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);

    ASSERT_TRUE(inducedDtmc.hasRewardModel("rew"));
    auto const& rewards = inducedDtmc.getRewardModel("rew").getStateRewardVector();
    ASSERT_EQ(rewards.size(), inducedDtmc.getNumberOfStates());
    EXPECT_DOUBLE_EQ(rewards.at(0), 8.0);
}

TEST(ApplyObservationBasedFiniteStateController, RejectsTransitionRewards) {
    for (bool withStateAndActionRewards : {false, true}) {
        SCOPED_TRACE(withStateAndActionRewards);
        for (bool emptyTransitionRewards : {false, true}) {
            SCOPED_TRACE(emptyTransitionRewards);
            auto pomdp = buildSingleStatePomdp(true);
            storm::storage::SparseMatrixBuilder<double> rewardBuilder(2, 1);
            if (!emptyTransitionRewards) {
                // Reject transition rewards even when they only occur on an action the FSC never chooses.
                rewardBuilder.addNextValue(1, 0, 7.0);
            }
            std::optional<std::vector<double>> stateRewards;
            std::optional<std::vector<double>> actionRewards;
            if (withStateAndActionRewards) {
                stateRewards = std::vector<double>{3.0};
                actionRewards = std::vector<double>{2.0, 6.0};
            }
            pomdp.getRewardModels().emplace(
                "transition", storm::models::sparse::StandardRewardModel<double>(std::move(stateRewards), std::move(actionRewards), rewardBuilder.build()));
            storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0);
            fsc.addDeterministicActionTransition(0, 0, 0, 0);
            for (bool dontCare : {false, true}) {
                SCOPED_TRACE(dontCare);
                EXPECT_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc, dontCare), storm::exceptions::NotSupportedException);
            }
        }
    }
}

TEST(ApplyObservationBasedFiniteStateController, PreservesLabelsOfUnreachableStates) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 0, 1.0);
    matrixBuilder.addNextValue(1, 1, 1.0);
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 1, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(2);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);
    stateLabeling.addLabel("goal");
    stateLabeling.addLabelToState("goal", 1);
    stateLabeling.addLabel("safe");
    stateLabeling.addLabelToState("safe", 0);
    stateLabeling.addLabel("empty");

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.observabilityClasses = std::vector<uint32_t>{0, 1};
    storm::models::sparse::Pomdp<double> pomdp(std::move(components), true);
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0);
    fsc.addDeterministicActionTransition(0, 0, 0, 0);

    auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);
    ASSERT_EQ(inducedDtmc.getNumberOfStates(), 1);
    auto const& labeling = inducedDtmc.getStateLabeling();
    EXPECT_EQ(labeling.getLabels(), pomdp.getStateLabeling().getLabels());
    ASSERT_TRUE(labeling.containsLabel("goal"));
    EXPECT_TRUE(labeling.getStates("goal").empty());
    EXPECT_TRUE(labeling.getStates("empty").empty());
    EXPECT_TRUE(labeling.getStateHasLabel("safe", 0));
    EXPECT_TRUE(labeling.getStateHasLabel("init", 0));

    storm::Environment env;
    storm::parser::FormulaParser formulaParser;
    auto formula = formulaParser.parseSingleFormulaFromString("P=? [F \"goal\"]");
    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> checker(inducedDtmc);
    auto result = checker.check(env, *formula);
    EXPECT_DOUBLE_EQ(result->asExplicitQuantitativeCheckResult<double>()[0], 0.0);
}

TEST(ApplyObservationBasedFiniteStateController, MatchesObservationValuationsIndependentOfStorageOrder) {
    auto sourcePomdp = buildSingleStatePomdpWithObservationValuations(false);
    auto targetPomdp = buildSingleStatePomdpWithObservationValuations(true);

    auto const sourceObservationSignature = storm::pomdp::policy::getObservationSignature(sourcePomdp, 0);
    EXPECT_EQ(sourceObservationSignature, storm::pomdp::policy::getObservationSignature(targetPomdp, 0));

    std::optional observationSignatures{std::unordered_map<uint64_t, storm::pomdp::policy::ObservationSignature>{{0, sourceObservationSignature}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0, observationSignatures, std::nullopt);
    fsc.addDeterministicActionTransition(0, 0, 0, 0);

    auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(targetPomdp, fsc);
    EXPECT_EQ(inducedDtmc.getNumberOfStates(), 1);
}

TEST(ApplyObservationBasedFiniteStateController, NamedActionMappingsTakePrecedenceOverUnnamedFallbacks) {
    for (uint64_t namedActionIndex : {0ul, 1ul}) {
        SCOPED_TRACE(namedActionIndex);
        storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
        matrixBuilder.newRowGroup(0);
        matrixBuilder.addNextValue(0, 0, 1.0);
        matrixBuilder.addNextValue(1, 0, 1.0);
        matrixBuilder.addNextValue(2, 0, 1.0);

        storm::models::sparse::StateLabeling stateLabeling(1);
        stateLabeling.addLabel("init");
        stateLabeling.addLabelToState("init", 0);
        storm::models::sparse::ChoiceLabeling choiceLabeling(3);
        choiceLabeling.addLabel("named");
        choiceLabeling.addLabelToChoice("named", namedActionIndex);

        storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
        components.observabilityClasses = std::vector<uint32_t>{0};
        components.choiceLabeling = std::move(choiceLabeling);
        components.rewardModels.emplace("rew", storm::models::sparse::StandardRewardModel<double>(std::nullopt, std::vector<double>{2.0, 6.0, 10.0}));
        storm::models::sparse::Pomdp<double> pomdp(std::move(components), true);

        uint64_t const fscNamedActionId = 1 - namedActionIndex;
        std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{fscNamedActionId, "named"}}}}};
        storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0, std::nullopt, actionNames);
        fsc.addDeterministicActionTransition(0, 0, fscNamedActionId, 0);

        auto namedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);
        EXPECT_DOUBLE_EQ(namedDtmc.getRewardModel("rew").getStateRewardVector().at(0), namedActionIndex == 0 ? 2.0 : 6.0);

        fsc.addDeterministicActionTransition(0, 0, 2, 0);
        auto unnamedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);
        EXPECT_DOUBLE_EQ(unnamedDtmc.getRewardModel("rew").getStateRewardVector().at(0), 10.0);
    }
}

TEST(ApplyObservationBasedFiniteStateController, MatchesLabelledChoicesForUnnamedActions) {
    auto pomdp = buildSingleStatePomdp(true, true);
    for (std::string const actionName : {"first", "second"}) {
        SCOPED_TRACE(actionName);
        std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{0, actionName}}}}};
        storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0, std::nullopt, actionNames);
        fsc.addDeterministicActionTransition(0, 0, 1, 0);

        auto unnamedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);
        EXPECT_EQ(unnamedDtmc.getNumberOfStates(), 1);
        EXPECT_DOUBLE_EQ(unnamedDtmc.getRewardModel("rew").getStateRewardVector().at(0), 9.0);

        fsc.addDeterministicActionTransition(0, 0, 0, 0);
        auto namedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);
        EXPECT_DOUBLE_EQ(namedDtmc.getRewardModel("rew").getStateRewardVector().at(0), actionName == "first" ? 5.0 : 9.0);
    }
}

TEST(ApplyObservationBasedFiniteStateController, RejectsNamedActionMissingFromUnlabelledChoices) {
    auto pomdp = buildSingleStatePomdp(true);
    pomdp.getOptionalChoiceLabeling() = storm::models::sparse::ChoiceLabeling(2);
    std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{0, "missing-action"}}}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0, std::nullopt, actionNames);
    fsc.addDeterministicActionTransition(0, 0, 0, 0);

    for (bool dontCare : {false, true}) {
        SCOPED_TRACE(dontCare);
        EXPECT_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc, dontCare), storm::exceptions::UnexpectedException);
    }
}

TEST(ApplyObservationBasedFiniteStateController, MatchesUnlabelledChoicesForUnnamedActionsOrMissingMetadata) {
    auto pomdp = buildSingleStatePomdp(true);
    pomdp.getOptionalChoiceLabeling() = storm::models::sparse::ChoiceLabeling(2);
    using ActionNames = std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>;
    std::vector<std::optional<ActionNames>> actionNameMaps{ActionNames{{0, {{0, ""}}}}, ActionNames{{0, {{1, "other-action"}}}},
                                                           ActionNames{{1, {{0, "other-action"}}}}, ActionNames{}, std::nullopt};
    for (auto const& actionNames : actionNameMaps) {
        storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(0, std::nullopt, actionNames);
        fsc.addDeterministicActionTransition(0, 0, 0, 0);

        auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);
        EXPECT_EQ(inducedDtmc.getNumberOfStates(), 1);
        EXPECT_DOUBLE_EQ(inducedDtmc.getRewardModel("rew").getStateRewardVector().at(0), 5.0);
    }
}

TEST(ApplyObservationBasedFiniteStateController, UnspecifiedOutputUsesLocalActionZeroWithReorderedActionNames) {
    auto pomdp = buildSingleStatePomdp(true, true);
    std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{0, "second"}, {1, "first"}}}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(7, std::nullopt, actionNames);

    EXPECT_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc), storm::exceptions::UnexpectedException);
    auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc, true);

    EXPECT_EQ(inducedDtmc.getNumberOfStates(), 1);
    EXPECT_DOUBLE_EQ(inducedDtmc.getRewardModel("rew").getStateRewardVector().at(0), 5.0);
}

TEST(ApplyObservationBasedFiniteStateController, UnspecifiedOutputUsesLocalActionZeroWithIncompleteActionNames) {
    auto pomdp = buildSingleStatePomdp(true, true);
    std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{1, "second"}}}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(7, std::nullopt, actionNames);

    auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc, true);

    EXPECT_EQ(inducedDtmc.getNumberOfStates(), 1);
    EXPECT_DOUBLE_EQ(inducedDtmc.getRewardModel("rew").getStateRewardVector().at(0), 5.0);
}

TEST(ApplyObservationBasedFiniteStateController, SpecifiedOutputRemapsActionNamesWithDontCareEnabled) {
    auto pomdp = buildSingleStatePomdp(true, true);
    std::optional actionNames{std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>{{0, {{0, "second"}, {1, "first"}}}}};
    storm::pomdp::policy::ObservationBasedFiniteStateController<double> fsc(7, std::nullopt, actionNames);
    fsc.addDeterministicActionTransition(7, 0, 0, 7);

    auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc, true);

    EXPECT_EQ(inducedDtmc.getNumberOfStates(), 1);
    EXPECT_DOUBLE_EQ(inducedDtmc.getRewardModel("rew").getStateRewardVector().at(0), 9.0);
}

TEST(PolicyExtractor, OmitsObservationSignaturesAfterTargetSplitting) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, 1.0);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 1, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(2);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);
    stateLabeling.addLabel("truncated");

    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    auto const beliefMdp = storm::models::sparse::Mdp<double>(components);
    components.observabilityClasses = std::vector<uint32_t>{0, 0};
    components.observationValuations = buildSingleStatePomdpWithObservationValuations(false).getObservationValuations();
    auto pomdp = std::make_shared<storm::models::sparse::Pomdp<double>>(std::move(components), true);

    storm::storage::Scheduler<double> beliefScheduler(2);
    beliefScheduler.setChoice(0, 0);
    beliefScheduler.setChoice(0, 1);
    std::unordered_map<uint64_t, uint32_t> beliefStateToObservation{{0, 0}, {1, 0}};
    storm::pomdp::policy::PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double> originalExtractor(*pomdp, beliefMdp, beliefStateToObservation,
                                                                                                                  beliefScheduler);
    EXPECT_TRUE(originalExtractor.exportPolicyAsFiniteStateController().hasIdToObservationSignatureMap());

    storm::storage::BitVector targetStates(2, false);
    targetStates.set(1);
    storm::transformer::MakeStateSetObservationClosed<double> observationCloser(pomdp);
    auto const [splitPomdp, targetObservations] = observationCloser.transform(targetStates);
    ASSERT_EQ(splitPomdp->getNrObservations(), 2);
    ASSERT_TRUE(splitPomdp->hasObservationValuations());
    ASSERT_EQ(splitPomdp->getObservationValuations().getNumberOfEntities(), 1);

    beliefStateToObservation.at(1) = splitPomdp->getObservation(1);
    storm::pomdp::policy::PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double> splitExtractor(*splitPomdp, beliefMdp, beliefStateToObservation,
                                                                                                               beliefScheduler);
    auto const fsc = splitExtractor.exportPolicyAsFiniteStateController();
    EXPECT_FALSE(fsc.hasIdToObservationSignatureMap());
    EXPECT_EQ(fsc.getActionForObservationInNode(1, splitPomdp->getObservation(1)), 0);
    EXPECT_NO_THROW(storm::pomdp::policy::applyObservationBasedFSCToPomdp(*splitPomdp, fsc));
}

TEST(PolicyExtractor, UsesCutoffSchedulerLabelInsteadOfLocalChoiceIndex) {
    auto pomdp = buildSingleStatePomdp();
    auto beliefMdp = buildBeliefMdpWithReorderedCutoffChoices();

    storm::storage::Scheduler<double> beliefScheduler(3);
    beliefScheduler.setChoice(0, 0);
    beliefScheduler.setChoice(0, 1);
    beliefScheduler.setChoice(0, 2);

    storm::storage::Scheduler<double> schedulerZero(1);
    schedulerZero.setChoice(0, 0);
    storm::storage::Scheduler<double> schedulerOne(1);
    schedulerOne.setChoice(1, 0);
    std::optional<std::vector<storm::storage::Scheduler<double>>> approximationSchedulers(
        std::vector<storm::storage::Scheduler<double>>{schedulerZero, schedulerOne});
    std::unordered_map<uint64_t, uint32_t> beliefStateToObservation{{0, 0}, {1, 0}};

    storm::pomdp::policy::PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double> extractor(pomdp, beliefMdp, beliefStateToObservation,
                                                                                                          beliefScheduler, approximationSchedulers);
    auto fsc = extractor.exportPolicyAsFiniteStateController();

    EXPECT_EQ(fsc.getActionForObservationInNode(1, 0), 1);
}

TEST(PolicyExtractor, UsesContinuationSchedulerForTruncatedInitialBelief) {
    auto pomdp = buildSingleStatePomdp(true);
    auto beliefMdp = buildBeliefMdpWithReorderedCutoffChoices(true);
    storm::storage::Scheduler<double> beliefScheduler(3);
    beliefScheduler.setChoice(0, 0);
    beliefScheduler.setChoice(0, 1);
    beliefScheduler.setChoice(0, 2);
    std::unordered_map<uint64_t, uint32_t> beliefStateToObservation{{1, 0}};

    for (bool randomised : {false, true}) {
        SCOPED_TRACE(randomised);
        storm::storage::Scheduler<double> schedulerZero(1);
        schedulerZero.setChoice(0, 0);
        storm::storage::Scheduler<double> schedulerOne(1);
        if (randomised) {
            storm::storage::Distribution<double, uint_fast64_t> actionDistribution;
            actionDistribution.addProbability(0, 0.25);
            actionDistribution.addProbability(1, 0.75);
            schedulerOne.setChoice(actionDistribution, 0);
        } else {
            schedulerOne.setChoice(1, 0);
        }
        std::optional<std::vector<storm::storage::Scheduler<double>>> approximationSchedulers(
            std::vector<storm::storage::Scheduler<double>>{schedulerZero, schedulerOne});

        storm::pomdp::policy::PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double> extractor(pomdp, beliefMdp, beliefStateToObservation,
                                                                                                              beliefScheduler, approximationSchedulers);
        auto fsc = extractor.exportPolicyAsFiniteStateController();

        EXPECT_EQ(fsc.isDeterministic(), !randomised);
        auto const [initialDistribution, continuationNode] = fsc.getActionDistributionAndSuccessorForObservationInNode(0, 0);
        ASSERT_TRUE(fsc.hasOutputForObservationInNode(continuationNode, 0));
        auto const [continuationDistribution, successorNode] = fsc.getActionDistributionAndSuccessorForObservationInNode(continuationNode, 0);
        EXPECT_EQ(successorNode, continuationNode);
        EXPECT_EQ(initialDistribution.size(), randomised ? 2 : 1);
        EXPECT_EQ(continuationDistribution.size(), initialDistribution.size());
        for (auto const& [action, probability] : schedulerOne.getChoice(0).getChoiceAsDistribution()) {
            EXPECT_DOUBLE_EQ(initialDistribution.getProbability(action), probability);
            EXPECT_DOUBLE_EQ(continuationDistribution.getProbability(action), probability);
        }

        auto inducedDtmc = storm::pomdp::policy::applyObservationBasedFSCToPomdp(pomdp, fsc);
        auto const& rewards = inducedDtmc.getRewardModel("rew").getStateRewardVector();
        for (auto const reward : rewards) {
            EXPECT_DOUBLE_EQ(reward, randomised ? 8.0 : 9.0);
        }
    }
}

TEST(PolicyExtractor, RequiresObservationBasedCutoffSchedulers) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 0, 1.0);
    matrixBuilder.addNextValue(1, 0, 1.0);
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 1, 1.0);
    matrixBuilder.addNextValue(3, 1, 1.0);
    storm::models::sparse::StateLabeling labeling(2);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(labeling));
    components.observabilityClasses = std::vector<uint32_t>{0, 0};
    storm::models::sparse::Pomdp<double> pomdp(std::move(components), true);

    auto beliefMdp = buildBeliefMdpWithReorderedCutoffChoices(true);
    storm::storage::Scheduler<double> beliefScheduler(3);
    beliefScheduler.setChoice(0, 0);
    beliefScheduler.setChoice(1, 1);  // Select __sched_0 at the truncated initial belief.
    beliefScheduler.setChoice(0, 2);
    std::unordered_map<uint64_t, uint32_t> beliefStateToObservation{{1, 0}};

    for (bool randomised : {false, true}) {
        SCOPED_TRACE(randomised);
        storm::storage::Scheduler<double> scheduler(2);
        if (randomised) {
            storm::storage::Distribution<double, uint_fast64_t> firstDistribution;
            firstDistribution.addProbability(0, 0.25);
            firstDistribution.addProbability(1, 0.75);
            scheduler.setChoice(firstDistribution, 0);
            storm::storage::Distribution<double, uint_fast64_t> secondDistribution;
            secondDistribution.addProbability(0, 0.75);
            secondDistribution.addProbability(1, 0.25);
            scheduler.setChoice(secondDistribution, 1);
        } else {
            scheduler.setChoice(0, 0);
            scheduler.setChoice(1, 1);
        }
        auto extractPolicy = [&]() {
            std::optional<std::vector<storm::storage::Scheduler<double>>> approximationSchedulers({scheduler});
            storm::pomdp::policy::PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double> extractor(pomdp, beliefMdp, beliefStateToObservation,
                                                                                                                  beliefScheduler, approximationSchedulers);
            return extractor.exportPolicyAsFiniteStateController();
        };
        STORM_SILENT_EXPECT_THROW(extractPolicy(), storm::exceptions::IllegalArgumentException);

        scheduler.setChoice(scheduler.getChoice(0), 1);
        auto const fsc = extractPolicy();
        auto const [distribution, continuationNode] = fsc.getActionDistributionAndSuccessorForObservationInNode(0, 0);
        EXPECT_EQ(fsc.isDeterministic(), !randomised);
        ASSERT_EQ(distribution.size(), randomised ? 2 : 1);
        for (auto const& [action, probability] : scheduler.getChoice(0).getChoiceAsDistribution()) {
            EXPECT_DOUBLE_EQ(distribution.getProbability(action), probability);
        }
        EXPECT_TRUE(fsc.hasOutputForObservationInNode(continuationNode, 0));
    }
}

TEST(PolicyExtractor, InducedMarkovChainClipsSelectedChoice) {
    auto pomdp = buildSingleStatePomdp();
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, 1.0);
    matrixBuilder.addNextValue(1, 1, 1.0);
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 0, 0.25);
    matrixBuilder.addNextValue(2, 2, 0.75);
    matrixBuilder.addNextValue(3, 0, 0.4);
    matrixBuilder.addNextValue(3, 3, 0.6);
    matrixBuilder.newRowGroup(4);
    matrixBuilder.addNextValue(4, 2, 1.0);
    matrixBuilder.newRowGroup(5);
    matrixBuilder.addNextValue(5, 3, 1.0);

    storm::models::sparse::StateLabeling stateLabeling(4);
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", 0);
    stateLabeling.addLabel("truncated");
    stateLabeling.addLabelToState("truncated", 1);
    storm::models::sparse::ChoiceLabeling choiceLabeling(6);
    for (uint64_t choice = 0; choice < 2; ++choice) {
        auto const candidateLabel = "candidate_" + std::to_string(choice);
        stateLabeling.addLabel(candidateLabel);
        stateLabeling.addLabelToState(candidateLabel, 2 + choice);
        auto const clipLabel = "clip_" + std::to_string(choice);
        choiceLabeling.addLabel(clipLabel);
        choiceLabeling.addLabelToChoice(clipLabel, 2 + choice);
    }
    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(stateLabeling));
    components.choiceLabeling = std::move(choiceLabeling);
    storm::models::sparse::Mdp<double> beliefMdp(std::move(components));
    std::unordered_map<uint64_t, uint32_t> beliefStateToObservation{{0, 0}, {1, 0}};

    for (uint64_t chosenAction = 0; chosenAction < 2; ++chosenAction) {
        SCOPED_TRACE(chosenAction);
        storm::storage::Scheduler<double> beliefScheduler(4);
        beliefScheduler.setChoice(1, 0);
        beliefScheduler.setChoice(chosenAction, 1);
        beliefScheduler.setChoice(0, 2);
        beliefScheduler.setChoice(0, 3);
        storm::pomdp::policy::PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double> extractor(pomdp, beliefMdp, beliefStateToObservation,
                                                                                                              beliefScheduler);
        auto inducedModel = extractor.exportPolicyAsInducedMarkovChain();

        EXPECT_EQ(inducedModel->getNumberOfStates(), 3);
        auto const& labeling = inducedModel->getStateLabeling();
        auto const& clippingStates = labeling.getStates("clipping");
        ASSERT_EQ(clippingStates.getNumberOfSetBits(), 1);
        auto row = inducedModel->getTransitionMatrix().getRow(clippingStates.getNextSetIndex(0));
        ASSERT_EQ(row.getNumberOfEntries(), 1);
        EXPECT_DOUBLE_EQ(row.begin()->getValue(), 1.0);
        EXPECT_TRUE(labeling.getStateHasLabel("candidate_" + std::to_string(chosenAction), row.begin()->getColumn()));
    }
}

TEST(PolicyExtractor, AddsEmptyLabelToSelectedUnlabelledChoice) {
    auto pomdp = buildSingleStatePomdp();
    auto beliefMdp = buildSingleStateBeliefMdpWithUnlabelledSecondChoice();
    storm::storage::Scheduler<double> beliefScheduler(1);
    beliefScheduler.setChoice(1, 0);
    std::unordered_map<uint64_t, uint32_t> beliefStateToObservation{{0, 0}};

    storm::pomdp::policy::PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double> extractor(pomdp, beliefMdp, beliefStateToObservation,
                                                                                                          beliefScheduler);
    auto inducedModel = extractor.exportPolicyAsInducedMarkovChain();

    ASSERT_TRUE(inducedModel->hasChoiceLabeling());
    EXPECT_TRUE(inducedModel->getChoiceLabeling().getLabelsOfChoice(0).contains(""));
}
