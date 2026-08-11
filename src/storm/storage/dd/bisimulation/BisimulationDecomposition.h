#pragma once

#include <memory>
#include <vector>

#include "storm/logic/Formula.h"
#include "storm/storage/bisimulation/BisimulationType.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/bisimulation/BisimulationOptions.h"
#include "storm/storage/dd/bisimulation/PreservationInformation.h"
#include "storm/storage/dd/bisimulation/QuotientFormat.h"
#include "storm/storage/dd/bisimulation/SignatureMode.h"

namespace storm {
namespace models {
template<typename ValueType>
class Model;

namespace symbolic {
template<storm::dd::DdType DdType, typename ValueType>
class Model;
}
}  // namespace models

namespace dd {
namespace bisimulation {
template<storm::dd::DdType DdType, typename ValueType>
class Partition;

template<storm::dd::DdType DdType, typename ValueType>
class PartitionRefiner;

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
class PartialQuotientExtractor;
}  // namespace bisimulation

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType = ValueType>
class BisimulationDecomposition {
   public:
    BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model, storm::storage::BisimulationType const& bisimulationType,
                              bisimulation::BisimulationOptions const& bisimulationOptions = bisimulation::BisimulationOptions());
    BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model, storm::storage::BisimulationType const& bisimulationType,
                              bisimulation::PreservationInformation<DdType, ValueType> const& preservationInformation,
                              bisimulation::BisimulationOptions const& bisimulationOptions = bisimulation::BisimulationOptions());
    BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model,
                              std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                              storm::storage::BisimulationType const& bisimulationType,
                              bisimulation::BisimulationOptions const& bisimulationOptions = bisimulation::BisimulationOptions());
    BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model,
                              bisimulation::Partition<DdType, ValueType> const& initialPartition,
                              bisimulation::PreservationInformation<DdType, ValueType> const& preservationInformation,
                              bisimulation::BisimulationOptions const& bisimulationOptions = bisimulation::BisimulationOptions());

    ~BisimulationDecomposition();

    /*!
     * Performs partition refinement until a fixpoint has been reached.
     */
    void compute(bisimulation::SignatureMode const& mode = bisimulation::SignatureMode::Eager);

    /*!
     * Performs the given number of refinement steps.
     *
     * @return True iff the computation arrived at a fixpoint.
     */
    bool compute(uint64_t steps, bisimulation::SignatureMode const& mode = bisimulation::SignatureMode::Eager);

    /*!
     * Retrieves whether a fixed point has been reached. Depending on this, extracting a quotient will either
     * give a full quotient or a partial one.
     */
    bool getReachedFixedPoint() const;

    /*!
     * Retrieves the quotient model after the bisimulation decomposition was computed.
     */
    std::shared_ptr<storm::models::Model<ExportValueType>> getQuotient(storm::dd::bisimulation::QuotientFormat const& quotientFormat) const;

   private:
    void initialize();
    void refineWrtRewardModels();

    // The model for which to compute the bisimulation decomposition.
    storm::models::symbolic::Model<DdType, ValueType> const& model;

    // The object capturing what is preserved.
    bisimulation::PreservationInformation<DdType, ValueType> preservationInformation;

    // The configuration used for the bisimulation minimization.
    bisimulation::BisimulationOptions bisimulationOptions;

    // The refiner to use.
    std::unique_ptr<bisimulation::PartitionRefiner<DdType, ValueType>> refiner;

    // A quotient extractor that is used when the fixpoint has not been reached yet.
    mutable std::unique_ptr<bisimulation::PartialQuotientExtractor<DdType, ValueType, ExportValueType>> partialQuotientExtractor;

    // A flag indicating whether progress is reported.
    bool verboseProgress;

    // The delay between progress reports.
    uint64_t showProgressDelay;
};

}  // namespace dd
}  // namespace storm
