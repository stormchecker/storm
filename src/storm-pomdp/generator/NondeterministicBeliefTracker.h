#pragma once
#include "storm/models/sparse/Pomdp.h"

namespace storm {
namespace generator {
/**
 * This class keeps track of common information of a set of beliefs.
 * It also keeps a reference to the POMDP. The manager is referenced by all beliefs.
 */
template<typename ValueType>
class BeliefStateManager {
   public:
    BeliefStateManager(storm::models::sparse::Pomdp<ValueType> const& pomdp);
    storm::models::sparse::Pomdp<ValueType> const& getPomdp() const;
    uint64_t getActionsForObservation(uint32_t observation) const;
    ValueType getRisk(uint64_t) const;
    void setRiskPerState(std::vector<ValueType> const& risk);
    uint64_t getFreshId();
    uint32_t getObservation(uint64_t state) const;
    uint64_t getObservationOffset(uint64_t state) const;
    uint64_t getState(uint32_t obs, uint64_t offset) const;
    uint64_t getNumberOfStates() const;
    uint64_t numberOfStatesPerObservation(uint32_t observation) const;

   private:
    storm::models::sparse::Pomdp<ValueType> const& pomdp;
    std::vector<ValueType> riskPerState;
    std::vector<uint64_t> numberActionsPerObservation;
    uint64_t beliefIdCounter = 0;
    std::vector<uint64_t> observationOffsetId;
    std::vector<std::vector<uint64_t>> statePerObservationAndOffset;
};

template<typename ValueType>
class SparseBeliefState;
template<typename ValueType>
bool operator==(SparseBeliefState<ValueType> const& lhs, SparseBeliefState<ValueType> const& rhs);

/**
 * SparseBeliefState stores beliefs in a sparse format.
 */
template<typename ValueType>
class SparseBeliefState {
   public:
    SparseBeliefState(std::shared_ptr<BeliefStateManager<ValueType>> const& manager, uint64_t state);
    /**
     * Update the belief using the new observation
     * @param newObservation
     * @param previousBeliefs put the new belief in this set
     */
    void update(uint32_t newObservation, std::unordered_set<SparseBeliefState>& previousBeliefs) const;
    std::size_t hash() const noexcept;
    /**
     * Get the estimate to be in the given state
     * @param state
     * @return
     */
    ValueType get(uint64_t state) const;
    /**
     * Get the weighted risk
     * @return
     */
    ValueType getRisk() const;

    // Various getters
    std::string toString() const;
    bool isValid() const;
    uint64_t getSupportSize() const;
    void setSupport(storm::storage::BitVector&) const;
    std::map<uint64_t, ValueType> const& getBeliefMap() const;

    friend bool operator== <>(SparseBeliefState<ValueType> const& lhs, SparseBeliefState<ValueType> const& rhs);

   private:
    void updateHelper(std::vector<std::map<uint64_t, ValueType>> const& partialBeliefs, std::vector<ValueType> const& sums,
                      typename std::map<uint64_t, ValueType>::const_iterator nextStateIt, uint32_t newObservation,
                      std::unordered_set<SparseBeliefState<ValueType>>& previousBeliefs) const;
    SparseBeliefState(std::shared_ptr<BeliefStateManager<ValueType>> const& manager, std::map<uint64_t, ValueType> const& belief, std::size_t newHash,
                      ValueType const& risk, uint64_t prevId);
    std::shared_ptr<BeliefStateManager<ValueType>> manager;

    std::map<uint64_t, ValueType> belief;  // map is ordered for unique hashing.
    std::size_t prestoredhash = 0;
    ValueType risk;
    uint64_t id;
    uint64_t prevId;
};

/**
 * This tracker implements state estimation for POMDPs.
 * This corresponds to forward filtering in Junges, Torfah, Seshia.
 *
 * @tparam ValueType How are probabilities stored
 * @tparam BeliefState What format to use for beliefs
 */
template<typename ValueType, typename BeliefState>
class NondeterministicBeliefTracker {
   public:
    struct Options {
        uint64_t trackTimeOut = 0;
        uint64_t timeOut = 0;  // for reduction, in milliseconds, 0 is no timeout
        ValueType wiggle;      // tolerance, anything above 0 means that we are incomplete.
    };
    NondeterministicBeliefTracker(storm::models::sparse::Pomdp<ValueType> const& pomdp,
                                  typename NondeterministicBeliefTracker<ValueType, BeliefState>::Options options = Options());
    /**
     * Start with a new trace.
     * @param observation The initial observation to start with.
     * @return
     */
    bool reset(uint32_t observation);
    /**
     * Extend the observed trace with the new observation
     * @param newObservation
     * @return
     */
    bool track(uint64_t newObservation);
    /**
     * Provides access to the current beliefs.
     * @return
     */
    std::unordered_set<BeliefState> const& getCurrentBeliefs() const;
    /**
     * What was the last obervation that we made?
     * @return
     */
    uint32_t getCurrentObservation() const;
    /**
     * How many beliefs are we currently tracking?
     * @return
     */
    uint64_t getNumberOfBeliefs() const;
    /**
     * What is the (worst-case/best-case) risk over all beliefs
     * @param max Should we take the max or the min?
     * @return
     */
    ValueType getCurrentRisk(bool max = true);
    /**
     * Sets the state-risk to use for all beliefs.
     * @param risk
     */
    void setRisk(std::vector<ValueType> const& risk);
    /**
     * What is the dimension of the current set of beliefs, i.e.,
     * what is the number of states we could possibly be in?
     * @return
     */
    uint64_t getCurrentDimension() const;
    /**
     * Apply reductions to the belief state
     * @return
     */
    uint64_t reduce();
    /**
     * Did we time out during the computation?
     * @return
     */
    bool hasTimedOut() const;

   private:
    storm::models::sparse::Pomdp<ValueType> const& pomdp;
    std::shared_ptr<BeliefStateManager<ValueType>> manager;
    std::unordered_set<BeliefState> beliefs;
    bool reductionTimedOut = false;
    Options options;
    uint32_t lastObservation;
};
}  // namespace generator
}  // namespace storm

//
namespace std {
template<typename T>
struct hash<storm::generator::SparseBeliefState<T>> {
    std::size_t operator()(storm::generator::SparseBeliefState<T> const& s) const noexcept {
        return s.hash();
    }
};
}  // namespace std
