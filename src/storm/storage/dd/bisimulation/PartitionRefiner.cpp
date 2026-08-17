#include "storm/storage/dd/bisimulation/PartitionRefiner.h"

#include "storm/models/symbolic/StandardRewardModel.h"

#include "storm/storage/dd/DdManager.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
PartitionRefiner<DdType, ValueType>::PartitionRefiner(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                      Partition<DdType, ValueType> const& initialStatePartition, BisimulationOptions const& bisimulationOptions)
    : status(Status::Initialized),
      refinements(0),
      statePartition(initialStatePartition),
      signatureComputer(model),
      signatureRefiner(model.getManager(), statePartition.getBlockVariable(), model.getRowAndNondeterminismVariables(), model.getColumnVariables(),
                       !model.isNondeterministicModel(), model.getNondeterminismVariables(), bisimulationOptions),
      totalSignatureTime(0),
      totalRefinementTime(0) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
bool PartitionRefiner<DdType, ValueType>::refine(SignatureMode const& mode) {
    Partition<DdType, ValueType> newStatePartition = this->internalRefine(signatureComputer, signatureRefiner, statePartition, statePartition, mode);
    if (statePartition.getNumberOfBlocks() == newStatePartition.getNumberOfBlocks()) {
        this->status = Status::FixedPoint;
        return false;
    } else {
        this->statePartition = newStatePartition;
        return true;
    }
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> PartitionRefiner<DdType, ValueType>::internalRefine(SignatureComputer<DdType, ValueType>& signatureComputer,
                                                                                 SignatureRefiner<DdType, ValueType>& signatureRefiner,
                                                                                 Partition<DdType, ValueType> const& oldPartition,
                                                                                 Partition<DdType, ValueType> const& targetPartition,
                                                                                 SignatureMode const& mode) {
    auto start = std::chrono::high_resolution_clock::now();

    if (this->status != Status::FixedPoint) {
        this->status = Status::InComputation;

        signatureComputer.setSignatureMode(mode);

        std::chrono::milliseconds::rep signatureTime = 0;
        std::chrono::milliseconds::rep refinementTime = 0;

        bool refined = false;
        uint64_t index = 0;
        Partition<DdType, ValueType> newPartition;
        auto signatureIterator = signatureComputer.compute(targetPartition);
        while (signatureIterator.hasNext() && !refined) {
            auto signatureStart = std::chrono::high_resolution_clock::now();
            auto signature = signatureIterator.next();
            auto signatureEnd = std::chrono::high_resolution_clock::now();
            totalSignatureTime += (signatureEnd - signatureStart);
            STORM_LOG_TRACE("Signature " << refinements << "[" << index << "] DD has " << signature.getSignatureAdd().getNodeCount() << " nodes.");

            auto refinementStart = std::chrono::high_resolution_clock::now();
            newPartition = signatureRefiner.refine(oldPartition, signature);
            auto refinementEnd = std::chrono::high_resolution_clock::now();
            totalRefinementTime += (refinementEnd - refinementStart);

            signatureTime += std::chrono::duration_cast<std::chrono::milliseconds>(signatureEnd - signatureStart).count();
            refinementTime = std::chrono::duration_cast<std::chrono::milliseconds>(refinementEnd - refinementStart).count();

            // Potentially exit early in case we have refined the partition already.
            if (newPartition.getNumberOfBlocks() > oldPartition.getNumberOfBlocks()) {
                refined = true;
            }
        }

        auto totalTimeInRefinement = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        STORM_LOG_INFO("Refinement " << refinements << " produced " << newPartition.getNumberOfBlocks() << " blocks and was completed in "
                                     << totalTimeInRefinement << "ms (signature: " << signatureTime << "ms, refinement: " << refinementTime << "ms).");
        ++refinements;
        return newPartition;
    } else {
        return oldPartition;
    }
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> PartitionRefiner<DdType, ValueType>::internalRefine(Signature<DdType, ValueType> const& signature,
                                                                                 SignatureRefiner<DdType, ValueType>& signatureRefiner,
                                                                                 Partition<DdType, ValueType> const& oldPartition) {
    STORM_LOG_TRACE("Signature " << refinements << " DD has " << signature.getSignatureAdd().getNodeCount() << " nodes.");
    auto refinementStart = std::chrono::high_resolution_clock::now();
    auto newPartition = signatureRefiner.refine(oldPartition, signature);
    auto refinementEnd = std::chrono::high_resolution_clock::now();
    totalRefinementTime += (refinementEnd - refinementStart);

    ++refinements;
    return newPartition;
}

template<storm::dd::DdType DdType, typename ValueType>
bool PartitionRefiner<DdType, ValueType>::refineWrtRewardModel(storm::models::symbolic::StandardRewardModel<DdType, ValueType> const& rewardModel) {
    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                    "Symbolic bisimulation currently does not support transition rewards.");
    STORM_LOG_TRACE("Refining with respect to reward model.");
    bool result = false;
    if (rewardModel.hasStateRewards()) {
        result |= refineWrtStateRewards(rewardModel.getStateRewardVector());
    }
    if (rewardModel.hasStateActionRewards()) {
        result |= refineWrtStateActionRewards(rewardModel.getStateActionRewardVector());
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
bool PartitionRefiner<DdType, ValueType>::refineWrtStateRewards(storm::dd::Add<DdType, ValueType> const& stateRewards) {
    STORM_LOG_TRACE("Refining with respect to state rewards.");
    Partition<DdType, ValueType> newPartition = signatureRefiner.refine(statePartition, Signature<DdType, ValueType>(stateRewards));
    if (newPartition == statePartition) {
        return false;
    } else {
        this->statePartition = newPartition;
        return true;
    }
}

template<storm::dd::DdType DdType, typename ValueType>
bool PartitionRefiner<DdType, ValueType>::refineWrtStateActionRewards(storm::dd::Add<DdType, ValueType> const& stateActionRewards) {
    STORM_LOG_TRACE("Refining with respect to state-action rewards.");
    // By default, we treat state-action rewards just like state-rewards, which works for DTMCs and CTMCs.
    return refineWrtStateRewards(stateActionRewards);
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> const& PartitionRefiner<DdType, ValueType>::getStatePartition() const {
    return statePartition;
}

template<storm::dd::DdType DdType, typename ValueType>
Signature<DdType, ValueType> PartitionRefiner<DdType, ValueType>::getFullSignature() const {
    return signatureComputer.getFullSignature(statePartition);
}

template<storm::dd::DdType DdType, typename ValueType>
Status PartitionRefiner<DdType, ValueType>::getStatus() const {
    return status;
}

template<storm::dd::DdType DdType, typename ValueType>
std::chrono::high_resolution_clock::duration PartitionRefiner<DdType, ValueType>::getTotalSignatureTime() const {
    return totalSignatureTime;
}

template<storm::dd::DdType DdType, typename ValueType>
std::chrono::high_resolution_clock::duration PartitionRefiner<DdType, ValueType>::getTotalRefinementTime() const {
    return totalRefinementTime;
}

template class PartitionRefiner<storm::dd::DdType::CUDD, double>;

template class PartitionRefiner<storm::dd::DdType::Sylvan, double>;
template class PartitionRefiner<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class PartitionRefiner<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
