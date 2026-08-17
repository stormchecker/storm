#include "storm/modelchecker/helper/ltl/internal/SparseLTLSchedulerHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/storage/SchedulerChoice.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"
#include "storm/storage/memorystructure/MemoryStructure.h"
#include "storm/storage/memorystructure/MemoryStructureBuilder.h"
#include "storm/transformer/DAProductBuilder.h"
#include "storm/utility/graph.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace internal {

template<typename ValueType, bool Nondeterministic>
const uint_fast64_t SparseLTLSchedulerHelper<ValueType, Nondeterministic>::DEFAULT_INFSET = 0;

template<typename ValueType, bool Nondeterministic>
uint_fast64_t SparseLTLSchedulerHelper<ValueType, Nondeterministic>::InfSetPool::getOrCreateIndex(storm::storage::BitVector&& infSet) {
    auto it = std::find(_storage.begin(), _storage.end(), infSet);
    if (it == _storage.end()) {
        _storage.push_back(std::move(infSet));
        return _storage.size() - 1;
    } else {
        return distance(_storage.begin(), it);
    }
}

template<typename ValueType, bool Nondeterministic>
storm::storage::BitVector const& SparseLTLSchedulerHelper<ValueType, Nondeterministic>::InfSetPool::get(uint_fast64_t index) const {
    STORM_LOG_ASSERT(index < size(), "Inf set index " << index << " is invalid.");
    return _storage[index];
}

template<typename ValueType, bool Nondeterministic>
uint_fast64_t SparseLTLSchedulerHelper<ValueType, Nondeterministic>::InfSetPool::size() const {
    return _storage.size();
}

template<typename ValueType, bool Nondeterministic>
SparseLTLSchedulerHelper<ValueType, Nondeterministic>::SparseLTLSchedulerHelper(uint_fast64_t numProductStates)
    : _randomScheduler(false), _accInfSets(numProductStates, boost::none) {
    // Intentionally left empty.
}

template<typename ValueType, bool Nondeterministic>
uint_fast64_t SparseLTLSchedulerHelper<ValueType, Nondeterministic>::SparseLTLSchedulerHelper::getMemoryState(uint_fast64_t daState, uint_fast64_t infSet) {
    return (daState * _infSets.size()) + infSet;
}

template<typename ValueType, bool Nondeterministic>
void SparseLTLSchedulerHelper<ValueType, Nondeterministic>::SparseLTLSchedulerHelper::setRandom() {
    this->_randomScheduler = true;
}

template<typename ValueType, bool Nondeterministic>
void SparseLTLSchedulerHelper<ValueType, Nondeterministic>::saveProductEcChoices(
    automata::AcceptanceCondition const& acceptance, storm::storage::MaximalEndComponent const& acceptingEc,
    storm::storage::MaximalEndComponent const& containingMec, std::vector<automata::AcceptanceCondition::acceptance_expr::ptr> const& conjunction,
    typename transformer::DAProduct<productModelType>::ptr product) {
    STORM_LOG_ASSERT(std::all_of(acceptingEc.begin(), acceptingEc.end(),
                                 [&containingMec](auto const& accStateChoicesPair) { return containingMec.containsState(accStateChoicesPair.first); }),
                     "All states in the acceptingEC must be contained in the containing mec.");
    storm::storage::BitVector acceptingEcStates(product->getProductModel().getNumberOfStates(), false);
    for (auto const& [state, _] : acceptingEc) {
        acceptingEcStates.set(state);
    }

    // Catch the case where there are states in the containing MEC that are not in the accepting EC
    if (acceptingEc.size() < containingMec.size()) {
        storm::storage::BitVector remainingMecStates(product->getProductModel().getNumberOfStates(), false);
        for (auto const& [state, _] : containingMec) {
            if (!acceptingEcStates.get(state)) {
                remainingMecStates.set(state);
            }
        }
        // Compute a scheduler that, with prob=1 reaches the overlap states
        storm::storage::Scheduler<ValueType> mecScheduler(product->getProductModel().getNumberOfStates());
        storm::utility::graph::computeSchedulerProb1E<ValueType>(remainingMecStates, product->getProductModel().getTransitionMatrix(),
                                                                 product->getProductModel().getBackwardTransitions(), remainingMecStates, acceptingEcStates,
                                                                 mecScheduler);

        // Extract scheduler choices
        for (auto pState : remainingMecStates) {
            this->_producedChoices.insert(
                {std::make_tuple(product->getModelState(pState), product->getAutomatonState(pState), DEFAULT_INFSET), mecScheduler.getChoice(pState)});
        }
    }

    // Set choices for the accepting EC. We know the conjunction is satisfied: Save InfSets.
    std::set<uint_fast64_t> infSetIds;
    for (auto const& literal : conjunction) {
        if (literal->isAtom()) {
            const cpphoafparser::AtomAcceptance& atom = literal->getAtom();
            if (atom.getType() == cpphoafparser::AtomAcceptance::TEMPORAL_INF) {
                storm::storage::BitVector infSet;
                if (atom.isNegated()) {
                    infSet = ~acceptance.getAcceptanceSet(atom.getAcceptanceSet());
                } else {
                    infSet = acceptance.getAcceptanceSet(atom.getAcceptanceSet());
                }
                // Save new InfSet
                infSetIds.insert(_infSets.getOrCreateIndex(std::move(infSet)));
            }
            // A TEMPORAL_FIN atom can be ignored at this point since the mec is already known to only contain "allowed" states
        }
        // TRUE literals can be ignored since those will be satisfied anyway
        // FALSE literals are not possible here since the MEC is known to be accepting.
    }
    if (infSetIds.empty()) {
        // There might not be any infSet at this point (e.g. because all literals are TEMPORAL_FIN atoms). We need at least one infset, though
        infSetIds.insert(_infSets.getOrCreateIndex(storm::storage::BitVector(product->getProductModel().getNumberOfStates(), true)));
    }

    //  Save the InfSets into the _accInfSets for states in this MEC
    for (auto mecState : acceptingEcStates) {
        STORM_LOG_ASSERT(!_accInfSets[mecState].is_initialized(), "Accepting inf sets were already defined for a MEC state which is not expected.");
        _accInfSets[mecState].emplace(infSetIds);
    }

    // Define scheduler choices for the states in this MEC (that are not in any other MEC)
    // The resulting scheduler will  visit each InfSet inf often
    for (uint_fast64_t id : infSetIds) {
        // Scheduler that satisfies the MEC acceptance condition
        storm::storage::Scheduler<ValueType> mecScheduler(product->getProductModel().getNumberOfStates());

        storm::storage::BitVector infStatesWithinMec = _infSets.get(id) & acceptingEcStates;
        // States not in InfSet: Compute a scheduler that, with prob=1, reaches the infSet via mecStates
        storm::utility::graph::computeSchedulerProb1E<ValueType>(acceptingEcStates, product->getProductModel().getTransitionMatrix(),
                                                                 product->getProductModel().getBackwardTransitions(), acceptingEcStates, infStatesWithinMec,
                                                                 mecScheduler);

        // States that already reached the InfSet
        for (auto pState : infStatesWithinMec) {
            // Prob1E sets an arbitrary choice for the psi states, but we want to stay in this accepting MEC.
            mecScheduler.setChoice(
                *acceptingEc.getChoicesForState(pState).begin() - product->getProductModel().getTransitionMatrix().getRowGroupIndices()[pState], pState);
        }

        // Extract scheduler choices
        for (auto pState : acceptingEcStates) {
            // We want to reach the InfSet, save choice:  <s, q, InfSetID> --->  choice
            this->_producedChoices.insert(
                {std::make_tuple(product->getModelState(pState), product->getAutomatonState(pState), id), mecScheduler.getChoice(pState)});
        }
    }
}

template<typename ValueType, bool Nondeterministic>
void SparseLTLSchedulerHelper<ValueType, Nondeterministic>::prepareScheduler(uint_fast64_t numDaStates, storm::storage::BitVector const& acceptingProductStates,
                                                                             std::unique_ptr<storm::storage::Scheduler<ValueType>> reachScheduler,
                                                                             transformer::DAProductBuilder const& productBuilder,
                                                                             typename transformer::DAProduct<productModelType>::ptr product,
                                                                             storm::storage::BitVector const& modelStatesOfInterest,
                                                                             storm::storage::SparseMatrix<ValueType> const& transitionMatrix) {
    STORM_LOG_ASSERT(_infSets.size() > 0, "There is no inf set. Were the accepting ECs processed before?");

    // Compute size of the resulting memory structure: A state <q, infSet> is encoded as (q* (|infSets|))+ |infSet|
    uint64_t numMemoryStates = (numDaStates) * (_infSets.size());
    _dontCareStates = std::vector<storm::storage::BitVector>(numMemoryStates, storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false));

    // Set choices for states or consider them "dontCare"
    for (storm::storage::sparse::state_type automatonState = 0; automatonState < numDaStates; ++automatonState) {
        for (storm::storage::sparse::state_type modelState = 0; modelState < transitionMatrix.getRowGroupCount(); ++modelState) {
            if (!product->isValidProductState(modelState, automatonState)) {
                // If the state <s,q> does not occur in the product model, all the infSet combinations are irrelevant for the scheduler.
                for (uint_fast64_t infSet = 0; infSet < _infSets.size(); ++infSet) {
                    _dontCareStates[getMemoryState(automatonState, infSet)].set(modelState, true);
                }
            } else {
                auto pState = product->getProductStateIndex(modelState, automatonState);
                if (acceptingProductStates.get(pState)) {
                    // For states in accepting ECs set the MEC-scheduler. Missing combinations are "dontCare", they are not reachable using the scheduler
                    // choices.
                    for (uint_fast64_t infSet = 0; infSet < _infSets.size(); ++infSet) {
                        if (_producedChoices.count(std::make_tuple(product->getModelState(pState), product->getAutomatonState(pState), infSet)) == 0) {
                            _dontCareStates[getMemoryState(product->getAutomatonState(pState), infSet)].set(product->getModelState(pState), true);
                        }
                    }

                } else {
                    // Extract the choices of the REACH-scheduler (choices to reach an acc. MEC) for the MDP-DA product: <s,q> -> choice. The memory structure
                    // corresponds to the "0th" copy of the DA (DEFAULT_INFSET).
                    this->_accInfSets[pState] = std::set<uint_fast64_t>({DEFAULT_INFSET});
                    if (reachScheduler->isDontCare(pState)) {
                        // Mark the maybe States of the untilProbability scheduler as "dontCare"
                        _dontCareStates[getMemoryState(product->getAutomatonState(pState), DEFAULT_INFSET)].set(product->getModelState(pState), true);
                    } else {
                        // Set choice For non-accepting states that are not in any accepting EC
                        this->_producedChoices.insert({std::make_tuple(product->getModelState(pState), product->getAutomatonState(pState), DEFAULT_INFSET),
                                                       reachScheduler->getChoice(pState)});
                    }
                    // All other InfSet combinations are unreachable (dontCare)
                    static_assert(DEFAULT_INFSET == 0, "This code assumes that the default infset is 0");
                    for (uint_fast64_t infSet = 1; infSet < _infSets.size(); ++infSet) {
                        _dontCareStates[getMemoryState(product->getAutomatonState(pState), infSet)].set(product->getModelState(pState), true);
                    }
                }
            }
        }
    }

    // Prepare the memory structure. For that, we need: transitions,  initialMemoryStates (and memoryStateLabeling)

    // The next move function of the memory, will be build based on the transitions of the DA and jumps between InfSets.
    _memoryTransitions = std::vector<std::vector<storm::storage::BitVector>>(
        numMemoryStates, std::vector<storm::storage::BitVector>(numMemoryStates, storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false)));
    for (storm::storage::sparse::state_type automatonFrom = 0; automatonFrom < numDaStates; ++automatonFrom) {
        for (storm::storage::sparse::state_type modelState = 0; modelState < transitionMatrix.getRowGroupCount(); ++modelState) {
            uint_fast64_t automatonTo = productBuilder.getSuccessor(automatonFrom, modelState);

            if (product->isValidProductState(modelState, automatonTo)) {
                uint_fast64_t daProductState = product->getProductStateIndex(modelState, automatonTo);
                STORM_LOG_ASSERT(_accInfSets[daProductState] != boost::none,
                                 "The list of InfSets for the product state <" << modelState << ", " << automatonTo << "> is undefined.");
                std::set<uint_fast64_t> const& daProductStateInfSets = _accInfSets[daProductState].get();
                // Add the modelState to one outgoing transition of all states of the form <automatonFrom, InfSet>
                // For non-accepting states that are not in any accepting EC and for overlapping accepting ECs we always use the '0th' copy of the DA.
                // For the states in accepting ECs that do not overlap we cycle through copies 0,1,2, ... k of the DA (skipping copies whose inf-sets are not
                // needed for the accepting EC).
                for (uint_fast64_t currentInfSet = 0; currentInfSet < _infSets.size(); ++currentInfSet) {
                    uint_fast64_t newInfSet;
                    // Check if we need to switch the inf set (i.e. the DA copy)
                    if (daProductStateInfSets.count(currentInfSet) == 0) {
                        // This infSet is not relevant for the daProductState. We need to switch to a copy representing a relevant infset.
                        newInfSet = *daProductStateInfSets.begin();
                    } else if (daProductStateInfSets.size() > 1 && (_infSets.get(currentInfSet).get(daProductState))) {
                        // We have reached a state from the current infSet and thus need to move on to the next infSet in the list.
                        // Note that if the list contains just a single item, the switch would have no effect.
                        // In particular, this is the case for states that are not in an accepting MEC as those only have DEFAULT_INFSET in their list
                        auto nextInfSetIt = daProductStateInfSets.find(currentInfSet);
                        STORM_LOG_ASSERT(nextInfSetIt != daProductStateInfSets.end(), "The list of InfSets for the product state <"
                                                                                          << modelState << ", " << automatonTo
                                                                                          << "> does not contain the infSet " << currentInfSet);
                        nextInfSetIt++;
                        if (nextInfSetIt == daProductStateInfSets.end()) {
                            // Start again.
                            nextInfSetIt = daProductStateInfSets.begin();
                        }
                        newInfSet = *nextInfSetIt;
                    } else {
                        // In all other cases we can keep the current inf set (i.e. the DA copy)
                        newInfSet = currentInfSet;
                    }
                    _memoryTransitions[getMemoryState(automatonFrom, currentInfSet)][getMemoryState(automatonTo, newInfSet)].set(modelState);
                }
            }
        }
    }
    // Finished creation of transitions.

    // Find initial memory states
    this->_memoryInitialStates = std::vector<uint_fast64_t>(transitionMatrix.getRowGroupCount());
    // Save for each relevant model state its initial memory state (get the s-successor q of q0)
    for (storm::storage::sparse::state_type modelState : modelStatesOfInterest) {
        storm::storage::sparse::state_type automatonState = productBuilder.getInitialState(modelState);
        STORM_LOG_ASSERT(product->isValidProductState(modelState, automatonState),
                         "The memory successor state for the model state " << modelState << "does not exist in the DA-Model Product.");
        STORM_LOG_ASSERT(_accInfSets[product->getProductStateIndex(modelState, automatonState)] != boost::none,
                         "The list of InfSets for the product state <" << modelState << ", " << automatonState << "> is undefined.");
        // Start in the first InfSet of <s, q>
        auto infSet = _accInfSets[product->getProductStateIndex(modelState, automatonState)].get().begin();
        _memoryInitialStates[modelState] = getMemoryState(automatonState, *infSet);
    }
}

