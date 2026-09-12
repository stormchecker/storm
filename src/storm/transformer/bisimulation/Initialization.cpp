#include "storm/transformer/bisimulation/Initialization.h"

#include <algorithm>
#include <map>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/AtomicExpressionFormula.h"
#include "storm/logic/AtomicLabelFormula.h"
#include "storm/logic/Formula.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

namespace {
auto resolveStateLabelPreservationOption(StateLabelPreservation input, bool haveFormulas) {
    using enum StateLabelPreservation;
    if (input == Default) {
        return haveFormulas ? FormulaPropositional : All;
    }
    return input;
}

}  // namespace
template<typename ValueType>
PreservationInformation Initialization<ValueType>::getPreservationInformation() const {
    auto const stateLabelPreservation = resolveStateLabelPreservationOption(options.stateLabelPreservation, !formulas.empty());
    storm::bisimulation::PreservationInformation information;
    // Add all labels that appear in all formulas
    for (auto const& f : formulas) {
        f->gatherReferencedRewardModels(information.preservedRewardModels);
        // A reward operator without a reward model name refers to the unique reward model of the model, cf. Model::getRewardModel.
        if (information.preservedRewardModels.contains("") && !model.getRewardModels().contains("")) {
            information.preservedRewardModels.erase("");
            information.preservedRewardModels.insert(model.getUniqueRewardModelName());
        }
        if (stateLabelPreservation != StateLabelPreservation::None) {
            for (auto const& l : f->getAtomicLabelFormulas()) {
                information.preservedStateLabels.insert(l->getLabel());
            }
            for (auto const& e : f->getAtomicExpressionFormulas()) {
                information.preservedStateLabels.insert(e->toString());
            }
        }
        STORM_LOG_ASSERT(std::all_of(information.preservedStateLabels.begin(), information.preservedStateLabels.end(),
                                     [this](std::string const& label) { return model.getStateLabeling().containsLabel(label); }),
                         "Formula " << *f << " uses a label that is not known in the model.");
        STORM_LOG_ASSERT(std::all_of(information.preservedRewardModels.begin(), information.preservedRewardModels.end(),
                                     [this](std::string const& rew) { return model.getRewardModels().contains(rew); }),
                         "Formula " << *f << " uses a reward model that is not known in the model.");
    }
    // if requested, also add all state labels of the model
    if (stateLabelPreservation == StateLabelPreservation::All) {
        for (auto const& label : model.getStateLabeling().getLabels()) {
            if (label == "init") {
                continue;  // "init" must not be preserved
            }
            information.preservedStateLabels.insert(label);
        }
    }
    if (options.preserveAllRewards.value_or(formulas.empty())) {
        for (auto const& [name, rm] : model.getRewardModels()) {
            information.preservedRewardModels.insert(name);
        }
    }
    // if requested and available, preserve the choice labels
    if (options.preserveChoiceLabels && model.hasChoiceLabeling()) {
        for (auto const& label : model.getChoiceLabeling().getLabels()) {
            information.preservedChoiceLabels.insert(label);
        }
    }
    return information;
}

