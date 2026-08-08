#pragma once

#include "storm/storage/dd/bisimulation/PartitionRefiner.h"

namespace storm {
namespace models {
namespace symbolic {
template<storm::dd::DdType DdType, typename ValueType>
class Mdp;
}
}  // namespace models

namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
class NondeterministicModelPartitionRefiner : public PartitionRefiner<DdType, ValueType> {
   public:
    NondeterministicModelPartitionRefiner(storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                          Partition<DdType, ValueType> const& initialStatePartition, BisimulationOptions const& bisimulationOptions);

    /*!
     * Refines the partition.
     *
     * @param mode The signature mode to use.
     * @return False iff the partition is stable and no refinement was actually performed.
     */
    virtual bool refine(bisimulation::SignatureMode const& mode = bisimulation::SignatureMode::Eager) override;

    /*!
     * Retrieves the current choice partition in the refinement process.
     */
    Partition<DdType, ValueType> const& getChoicePartition() const;

   private:
    virtual bool refineWrtStateActionRewards(storm::dd::Add<DdType, ValueType> const& stateActionRewards) override;

    virtual bool refineWrtStateRewards(storm::dd::Add<DdType, ValueType> const& stateRewards) override;

    /// The model to refine.
    storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model;

    /// The choice partition in the refinement process.
    Partition<DdType, ValueType> choicePartition;

    /// The object used to refine the state partition based on the signatures.
    SignatureRefiner<DdType, ValueType> stateSignatureRefiner;

    /// A flag indicating whether the state partition has been refined at least once.
    bool statePartitonHasBeenRefinedOnce;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