template<typename ValueType, bool Nondeterministic>
storm::storage::Scheduler<ValueType> SparseLTLSchedulerHelper<ValueType, Nondeterministic>::SparseLTLSchedulerHelper::extractScheduler(
    storm::models::sparse::Model<ValueType> const& model, bool onlyInitialStatesRelevant) {
    if (_randomScheduler) {
        storm::storage::Scheduler<ValueType> scheduler(model.getNumberOfStates());
        for (storm::storage::sparse::state_type state = 0; state < model.getNumberOfStates(); ++state) {
            scheduler.setChoice(0, state);
        }
        return scheduler;
    }

    // Otherwise, we compute a scheduler with memory.

    // Create a memory structure for the MDP scheduler with memory. If hasRelevantStates is set, we only consider initial model states relevant.
    auto memoryBuilder = storm::storage::MemoryStructureBuilder<ValueType>(this->_memoryTransitions.size(), model, onlyInitialStatesRelevant);

    // Build the transitions between the memory states: startState to goalState using modelStates (transitionVector).
    for (storm::storage::sparse::state_type startState = 0; startState < this->_memoryTransitions.size(); ++startState) {
        for (storm::storage::sparse::state_type goalState = 0; goalState < this->_memoryTransitions.size(); ++goalState) {
            // Bitvector that represents modelStates the model states that trigger this transition.
            memoryBuilder.setTransition(startState, goalState, this->_memoryTransitions[startState][goalState]);
        }
    }

    // InitialMemoryStates: Assign an initial memory state model states
    if (onlyInitialStatesRelevant) {
        // Only consider initial model states
        for (uint_fast64_t modelState : model.getInitialStates()) {
            memoryBuilder.setInitialMemoryState(modelState, this->_memoryInitialStates[modelState]);
        }
    } else {
        // All model states are relevant
        for (uint_fast64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) {
            memoryBuilder.setInitialMemoryState(modelState, this->_memoryInitialStates[modelState]);
        }
    }

    // Build the memoryStructure.
    storm::storage::MemoryStructure memoryStructure = memoryBuilder.build();

    // Create a scheduler (with memory) for the model from the REACH and MEC scheduler of the MDP-DA-product model.
    storm::storage::Scheduler<ValueType> scheduler(model.getNumberOfStates(), memoryStructure);

    // Use choices in the product model to create a choice based on model state and memory state
    for (const auto& choice : this->_producedChoices) {
        // <s, q, InfSet> -> choice
        storm::storage::sparse::state_type modelState = std::get<0>(choice.first);
        storm::storage::sparse::state_type daState = std::get<1>(choice.first);
        uint_fast64_t infSet = std::get<2>(choice.first);
        STORM_LOG_ASSERT(!this->_dontCareStates[getMemoryState(daState, infSet)].get(modelState), "Tried to set choice for dontCare state.");
        scheduler.setChoice(choice.second, modelState, getMemoryState(daState, infSet));
    }

    // Set "dontCare" states
    for (uint_fast64_t memoryState = 0; memoryState < this->_dontCareStates.size(); ++memoryState) {
        for (auto state : this->_dontCareStates[memoryState]) {
            scheduler.setDontCare(state, memoryState);
        }
    }

    // Sanity check for created scheduler.
    STORM_LOG_ASSERT(scheduler.isDeterministicScheduler(), "Expected a deterministic scheduler.");
    STORM_LOG_ASSERT(!scheduler.isPartialScheduler(), "Expected a fully defined scheduler.");

    return scheduler;
}

template class SparseLTLSchedulerHelper<double, false>;
template class SparseLTLSchedulerHelper<double, true>;

template class SparseLTLSchedulerHelper<storm::RationalNumber, false>;
template class SparseLTLSchedulerHelper<storm::RationalNumber, true>;
template class SparseLTLSchedulerHelper<storm::RationalFunction, false>;

}  // namespace internal
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
