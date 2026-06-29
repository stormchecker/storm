#include "storm-pars/transformer/BinaryDtmcTransformer.h"

#include <carl/formula/Constraint.h>
#include <queue>

#include "storm-pars/utility/parametric.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/constants.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace transformer {
std::shared_ptr<storm::models::sparse::Dtmc<RationalFunction>> BinaryDtmcTransformer::transform(storm::models::sparse::Dtmc<RationalFunction> const& dtmc,
                                                                                                bool keepStateValuations) const {
    auto data = transformTransitions(dtmc);
    storm::storage::sparse::ModelComponents<RationalFunction> components;
    components.stateLabeling = transformStateLabeling(dtmc, data);
    for (auto const& rewModel : dtmc.getRewardModels()) {
        components.rewardModels.emplace(rewModel.first, transformRewardModel(dtmc, rewModel.second, data));
    }
    components.transitionMatrix = std::move(data.simpleMatrix);
    if (keepStateValuations && dtmc.hasStateValuations()) {
        components.stateValuations = dtmc.getStateValuations().selectEntities(data.simpleStateToOriginalState);
    }

    return std::make_shared<storm::models::sparse::Dtmc<RationalFunction>>(std::move(components.transitionMatrix), std::move(components.stateLabeling),
                                                                           std::move(components.rewardModels));
}

struct StateWithRow {
    uint64_t state;
    std::vector<storage::MatrixEntry<uint64_t, RationalFunction>> row;
};

typename BinaryDtmcTransformer::TransformationData BinaryDtmcTransformer::transformTransitions(
    storm::models::sparse::Dtmc<RationalFunction> const& dtmc) const {
    auto const& matrix = dtmc.getTransitionMatrix();

    // Initialize a FIFO Queue that stores the start and the end of each row
    std::queue<StateWithRow> queue;
    for (uint64_t state = 0; state < matrix.getRowCount(); ++state) {
        std::vector<storage::MatrixEntry<uint64_t, RationalFunction>> diyRow;
        for (auto const& entry : matrix.getRow(state)) {
            diyRow.push_back(entry);
        }
        queue.emplace(StateWithRow{state, diyRow});
    }

    storm::storage::SparseMatrixBuilder<RationalFunction> builder;
    uint64_t currRow = 0;
    uint64_t currAuxState = queue.size();
    std::vector<uint64_t> origStates;

    while (!queue.empty()) {
        auto stateWithRow = std::move(queue.front());
        queue.pop();

        std::set<RationalFunctionVariable> variablesInRow;

        for (auto const& entry : stateWithRow.row) {
            for (auto const& variable : entry.getValue().gatherVariables()) {
                variablesInRow.emplace(variable);
            }
        }

        if (variablesInRow.size() == 0) {
            // Insert the row directly
            for (auto const& entry : stateWithRow.row) {
                builder.addNextValue(currRow, entry.getColumn(), entry.getValue());
            }
            ++currRow;
        } else if (variablesInRow.size() == 1) {
            auto parameter = *variablesInRow.begin();
            auto parameterPol = RawPolynomial(parameter);
            auto oneMinusParameter = RawPolynomial(1) - parameterPol;

            std::vector<storage::MatrixEntry<uint64_t, RationalFunction>> outgoing;
            // p * .. state
            std::vector<storage::MatrixEntry<uint64_t, RationalFunction>> newStateLeft;
            // (1-p) * .. state
            std::vector<storage::MatrixEntry<uint64_t, RationalFunction>> newStateRight;

            RationalFunction sumOfLeftBranch;
            RationalFunction sumOfRightBranch;

            for (auto const& entry : stateWithRow.row) {
                if (entry.getValue().isConstant()) {
                    outgoing.push_back(entry);
                }
                auto nominator = entry.getValue().nominator();
                auto denominator = entry.getValue().denominator();
                auto byP = RawPolynomial(nominator).divideBy(parameterPol);
                if (byP.remainder.isZero()) {
                    auto probability = RationalFunction(carl::makePolynomial<Polynomial>(byP.quotient), denominator);
                    newStateLeft.push_back(storage::MatrixEntry<uint64_t, RationalFunction>(entry.getColumn(), probability));
                    sumOfLeftBranch += probability;
                    continue;
                }
                auto byOneMinusP = RawPolynomial(nominator).divideBy(oneMinusParameter);
                if (byOneMinusP.remainder.isZero()) {
                    auto probability = RationalFunction(carl::makePolynomial<Polynomial>(byOneMinusP.quotient), denominator);
                    newStateRight.push_back(storage::MatrixEntry<uint64_t, RationalFunction>(entry.getColumn(), probability));
                    sumOfRightBranch += probability;
                    continue;
                }
                STORM_LOG_ERROR("Invalid transition!");
            }
            sumOfLeftBranch.simplify();
            sumOfRightBranch.simplify();
            for (auto& entry : newStateLeft) {
                entry.setValue(entry.getValue() / sumOfLeftBranch);
            }
            for (auto& entry : newStateRight) {
                entry.setValue(entry.getValue() / sumOfRightBranch);
            }

            queue.push(StateWithRow{currAuxState, newStateLeft});
            outgoing.push_back(storage::MatrixEntry<uint64_t, RationalFunction>(
                currAuxState, (sumOfLeftBranch)*RationalFunction(carl::makePolynomial<Polynomial>(parameter))));
            ++currAuxState;
            queue.push(StateWithRow{currAuxState, newStateRight});
            outgoing.push_back(storage::MatrixEntry<uint64_t, RationalFunction>(
                currAuxState, (sumOfRightBranch) * (utility::one<RationalFunction>() - RationalFunction(carl::makePolynomial<Polynomial>(parameter)))));
            ++currAuxState;

            for (auto const& entry : outgoing) {
                builder.addNextValue(currRow, entry.getColumn(), entry.getValue());
            }
            ++currRow;
        } else {
            STORM_LOG_ERROR("More than one variable in row " << currRow << "!");
        }
        origStates.push_back(stateWithRow.state);
    }
    TransformationData result;
    result.simpleMatrix = builder.build(currRow, currAuxState, currAuxState);
    result.simpleStateToOriginalState = std::move(origStates);
    return result;
}

