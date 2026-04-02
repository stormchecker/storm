#include "storm/models/sparse/Pomdp.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace models {
namespace sparse {

template<typename ValueType, typename RewardModelType>
Pomdp<ValueType, RewardModelType>::Pomdp(storm::storage::SparseMatrix<ValueType> const &transitionMatrix,
                                         storm::models::sparse::StateLabeling const &stateLabeling,
                                         std::unordered_map<std::string, RewardModelType> const &rewardModels)
    : Mdp<ValueType, RewardModelType>(transitionMatrix, stateLabeling, rewardModels, storm::models::ModelType::Pomdp) {
    computeNrObservations();
}

template<typename ValueType, typename RewardModelType>
Pomdp<ValueType, RewardModelType>::Pomdp(storm::storage::SparseMatrix<ValueType> &&transitionMatrix, storm::models::sparse::StateLabeling &&stateLabeling,
                                         std::unordered_map<std::string, RewardModelType> &&rewardModels)
    : Mdp<ValueType, RewardModelType>(transitionMatrix, stateLabeling, rewardModels, storm::models::ModelType::Pomdp) {
    computeNrObservations();
}

template<typename ValueType, typename RewardModelType>
Pomdp<ValueType, RewardModelType>::Pomdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const &components, bool canonicFlag)
    : Mdp<ValueType, RewardModelType>(components, storm::models::ModelType::Pomdp),
      observations(components.observabilityClasses.value()),
      canonicFlag(canonicFlag),
      observationValuations(components.observationValuations) {
    computeNrObservations();
}

template<typename ValueType, typename RewardModelType>
Pomdp<ValueType, RewardModelType>::Pomdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> &&components, bool canonicFlag)
    : Mdp<ValueType, RewardModelType>(components, storm::models::ModelType::Pomdp),
      observations(components.observabilityClasses.value()),
      canonicFlag(canonicFlag),
      observationValuations(components.observationValuations) {
    computeNrObservations();
}

template<typename ValueType, typename RewardModelType>
void Pomdp<ValueType, RewardModelType>::printModelInformationToStream(std::ostream &out) const {
    this->printModelInformationHeaderToStream(out);
    out << "Choices: \t" << this->getNumberOfChoices() << '\n';
    out << "Observations: \t" << this->nrObservations << '\n';
    this->printModelInformationFooterToStream(out);
}

template<typename ValueType, typename RewardModelType>
void Pomdp<ValueType, RewardModelType>::computeNrObservations() {
    uint64_t highestEntry = 0;
    for (uint32_t entry : observations) {
        if (entry > highestEntry) {
            highestEntry = entry;
        }
    }
    nrObservations = highestEntry + 1;  // Smallest entry should be zero.
    // In debug mode, ensure that every observability is used.
}

template<typename ValueType, typename RewardModelType>
uint32_t Pomdp<ValueType, RewardModelType>::getObservation(uint64_t state) const {
    return observations.at(state);
}

template<typename ValueType, typename RewardModelType>
uint64_t Pomdp<ValueType, RewardModelType>::getNrObservations() const {
    return nrObservations;
}

template<typename ValueType, typename RewardModelType>
uint64_t Pomdp<ValueType, RewardModelType>::getMaxNrStatesWithSameObservation() const {
    std::map<uint32_t, uint64_t> counts;
    for (auto const &obs : observations) {
        auto insertionRes = counts.emplace(obs, 1ull);
        if (!insertionRes.second) {
            ++insertionRes.first->second;
        }
    }
    uint64_t result = 0;
    for (auto const &count : counts) {
        result = std::max(result, count.second);
    }
    return result;
}

template<typename ValueType, typename RewardModelType>
std::vector<uint32_t> const &Pomdp<ValueType, RewardModelType>::getObservations() const {
    return observations;
}

template<typename ValueType, typename RewardModelType>
void Pomdp<ValueType, RewardModelType>::updateObservations(std::vector<uint32_t> &&newObservations, bool preservesCanonicity) {
    observations = std::move(newObservations);
    computeNrObservations();
    setIsCanonic(isCanonic() && preservesCanonicity);
}

template<typename ValueType, typename RewardModelType>
std::string Pomdp<ValueType, RewardModelType>::additionalDotStateInfo(uint64_t state) const {
    return "<" + std::to_string(getObservation(state)) + ">";
}

template<typename ValueType, typename RewardModelType>
std::vector<uint64_t> Pomdp<ValueType, RewardModelType>::getStatesWithObservation(uint32_t observation) const {
    std::vector<uint64_t> result;
    for (uint64_t state = 0; state < this->getNumberOfStates(); ++state) {
        if (this->getObservation(state) == observation) {
            result.push_back(state);
        }
    }
    return result;
}

template<typename ValueType, typename RewardModelType>
bool Pomdp<ValueType, RewardModelType>::hasObservationValuations() const {
    return static_cast<bool>(observationValuations);
}

template<typename ValueType, typename RewardModelType>
storm::storage::sparse::StateValuations const &Pomdp<ValueType, RewardModelType>::getObservationValuations() const {
    return observationValuations.value();
}

template<typename ValueType, typename RewardModelType>
std::optional<storm::storage::sparse::StateValuations> const &Pomdp<ValueType, RewardModelType>::getOptionalObservationValuations() const {
    return observationValuations;
}

template<typename ValueType, typename RewardModelType>
bool Pomdp<ValueType, RewardModelType>::isCanonic() const {
    return canonicFlag;
}

template<typename ValueType, typename RewardModelType>
void Pomdp<ValueType, RewardModelType>::setIsCanonic(bool newValue) {
    this->canonicFlag = newValue;
}

template<typename ValueType, typename RewardModelType>
bool Pomdp<ValueType, RewardModelType>::isPartiallyObservable() const {
    return true;
}

template<typename ValueType, typename RewardModelType>
std::size_t Pomdp<ValueType, RewardModelType>::hash() const {
    std::size_t seed = 0;
    boost::hash_combine(seed, sparse::Model<ValueType, RewardModelType>::hash());
    boost::hash_combine(seed, boost::hash_range(observations.begin(), observations.end()));
    return seed;
}

template class Pomdp<double>;
template class Pomdp<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class Pomdp<storm::RationalNumber>;
template class Pomdp<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class Pomdp<storm::Interval>;
template class Pomdp<storm::RationalInterval>;
template class Pomdp<storm::RationalFunction>;
}  // namespace sparse
}  // namespace models
}  // namespace storm
