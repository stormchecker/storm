#include "storm/storage/dd/bisimulation/NondeterministicModelPartitionRefiner.h"

#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/StandardRewardModel.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
NondeterministicModelPartitionRefiner<DdType, ValueType>::NondeterministicModelPartitionRefiner(
    storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model, Partition<DdType, ValueType> const& initialStatePartition,
    BisimulationOptions const& bisimulationOptions)
    : PartitionRefiner<DdType, ValueType>(model, initialStatePartition, bisimulationOptions),
      model(model),
      choicePartition(Partition<DdType, ValueType>::createTrivialChoicePartition(model, initialStatePartition.getBlockVariables())),
      stateSignatureRefiner(model.getManager(), this->statePartition.getBlockVariable(), model.getRowVariables(), model.getColumnVariables(), true,
                            std::set<storm::expressions::Variable>(), bisimulationOptions),
      statePartitonHasBeenRefinedOnce(false) {
    // For Markov automata, we refine the state partition wrt. to their exit rates.
    if (model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        STORM_LOG_TRACE("Refining with respect to exit rates.");
        auto exitRateVector = this->model.template as<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>()->getExitRateVector();
        this->statePartition = stateSignatureRefiner.refine(this->statePartition, Signature<DdType, ValueType>(exitRateVector));
    }
}

template<storm::dd::DdType DdType, typename ValueType>
bool NondeterministicModelPartitionRefiner<DdType, ValueType>::refine(bisimulation::SignatureMode const& mode) {
    // In this procedure, we will
    // (1) refine the partition of nondeterministic choices based on the state partition. For this, we use
    // the signature computer/refiner of the superclass. These objects use the full transition matrix.
    // (2) if the choice partition was in fact split, the state partition also needs to be refined.
    // For this, we use the signature computer/refiner of this class.

    STORM_LOG_TRACE("Refining choice partition.");
    Partition<DdType, ValueType> newChoicePartition =
        this->internalRefine(this->signatureComputer, this->signatureRefiner, this->choicePartition, this->statePartition, mode);

    // If the choice partition has become stable in an iteration that is not the starting one, we have
    // reached a fixed point and can return.
    if (newChoicePartition.getNumberOfBlocks() == choicePartition.getNumberOfBlocks() && statePartitonHasBeenRefinedOnce) {
        this->status = Status::FixedPoint;
        return false;
    } else {
        this->choicePartition = newChoicePartition;

        // Compute state signatures.
        storm::dd::Bdd<DdType> choicePartitionAsBdd;
        if (this->choicePartition.storedAsBdd()) {
            choicePartitionAsBdd = this->choicePartition.asBdd();
        } else {
            choicePartitionAsBdd = this->choicePartition.asAdd().notZero();
        }

        auto signatureStart = std::chrono::high_resolution_clock::now();
        Signature<DdType, ValueType> stateSignature(choicePartitionAsBdd.existsAbstract(model.getNondeterminismVariables()).template toAdd<ValueType>());
        auto signatureEnd = std::chrono::high_resolution_clock::now();
        this->totalSignatureTime += (signatureEnd - signatureStart);

        // If the choice partition changed, refine the state partition.
        STORM_LOG_TRACE("Refining state partition.");
        auto refinementStart = std::chrono::high_resolution_clock::now();
        Partition<DdType, ValueType> newStatePartition = this->internalRefine(stateSignature, this->stateSignatureRefiner, this->statePartition);
        statePartitonHasBeenRefinedOnce = true;
        auto refinementEnd = std::chrono::high_resolution_clock::now();

        auto signatureTime = std::chrono::duration_cast<std::chrono::milliseconds>(signatureEnd - signatureStart).count();
        auto refinementTime = std::chrono::duration_cast<std::chrono::milliseconds>(refinementEnd - refinementStart).count();
        STORM_LOG_INFO("Refinement " << (this->refinements - 1) << " produced " << newStatePartition.getNumberOfBlocks() << " blocks and was completed in "
                                     << (signatureTime + refinementTime) << "ms (signature: " << signatureTime << "ms, refinement: " << refinementTime
                                     << "ms).");

        if (newStatePartition == this->statePartition) {
            this->status = Status::FixedPoint;
            return false;
        } else {
            this->statePartition = newStatePartition;
            return true;
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> const& NondeterministicModelPartitionRefiner<DdType, ValueType>::getChoicePartition() const {
    return choicePartition;
}

template<storm::dd::DdType DdType, typename ValueType>
bool NondeterministicModelPartitionRefiner<DdType, ValueType>::refineWrtStateRewards(storm::dd::Add<DdType, ValueType> const& stateRewards) {
    STORM_LOG_TRACE("Refining with respect to state rewards.");
    Partition<DdType, ValueType> newStatePartition = this->stateSignatureRefiner.refine(this->statePartition, Signature<DdType, ValueType>(stateRewards));
    if (newStatePartition == this->statePartition) {
        return false;
    } else {
        this->statePartition = newStatePartition;
        return true;
    }
}

template<storm::dd::DdType DdType, typename ValueType>
bool NondeterministicModelPartitionRefiner<DdType, ValueType>::refineWrtStateActionRewards(storm::dd::Add<DdType, ValueType> const& stateActionRewards) {
    STORM_LOG_TRACE("Refining with respect to state-action rewards.");
    Partition<DdType, ValueType> newChoicePartition = this->signatureRefiner.refine(this->choicePartition, Signature<DdType, ValueType>(stateActionRewards));
    if (newChoicePartition == this->choicePartition) {
        return false;
    } else {
        this->choicePartition = newChoicePartition;
        return true;
    }
}

template class NondeterministicModelPartitionRefiner<storm::dd::DdType::CUDD, double>;

template class NondeterministicModelPartitionRefiner<storm::dd::DdType::Sylvan, double>;
template class NondeterministicModelPartitionRefiner<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class NondeterministicModelPartitionRefiner<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
