#include "storm/api/transformation.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Model.h"
#include "storm/transformer/GoalStateMerger.h"

namespace storm {
namespace api {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> mergeEquivalentStatesForFormula(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                                                                         storm::logic::Formula const& formula) {
    if (model->isOfType(storm::models::ModelType::Dtmc) || model->isOfType(storm::models::ModelType::Ctmc) || model->isOfType(storm::models::ModelType::Mdp) ||
        model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        storm::transformer::GoalStateMerger<ValueType> merger(*model);
        if (auto result = merger.mergeForFormula(formula, false)) {
            return result->model;
        }
    }
    return nullptr;
}

template std::shared_ptr<storm::models::sparse::Model<double>> mergeEquivalentStatesForFormula(
    std::shared_ptr<storm::models::sparse::Model<double>> const& model, storm::logic::Formula const& formula);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> mergeEquivalentStatesForFormula(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> const& model, storm::logic::Formula const& formula);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> mergeEquivalentStatesForFormula(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const& model, storm::logic::Formula const& formula);

}  // namespace api
}  // namespace storm
