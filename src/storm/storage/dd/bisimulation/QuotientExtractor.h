#pragma once

#include <memory>

#include "storm/storage/dd/DdType.h"

#include "storm/models/sparse/Model.h"
#include "storm/models/symbolic/Model.h"

#include "storm/storage/dd/bisimulation/BisimulationOptions.h"
#include "storm/storage/dd/bisimulation/Partition.h"
#include "storm/storage/dd/bisimulation/PreservationInformation.h"
#include "storm/storage/dd/bisimulation/QuotientFormat.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType = ValueType>
class QuotientExtractor {
   public:
    QuotientExtractor(storm::dd::bisimulation::QuotientFormat const& quotientFormat, BisimulationOptions const& bisimulationOptions);

    std::shared_ptr<storm::models::Model<ExportValueType>> extract(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                   Partition<DdType, ValueType> const& partition,
                                                                   PreservationInformation<DdType, ValueType> const& preservationInformation);

   private:
    std::shared_ptr<storm::models::sparse::Model<ExportValueType>> extractSparseQuotient(
        storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
        PreservationInformation<DdType, ValueType> const& preservationInformation);

    std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>> extractDdQuotient(
        storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
        PreservationInformation<DdType, ValueType> const& preservationInformation);
    std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>> extractQuotientUsingBlockVariables(
        storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
        PreservationInformation<DdType, ValueType> const& preservationInformation);
    std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>> extractQuotientUsingOriginalVariables(
        storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
        PreservationInformation<DdType, ValueType> const& preservationInformation);

    bool useRepresentatives;
    bool useOriginalVariables;
    storm::dd::bisimulation::QuotientFormat quotientFormat;
};

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
