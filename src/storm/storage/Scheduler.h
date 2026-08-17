#pragma once

#include <cstdint>
#include "storm/storage/BitVector.h"
#include "storm/storage/SchedulerChoice.h"
#include "storm/storage/memorystructure/MemoryStructure.h"

namespace storm {

namespace storage {

/**
 * This class defines which action is chosen in a particular state of a non-deterministic model. More concretely, a scheduler maps a state s to i
 * if the scheduler takes the i-th action available in s (i.e. the choices are relative to the states).
 * A Choice can be undefined, deterministic
 */
template<typename ValueType>
class Scheduler {
   public:
    /*!
     * Initializes a scheduler for the given number of model states.
     *
     * @param numberOfModelStates number of model states
     * @param memoryStructure the considered memory structure. If not given, the scheduler is considered as memoryless.
     */
    Scheduler(uint_fast64_t numberOfModelStates, boost::optional<storm::storage::MemoryStructure> const& memoryStructure = boost::none);
    Scheduler(uint_fast64_t numberOfModelStates, boost::optional<storm::storage::MemoryStructure>&& memoryStructure);

    /*!
     * Sets the choice defined by the scheduler for the given state.
     *
     * @param choice The choice to set for the given state.
     * @param modelState The state of the model for which to set the choice.
     * @param memoryState The state of the memoryStructure for which to set the choice.
     */
    void setChoice(SchedulerChoice<ValueType> const& choice, uint_fast64_t modelState, uint_fast64_t memoryState = 0);

    /*!
     * Is the scheduler defined on the states indicated by the selected-states bitvector?
     */
    bool isChoiceSelected(BitVector const& selectedStates, uint64_t memoryState = 0) const;

    /*!
     * Is the scheduler defined on the given state
     */
    bool isChoiceSelected(uint64_t modelState, uint64_t memoryState = 0) const;

    /*!
     * Clears the choice defined by the scheduler for the given state.
     *
     * @param modelState The state of the model for which to clear the choice.
     * @param memoryState The state of the memoryStructure for which to clear the choice.
     */
    void clearChoice(uint_fast64_t modelState, uint_fast64_t memoryState = 0);

    /*!
     * Gets the choice defined by the scheduler for the given model and memory state.
     *
     * @param state The state for which to get the choice.
     * @param memoryState the memory state which we consider.
     */
    SchedulerChoice<ValueType> const& getChoice(uint_fast64_t modelState, uint_fast64_t memoryState = 0) const;

    /*!
     * Set the combination of model state and memoryStructure state to dontCare.
     * These states are considered unreachable and are ignored when printing the scheduler.
     * If not specified otherwise, an arbitrary choice is set if no choice exists.
     *
     * @param modelState The state of the model.
     * @param memoryState The state of the memoryStructure.
     * @param memoryState A flag indicating whether to set an arbitrary choice.
     */
    void setDontCare(uint_fast64_t modelState, uint_fast64_t memoryState = 0, bool setArbitraryChoice = true);

    /*!
     * Unset the combination of model state and memoryStructure state to dontCare.
     *
     * @param modelState The state of the model.
     * @param memoryState The state of the memoryStructure.
     */
    void unSetDontCare(uint_fast64_t modelState, uint_fast64_t memoryState = 0);

    /*!
     * Is the combination of model state and memoryStructure state to reachable?
     */
    bool isDontCare(uint_fast64_t modelState, uint64_t memoryState = 0) const;

    /*!
     * Compute the Action Support: A bit vector that indicates all actions that are selected with positive probability in some memory state
     */
    storm::storage::BitVector computeActionSupport(std::vector<uint64_t> const& nondeterministicChoiceIndicies) const;

    /*!
     * Retrieves whether there is a *reachable* pair of model and memory state for which the choice is undefined.
     */
    bool isPartialScheduler() const;

    /*!
     * Retrieves whether all defined choices are deterministic
     */
    bool isDeterministicScheduler() const;

    /*!
     * Retrieves whether the scheduler considers a trivial memory structure (i.e., a memory structure with just a single state)
     */
    bool isMemorylessScheduler() const;

    /*!
     * Retrieves the number of memory states this scheduler considers.
     */
    uint_fast64_t getNumberOfMemoryStates() const;
    /*!
     * Retrieves the number of model states this scheduler considers.
     */
    uint_fast64_t getNumberOfModelStates() const;

    /*!
     * Retrieves the memory structure associated with this scheduler
     */
    boost::optional<storm::storage::MemoryStructure> const& getMemoryStructure() const;

    /*!
     * Retrieves a memoryless scheduler that corresponds to the given memory state.
     *
     * @param memoryState the memory state to fix
     */
    Scheduler<ValueType> getMemorylessSchedulerForMemoryState(uint64_t memoryState = 0) const;

    /*!
     * Returns a copy of this scheduler with the new value type
     */
    template<typename NewValueType>
    Scheduler<NewValueType> toValueType() const {
        uint_fast64_t numModelStates = schedulerChoices.front().size();
        Scheduler<NewValueType> newScheduler(numModelStates, memoryStructure);
        for (uint_fast64_t memState = 0; memState < this->getNumberOfMemoryStates(); ++memState) {
            for (uint_fast64_t modelState = 0; modelState < numModelStates; ++modelState) {
                newScheduler.setChoice(getChoice(modelState, memState).template toValueType<NewValueType>(), modelState, memState);
            }
        }
        return newScheduler;
    }

    /*!
     * Prints the scheduler to the given output stream.
     * @param out The output stream
     * @param model If given, provides additional information for printing (e.g., displaying the state valuations instead of state indices).
     *              Must be passed if the scheduler is not memoryless.
     * @param skipUniqueChoices If true, the (unique) choice for deterministic states (i.e., states with only one enabled choice) is not printed explicitly.
     *                          Requires a model to be given.
     * @param skipDontCareStates If true, the choice for dontCareStates states is not printed explicitly.
     */
    void printToStream(std::ostream& out, std::shared_ptr<storm::models::sparse::Model<ValueType>> model = nullptr, bool skipUniqueChoices = false,
                       bool skipDontCareStates = false) const;

    /*!
     * Prints the scheduler in json format to the given output stream.
     * @param out The output stream
     * @param model If given, provides additional information for printing (e.g., displaying the state valuations instead of state indices).
     *              Must be passed if the scheduler is not memoryless.
     * @param skipUniqueChoices If true, the (unique) choice for deterministic states (i.e., states with only one enabled choice) is not printed explicitly.
     *                          Requires a model to be given.
     * @param skipDontCareStates If true, the choice for dontCareStates states is not printed explicitly.
     */
    void printJsonToStream(std::ostream& out, std::shared_ptr<storm::models::sparse::Model<ValueType>> model = nullptr, bool skipUniqueChoices = false,
                           bool skipDontCareStates = false) const;

   private:
    boost::optional<storm::storage::MemoryStructure> memoryStructure;
    std::vector<std::vector<SchedulerChoice<ValueType>>> schedulerChoices;
    std::vector<storm::storage::BitVector> dontCareStates;
    uint_fast64_t numOfUndefinedChoices;
    uint_fast64_t numOfDeterministicChoices;
    uint_fast64_t numOfDontCareStates;
};
}  // namespace storage
}  // namespace storm
