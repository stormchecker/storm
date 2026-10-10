#include "storm-pars/transformer/BinaryDtmcTransformer.h"

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

namespace {

/*!
 * Retrieves the polynomial cache to be used for newly created factorized polynomials.
 *
 * The cache of the given polynomial is reused, so that the newly created polynomials share their factorizations
 * with the polynomials of the model instead of allocating a cache of their own. Only a polynomial that has not
 * been factored (which includes every constant polynomial) has no cache; in that case a fresh one is created.
 *
 * @param polynomial A polynomial of the model whose cache is to be reused.
 * @return The cache to be used for newly created polynomials.
 */
std::shared_ptr<RawPolynomialCache> cacheOf(Polynomial const& polynomial) {
    auto cache = polynomial.pCache();
    if (!cache) {
        // The polynomial was never factored, so there is nothing to share. This is only expected for constant
        // polynomials.
        STORM_LOG_ASSERT(polynomial.isConstant(), "Expected a non-constant polynomial to carry a cache.");
        return std::make_shared<RawPolynomialCache>();
    }
    return cache;
}

/*!
 * Builds the rational function that has the given numerator over the given denominator, building the numerator as
 * a polynomial that shares the given factorization cache.
 *
 * @param numerator The numerator of the rational function.
 * @param denominator The denominator of the rational function.
 * @param cache The factorization cache that the numerator shares.
 * @return The corresponding rational function.
 */
RationalFunction toRationalFunction(RawPolynomial const& numerator, Polynomial const& denominator, std::shared_ptr<RawPolynomialCache> const& cache) {
    return RationalFunction(Polynomial(numerator, cache), denominator);
}

}  // namespace

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

            // Every entry of this row depends on the parameter, so at least one of them is non-constant and thus
            // carries the cache of the model, which the polynomials created below share.
            std::shared_ptr<RawPolynomialCache> cache;
            for (auto const& entry : stateWithRow.row) {
                if (!entry.getValue().isConstant()) {
                    cache = cacheOf(entry.getValue().nominator());
                    break;
                }
            }

            for (auto const& entry : stateWithRow.row) {
                if (entry.getValue().isConstant()) {
                    outgoing.push_back(entry);
                }
                auto nominator = entry.getValue().nominator();
                auto denominator = entry.getValue().denominator();
                auto byP = RawPolynomial(nominator).divideBy(parameterPol);
                if (byP.remainder.isZero()) {
                    auto probability = toRationalFunction(byP.quotient, denominator, cache);
                    newStateLeft.push_back(storage::MatrixEntry<uint64_t, RationalFunction>(entry.getColumn(), probability));
                    sumOfLeftBranch += probability;
                    continue;
                }
                auto byOneMinusP = RawPolynomial(nominator).divideBy(oneMinusParameter);
                if (byOneMinusP.remainder.isZero()) {
                    auto probability = toRationalFunction(byOneMinusP.quotient, denominator, cache);
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

            auto parameterPolynomial = Polynomial(RawPolynomial(parameter), cache);

            queue.push(StateWithRow{currAuxState, newStateLeft});
            outgoing.push_back(storage::MatrixEntry<uint64_t, RationalFunction>(currAuxState, (sumOfLeftBranch)*RationalFunction(parameterPolynomial)));
            ++currAuxState;
            queue.push(StateWithRow{currAuxState, newStateRight});
            outgoing.push_back(storage::MatrixEntry<uint64_t, RationalFunction>(
                currAuxState, (sumOfRightBranch) * (utility::one<RationalFunction>() - RationalFunction(parameterPolynomial))));
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
