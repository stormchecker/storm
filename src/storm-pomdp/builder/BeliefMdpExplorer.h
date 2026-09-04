#pragma once

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "storm-pomdp/storage/BeliefExplorationBounds.h"
#include "storm-pomdp/storage/BeliefManager.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/BitVector.h"

namespace storm {
class Environment;

namespace modelchecker {
template<typename FormulaType, typename ValueType>
class CheckTask;
class CheckResult;
}  // namespace modelchecker
namespace builder {
enum class ExplorationHeuristic { BreadthFirst, LowerBoundPrio, UpperBoundPrio, GapPrio, ProbabilityPrio };

template<typename PomdpType, typename BeliefValueType = typename PomdpType::ValueType>
class BeliefMdpExplorer {
   public:
    typedef typename PomdpType::ValueType ValueType;
    typedef storm::storage::BeliefManager<PomdpType, BeliefValueType> BeliefManagerType;
    typedef typename BeliefManagerType::BeliefId BeliefId;
    typedef uint64_t MdpStateType;

    struct SuccessorObservationInformation {
        SuccessorObservationInformation(ValueType const &obsProb, ValueType const &maxProb, uint64_t const &count);
        void join(SuccessorObservationInformation other);
        ValueType observationProbability;            /// The probability we move to the corresponding observation.
        ValueType maxProbabilityToSuccessorWithObs;  /// The maximal probability to move to a successor with the corresponding observation.
        uint64_t successorWithObsCount;              /// The number of successor beliefstates with this observation
        typename BeliefManagerType::BeliefSupportType support;
    };

    enum class Status { Uninitialized, Exploring, ModelFinished, ModelChecked };

    BeliefMdpExplorer(std::shared_ptr<BeliefManagerType> beliefManager, storm::pomdp::storage::PreprocessingPomdpValueBounds<ValueType> const &pomdpValueBounds,
                      ExplorationHeuristic explorationHeuristic = ExplorationHeuristic::BreadthFirst);

    BeliefMdpExplorer(BeliefMdpExplorer &&other) = default;

    BeliefManagerType const &getBeliefManager() const;

    void startNewExploration(std::optional<ValueType> extraTargetStateValue = boost::none, std::optional<ValueType> extraBottomStateValue = std::nullopt);

    /*!
     * Restarts the exploration to allow re-exploring each state.
     * After calling this, the "currently explored" MDP has the same number of states and choices as the "old" one, but the choices are still empty
     * This method inserts the initial state of the MDP in the exploration queue.
     * While re-exploring, the reference to the old MDP remains valid.
     */
    void restartExploration();

    bool hasUnexploredState() const;

    std::vector<uint64_t> getUnexploredStates();

    BeliefId exploreNextState();

    void addChoiceLabelToCurrentState(uint64_t const &localActionIndex, std::string const &label);

    void addTransitionsToExtraStates(uint64_t const &localActionIndex, ValueType const &targetStateValue = storm::numbers::zero<ValueType>(),
                                     ValueType const &bottomStateValue = storm::numbers::zero<ValueType>());

    void addSelfloopTransition(uint64_t const &localActionIndex = 0, ValueType const &value = storm::numbers::one<ValueType>());

    /*!
     * Adds the next transition to the given successor belief
     * @param localActionIndex
     * @param transitionTarget
     * @param value
     * @param ignoreNewBeliefs If true, beliefs that were not found before are not inserted, i.e. we might not insert the transition.
     * @return true iff a transition was actually inserted. False can only happen if ignoreNewBeliefs is true.
     */
    bool addTransitionToBelief(uint64_t const &localActionIndex, BeliefId const &transitionTarget, ValueType const &value, bool ignoreNewBeliefs);

    void computeRewardAtCurrentState(uint64_t const &localActionIndex, ValueType extraReward = storm::numbers::zero<ValueType>());

