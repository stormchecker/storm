#pragma once

#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/MaximalEndComponent.h"
#include "storm/storage/Scheduler.h"
#include "storm/transformer/DAProductBuilder.h"

namespace storm {

namespace modelchecker {
namespace helper {
namespace internal {

/*!
 * Helper class for scheduler construction in LTL model checking
 * @tparam ValueType the type a value can have
 * @tparam Nondeterministic A flag indicating if there is nondeterminism in the Model (MDP)
 */
template<typename ValueType, bool Nondeterministic>
class SparseLTLSchedulerHelper {
   public:
    /*!
     * The type of the product model (DTMC or MDP) that is used during the computation.
     */
    using productModelType = typename std::conditional<Nondeterministic, storm::models::sparse::Mdp<ValueType>, storm::models::sparse::Dtmc<ValueType>>::type;

    /*!
     * Initializes the helper.
     * @param numProductStates number of product states
     */
    SparseLTLSchedulerHelper(uint_fast64_t numProductStates);

    /*!
     * Set the scheduler choices to random.
     */
    void setRandom();

    /*!
     * Save choices for states in the accepting end component of the DA-Model product and for the states in the maximal end component that contains the
     * accepting EC. We know the acceptingEC satisfies the given conjunction of the acceptance condition. Therefore, we want to reach each infinity set in the
     * conjunction infinitely often. Choices are of the Form <s, q, InfSetID> -> choice. For states in the containingMEC that are not in the accepting EC, this
     * method will set a scheduler under which the states of the contained acceptingEC are reached almost surely.
     *
     * @note This method only sets new choices for states that do not have a choice set yet.
     *
     * @param acceptance the acceptance condition (in DNF)
     * @param mec the accepting end component which shall only contain states that can be visited infinitely often (without violating the acc cond.)
     * @param conjunction the conjunction satisfied by the end component
     * @param product the product model
     */
    void saveProductEcChoices(automata::AcceptanceCondition const& acceptance, storm::storage::MaximalEndComponent const& acceptingEC,
                              storm::storage::MaximalEndComponent const& containingMEC,
                              std::vector<automata::AcceptanceCondition::acceptance_expr::ptr> const& conjunction,
                              typename transformer::DAProduct<productModelType>::ptr product);

    /*!
     * Extracts scheduler choices and creates the memory structure for the LTL-Scheduler.
     *
     * @pre saveProductEcChoices has been called on some mec before, in particular there must be at least one accepting product state.
     *
     * @param numDaStates number of DA-states
     * @param acceptingProductStates states in accepting end components of the model-DA product
     * @param reachScheduler the scheduler ensuring to reach some acceptingState, defined on the model-DA product
     * @param productBuilder the product builder
     * @param product the model-DA product
     * @param modelStatesOfInterest relevant states of the model
     * @param transitionMatrix the transition matrix of the model
     */
    void prepareScheduler(uint_fast64_t numDaStates, storm::storage::BitVector const& acceptingProductStates,
                          std::unique_ptr<storm::storage::Scheduler<ValueType>> reachScheduler, transformer::DAProductBuilder const& productBuilder,
                          typename transformer::DAProduct<productModelType>::ptr product, storm::storage::BitVector const& modelStatesOfInterest,
                          storm::storage::SparseMatrix<ValueType> const& transitionMatrix);

    /*!
     * Returns a deterministic fully defined scheduler (except for dontCareStates) for the given model.
     * The choices are random if isRandom was set to true.
     *
     * @param model the model to construct the scheduler for
     * @param onlyInitialStatesRelevant flag indicating whether we only consider the fragment reachable from initial model states.
     * @return the scheduler
     */
    storm::storage::Scheduler<ValueType> extractScheduler(storm::models::sparse::Model<ValueType> const& model, bool onlyInitialStatesRelevant);

   private:
    /**
     * Computes the memory state for a given DA-state and infSet ID.
     * Encoded as (DA-state * (numberOfInfSets+1)) + infSet. (+1 because an additional "copy" of the DA is used for states outside accepting ECs)
     * @param daState DA-state
     * @param infSet infSet ID
     */
    uint_fast64_t getMemoryState(uint_fast64_t daState, uint_fast64_t infSet);

    /*!
     * Manages the InfSets of the acceptance condition by maintaining an assignment from infSets to unique indices
     * An infSet is a BitVector that encodes the product model states that are contained in the infSet
     */
    class InfSetPool {
       public:
        InfSetPool() = default;

        /*!
         * If the given infSet has been seen before, the corresponding index is returned. Otherwise, a new index is assigned to the infset.
         * Indices are assigned in a consecutive way starting from 0
         */
        uint_fast64_t getOrCreateIndex(storm::storage::BitVector&& infSet);
        /*!
         * Gets the inf set from the given index.
         * @param index
         * @return
         */
        storm::storage::BitVector const& get(uint_fast64_t index) const;

        /*!
         * @return The current number of stored infSets (which coincides with the index of the most recently added infSet (or 0 if there is none)
         */
        uint_fast64_t size() const;

       private:
        std::vector<storm::storage::BitVector> _storage;
    } _infSets;

    static const uint_fast64_t DEFAULT_INFSET;  /// Some arbitrary fixed infset index that will be used e.g. for states that are not in any accepting EC

    bool _randomScheduler;
    std::vector<storm::storage::BitVector> _dontCareStates;  // memorySate-modelState combinations that are never visited

    std::map<std::tuple<uint_fast64_t, uint_fast64_t, uint_fast64_t>, storm::storage::SchedulerChoice<ValueType>>
        _producedChoices;  // <s, q, DEFAULT_INFSET)> -> ReachChoice  and    <s, q, InfSetIndex> -> MecChoice

    std::vector<boost::optional<std::set<uint_fast64_t>>>
        _accInfSets;  // Save for each product state (which is assigned to an acceptingMEC), the infSets that need to be visited inf often to satisfy the
                      // acceptance condition. Remaining states belonging to no accepting EC, are assigned  len(_infSets) (REACH scheduler)

    // Memory structure
    std::vector<std::vector<storm::storage::BitVector>>
        _memoryTransitions;  // The BitVector contains the model states that lead from startState <q, mec, infSet> to <q', mec', infSet'>. This is
                             // deterministic, because each state <s, q> is assigned to a unique MEC (scheduler).
    std::vector<uint_fast64_t> _memoryInitialStates;  // Save for each relevant state (initial or all) s its unique initial memory state (which memory state is
                                                      // reached from the initial state after reading s)
};
}  // namespace internal
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm