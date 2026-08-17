#include "storm-dft/generator/DftNextStateGenerator.h"

#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidModelException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace generator {

template<typename ValueType, typename StateType>
DftNextStateGenerator<ValueType, StateType>::DftNextStateGenerator(storm::dft::storage::DFT<ValueType> const& dft,
                                                                   storm::dft::storage::DFTStateGenerationInfo const& stateGenerationInfo)
    : mDft(dft), mStateGenerationInfo(stateGenerationInfo), state(nullptr), uniqueFailedState(false) {
    deterministicModel = !mDft.canHaveNondeterminism();
    mTakeFirstDependency = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>().isTakeFirstDependency();
}

template<typename ValueType, typename StateType>
bool DftNextStateGenerator<ValueType, StateType>::isDeterministicModel() const {
    return deterministicModel;
}

template<typename ValueType, typename StateType>
typename DftNextStateGenerator<ValueType, StateType>::DFTStatePointer DftNextStateGenerator<ValueType, StateType>::createInitialState() const {
    DFTStatePointer initialState = std::make_shared<storm::dft::storage::DFTState<ValueType>>(mDft, mStateGenerationInfo, 0);

    // Check whether constant failed BEs are present
    if (mStateGenerationInfo.immediateFailedBE().size() > 0) {
        STORM_LOG_THROW(mStateGenerationInfo.immediateFailedBE().size() < 2, storm::exceptions::NotSupportedException,
                        "DFTs with more than one constantly failed BE are not supported. Transform DFT to contain a unique failed BE.");
        // Propagate the constant failure to reach the real initial state
        auto constFailedBE = mDft.getBasicElement(mStateGenerationInfo.immediateFailedBE().front());
        initialState = createSuccessorState(initialState, constFailedBE);
    }

    return initialState;
}

template<typename ValueType, typename StateType>
std::vector<StateType> DftNextStateGenerator<ValueType, StateType>::getInitialStates(StateToIdCallback const& stateToIdCallback) {
    DFTStatePointer initialState = createInitialState();
    // Register initial state
    auto [id, shouldStop] = getNewStateId(initialState, stateToIdCallback);
    initialState->setId(id);
    STORM_LOG_THROW(!shouldStop, storm::exceptions::InvalidModelException, "Initial state is already invalid due to a restriction.");
    return {id};
}

template<typename ValueType, typename StateType>
void DftNextStateGenerator<ValueType, StateType>::load(storm::storage::BitVector const& state) {
    // Load the state from bitvector
    StateType id = 0;  // TODO: set correct id
    this->state = std::make_shared<storm::dft::storage::DFTState<ValueType>>(state, mDft, mStateGenerationInfo, id);
}

template<typename ValueType, typename StateType>
void DftNextStateGenerator<ValueType, StateType>::load(DFTStatePointer const& state) {
    // Store a pointer to the state itself, because we need to be able to access it when expanding it.
    this->state = state;
}

template<typename ValueType, typename StateType>
storm::generator::StateBehavior<ValueType, StateType> DftNextStateGenerator<ValueType, StateType>::expand(StateToIdCallback const& stateToIdCallback) {
    STORM_LOG_DEBUG("Explore state: " << mDft.getStateString(state));
    // Initialization
    bool hasDependencies = this->state->getFailableElements().hasDependencies();
    return exploreState(stateToIdCallback, hasDependencies, mTakeFirstDependency);
}

template<typename ValueType, typename StateType>
storm::generator::StateBehavior<ValueType, StateType> DftNextStateGenerator<ValueType, StateType>::exploreState(StateToIdCallback const& stateToIdCallback,
                                                                                                                bool exploreDependencies,
                                                                                                                bool takeFirstDependency) {
    // Prepare the result, in case we return early.
    storm::generator::StateBehavior<ValueType, StateType> result;

    STORM_LOG_TRACE("Currently failable: " << state->getFailableElements().getCurrentlyFailableString(!exploreDependencies));
    auto iterFailable = this->state->getFailableElements().begin(!exploreDependencies);

    // Check for absorbing state:
    // - either no relevant event remains (i.e., all relevant events have failed already), or
    // - no BE can fail
    if (!this->state->hasOperationalRelevantEvent() || iterFailable == this->state->getFailableElements().end(!exploreDependencies)) {
        storm::generator::Choice<ValueType, StateType> choice(0, true);
        // Add self loop
        choice.addProbability(this->state->getId(), storm::utility::one<ValueType>());
        STORM_LOG_TRACE("Added self loop for " << state->getId());
        // No further exploration required
        result.addChoice(std::move(choice));
        result.setExpanded();
        return result;
    }

    storm::generator::Choice<ValueType, StateType> choice(0, !exploreDependencies);

    // Let BE fail
    for (; iterFailable != this->state->getFailableElements().end(!exploreDependencies); ++iterFailable) {
        DFTStatePointer newState;
        if (iterFailable.isFailureDueToDependency()) {
            // Next failure due to dependency
            STORM_LOG_ASSERT(exploreDependencies, "Failure should be due to dependency.");
            std::shared_ptr<storm::dft::storage::elements::DFTDependency<ValueType> const> dependency = iterFailable.asDependency(mDft);
            // Obtain successor state by propagating dependency failure to dependent BE
            newState = createSuccessorState(this->state, dependency, true);

            auto [newStateId, shouldStop] = getNewStateId(newState, stateToIdCallback);
            if (shouldStop) {
                continue;
            }
            STORM_LOG_ASSERT(newStateId != this->state->getId(),
                             "Self loop was added for " << newStateId << " and successful trigger of " << dependency->name());

            // Add non-deterministic choice if necessary
            ValueType probability = dependency->probability();
            choice.addProbability(newStateId, probability);
            STORM_LOG_TRACE("Added transition to " << newStateId << " with probability " << probability);

            if (!storm::utility::isOne(probability)) {
                // Add transition to state where dependency was unsuccessful
                DFTStatePointer unsuccessfulState = createSuccessorState(this->state, dependency, false);
                // Add state
                StateType unsuccessfulStateId = stateToIdCallback(unsuccessfulState);
                ValueType remainingProbability = storm::utility::one<ValueType>() - probability;
                choice.addProbability(unsuccessfulStateId, remainingProbability);
                STORM_LOG_TRACE("Added transition to " << unsuccessfulStateId << " with remaining probability " << remainingProbability);
                STORM_LOG_ASSERT(unsuccessfulStateId != this->state->getId(),
                                 "Self loop was added for " << unsuccessfulStateId << " and unsuccessful trigger of " << dependency->name());
            }
            result.addChoice(std::move(choice));
            // Start a fresh choice for the next conflicting dependency.
            choice = storm::generator::Choice<ValueType, StateType>(0, !exploreDependencies);

            // Handle premature stop for dependencies
            if (!iterFailable.isConflictingDependency()) {
                // We only explore the first non-conflicting dependency because we can fix an order.
                break;
            }
            if (takeFirstDependency) {
                // We discard further exploration as we already chose one dependent event
                break;
            }
        } else {
            STORM_LOG_ASSERT(!exploreDependencies, "Failure due to dependency should not be possible.");

            // Next failure due to BE failing on its own
            std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const> nextBE = iterFailable.asBE(mDft);
            // Obtain successor state by propagating failure of BE
            newState = createSuccessorState(this->state, nextBE);

            auto [newStateId, shouldStop] = getNewStateId(newState, stateToIdCallback);
            if (shouldStop) {
                continue;
            }
            STORM_LOG_ASSERT(newStateId != this->state->getId(), "Self loop was added for " << newStateId << " and failure of " << nextBE->name());

            // Set failure rate according to activation
            ValueType rate = this->state->getBERate(nextBE->id());
            STORM_LOG_ASSERT(!storm::utility::isZero(rate), "Failure rate should not be zero.");
            choice.addProbability(newStateId, rate);
            STORM_LOG_TRACE("Added transition to " << newStateId << " with failure rate " << rate);
        }

    }  // end iteration of failing BE

    if (exploreDependencies) {
        if (result.empty()) {
            // Dependencies might have been prevented from sequence enforcer -> explore BEs now instead of dependencies
            return exploreState(stateToIdCallback, false, takeFirstDependency);
        }
    } else {
        if (choice.size() == 0) {
            // No transition was generated
            STORM_LOG_TRACE("No transitions were generated.");
            // Add self loop
            choice.addProbability(this->state->getId(), storm::utility::one<ValueType>());
            STORM_LOG_TRACE("Added self loop for " << state->getId());
        }
        STORM_LOG_ASSERT(choice.size() > 0, "At least one choice should have been generated.");
        // Add all rates as one choice
        result.addChoice(std::move(choice));
    }

    STORM_LOG_TRACE("Finished exploring state: " << mDft.getStateString(state));
    result.setExpanded();
    return result;
}

template<typename ValueType, typename StateType>
std::pair<StateType, bool> DftNextStateGenerator<ValueType, StateType>::getNewStateId(DFTStatePointer newState,
                                                                                      StateToIdCallback const& stateToIdCallback) const {
    if (newState->isInvalid() || newState->isTransient()) {
        STORM_LOG_TRACE("State is ignored because " << (newState->isInvalid() ? "it is invalid" : "the transient fault is ignored"));
        return std::make_pair(0, true);
    }

    if (newState->hasFailed(mDft.getTopLevelIndex()) && uniqueFailedState) {
        // Use unique failed state
        return std::make_pair(0, false);
    } else {
        // Add new state
        return std::make_pair(stateToIdCallback(newState), false);
    }
}

template<typename ValueType, typename StateType>
typename DftNextStateGenerator<ValueType, StateType>::DFTStatePointer DftNextStateGenerator<ValueType, StateType>::createSuccessorState(
    DFTStatePointer const origState, std::shared_ptr<storm::dft::storage::elements::DFTDependency<ValueType> const> dependency,
    bool dependencySuccessful) const {
    // Construct new state as copy from original one
    DFTStatePointer newState = origState->copy();

    if (dependencySuccessful) {
        // Dependency was successful -> dependent BE fails
        STORM_LOG_TRACE("With the successful triggering of PDEP " << dependency->name() << " [" << dependency->id() << "]" << " in "
                                                                  << mDft.getStateString(origState));
        newState->letDependencyTrigger(dependency, true);
        STORM_LOG_ASSERT(dependency->dependentEvents().size() == 1, "Dependency " << dependency->name() << " does not have unique dependent event.");
        STORM_LOG_ASSERT(dependency->dependentEvents().front()->isBasicElement(),
                         "Trigger event " << dependency->dependentEvents().front()->name() << " is not a BE.");
        auto trigger = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(dependency->dependentEvents().front());
        return createSuccessorState(newState, trigger);
    } else {
        // Dependency was unsuccessful -> no BE fails
        STORM_LOG_TRACE("With the unsuccessful triggering of PDEP " << dependency->name() << " [" << dependency->id() << "]" << " in "
                                                                    << mDft.getStateString(origState));
        newState->letDependencyTrigger(dependency, false);
        return newState;
    }
}