template<typename ValueType>
Initialization<ValueType>::Initialization(storm::models::sparse::Model<ValueType> const& model, Options const& options,
                                          std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas)
    : model(model), options(options), formulas(formulas) {
    // sanity checks
    if (options.bisimulationType == BisimulationType::Weak) {
        STORM_LOG_THROW(model.isOfType(storm::models::ModelType::Dtmc) || model.isOfType(storm::models::ModelType::Ctmc),
                        storm::exceptions::NotSupportedException,
                        "Weak bisimulation is only implemented for DTMCs and CTMCs, but the given model is of type " << model.getType() << ".");
        // Weak bisimulation generally does not preserve formulas that depend on the number of steps taken.
        storm::logic::FragmentSpecification preservedFragment = storm::logic::propositional();
        preservedFragment.setProbabilityOperatorsAllowed(true)
            .setUntilFormulasAllowed(true)
            .setReachabilityProbabilityFormulasAllowed(true)
            .setGloballyFormulasAllowed(true)
            .setRewardOperatorsAllowed(true)
            .setReachabilityRewardFormulasAllowed(true);
        if (model.isOfType(storm::models::ModelType::Ctmc)) {
            // Weak bisimulation on a CTMC preserves the distribution of the time spent within a block, so time bounds and expected times are fine.
            // Step bounds are not, which is why we only enable the time-based cases here.
            preservedFragment.setBoundedUntilFormulasAllowed(true)
                .setTimeBoundedUntilFormulasAllowed(true)
                .setTimeOperatorsAllowed(true)
                .setReachbilityTimeFormulasAllowed(true);
        }
        for (auto const& f : this->formulas) {
            STORM_LOG_THROW(f->isInFragment(preservedFragment), storm::exceptions::IllegalFunctionCallException,
                            "The formula " << *f << " is not known to be preserved by weak bisimulation.");
        }
    }

    auto const preservationInformation = getPreservationInformation();
    auto const stateLabelPreservation = resolveStateLabelPreservationOption(options.stateLabelPreservation, !formulas.empty());

    // Go through all relevant labels / rewards / model components and add them to the corresponding preserved annotations.
    if (stateLabelPreservation != StateLabelPreservation::FormulaPropositional) {
        // Each preserved label is considered on its own. Note that nothing is preserved for StateLabelPreservation::None.
        for (auto const& label : preservationInformation.preservedStateLabels) {
            preservedStateAnnotations.addBoolean(model.getStateLabeling().getStates(label));
        }
    } else {
        // The formulas only observe the labels through their maximal propositional subformulas, so it suffices to preserve the truth values of those.
        using PropositionalChecker = storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<ValueType>>;
        PropositionalChecker propositionalChecker(model);
        auto const collectPropositionalStates = [this, &propositionalChecker](storm::logic::Formula const& subformula) {
            if (!propositionalChecker.canHandle(subformula)) {
                // The checker handles exactly the propositional formulas, so we have to continue with the subformulas.
                return true;
            } else if (subformula.isBooleanLiteralFormula()) {
                // Boolean literals do not distinguish any states, so we can skip them.
                return false;
            }
            auto states = propositionalChecker.check(subformula)
                              ->template asExplicitQualitativeCheckResult<typename PropositionalChecker::SolutionType>()
                              .getTruthValuesVector();
            // A subformula that holds in all or no states, or in the same states as a previous one, does not distinguish any further states.
            if (!states.empty() && !states.full() &&
                std::ranges::none_of(preservedStateAnnotations.getBooleans(), [&states](auto const& b) { return b == states; })) {
                preservedStateAnnotations.addBoolean(std::move(states));
            }
            return false;
        };
        for (auto const& f : this->formulas) {
            f->traverse(collectPropositionalStates);
        }
        // If a formula considers the init label, we need to preserve it precisely. Otherwise, e.g. "P=? [ F "init" & "a" ]" would be handled incorrectly:
        // A block containing some "init"-states and some "a"-states (but no "init" & "a"-states) could get collapsed into a state with both, the "init"-label
        // and the "a" label (obtained through the representative state)
        if (preservationInformation.preservedStateLabels.contains("init")) {
            preservedStateAnnotations.addBoolean(model.getStateLabeling().getStates("init"));
        }
    }
    for (auto const& label : preservationInformation.preservedChoiceLabels) {
        STORM_LOG_ASSERT(model.hasChoiceLabeling(), "Preserving choice labels is only possible if the model has a choice labeling.");
        preservedChoiceAnnotations.addBoolean(model.getChoiceLabeling().getChoices(label));
    }
    if (model.hasChoiceOrigins() && options.preserveChoiceOrigins) {
        preservedChoiceAnnotations.integers.emplace_back(model.getChoiceOrigins()->getIdentifiers());
    }
    for (auto const& rewName : preservationInformation.preservedRewardModels) {
        auto const& rewardModel = model.getRewardModel(rewName);
        STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                        "Bisimulation initialization does not handle transition rewards of reward model '" << rewName << "'.");
        // Weak bisimulation on a CTMC drops the transitions within a block. The state rewards of a CTMC are rate rewards, so they are unaffected (the
        // distribution of the time spent within a block is preserved), but the state-action rewards are earned per taken transition and thus are not.
        STORM_LOG_THROW(
            !(options.bisimulationType == BisimulationType::Weak && model.isOfType(storm::models::ModelType::Ctmc) && rewardModel.hasStateActionRewards()),
            storm::exceptions::NotSupportedException,
            "Weak bisimulation on CTMCs does not preserve the state-action rewards of reward model '" << rewName << "'.");
        if (rewardModel.hasStateRewards()) {
            preservedStateAnnotations.values.emplace_back(rewardModel.getStateRewardVector());
        }
        if (rewardModel.hasStateActionRewards()) {
            preservedChoiceAnnotations.values.emplace_back(rewardModel.getStateActionRewardVector());
        }
    }
    using enum storm::models::ModelType;
    if (model.isOfType(MarkovAutomaton)) {
        auto const& ma = model.template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
        preservedStateAnnotations.addBoolean(ma->getMarkovianStates());
        preservedStateAnnotations.values.emplace_back(ma->getExitRates());
    } else {
        STORM_LOG_THROW(model.isOfType(Dtmc) || model.isOfType(Ctmc) || model.isOfType(Mdp), storm::exceptions::NotSupportedException,
                        "Bisimulation initialization is not implemented for model type '" << model.getType() << "'.");
    }
}

