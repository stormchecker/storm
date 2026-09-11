#pragma once

#include "storm-config.h"
#include "test/storm_gtest.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "storm-parsers/api/explicit_models.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/api/verification.h"
#include "storm/builder/BuilderOptions.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/transformer/bisimulation/Bisimulation.h"
#include "storm/utility/constants.h"

/*!
 * Shared helpers for the bisimulation tests, cf. StrongBisimulationTest.cpp, WeakBisimulationTest.cpp and BisimulationIssueTest.cpp.
 */
namespace storm::test::bisimulation {

using Options = storm::bisimulation::Options;

inline Options strongOptions() {
    return Options{};
}

inline Options weakOptions() {
    Options options;
    options.bisimulationType = Options::BisimulationType::Weak;
    return options;
}

/*!
 * Builds a deterministic model with the given (probability or rate) matrix. Every entry of `labels` names a label and lists the states carrying it.
 * `stateRewards` and `stateActionRewards`, if non-empty, are added as a reward model named "rew".
 */
template<typename ModelType, typename ValueType = typename ModelType::ValueType>
std::shared_ptr<ModelType> buildModel(storm::storage::SparseMatrix<ValueType> matrix, std::map<std::string, std::vector<uint64_t>> const& labels,
                                      std::vector<ValueType> const& stateRewards = {}, std::vector<ValueType> const& stateActionRewards = {}) {
    uint64_t const numStates = matrix.getColumnCount();
    storm::models::sparse::StateLabeling labeling(numStates);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    for (auto const& [label, states] : labels) {
        labeling.addLabel(label);
        for (uint64_t const state : states) {
            labeling.addLabelToState(label, state);
        }
    }
    storm::storage::sparse::ModelComponents<ValueType> components(std::move(matrix), std::move(labeling));
    if constexpr (std::is_same_v<ModelType, storm::models::sparse::Ctmc<ValueType>>) {
        components.rateTransitions = true;
    }
    if (!stateRewards.empty() || !stateActionRewards.empty()) {
        components.rewardModels.emplace("rew", storm::models::sparse::StandardRewardModel<ValueType>(
                                                   stateRewards.empty() ? std::nullopt : std::optional<std::vector<ValueType>>(stateRewards),
                                                   stateActionRewards.empty() ? std::nullopt : std::optional<std::vector<ValueType>>(stateActionRewards)));
    }
    return std::make_shared<ModelType>(std::move(components));
}

/*!
 * Builds a Markov automaton. The rows of `matrix` that belong to a state in `markovianStates` hold the transition rates, all other rows hold probabilities.
 * Every entry of `labels` names a label and lists the states carrying it.
 */
template<typename ValueType = double>
std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> buildMarkovAutomaton(storm::storage::SparseMatrix<ValueType> matrix,
                                                                                        storm::storage::BitVector markovianStates,
                                                                                        std::map<std::string, std::vector<uint64_t>> const& labels) {
    uint64_t const numStates = matrix.getColumnCount();
    storm::models::sparse::StateLabeling labeling(numStates);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    for (auto const& [label, states] : labels) {
        labeling.addLabel(label);
        for (uint64_t const state : states) {
            labeling.addLabelToState(label, state);
        }
    }
    storm::storage::sparse::ModelComponents<ValueType> components(std::move(matrix), std::move(labeling));
    components.markovianStates = std::move(markovianStates);
    components.rateTransitions = true;  // The rows of the Markovian states hold rates, which the model turns into probabilities plus exit rates.
    return std::make_shared<storm::models::sparse::MarkovAutomaton<ValueType>>(std::move(components));
}

/*!
 * @return the value of the given formula in the (unique) initial state of the given model.
 */
template<typename ValueType>
ValueType checkFormula(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::string const& formulaString) {
    storm::parser::FormulaParser formulaParser;
    auto const formula = formulaParser.parseSingleFormulaFromString(formulaString);
    auto const result = storm::api::verifyWithSparseEngine<ValueType>(model, storm::api::createTask<ValueType>(formula, true));
    return result->template asExplicitQuantitativeCheckResult<ValueType>()[*model->getInitialStates().begin()];
}

/*!
 * The model built from the given PRISM file together with the formulas that the minimization has to preserve.
 * @note building from a PRISM program requires an SMT solver, so callers have to guard with STORM_HAVE_Z3.
 */
template<typename ValueType>
struct PrismInput {
    std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
};

/*!
 * @param buildAllLabels if set, the full state space is built together with all labels of the program. Otherwise, the formulas may restrict the exploration,
 * e.g. by making the target states of a reachability formula absorbing.
 */
template<typename ValueType>
PrismInput<ValueType> buildFromPrism(std::string const& prismFile, std::string const& formulaString, bool const buildAllLabels = false) {
    // Preprocessing substitutes the constants. This is also where the declared type of a constant is taken into account, e.g. `const double x = 1/500;` only
    // becomes 0.002 instead of the integer division 0 after preprocessing.
    auto const program = storm::parser::PrismParser::parse(prismFile, true).preprocess();
    PrismInput<ValueType> result;
    result.formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaString, program));
    if (buildAllLabels) {
        storm::builder::BuilderOptions const builderOptions(false, true);
        result.model = storm::api::buildSparseModel<ValueType>(program, builderOptions);
    } else {
        result.model = storm::api::buildSparseModel<ValueType>(program, result.formulas);
    }
    return result;
}

}  // namespace storm::test::bisimulation
