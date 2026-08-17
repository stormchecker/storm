#pragma once

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/transformer/ContinuousToDiscreteTimeModelTransformer.h"
#include "storm/transformer/GoalStateMerger.h"
#include "storm/transformer/NonMarkovianChainTransformer.h"
#include "storm/transformer/StatePermuter.h"
#include "storm/transformer/SymbolicToSparseTransformer.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/builder.h"
#include "storm/utility/macros.h"
#include "storm/utility/permutation.h"

namespace storm {
namespace api {

/*!
 * Eliminates chains of non-Markovian states from a given Markov Automaton
 */
template<typename ValueType>
std::pair<std::shared_ptr<storm::models::sparse::Model<ValueType>>, std::vector<std::shared_ptr<storm::logic::Formula const>>> eliminateNonMarkovianChains(
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> const& ma, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
    storm::transformer::EliminationLabelBehavior labelBehavior) {
    auto newFormulas = storm::transformer::NonMarkovianChainTransformer<ValueType>::checkAndTransformFormulas(formulas);
    STORM_LOG_WARN_COND(newFormulas.size() == formulas.size(), "The state elimination does not preserve all properties.");
    STORM_LOG_WARN_COND(!(labelBehavior == storm::transformer::EliminationLabelBehavior::KeepLabels),
                        "Labels are not preserved by the state elimination. This may cause incorrect results.");
    return std::make_pair(storm::transformer::NonMarkovianChainTransformer<ValueType>::eliminateNonmarkovianStates(ma, labelBehavior), newFormulas);
}

/*!
 * Transforms the given continuous model to a discrete time model.
 * If such a transformation does not preserve one of the given formulas, a warning is issued.
 */
template<typename ValueType>
std::pair<std::shared_ptr<storm::models::sparse::Model<ValueType>>, std::vector<std::shared_ptr<storm::logic::Formula const>>>
transformContinuousToDiscreteTimeSparseModel(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                             std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    storm::transformer::ContinuousToDiscreteTimeModelTransformer<ValueType> transformer;

    std::string timeRewardName = "_time";
    while (model->hasRewardModel(timeRewardName)) {
        timeRewardName += "_";
    }
    auto newFormulas = transformer.checkAndTransformFormulas(formulas, timeRewardName);
    STORM_LOG_WARN_COND(newFormulas.size() == formulas.size(),
                        "Transformation of a " << model->getType() << " to a discrete time model does not preserve all properties.");

    if (model->isOfType(storm::models::ModelType::Ctmc)) {
        return std::make_pair(transformer.transform(*model->template as<storm::models::sparse::Ctmc<ValueType>>(), timeRewardName), newFormulas);
    } else if (model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        return std::make_pair(transformer.transform(*model->template as<storm::models::sparse::MarkovAutomaton<ValueType>>(), timeRewardName), newFormulas);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                        "Transformation of a " << model->getType() << " to a discrete time model is not supported.");
    }
    return std::make_pair(nullptr, newFormulas);
}

/*!
 * Transforms the given continuous model to a discrete time model IN PLACE.
 * This means that the input continuous time model is replaced by the new discrete time model.
 * If such a transformation does not preserve one of the given formulas, an error is issued.
 */
template<typename ValueType>
std::pair<std::shared_ptr<storm::models::sparse::Model<ValueType>>, std::vector<std::shared_ptr<storm::logic::Formula const>>>
transformContinuousToDiscreteTimeSparseModel(storm::models::sparse::Model<ValueType>&& model,
                                             std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    storm::transformer::ContinuousToDiscreteTimeModelTransformer<ValueType> transformer;

    std::string timeRewardName = "_time";
    while (model.hasRewardModel(timeRewardName)) {
        timeRewardName += "_";
    }
    auto newFormulas = transformer.checkAndTransformFormulas(formulas, timeRewardName);
    STORM_LOG_WARN_COND(newFormulas.size() == formulas.size(),
                        "Transformation of a " << model.getType() << " to a discrete time model does not preserve all properties.");

    if (model.isOfType(storm::models::ModelType::Ctmc)) {
        return std::make_pair(transformer.transform(std::move(*model.template as<storm::models::sparse::Ctmc<ValueType>>()), timeRewardName), newFormulas);
    } else if (model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        return std::make_pair(transformer.transform(std::move(*model.template as<storm::models::sparse::MarkovAutomaton<ValueType>>()), timeRewardName),
                              newFormulas);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                        "Transformation of a " << model.getType() << " to a discrete time model is not supported.");
    }
    return std::make_pair(nullptr, newFormulas);
}

template<typename ValueType>
std::shared_ptr<storm::logic::Formula const> checkAndTransformContinuousToDiscreteTimeFormula(storm::logic::Formula const& formula,
                                                                                              std::string const& timeRewardName = "_time") {
    storm::transformer::ContinuousToDiscreteTimeModelTransformer<ValueType> transformer;
    if (transformer.preservesFormula(formula)) {
        return transformer.checkAndTransformFormulas({formula.asSharedPointer()}, timeRewardName).front();
    } else {
        STORM_LOG_ERROR("Unable to transform formula " << formula << " to discrete time.");
    }
    return nullptr;
}