    /*!
     * Adds the provided reward value to the given action of the current state
     *
     * @param localActionIndex
     * @param rewardValue
     */
    void addRewardToCurrentState(uint64_t const &localActionIndex, ValueType rewardValue);

    void setCurrentStateIsTarget();

    void setCurrentStateIsTruncated();

    void setCurrentStateIsClipped();

    void setCurrentChoiceIsDelayed(uint64_t const &localActionIndex);

    bool currentStateHasOldBehavior() const;

    bool getCurrentStateWasTruncated() const;

    bool getCurrentStateWasClipped() const;

    /*!
     * Retrieves whether the current state can be reached under an optimal scheduler
     * This requires a previous call of computeOptimalChoicesAndReachableMdpStates.
     */
    bool stateIsOptimalSchedulerReachable(MdpStateType mdpState) const;

    /*!
     * Retrieves whether the given action at the current state was optimal in the most recent check.
     * This requires a previous call of computeOptimalChoicesAndReachableMdpStates.
     */
    bool actionIsOptimal(uint64_t const &globalActionIndex) const;

    /*!
     * Retrieves whether the current state can be reached under a scheduler that was optimal in the most recent check.
     * This requires (i) a previous call of computeOptimalChoicesAndReachableMdpStates and (ii) that the current state has old behavior.
     */
    bool currentStateIsOptimalSchedulerReachable() const;

    /*!
     * Retrieves whether the given action at the current state was optimal in the most recent check.
     * This requires (i) a previous call of computeOptimalChoicesAndReachableMdpStates and (ii) that the current state has old behavior.
     */
    bool actionAtCurrentStateWasOptimal(uint64_t const &localActionIndex) const;

    bool getCurrentStateActionExplorationWasDelayed(uint64_t const &localActionIndex) const;

    /*!
     * Inserts transitions and rewards at the given action as in the MDP of the previous exploration.
     * Does NOT set whether the state is truncated and/or target.
     * Will add "old" states that have not been considered before into the exploration queue
     * @param localActionIndex
     */
    void restoreOldBehaviorAtCurrentState(uint64_t const &localActionIndex);

    void finishExploration();

    void dropUnexploredStates();

    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> getExploredMdp() const;

    MdpStateType getCurrentNumberOfMdpStates() const;

    MdpStateType getCurrentNumberOfMdpChoices() const;

    MdpStateType getStartOfCurrentRowGroup() const;

    uint64_t getSizeOfCurrentRowGroup() const;

    uint64_t getRowGroupSizeOfState(uint64_t state) const;

    bool needsActionAdjustment(uint64_t numActionsNeeded);

    ValueType getLowerValueBoundAtCurrentState() const;

    ValueType getUpperValueBoundAtCurrentState() const;

    ValueType computeLowerValueBoundAtBelief(BeliefId const &beliefId) const;

    ValueType computeUpperValueBoundAtBelief(BeliefId const &beliefId) const;

    ValueType computeLowerValueBoundForScheduler(BeliefId const &beliefId, uint64_t schedulerId) const;

    ValueType computeUpperValueBoundForScheduler(BeliefId const &beliefId, uint64_t schedulerId) const;

    std::pair<bool, ValueType> computeFMSchedulerValueForMemoryNode(BeliefId const &beliefId, uint64_t memoryNode) const;

    storm::storage::Scheduler<ValueType> getUpperValueBoundScheduler(uint64_t schedulerId) const;

    storm::storage::Scheduler<ValueType> getLowerValueBoundScheduler(uint64_t schedulerId) const;

    std::vector<storm::storage::Scheduler<ValueType>> getUpperValueBoundSchedulers() const;

    std::vector<storm::storage::Scheduler<ValueType>> getLowerValueBoundSchedulers() const;

    void computeValuesOfExploredMdp(storm::Environment const &env, storm::solver::OptimizationDirection const &dir);

    bool hasComputedValues() const;

    bool hasFMSchedulerValues() const;

