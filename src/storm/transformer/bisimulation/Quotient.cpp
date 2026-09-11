#include "storm/transformer/bisimulation/Quotient.h"

#include <map>
#include <optional>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/builder.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

template<typename ValueType>
auto Quotient<ValueType>::buildFromPartition(storm::models::sparse::Model<ValueType> const& model, storm::bisimulation::Options const& options,
                                             storm::bisimulation::PreservationInformation const& preservationInformation,
                                             QuotientData<ValueType> const& quotientData) -> std::shared_ptr<storm::models::sparse::Model<ValueType>> {
    auto const& weakData = quotientData.weakData;
    bool const isWeak = options.bisimulationType == Options::BisimulationType::Weak;
    STORM_LOG_ASSERT(isWeak == weakData.has_value(), "Weak bisimulation data must be given for (and only for) weak bisimulation.");
    bool const useSignature = quotientData.signatureData.has_value();
    bool const isNondeterministic = model.isNondeterministicModel();
    STORM_LOG_ASSERT(useSignature || !isNondeterministic, "Signature data is required for nondeterministic models.");
    auto const& toQuotientState = quotientData.toQuotientState;
    auto const& toRepresentativeState = quotientData.toRepresentativeState;
    auto const& toRepresentativeChoice = useSignature ? quotientData.signatureData->toRepresentativeChoice : quotientData.toRepresentativeState;

    uint64_t const numberOfQuotientStates = toRepresentativeState.size();
    uint64_t const numberOfQuotientChoices = toRepresentativeChoice.size();
    STORM_LOG_ASSERT(isNondeterministic || numberOfQuotientStates == numberOfQuotientChoices, "Unexpected choice count.");

    // Now build the model components one after the other
    storm::storage::sparse::ModelComponents<ValueType> components;

    // Build the transition matrix
    {
        // Helper function to get the distribution over successor quotient states for a given quotient choice.
        auto getQuotientRow = [&model, &useSignature, &quotientData, &toRepresentativeChoice, &toQuotientState, &isWeak,
                               &weakData](uint64_t const quotientChoice) -> std::map<uint64_t, ValueType> {
            if (useSignature) {
                return quotientData.signatureData->quotientChoiceDistributions[quotientChoice];
            }
            uint64_t const representative = toRepresentativeChoice[quotientChoice];
            uint64_t const ownQuotientState = toQuotientState[representative];
            if (isWeak) {
                if (weakData->divergentStates.get(representative)) {
                    // The states of this block can never leave it, so the quotient state is absorbing. For a CTMC the rate of the self-loop is irrelevant.
                    return {{ownQuotientState, storm::utility::one<ValueType>()}};
                }
                // Non-divergent, representative states must not be silent because we have to represent the probability of exiting a block.
                // It is ruled out by the caller passing the non-silent states as the preferred representatives in the constructor of QuotientData.
                STORM_LOG_ASSERT(!weakData->silentStates.get(representative),
                                 "Weak bisimulation quotient: The representative of a non-divergent block is silent.");
            }
            std::map<uint64_t, ValueType> quotientRow;
            for (auto const& entry : model.getTransitionMatrix().getRow(representative)) {
                if (auto const ret = quotientRow.emplace(toQuotientState[entry.getColumn()], entry.getValue()); !ret.second) {
                    ret.first->second += entry.getValue();
                }
            }
            if constexpr (!storm::IsIntervalType<ValueType>) {
                if (isWeak && !weakData->stepSensitiveStates.get(representative)) {
                    // Moves within the own block are unobservable, so they are dropped.
                    if (auto const ownBlockIt = quotientRow.find(ownQuotientState); ownBlockIt != quotientRow.end()) {
                        quotientRow.erase(ownBlockIt);
                        // If the quotientRow is now empty, it means that the representative state of this non-divergent block is silent.
                        // We already ruled that out above but check here again for consistency.
                        STORM_LOG_THROW(!quotientRow.empty(), storm::exceptions::UnexpectedException,
                                        "Weak bisimulation quotient: the representative of a non-divergent block cannot leave its block.");
                        if (model.isDiscreteTimeModel()) {
                            // For a discrete-time model the remaining probabilities have to be renormalized.
                            // Note that we deliberately renormalize by the sum of the remaining entries rather than by 1 - ownBlockIt->second
                            // to avoid numerical issues if the two values are not exactly equal.
                            ValueType escapeValue = storm::utility::zero<ValueType>();
                            for (auto const& [_, value] : quotientRow) {
                                escapeValue += value;
                            }
                            for (auto& [_, value] : quotientRow) {
                                value /= escapeValue;
                            }
                        }
                    }
                }
            }
            return quotientRow;
        };

        storm::storage::SparseMatrixBuilder<ValueType> builder(numberOfQuotientChoices, numberOfQuotientStates, 0, true, isNondeterministic,
                                                               isNondeterministic ? numberOfQuotientStates : 0);
        for (uint64_t quotientState = 0, quotientChoice = 0; quotientState < numberOfQuotientStates; ++quotientState) {
            if (isNondeterministic) {
                builder.newRowGroup(quotientChoice);
            }
            uint64_t const quotientChoiceEnd = useSignature ? quotientData.signatureData->quotientChoiceGroupIndices[quotientState + 1] : quotientChoice + 1;
            for (; quotientChoice < quotientChoiceEnd; ++quotientChoice) {
                for (auto const& [column, value] : getQuotientRow(quotientChoice)) {
                    builder.addNextValue(quotientChoice, column, value);
                }
            }
        }
        components.transitionMatrix = builder.build();
    }

    // build state labeling
    {
        components.stateLabeling = storm::models::sparse::StateLabeling(numberOfQuotientStates);
        for (auto const& l : preservationInformation.preservedStateLabels) {
            auto const& in = model.getStateLabeling().getStates(l);
            storm::storage::BitVector out(numberOfQuotientStates, false);
            for (uint64_t quotientState = 0; quotientState < numberOfQuotientStates; ++quotientState) {
                if (in.get(toRepresentativeState[quotientState])) {
                    out.set(quotientState, true);
                }
            }
            components.stateLabeling.addLabel(l, std::move(out));
        }
        if (!preservationInformation.preservedStateLabels.contains("init")) {
            storm::storage::BitVector init(numberOfQuotientStates, false);
            for (auto const i : model.getStateLabeling().getStates("init")) {
                init.set(toQuotientState[i], true);
            }
            components.stateLabeling.addLabel("init", std::move(init));
        }
    }

    // build choice labeling
    if (!preservationInformation.preservedChoiceLabels.empty()) {
        STORM_LOG_ASSERT(model.hasChoiceLabeling(), "Model has no choice labeling but bisimulation preserved some.");
        components.choiceLabeling.emplace(numberOfQuotientChoices);
        for (auto const& l : preservationInformation.preservedChoiceLabels) {
            auto const& in = model.getChoiceLabeling().getChoices(l);
            storm::storage::BitVector out(numberOfQuotientChoices, false);
            for (uint64_t quotientChoice = 0; quotientChoice < numberOfQuotientChoices; ++quotientChoice) {
                if (in.get(toRepresentativeChoice[quotientChoice])) {
                    out.set(quotientChoice, true);
                }
            }
            components.choiceLabeling->addLabel(l, std::move(out));
        }
    }

    // build choice origins
    if (model.hasChoiceOrigins() && options.preserveChoiceOrigins) {
        components.choiceOrigins = model.getChoiceOrigins()->selectChoices(toRepresentativeChoice);
    }

    // build reward models
    {
        for (auto const& r : preservationInformation.preservedRewardModels) {
            std::optional<std::vector<ValueType>> stateRewards, stateActionRewards;
            auto const& rm = model.getRewardModel(r);
            if (rm.hasStateRewards()) {
                stateRewards.emplace();
                stateRewards->reserve(numberOfQuotientStates);
                for (auto const representativeState : toRepresentativeState) {
                    stateRewards->push_back(rm.getStateReward(representativeState));
                }
            }
            if (rm.hasStateActionRewards()) {
                stateActionRewards.emplace();
                stateActionRewards->reserve(numberOfQuotientChoices);
                for (auto const representativeChoice : toRepresentativeChoice) {
                    stateActionRewards->push_back(rm.getStateActionReward(representativeChoice));
                }
            }
            STORM_LOG_THROW(!rm.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                            "Transition rewards are not supported for quotient construction");
            components.rewardModels.emplace(r, storm::models::sparse::StandardRewardModel<ValueType>(std::move(stateRewards), std::move(stateActionRewards)));
        }
    }

    // build model type specific components
    using enum storm::models::ModelType;
    if (model.isOfType(Ctmc)) {
        components.rateTransitions = true;
    } else if (model.isOfType(MarkovAutomaton)) {
        auto const& ma = model.template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
        components.markovianStates.emplace(numberOfQuotientStates, false);
        components.exitRates.emplace(numberOfQuotientStates, storm::utility::zero<ValueType>());
        for (uint64_t quotientState = 0; quotientState < numberOfQuotientStates; ++quotientState) {
            auto const representativeState = toRepresentativeState[quotientState];
            if (ma->isMarkovianState(representativeState)) {
                components.markovianStates->set(quotientState, true);
                STORM_LOG_ASSERT(components.transitionMatrix.getRowGroupSize(quotientState) == 1, "Unexpected number of choices for Markovian state.");
                components.exitRates.value()[quotientState] = ma->getExitRate(representativeState);
            }
        }
    } else {
        STORM_LOG_THROW(model.isOfType(Dtmc) || model.isOfType(Mdp), storm::exceptions::NotSupportedException,
                        "The model type " << model.getType() << " is not supported for quotient construction.");
    }
    return storm::utility::builder::buildModelFromComponents(model.getType(), std::move(components));
}

template class Quotient<double>;
template class Quotient<storm::RationalNumber>;
template class Quotient<storm::RationalFunction>;
template class Quotient<storm::Interval>;
template class Quotient<storm::RationalInterval>;

}  // namespace storm::bisimulation