/*!
 * Transforms the given symbolic model to a sparse model.
 */
template<storm::dd::DdType Type, typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> transformSymbolicToSparseModel(
    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> const& symbolicModel,
    std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas = std::vector<std::shared_ptr<storm::logic::Formula const>>()) {
    switch (symbolicModel->getType()) {
        case storm::models::ModelType::Dtmc:
            return storm::transformer::SymbolicDtmcToSparseDtmcTransformer<Type, ValueType>().translate(
                *symbolicModel->template as<storm::models::symbolic::Dtmc<Type, ValueType>>(), formulas);
        case storm::models::ModelType::Mdp:
            return storm::transformer::SymbolicMdpToSparseMdpTransformer<Type, ValueType>::translate(
                *symbolicModel->template as<storm::models::symbolic::Mdp<Type, ValueType>>(), formulas);
        case storm::models::ModelType::Ctmc:
            return storm::transformer::SymbolicCtmcToSparseCtmcTransformer<Type, ValueType>::translate(
                *symbolicModel->template as<storm::models::symbolic::Ctmc<Type, ValueType>>(), formulas);
        case storm::models::ModelType::MarkovAutomaton:
            return storm::transformer::SymbolicMaToSparseMaTransformer<Type, ValueType>::translate(
                *symbolicModel->template as<storm::models::symbolic::MarkovAutomaton<Type, ValueType>>(), formulas);
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Transformation of symbolic " << symbolicModel->getType() << " to sparse model is not supported.");
    }
    return nullptr;
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> transformToNondeterministicModel(storm::models::sparse::Model<ValueType>&& model) {
    storm::storage::sparse::ModelComponents<ValueType> components(std::move(model.getTransitionMatrix()), std::move(model.getStateLabeling()),
                                                                  std::move(model.getRewardModels()));
    components.choiceLabeling = std::move(model.getOptionalChoiceLabeling());
    components.stateValuations = std::move(model.getOptionalStateValuations());
    components.choiceOrigins = std::move(model.getOptionalChoiceOrigins());
    if (model.isOfType(storm::models::ModelType::Dtmc)) {
        return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::Mdp, std::move(components));
    } else if (model.isOfType(storm::models::ModelType::Ctmc)) {
        components.rateTransitions = true;
        components.markovianStates = storm::storage::BitVector(components.transitionMatrix.getRowGroupCount(), true);
        return storm::utility::builder::buildModelFromComponents(storm::models::ModelType::MarkovAutomaton, std::move(components));
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException,
                        "Cannot transform model of type " << model.getType() << " to a nondeterministic model.");
    }
}

/*!
 * Identifies states of the given model and formula that are equivalent and merges them into a single state, yielding a (potentially smaller) model on which the
 * given formula can still be checked.
 * @note depending on the formula, this merges, e.g., states with probability 0/1 or reward 0/infty based on graph-based analysis.
 * @return the resulting model, or nullptr if merging is not applicable (e.g. because the model type or the formula is not supported).
 */
template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> mergeEquivalentStatesForFormula(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                                                                         storm::logic::Formula const& formula) {
    // storm::transformer::GoalStateMerger is only instantiated for double, rational number, and rational function value types.
    if constexpr (std::is_same_v<ValueType, double> || std::is_same_v<ValueType, storm::RationalNumber> || std::is_same_v<ValueType, storm::RationalFunction>) {
        if (model->isOfType(storm::models::ModelType::Dtmc) || model->isOfType(storm::models::ModelType::Ctmc) ||
            model->isOfType(storm::models::ModelType::Mdp) || model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
            storm::transformer::GoalStateMerger<ValueType> merger(*model);
            if (auto result = merger.mergeForFormula(formula, false)) {
                return result->model;
            }
        }
    }
    return nullptr;
}

/*!
 * Permutes the order of the states of the model according to the given order.
 * The order of the available choices at a state (of a nondeterministic model) is not changed.
 * A seed can be given which will be respected if a random permutation is requested.
 */
template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> permuteModelStates(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                                                            storm::utility::permutation::OrderKind order,
                                                                            std::optional<uint64_t> seed = std::nullopt) {
    std::vector<storm::utility::permutation::index_type> permutation;
    if (order == storm::utility::permutation::OrderKind::Random && seed.has_value()) {
        permutation = storm::utility::permutation::createRandomPermutation(model->getNumberOfStates(), seed.value());
    } else {
        permutation = storm::utility::permutation::createPermutation(order, model->getTransitionMatrix(), model->getInitialStates());
    }
    return storm::transformer::permuteStates(*model, permutation);
}

}  // namespace api
}  // namespace storm
