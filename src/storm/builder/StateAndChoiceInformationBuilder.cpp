#include "storm/builder/StateAndChoiceInformationBuilder.h"

#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace builder {

StateAndChoiceInformationBuilder::StateAndChoiceInformationBuilder()
    : _buildChoiceLabels(false), _buildChoiceOrigins(false), _buildStatePlayerIndications(false), _buildMarkovianStates(false), _buildStateValuations(false) {
    // Intentionally left empty
}

void StateAndChoiceInformationBuilder::setBuildChoiceLabels(bool value) {
    _buildChoiceLabels = value;
}

bool StateAndChoiceInformationBuilder::isBuildChoiceLabels() const {
    return _buildChoiceLabels;
}

void StateAndChoiceInformationBuilder::addChoiceLabel(std::string const& label, uint_fast64_t choiceIndex) {
    STORM_LOG_ASSERT(_buildChoiceLabels, "Building ChoiceLabels was not enabled.");
    storm::storage::BitVector& labeledChoices = _choiceLabels[label];
    labeledChoices.grow(choiceIndex + 1, false);
    labeledChoices.set(choiceIndex, true);
}

storm::models::sparse::ChoiceLabeling StateAndChoiceInformationBuilder::buildChoiceLabeling(uint_fast64_t totalNumberOfChoices) {
    storm::models::sparse::ChoiceLabeling result(totalNumberOfChoices);
    for (auto& label : _choiceLabels) {
        label.second.resize(totalNumberOfChoices, false);
        result.addLabel(label.first, std::move(label.second));
    }
    return result;
}

void StateAndChoiceInformationBuilder::setBuildChoiceOrigins(bool value) {
    _buildChoiceOrigins = value;
}

bool StateAndChoiceInformationBuilder::isBuildChoiceOrigins() const {
    return _buildChoiceOrigins;
}

void StateAndChoiceInformationBuilder::addChoiceOriginData(boost::any const& originData, uint_fast64_t choiceIndex) {
    STORM_LOG_ASSERT(_buildChoiceOrigins, "Building ChoiceOrigins was not enabled.");
    STORM_LOG_ASSERT(_dataOfChoiceOrigins.size() <= choiceIndex, "Unexpected choice index. Apparently, the choice indices are provided in an incorrect order.");
    if (_dataOfChoiceOrigins.size() != choiceIndex) {
        _dataOfChoiceOrigins.resize(choiceIndex);
    }
    _dataOfChoiceOrigins.push_back(originData);
}

std::vector<boost::any> StateAndChoiceInformationBuilder::buildDataOfChoiceOrigins(uint_fast64_t totalNumberOfChoices) {
    STORM_LOG_ASSERT(_buildChoiceOrigins, "Building ChoiceOrigins was not enabled.");
    _dataOfChoiceOrigins.resize(totalNumberOfChoices);
    _dataOfChoiceOrigins.shrink_to_fit();
    return std::move(_dataOfChoiceOrigins);
}

void StateAndChoiceInformationBuilder::setBuildStatePlayerIndications(bool value) {
    _buildStatePlayerIndications = value;
}

bool StateAndChoiceInformationBuilder::isBuildStatePlayerIndications() const {
    return _buildStatePlayerIndications;
}

void StateAndChoiceInformationBuilder::addStatePlayerIndication(storm::storage::PlayerIndex player, uint_fast64_t stateIndex) {
    STORM_LOG_ASSERT(_buildStatePlayerIndications, "Building StatePlayerIndications was not enabled.");
    STORM_LOG_ASSERT(_statePlayerIndications.size() <= stateIndex,
                     "Unexpected choice index. Apparently, the choice indices are provided in an incorrect order.");
    if (_statePlayerIndications.size() != stateIndex) {
        _statePlayerIndications.resize(stateIndex, storm::storage::INVALID_PLAYER_INDEX);
    }
    _statePlayerIndications.push_back(player);
}

bool StateAndChoiceInformationBuilder::hasStatePlayerIndicationBeenSet(storm::storage::PlayerIndex expectedPlayer, uint_fast64_t stateIndex) const {
    STORM_LOG_ASSERT(_buildStatePlayerIndications, "Building StatePlayerIndications was not enabled.");
    return (stateIndex < _statePlayerIndications.size()) && (_statePlayerIndications[stateIndex] == expectedPlayer);
}

std::vector<storm::storage::PlayerIndex> StateAndChoiceInformationBuilder::buildStatePlayerIndications(uint_fast64_t totalNumberOfStates) {
    STORM_LOG_ASSERT(_buildStatePlayerIndications, "Building StatePlayerIndications was not enabled.");
    _statePlayerIndications.resize(totalNumberOfStates, storm::storage::INVALID_PLAYER_INDEX);
    _statePlayerIndications.shrink_to_fit();
    return std::move(_statePlayerIndications);
}

void StateAndChoiceInformationBuilder::setBuildMarkovianStates(bool value) {
    _buildMarkovianStates = value;
}

bool StateAndChoiceInformationBuilder::isBuildMarkovianStates() const {
    return _buildMarkovianStates;
}

void StateAndChoiceInformationBuilder::addMarkovianState(uint_fast64_t markovianStateIndex) {
    STORM_LOG_ASSERT(_buildMarkovianStates, "Building MarkovianStates was not enabled.");
    _markovianStates.grow(markovianStateIndex + 1, false);
    _markovianStates.set(markovianStateIndex, true);
}

storm::storage::BitVector StateAndChoiceInformationBuilder::buildMarkovianStates(uint_fast64_t totalNumberOfStates) {
    STORM_LOG_ASSERT(_buildMarkovianStates, "Building MarkovianStates was not enabled.");
    _markovianStates.resize(totalNumberOfStates, false);
    return _markovianStates;
}

void StateAndChoiceInformationBuilder::setBuildStateValuations(bool value) {
    _buildStateValuations = value;
}

bool StateAndChoiceInformationBuilder::isBuildStateValuations() const {
    return _buildStateValuations;
}

void StateAndChoiceInformationBuilder::initializeStateValuations(storm::storage::sparse::Valuations&& valuations) {
    STORM_LOG_ASSERT(_buildStateValuations, "Building StateValuations was not enabled.");
    _stateValuations = std::move(valuations);
}

storm::storage::sparse::Valuations& StateAndChoiceInformationBuilder::stateValuations() {
    STORM_LOG_ASSERT(_buildStateValuations, "Building StateValuations was not enabled.");
    STORM_LOG_ASSERT(_stateValuations.has_value(), "State valuations not initialized.");
    return _stateValuations.value();
}

}  // namespace builder
}  // namespace storm