storm::models::sparse::StateLabeling BinaryDtmcTransformer::transformStateLabeling(storm::models::sparse::Dtmc<RationalFunction> const& dtmc,
                                                                                   TransformationData const& data) const {
    storm::models::sparse::StateLabeling labeling(data.simpleMatrix.getRowCount());
    for (auto const& labelName : dtmc.getStateLabeling().getLabels()) {
        storm::storage::BitVector newStates = dtmc.getStateLabeling().getStates(labelName);
        newStates.resize(data.simpleMatrix.getRowCount(), false);
        if (labelName != "init") {
            for (uint64_t newState = dtmc.getNumberOfStates(); newState < data.simpleMatrix.getRowCount(); ++newState) {
                newStates.set(newState, newStates[data.simpleStateToOriginalState[newState]]);
            }
        }
        labeling.addLabel(labelName, std::move(newStates));
    }
    return labeling;
}

storm::models::sparse::StandardRewardModel<RationalFunction> BinaryDtmcTransformer::transformRewardModel(
    storm::models::sparse::Dtmc<RationalFunction> const& dtmc, storm::models::sparse::StandardRewardModel<RationalFunction> const& rewardModel,
    TransformationData const& data) const {
    std::optional<std::vector<RationalFunction>> stateRewards, actionRewards;
    STORM_LOG_THROW(rewardModel.hasStateActionRewards(), storm::exceptions::NotSupportedException, "Only state rewards supported.");
    if (rewardModel.hasStateRewards()) {
        stateRewards = rewardModel.getStateRewardVector();
        stateRewards->resize(data.simpleMatrix.getRowCount(), storm::utility::zero<RationalFunction>());
    }
    return storm::models::sparse::StandardRewardModel<RationalFunction>(std::move(stateRewards), std::move(actionRewards));
}
}  // namespace transformer
}  // namespace storm