    std::vector<ValueType> const &getValuesOfExploredMdp() const;

    ValueType const &getComputedValueAtInitialState() const;

    MdpStateType getBeliefId(MdpStateType exploredMdpState) const;

    void gatherSuccessorObservationInformationAtCurrentState(uint64_t localActionIndex,
                                                             std::map<uint32_t, SuccessorObservationInformation> &gatheredSuccessorObservations);

    void gatherSuccessorObservationInformationAtMdpChoice(uint64_t mdpChoice,
                                                          std::map<uint32_t, SuccessorObservationInformation> &gatheredSuccessorObservations);

    bool currentStateHasSuccessorObservationInObservationSet(uint64_t localActionIndex, storm::storage::BitVector const &observationSet);

    void takeCurrentValuesAsUpperBounds();

    void takeCurrentValuesAsLowerBounds();

    /*!
     *
     * Computes the set of states that are reachable via a path that is consistent with an optimal MDP scheduler.
     * States that are only reachable via target states will not be in this set.
     * @param ancillaryChoicesEpsilon if the difference of a 1-step value of a choice is only epsilon away from the optimal value, the choice will be included.
     * @param relative if set, we consider the relative difference to detect ancillaryChoices
     */
    void computeOptimalChoicesAndReachableMdpStates(ValueType const &ancillaryChoicesEpsilon, bool relativeDifference);

    std::vector<BeliefId> getBeliefsWithObservationInMdp(uint32_t obs) const;

    std::vector<BeliefId> getBeliefsInMdp();

    void addClippingRewardToCurrentState(uint64_t const &localActionIndex, ValueType rewardValue);

    ValueType getTrivialUpperBoundAtPOMDPState(uint64_t const &pomdpState);

    ValueType getTrivialLowerBoundAtPOMDPState(uint64_t const &pomdpState);

    void setExtremeValueBound(storm::pomdp::storage::ExtremePOMDPValueBound<ValueType> valueBound);

    ValueType getExtremeValueBoundAtPOMDPState(uint64_t const &pomdpState);

    MdpStateType getExploredMdpState(BeliefId const &beliefId) const;

    bool beliefHasMdpState(BeliefId const &beliefId) const;

    storm::storage::BitVector getStateExtremeBoundIsInfinite();

    uint64_t getNrSchedulersForUpperBounds();

    uint64_t getNrSchedulersForLowerBounds();

    void markAsGridBelief(BeliefId const &beliefId);

    bool isMarkedAsGridBelief(BeliefId const &beliefId);

    const std::shared_ptr<storm::storage::Scheduler<BeliefMdpExplorer<PomdpType, BeliefValueType>::ValueType>> &getSchedulerForExploredMdp() const;

    void setFMSchedValueList(std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> valueList);

    uint64_t getNrOfMemoryNodesForObservation(uint32_t observation) const;

    void storeExplorationState();

    void restoreExplorationState();

    void adjustActions(uint64_t totalNumberOfActions);

    std::vector<BeliefValueType> computeProductWithSparseMatrix(BeliefId const &beliefId, storm::storage::SparseMatrix<BeliefValueType> &matrix) const;

   private:
    MdpStateType noState() const;

    std::shared_ptr<storm::logic::Formula const> createStandardProperty(storm::solver::OptimizationDirection const &dir, bool computeRewards);

    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> createStandardCheckTask(std::shared_ptr<storm::logic::Formula const> &property);

    MdpStateType getCurrentMdpState() const;

    MdpStateType getCurrentBeliefId() const;

    void internalAddTransition(uint64_t const &row, MdpStateType const &column, ValueType const &value);

    void internalAddRowGroupIndex();

    void insertValueHints(ValueType const &lowerBound, ValueType const &upperBound);

    MdpStateType getOrAddMdpState(BeliefId const &beliefId, ValueType const &transitionValue = storm::numbers::zero<ValueType>());