template<typename ValueType, typename StateType>
typename DftNextStateGenerator<ValueType, StateType>::DFTStatePointer DftNextStateGenerator<ValueType, StateType>::createSuccessorState(
    DFTStatePointer const origState, std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const> be) const {
    // Construct new state as copy from original one
    DFTStatePointer newState = origState->copy();

    STORM_LOG_TRACE("With the failure of " << be->name() << " [" << be->id() << "]" << " in " << mDft.getStateString(origState));
    newState->letBEFail(be);

    // Propagate
    storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType> queues;
    propagateFailure(newState, be, queues);

    // Check whether transient failure lead to TLE failure
    // TODO handle for all types of BEs.
    if (be->beType() == storm::dft::storage::elements::BEType::EXPONENTIAL) {
        auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<ValueType> const>(be);
        if (beExp->isTransient() && !newState->hasFailed(mDft.getTopLevelIndex())) {
            newState->markAsTransient();
        }
    }

    // Check whether failsafe propagation can be discarded
    bool discardFailSafe = false;
    discardFailSafe |= newState->isInvalid();
    discardFailSafe |= newState->isTransient();
    discardFailSafe |= (newState->hasFailed(mDft.getTopLevelIndex()) && uniqueFailedState);

    // Propagate failsafe (if necessary)
    if (!discardFailSafe) {
        propagateFailsafe(newState, be, queues);

        // Update failable dependencies
        newState->updateFailableDependencies(be->id());
        newState->updateDontCareDependencies(be->id());
        newState->updateFailableInRestrictions(be->id());
    }
    return newState;
}

template<typename ValueType, typename StateType>
void DftNextStateGenerator<ValueType, StateType>::propagateFailure(DFTStatePointer newState,
                                                                   std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const>& nextBE,
                                                                   storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const {
    // Propagate failure
    for (DFTGatePointer parent : nextBE->parents()) {
        if (newState->isOperational(parent->id())) {
            queues.propagateFailure(parent);
        }
    }
    // Propagate failures
    while (!queues.failurePropagationDone()) {
        DFTGatePointer next = queues.nextFailurePropagation();
        next->checkFails(*newState, queues);
        newState->updateFailableDependencies(next->id());
        newState->updateFailableInRestrictions(next->id());
    }

    // Check restrictions
    for (DFTRestrictionPointer restr : nextBE->restrictions()) {
        queues.checkRestrictionLater(restr);
    }
    // Check restrictions
    while (!queues.restrictionChecksDone()) {
        DFTRestrictionPointer next = queues.nextRestrictionCheck();
        next->checkFails(*newState, queues);
        newState->updateFailableDependencies(next->id());
        newState->updateFailableInRestrictions(next->id());
    }
}

template<typename ValueType, typename StateType>
void DftNextStateGenerator<ValueType, StateType>::propagateFailsafe(DFTStatePointer newState,
                                                                    std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const>& nextBE,
                                                                    storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const {
    // Propagate failsafe
    while (!queues.failsafePropagationDone()) {
        DFTGatePointer next = queues.nextFailsafePropagation();
        next->checkFailsafe(*newState, queues);
    }

    // Propagate dont cares
    // Relevance is considered for each element independently
    while (!queues.dontCarePropagationDone()) {
        DFTElementPointer next = queues.nextDontCarePropagation();
        next->checkDontCareAnymore(*newState, queues);
    }
}

template<typename ValueType, typename StateType>
storm::generator::StateBehavior<ValueType, StateType> DftNextStateGenerator<ValueType, StateType>::createMergeFailedState(
    StateToIdCallback const& stateToIdCallback) {
    this->uniqueFailedState = true;
    // Introduce explicit fail state with id 0
    DFTStatePointer failedState = std::make_shared<storm::dft::storage::DFTState<ValueType>>(mDft, mStateGenerationInfo, 0);
    [[maybe_unused]] StateType failedStateId = stateToIdCallback(failedState);
    STORM_LOG_ASSERT(failedStateId == 0, "Unique failed state has not id 0.");
    STORM_LOG_TRACE("Introduce fail state with id 0.");

    // Add self loop
    storm::generator::Choice<ValueType, StateType> choice(0, true);
    choice.addProbability(0, storm::utility::one<ValueType>());

    // No further exploration required
    storm::generator::StateBehavior<ValueType, StateType> result;
    result.addChoice(std::move(choice));
    result.setExpanded();
    return result;
}

template class DftNextStateGenerator<double>;
template class DftNextStateGenerator<storm::RationalFunction>;

}  // namespace generator
}  // namespace storm::dft
