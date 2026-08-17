#pragma once

#include "storm/models/sparse/Mdp.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class represents a partially observable Markov decision process.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class Pomdp : public Mdp<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Pomdp(storm::storage::SparseMatrix<ValueType> const &transitionMatrix, storm::models::sparse::StateLabeling const &stateLabeling,
          std::unordered_map<std::string, RewardModelType> const &rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model by moving the given data.
     *
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param stateLabeling The labeling of the states.
     * @param rewardModels A mapping of reward model names to reward models.
     */
    Pomdp(storm::storage::SparseMatrix<ValueType> &&transitionMatrix, storm::models::sparse::StateLabeling &&stateLabeling,
          std::unordered_map<std::string, RewardModelType> &&rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param components The components for this model.
     */
    Pomdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const &components, bool canonicFlag = false);

    Pomdp(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> &&components, bool canonicFlag = false);

    Pomdp(Pomdp<ValueType, RewardModelType> const &other) = default;

    Pomdp &operator=(Pomdp<ValueType, RewardModelType> const &other) = default;

    Pomdp(Pomdp<ValueType, RewardModelType> &&other) = default;

    Pomdp &operator=(Pomdp<ValueType, RewardModelType> &&other) = default;

    virtual void printModelInformationToStream(std::ostream &out) const override;

    uint32_t getObservation(uint64_t state) const;

    uint64_t getNrObservations() const;

    /*!
     * Returns the number of hidden values, i.e. the maximum number of states with the same observation
     */
    uint64_t getMaxNrStatesWithSameObservation() const;

    std::vector<uint32_t> const &getObservations() const;

    /*!
     * Changes the observations to the given ones and updates redundant informations (like the number of observations)
     * After calling this method, isCanonic() returns true iff (i) isCanonic() returned true before calling this method and (ii) preservesCanonicity was set to
     * true.
     *
     * @param newObservations The new observations
     * @param preservesCanonicity specifies whether the pomdp is still canonic (assuming that it was canonic before)
     *
     */
    void updateObservations(std::vector<uint32_t> &&newObservations, bool preservesCanonicity);

    std::vector<uint64_t> getStatesWithObservation(uint32_t observation) const;

    bool hasObservationValuations() const;

    storm::storage::sparse::Valuations const &getObservationValuations() const;

    std::optional<storm::storage::sparse::Valuations> const &getOptionalObservationValuations() const;

    bool isCanonic() const;

    void setIsCanonic(bool newValue = true);

    virtual std::size_t hash() const override;

    virtual bool isPartiallyObservable() const override;

   protected:
    /*!
     * Return a string that is additonally added to the state information in the dot stream.
     * @param state
     * @return
     */
    virtual std::string additionalDotStateInfo(uint64_t state) const override;

    // TODO: consider a bitvector based presentation (depending on our needs).
    std::vector<uint32_t> observations;
    uint64_t nrObservations;
    bool canonicFlag = false;

    std::optional<storm::storage::sparse::Valuations> observationValuations;

    void computeNrObservations();
};
}  // namespace sparse
}  // namespace models
}  // namespace storm
