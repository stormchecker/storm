#include "storm/models/sparse/MarkovAutomaton.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/transformer/SubsystemBuilder.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace models {
namespace sparse {

template<typename ValueType, typename RewardModelType>
MarkovAutomaton<ValueType, RewardModelType>::MarkovAutomaton(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                             storm::models::sparse::StateLabeling const& stateLabeling,
                                                             storm::storage::BitVector const& markovianStates,
                                                             std::unordered_map<std::string, RewardModelType> const& rewardModels)
    : MarkovAutomaton<ValueType, RewardModelType>(
          storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(transitionMatrix, stateLabeling, rewardModels, true, markovianStates)) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
MarkovAutomaton<ValueType, RewardModelType>::MarkovAutomaton(storm::storage::SparseMatrix<ValueType>&& transitionMatrix,
                                                             storm::models::sparse::StateLabeling&& stateLabeling, storm::storage::BitVector&& markovianStates,
                                                             std::unordered_map<std::string, RewardModelType>&& rewardModels)
    : MarkovAutomaton<ValueType, RewardModelType>(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>(
          std::move(transitionMatrix), std::move(stateLabeling), std::move(rewardModels), true, std::move(markovianStates))) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
MarkovAutomaton<ValueType, RewardModelType>::MarkovAutomaton(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components)
    : NondeterministicModel<ValueType, RewardModelType>(ModelType::MarkovAutomaton, components), markovianStates(components.markovianStates.get()) {
    if (components.exitRates) {
        exitRates = components.exitRates.get();
    }

    if (components.rateTransitions) {
        this->turnRatesToProbabilities();
    }
    closed = this->checkIsClosed();
}

template<typename ValueType, typename RewardModelType>
MarkovAutomaton<ValueType, RewardModelType>::MarkovAutomaton(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components)
    : NondeterministicModel<ValueType, RewardModelType>(ModelType::MarkovAutomaton, std::move(components)),
      markovianStates(std::move(components.markovianStates.get())) {
    if (components.exitRates) {
        exitRates = std::move(components.exitRates.get());
    }

    if (components.rateTransitions) {
        this->turnRatesToProbabilities();
    }
    closed = this->checkIsClosed();
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::isClosed() const {
    return closed;
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::containsZenoCycle() const {
    if (!this->hasZenoCycle.is_initialized()) {
        this->hasZenoCycle = this->checkContainsZenoCycle();
    }
    return this->hasZenoCycle.get();
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::isHybridState(storm::storage::sparse::state_type state) const {
    return isMarkovianState(state) && (this->getTransitionMatrix().getRowGroupSize(state) > 1);
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::isMarkovianState(storm::storage::sparse::state_type state) const {
    return this->markovianStates.get(state);
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::isProbabilisticState(storm::storage::sparse::state_type state) const {
    return !this->markovianStates.get(state);
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType> const& MarkovAutomaton<ValueType, RewardModelType>::getExitRates() const {
    return this->exitRates;
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType>& MarkovAutomaton<ValueType, RewardModelType>::getExitRates() {
    return this->exitRates;
}

template<typename ValueType, typename RewardModelType>
ValueType const& MarkovAutomaton<ValueType, RewardModelType>::getExitRate(storm::storage::sparse::state_type state) const {
    return this->exitRates[state];
}

template<typename ValueType, typename RewardModelType>
ValueType MarkovAutomaton<ValueType, RewardModelType>::getMaximalExitRate() const {
    return storm::utility::vector::max_if(this->exitRates, this->markovianStates);
}

template<typename ValueType, typename RewardModelType>
storm::storage::BitVector const& MarkovAutomaton<ValueType, RewardModelType>::getMarkovianStates() const {
    return this->markovianStates;
}

template<typename ValueType, typename RewardModelType>
void MarkovAutomaton<ValueType, RewardModelType>::close() {
    if (!closed) {
        // Get the choices that we will keep
        storm::storage::BitVector keptChoices(this->getNumberOfChoices(), true);
        for (auto state : this->getMarkovianStates()) {
            if (this->getTransitionMatrix().getRowGroupSize(state) > 1) {
                // The state is hybrid, hence, we remove the first choice.
                keptChoices.set(this->getTransitionMatrix().getRowGroupIndices()[state], false);
                // Afterwards, the state will no longer be Markovian.
                this->markovianStates.set(state, false);
                exitRates[state] = storm::utility::zero<ValueType>();
            }
        }

        if (!keptChoices.full()) {
            *this = std::move(*storm::transformer::buildSubsystem(*this, storm::storage::BitVector(this->getNumberOfStates(), true), keptChoices, false)
                                   .model->template as<MarkovAutomaton<ValueType, RewardModelType>>());
        }

        // Mark the automaton as closed.
        closed = true;
    }
}

template<typename ValueType, typename RewardModelType>
void MarkovAutomaton<ValueType, RewardModelType>::turnRatesToProbabilities() {
    bool assertRates = (this->exitRates.size() == this->getNumberOfStates());
    if (!assertRates) {
        STORM_LOG_THROW(this->exitRates.empty(), storm::exceptions::InvalidArgumentException, "The specified exit rate vector has an unexpected size.");
        this->exitRates.reserve(this->getNumberOfStates());
    }

    for (uint_fast64_t state = 0; state < this->getNumberOfStates(); ++state) {
        uint_fast64_t row = this->getTransitionMatrix().getRowGroupIndices()[state];
        if (this->markovianStates.get(state)) {
            if (assertRates) {
                STORM_LOG_THROW(this->exitRates[state] == this->getTransitionMatrix().getRowSum(row), storm::exceptions::InvalidArgumentException,
                                "The specified exit rate is inconsistent with the rate matrix. Difference is "
                                    << (this->exitRates[state] - this->getTransitionMatrix().getRowSum(row)) << ".");
            } else {
                this->exitRates.push_back(this->getTransitionMatrix().getRowSum(row));
            }
            for (auto& transition : this->getTransitionMatrix().getRow(row)) {
                transition.setValue(transition.getValue() / this->exitRates[state]);
            }
            ++row;
        } else {
            if (assertRates) {
                STORM_LOG_THROW(storm::utility::isZero(this->exitRates[state]), storm::exceptions::InvalidArgumentException,
                                "The specified exit rate for (non-Markovian) choice should be 0.");
            } else {
                this->exitRates.push_back(storm::utility::zero<ValueType>());
            }
        }
    }
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::isConvertibleToCtmc() const {
    return isClosed() && markovianStates.full();
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::hasOnlyTrivialNondeterminism() const {
    // Check every state
    for (uint_fast64_t state = 0; state < this->getNumberOfStates(); ++state) {
        STORM_LOG_ASSERT(!isHybridState(state), "State is hybrid.");
        if (this->getNumberOfChoices(state) > 1) {
            // Non-deterministic choice present
            STORM_LOG_ASSERT(isProbabilisticState(state), "State is not probabilistic.");
            return false;
        }
    }
    return true;
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::checkIsClosed() const {
    for (auto state : markovianStates) {
        if (this->getTransitionMatrix().getRowGroupSize(state) > 1) {
            return false;
        }
    }
    return true;
}

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Ctmc<ValueType, RewardModelType>> MarkovAutomaton<ValueType, RewardModelType>::convertToCtmc() const {
    STORM_LOG_THROW(isConvertibleToCtmc(), storm::exceptions::InvalidArgumentException, "MA cannot be converted to CTMC.");

    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> components(this->getTransitionMatrix(), this->getStateLabeling(),
                                                                                   this->getRewardModels(), false);
    components.transitionMatrix.makeRowGroupingTrivial();
    components.exitRates = this->getExitRates();
    if (this->hasChoiceLabeling()) {
        components.choiceLabeling = this->getChoiceLabeling();
    }
    if (this->hasStateValuations()) {
        components.stateValuations = this->getStateValuations();
    }
    if (this->hasChoiceOrigins()) {
        components.choiceOrigins = this->getChoiceOrigins();
    }
    return std::make_shared<storm::models::sparse::Ctmc<ValueType, RewardModelType>>(std::move(components));
}

template<typename ValueType, typename RewardModelType>
bool MarkovAutomaton<ValueType, RewardModelType>::checkContainsZenoCycle() const {
    if (isClosed() && markovianStates.empty()) {
        return true;
    }
    storm::storage::BitVector statesWithZenoCycle =
        storm::utility::graph::performProb0E(*this, this->getBackwardTransitions(), ~markovianStates, markovianStates);
    return !statesWithZenoCycle.empty();
}

template<typename ValueType, typename RewardModelType>
void MarkovAutomaton<ValueType, RewardModelType>::printModelInformationToStream(std::ostream& out) const {
    this->printModelInformationHeaderToStream(out);
    out << "Choices: \t" << this->getNumberOfChoices() << '\n';
    out << "Markovian St.: \t" << this->getMarkovianStates().getNumberOfSetBits() << '\n';
    out << "Max. Rate: \t";
    if (this->getMarkovianStates().empty()) {
        out << "None";
    } else {
        out << this->getMaximalExitRate();
    }
    out << '\n';
    this->printModelInformationFooterToStream(out);
}

template class MarkovAutomaton<double>;
template class MarkovAutomaton<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class MarkovAutomaton<storm::RationalNumber>;
template class MarkovAutomaton<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class MarkovAutomaton<storm::Interval>;
template class MarkovAutomaton<storm::RationalInterval>;
template class MarkovAutomaton<storm::RationalFunction>;
}  // namespace sparse
}  // namespace models
}  // namespace storm