    // Belief state related information
    std::shared_ptr<BeliefManagerType> beliefManager;
    std::vector<BeliefId> mdpStateToBeliefIdMap;
    std::map<BeliefId, MdpStateType> beliefIdToMdpStateMap;
    storm::storage::BitVector exploredBeliefIds;
    std::map<BeliefId, std::map<uint64_t, std::string>> mdpStateToChoiceLabelsMap;

    // Exploration information
    std::multimap<ValueType, uint64_t> mdpStatesToExplorePrioState;
    std::map<uint64_t, ValueType> mdpStatesToExploreStatePrio;
    std::vector<ValueType> probabilityEstimation;
    std::vector<std::map<MdpStateType, ValueType>> exploredMdpTransitions;
    std::vector<MdpStateType> exploredChoiceIndices;
    std::vector<MdpStateType> previousChoiceIndices;
    std::vector<ValueType> mdpActionRewards;
    std::map<MdpStateType, ValueType> clippingTransitionRewards;
    uint64_t currentMdpState;
    std::map<MdpStateType, MdpStateType> stateRemapping;
    uint64_t nextId;
    ValueType prio;

    // Special states and choices during exploration
    std::optional<MdpStateType> extraTargetState;
    std::optional<MdpStateType> extraBottomState;
    storm::storage::BitVector targetStates;
    storm::storage::BitVector truncatedStates;
    storm::storage::BitVector clippedStates;
    MdpStateType initialMdpState;
    storm::storage::BitVector delayedExplorationChoices;
    std::unordered_set<BeliefId> gridBeliefs;

    // Final Mdp
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> exploredMdp;

    // Value and scheduler related information
    storm::pomdp::storage::PreprocessingPomdpValueBounds<ValueType> pomdpValueBounds;
    storm::pomdp::storage::ExtremePOMDPValueBound<ValueType> extremeValueBound;
    std::vector<std::vector<std::unordered_map<uint64_t, ValueType>>> fmSchedulerValueList;
    std::vector<ValueType> lowerValueBounds;
    std::vector<ValueType> upperValueBounds;
    std::vector<ValueType> values;  // Contains an estimate during building and the actual result after a check has performed
    std::optional<storm::storage::BitVector> optimalChoices;
    std::optional<storm::storage::BitVector> optimalChoicesReachableMdpStates;
    std::shared_ptr<storm::storage::Scheduler<ValueType>> scheduler;

    // The current status of this explorer
    ExplorationHeuristic explHeuristic;
    Status status;

    struct ExplorationStorage {
        std::vector<BeliefId> storedMdpStateToBeliefIdMap;
        std::map<BeliefId, MdpStateType> storedBeliefIdToMdpStateMap;
        storm::storage::BitVector storedExploredBeliefIds;
        std::map<BeliefId, std::map<uint64_t, std::string>> storedMdpStateToChoiceLabelsMap;
        std::multimap<ValueType, uint64_t> storedMdpStatesToExplorePrioState;
        std::map<uint64_t, ValueType> storedMdpStatesToExploreStatePrio;
        std::vector<ValueType> storedProbabilityEstimation;
        std::vector<std::map<MdpStateType, ValueType>> storedExploredMdpTransitions;
        std::vector<MdpStateType> storedExploredChoiceIndices;
        std::vector<ValueType> storedMdpActionRewards;
        std::map<MdpStateType, ValueType> storedClippingTransitionRewards;
        uint64_t storedCurrentMdpState;
        std::map<MdpStateType, MdpStateType> storedStateRemapping;
        uint64_t storedNextId;
        ValueType storedPrio;
        std::vector<ValueType> storedLowerValueBounds;
        std::vector<ValueType> storedUpperValueBounds;
        std::vector<ValueType> storedValues;
        storm::storage::BitVector storedTargetStates;
    };

    ExplorationStorage explorationStorage;
};
}  // namespace builder
}  // namespace storm