template<typename ValueType>
void Initialization<ValueType>::PreservedAnnotations::addBoolean(storm::storage::BitVector const& annotation) {
    optionallyOwnedBooleans.emplace_back(std::cref(annotation));
}

template<typename ValueType>
void Initialization<ValueType>::PreservedAnnotations::addBoolean(storm::storage::BitVector&& annotation) {
    optionallyOwnedBooleans.emplace_back(std::move(annotation));
}

template<typename ValueType>
bool Initialization<ValueType>::PreservedAnnotations::empty() const {
    return optionallyOwnedBooleans.empty() && integers.empty() && values.empty();
}

template<typename ValueType>
void Initialization<ValueType>::PreservedAnnotations::applySplit(Partition& partition, ValueType const& tolerance,
                                                                 std::vector<uint64_t> const& extraAnnotation) const {
    uint64_t const numElements = partition.getNumberOfElements();

    // Split according to Boolean annotations
    for (auto const& b : getBooleans()) {
        STORM_LOG_ASSERT(numElements == b.size(), "Boolean annotation has wrong size.");
        partition.forEachBlock([&b, &numElements, &partition](auto const& block) {
            // No need to split singleton blocks.
            if (block.size() > 1) {
                // For large blocks, it's likely cheaper to iterate over the 'true' elements of b
                if (block.size() * 64 > numElements) {
                    partition.splitBlockByRange(block, b);
                } else {
                    partition.splitBlockByPredicate(block, [&b](uint64_t const e) { return b.get(e); });
                }
            }
        });
    }

    // Helper for the remaining numeric annotations
    auto splitByNumeric = [&numElements, &partition, &tolerance](auto const& v) {
        STORM_LOG_ASSERT(numElements == v.size(), "Annotation has wrong size.");
        auto const less = [&v](auto const& a, auto const& b) { return v[a] < v[b]; };
        if constexpr (std::is_same_v<ValueType, typename std::remove_cvref_t<decltype(v)>::value_type>) {
            if (!storm::utility::isZero(tolerance)) {
                auto const lessTol = [&v, &tolerance](auto const& a, auto const& b) {
                    // A zero value is never grouped with a non-zero one, whether an annotation (e.g. reward) is zero can be semantically relevant
                    if (storm::utility::isZero(v[a]) || storm::utility::isZero(v[b])) {
                        return v[a] < v[b];
                    }
                    return v[a] + tolerance < v[b];
                };
                partition.forEachBlock([&partition, &less, &lessTol](auto const& block) {
                    if (block.size() > 1) {  // No need to split singleton blocks
                        partition.splitBlockByOrder(block, less, lessTol);
                    }
                });
                return;
            }
        }
        partition.forEachBlock([&partition, &less](auto const& block) {
            if (block.size() > 1) {
                partition.splitBlockByOrder(block, less);
            }
        });
    };

    for (auto const& v : integers) {
        splitByNumeric(v);
    }
    for (auto const& v : values) {
        splitByNumeric(v);
    }
    if (!extraAnnotation.empty()) {
        splitByNumeric(extraAnnotation);
    }
}

