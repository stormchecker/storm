#pragma once

#include "storm/storage/dd/bisimulation/BisimulationOptions.h"
#include "storm/storage/dd/bisimulation/Partition.h"
#include "storm/storage/dd/bisimulation/Status.h"

#include "storm/storage/dd/bisimulation/SignatureComputer.h"
#include "storm/storage/dd/bisimulation/SignatureRefiner.h"

namespace storm {
namespace models {
namespace symbolic {
template<storm::dd::DdType DdType, typename ValueType>
class Model;
}
}  // namespace models

namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
class PartitionRefiner {
   public:
    PartitionRefiner(storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& initialStatePartition,
                     BisimulationOptions const& bisimulationOptions);

    virtual ~PartitionRefiner() = default;

    /*!
     * Refines the partition.
     *
     * @param mode The signature mode to use.
     * @return False iff the partition is stable and no refinement was actually performed.
     */
    virtual bool refine(SignatureMode const& mode = SignatureMode::Eager);

    /*!
     * Refines the partition wrt. to the reward model.
     * @return True iff the partition is stable and no refinement was actually performed.
     */
    bool refineWrtRewardModel(storm::models::symbolic::StandardRewardModel<DdType, ValueType> const& rewardModel);

    /*!
     * Retrieves the current state partition in the refinement process.
     */
    Partition<DdType, ValueType> const& getStatePartition() const;

    /*!
     * Retrieves the full signature of all states wrt. the current state partition.
     */
    Signature<DdType, ValueType> getFullSignature() const;

    /*!
     * Retrieves the status of the refinement process.
     */
    Status getStatus() const;

    std::chrono::high_resolution_clock::duration getTotalSignatureTime() const;
    std::chrono::high_resolution_clock::duration getTotalRefinementTime() const;

   protected:
    Partition<DdType, ValueType> internalRefine(SignatureComputer<DdType, ValueType>& stateSignatureComputer,
                                                SignatureRefiner<DdType, ValueType>& signatureRefiner, Partition<DdType, ValueType> const& oldPartition,
                                                Partition<DdType, ValueType> const& targetPartition, SignatureMode const& mode = SignatureMode::Eager);
    Partition<DdType, ValueType> internalRefine(Signature<DdType, ValueType> const& signature, SignatureRefiner<DdType, ValueType>& signatureRefiner,
                                                Partition<DdType, ValueType> const& oldPartition);

    virtual bool refineWrtStateRewards(storm::dd::Add<DdType, ValueType> const& stateRewards);
    virtual bool refineWrtStateActionRewards(storm::dd::Add<DdType, ValueType> const& stateActionRewards);

    // The current status.
    Status status;

    // The number of refinements that were made.
    uint64_t refinements;

    // The state partition in the refinement process. Initially set to the initial partition.
    Partition<DdType, ValueType> statePartition;

    // The object used to compute the signatures.
    SignatureComputer<DdType, ValueType> signatureComputer;

    // The object used to refine the state partition based on the signatures.
    SignatureRefiner<DdType, ValueType> signatureRefiner;

    // Time measurements.
    std::chrono::high_resolution_clock::duration totalSignatureTime;
    std::chrono::high_resolution_clock::duration totalRefinementTime;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