template<typename ValueType>
std::optional<std::vector<uint64_t>> Initialization<ValueType>::getChoiceClasses() const {
    // Terminate early if there aren't any choice classes.
    if (preservedChoiceAnnotations.empty() && (!options.actionSensitive || !model.isNondeterministicModel())) {
        return std::nullopt;
    }
    // Auxiliary vector, will later hold the resulting classes.
    std::vector<uint64_t> auxVector;

    // Create a partition of the choices
    Partition choicePartition(model.getNumberOfChoices());
    if (options.actionSensitive && model.isNondeterministicModel()) {
        auxVector.reserve(model.getNumberOfChoices());
        // fill the auxVector with the local choice indices and treat it as any other annotation.
        for (uint64_t state = 0; state < model.getNumberOfStates(); ++state) {
            for (uint64_t act = 0; act < model.getTransitionMatrix().getRowGroupSize(state); ++act) {
                auxVector.push_back(act);
            }
        }
    }
    // Split the partition based on preserved choice annotations.
    preservedChoiceAnnotations.applySplit(choicePartition, storm::utility::convertNumber<ValueType>(options.tolerance), auxVector);

    // Catch the case where all choices are equal so we don't have to deal with choice classes.
    if (choicePartition.getNumberOfBlocks() == 1) {
        return std::nullopt;
    }

    // Prepare the result
    auxVector.resize(model.getNumberOfChoices());
    uint64_t choiceClass = 0;
    choicePartition.forEachBlock([&choiceClass, &auxVector](auto const& block) {
        for (uint64_t const choice : block) {
            auxVector[choice] = choiceClass;
        }
        ++choiceClass;
    });
    return std::optional<std::vector<uint64_t>>(std::move(auxVector));
}

template<typename ValueType>
Partition Initialization<ValueType>::getInitialStatePartition(std::optional<std::vector<uint64_t>> const& choiceClasses) const {
    ValueType const tolerance = storm::utility::convertNumber<ValueType>(options.tolerance);
    Partition statePartition(model.getNumberOfStates());
    if (choiceClasses && model.isNondeterministicModel()) {
        auto const& groupIndices = model.getTransitionMatrix().getRowGroupIndices();
        // We need to distinguish the states by the available choice classes
        // We compute for each state the actionSignature, which is the set of available choice classes represented as ascending, deduplicated vector
        using Signature = std::vector<uint64_t>;
        std::vector<uint64_t> stateActionSignature;
        stateActionSignature.reserve(model.getNumberOfStates());
        std::map<Signature, uint64_t> actionSignatureToIndex;
        for (uint64_t state = 0; state < model.getNumberOfStates(); ++state) {
            // Compute signature
            Signature actionSignature(choiceClasses->begin() + groupIndices[state], choiceClasses->begin() + groupIndices[state + 1]);
            std::sort(actionSignature.begin(), actionSignature.end());
            actionSignature.erase(std::unique(actionSignature.begin(), actionSignature.end()), actionSignature.end());
            // Retrieve the index of the signature, potentially adding a new signature if it is not already known.
            uint64_t const signatureIndex = actionSignatureToIndex.emplace(actionSignature, actionSignatureToIndex.size()).first->second;
            // Store the signature index for this state.
            stateActionSignature.push_back(signatureIndex);
        }
        // split the partition based on preserved state annotations and the state action signatures.
        preservedStateAnnotations.applySplit(statePartition, tolerance, stateActionSignature);
    } else if (choiceClasses && !model.isNondeterministicModel()) {
        STORM_LOG_ASSERT(choiceClasses->size() == model.getNumberOfStates(), "Unexpected number of choice classes.");
        // As there is exactly one choice per state, there is no need for computing the action signatures
        preservedStateAnnotations.applySplit(statePartition, tolerance, choiceClasses.value());
    } else {
        preservedStateAnnotations.applySplit(statePartition, tolerance);
    }
    return statePartition;
}

namespace detail {

/*!
 * Computes the states from which no state outside of their own block can be reached.
 */
template<typename ValueType>
storm::storage::BitVector computeDivergentStates(storm::storage::SparseMatrix<ValueType> const& transitions,
                                                 storm::storage::SparseMatrix<ValueType> const& backwardTransitions, Partition const& partition) {
    //  We compute the complement: a state with a transition leaving its block is non-divergent, and so is every state that can reach such a state without
    //  leaving the block. We do a backwards DFS from the states that are at the boundary of the block.
    storm::storage::BitVector divergentStates(partition.getNumberOfElements(), true);
    std::vector<uint64_t> stateStack;
    partition.forEachBlock([&transitions, &backwardTransitions, &partition, &divergentStates, &stateStack](Partition::Block const& block) {
        // Find the states that can leave the block in a single step.
        for (uint64_t const state : block) {
            for (auto const& entry : transitions.getRow(state)) {
                if (!storm::utility::isZero(entry.getValue()) && !partition.isBlockOfElement(block, entry.getColumn())) {
                    divergentStates.set(state, false);
                    stateStack.push_back(state);
                    break;
                }
            }
        }
        // Everything that reaches one of those can leave the block as well.
        while (!stateStack.empty()) {
            uint64_t const currentState = stateStack.back();
            stateStack.pop_back();
            for (auto const& predecessorEntry : backwardTransitions.getRow(currentState)) {
                uint64_t const predecessor = predecessorEntry.getColumn();
                if (divergentStates.get(predecessor) && partition.isBlockOfElement(block, predecessor)) {
                    divergentStates.set(predecessor, false);
                    stateStack.push_back(predecessor);
                }
            }
        }
    });
    return divergentStates;
}

/*!
 * Computes the states all of whose transitions stay within their own block.
 */
template<typename ValueType>
storm::storage::BitVector computeSilentStates(storm::storage::SparseMatrix<ValueType> const& transitions, Partition const& partition) {
    storm::storage::BitVector silentStates(partition.getNumberOfElements(), false);
    for (uint64_t state = 0; state < partition.getNumberOfElements(); ++state) {
        silentStates.set(state, isSilentState(transitions, partition, state));
    }
    return silentStates;
}

}  // namespace detail

template<typename ValueType>
WeakBisimulationData Initialization<ValueType>::getWeakBisimulationData(Partition& partition,
                                                                        storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                        PreservationInformation const& preservationInformation) const {
    STORM_LOG_ASSERT(options.bisimulationType == BisimulationType::Weak, "Weak bisimulation data requested for a non-weak bisimulation.");
    STORM_LOG_ASSERT(!model.isNondeterministicModel(), "Weak bisimulation is only supported for deterministic models.");
    uint64_t const numberOfStates = model.getNumberOfStates();
    // Identify step-sensitive states: those states where the number of block-internal steps taken affects the semantics. This concerns rewards in DTMC.
    // We will apply strong bisimulation on those states.
    // The result is homogeneous per block, since the initial partition never groups a zero reward with a non-zero one, cf. PreservedAnnotations::applySplit.
    storm::storage::BitVector stepSensitiveStates(numberOfStates, false);
    if (model.isDiscreteTimeModel()) {
        for (auto const& rewardModelName : preservationInformation.preservedRewardModels) {
            stepSensitiveStates |= ~model.getRewardModel(rewardModelName).getStatesWithZeroReward(model.getTransitionMatrix());
        }
    } else {
        // This step does not apply to CTMC: The state rewards of a CTMC are rate rewards. We have already excluded non-state rewards for weak bisimulation
        // above. Weak bisimulation preserves the distribution of the time spent within a block and thus the accumulated (state) reward.
        STORM_LOG_ASSERT(std::all_of(preservationInformation.preservedRewardModels.begin(), preservationInformation.preservedRewardModels.end(),
                                     [this](std::string const& rewardModelName) {
                                         auto const& rewardModel = model.getRewardModel(rewardModelName);
                                         return !rewardModel.hasStateActionRewards() && !rewardModel.hasTransitionRewards();
                                     }),
                         "Weak bisimulation on a continuous-time model preserves a reward model with state-action or transition rewards, which the "
                         "constructor should have rejected.");
    }

    storm::storage::BitVector divergentStates = detail::computeDivergentStates(model.getTransitionMatrix(), backwardTransitions, partition);

    // Split off the divergent states so that the partition becomes homogeneous. Note that this cannot invalidate the step sensitive states, since a block is
    // only ever split into sub-blocks that all carry the flag of their super-block.
    if (!divergentStates.empty()) {
        partition.forEachBlock([&partition, &divergentStates](Partition::Block const& block) {
            if (block.size() > 1) {
                partition.splitBlockByPredicate(block, [&divergentStates](uint64_t const state) { return divergentStates.get(state); });
            }
        });
    }

    WeakBisimulationData result(std::move(divergentStates), std::move(stepSensitiveStates),
                                detail::computeSilentStates(model.getTransitionMatrix(), partition));
    STORM_LOG_ASSERT(result.checkBlockHomogeneity(partition), "Partition is not homogeneous with respect to the weak bisimulation data.");
    return result;
}

template class Initialization<double>;
template class Initialization<storm::RationalNumber>;
template class Initialization<storm::RationalFunction>;
template class Initialization<storm::Interval>;
template class Initialization<storm::RationalInterval>;

}  // namespace storm::bisimulation
