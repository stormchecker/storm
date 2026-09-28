#include "storm/storage/dd/bisimulation/BisimulationDecomposition.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/Model.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/bisimulation/NondeterministicModelPartitionRefiner.h"
#include "storm/storage/dd/bisimulation/PartialQuotientExtractor.h"
#include "storm/storage/dd/bisimulation/Partition.h"
#include "storm/storage/dd/bisimulation/PartitionRefiner.h"
#include "storm/storage/dd/bisimulation/QuotientExtractor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

using namespace bisimulation;

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<PartitionRefiner<DdType, ValueType>> createRefiner(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                   Partition<DdType, ValueType> const& initialPartition,
                                                                   BisimulationOptions const& bisimulationOptions) {
    if (model.isOfType(storm::models::ModelType::Mdp) || model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        return std::make_unique<NondeterministicModelPartitionRefiner<DdType, ValueType>>(
            *model.template as<storm::models::symbolic::NondeterministicModel<DdType, ValueType>>(), initialPartition, bisimulationOptions);
    } else {
        return std::make_unique<PartitionRefiner<DdType, ValueType>>(model, initialPartition, bisimulationOptions);
    }
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
BisimulationDecomposition<DdType, ValueType, ExportValueType>::BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                                         storm::storage::BisimulationType const& bisimulationType,
                                                                                         bisimulation::BisimulationOptions const& bisimulationOptions)
    : model(model),
      preservationInformation(model),
      bisimulationOptions(bisimulationOptions),
      refiner(createRefiner(model, Partition<DdType, ValueType>::create(model, bisimulationType, preservationInformation), bisimulationOptions)) {
    this->initialize();
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
BisimulationDecomposition<DdType, ValueType, ExportValueType>::BisimulationDecomposition(
    storm::models::symbolic::Model<DdType, ValueType> const& model, storm::storage::BisimulationType const& bisimulationType,
    bisimulation::PreservationInformation<DdType, ValueType> const& preservationInformation, bisimulation::BisimulationOptions const& bisimulationOptions)
    : model(model),
      preservationInformation(preservationInformation),
      bisimulationOptions(bisimulationOptions),
      refiner(createRefiner(model, Partition<DdType, ValueType>::create(model, bisimulationType, preservationInformation), bisimulationOptions)) {
    this->initialize();
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
BisimulationDecomposition<DdType, ValueType, ExportValueType>::BisimulationDecomposition(
    storm::models::symbolic::Model<DdType, ValueType> const& model, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
    storm::storage::BisimulationType const& bisimulationType, bisimulation::BisimulationOptions const& bisimulationOptions)
    : model(model),
      preservationInformation(model, formulas),
      bisimulationOptions(bisimulationOptions),
      refiner(createRefiner(model, Partition<DdType, ValueType>::create(model, bisimulationType, formulas, bisimulationOptions), bisimulationOptions)) {
    this->initialize();
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
BisimulationDecomposition<DdType, ValueType, ExportValueType>::BisimulationDecomposition(
    storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& initialPartition,
    bisimulation::PreservationInformation<DdType, ValueType> const& preservationInformation, bisimulation::BisimulationOptions const& bisimulationOptions)
    : model(model),
      preservationInformation(preservationInformation),
      bisimulationOptions(bisimulationOptions),
      refiner(createRefiner(model, initialPartition, bisimulationOptions)) {
    this->initialize();
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
BisimulationDecomposition<DdType, ValueType, ExportValueType>::~BisimulationDecomposition() = default;

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
void BisimulationDecomposition<DdType, ValueType, ExportValueType>::initialize() {
    verboseProgress = bisimulationOptions.verbose;
    showProgressDelay = bisimulationOptions.showProgressDelay;

    auto start = std::chrono::high_resolution_clock::now();
    this->refineWrtRewardModels();
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_INFO("Refining with respect to reward models took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    STORM_LOG_INFO("Initial partition has " << refiner->getStatePartition().getNumberOfBlocks() << " blocks.");
    STORM_LOG_TRACE("Initial partition has " << refiner->getStatePartition().getNodeCount() << " nodes.");
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
void BisimulationDecomposition<DdType, ValueType, ExportValueType>::compute(bisimulation::SignatureMode const& mode) {
    STORM_LOG_ASSERT(refiner, "No suitable refiner.");
    STORM_LOG_ASSERT(this->refiner->getStatus() != Status::FixedPoint, "Can only proceed if no fixpoint has been reached yet.");

    auto start = std::chrono::high_resolution_clock::now();
    auto timeOfLastMessage = start;
    uint64_t iterations = 0;
    bool refined = true;
    while (refined) {
        refined = refiner->refine(mode);

        ++iterations;

        auto now = std::chrono::high_resolution_clock::now();
        auto durationSinceLastMessage = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeOfLastMessage).count();
        if (static_cast<uint64_t>(durationSinceLastMessage) >= showProgressDelay * 1000 || verboseProgress) {
            auto durationSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            STORM_LOG_INFO("State partition after " << iterations << " iterations (" << durationSinceStart << "ms) has "
                                                    << refiner->getStatePartition().getNumberOfBlocks() << " blocks.");
            timeOfLastMessage = std::chrono::high_resolution_clock::now();
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    STORM_LOG_INFO("Partition refinement completed in "
                   << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms (" << iterations
                   << " iterations, signature: " << std::chrono::duration_cast<std::chrono::milliseconds>(refiner->getTotalSignatureTime()).count()
                   << "ms, refinement: " << std::chrono::duration_cast<std::chrono::milliseconds>(refiner->getTotalRefinementTime()).count() << "ms).");
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
bool BisimulationDecomposition<DdType, ValueType, ExportValueType>::compute(uint64_t steps, bisimulation::SignatureMode const& mode) {
    STORM_LOG_ASSERT(refiner, "No suitable refiner.");
    STORM_LOG_ASSERT(this->refiner->getStatus() != Status::FixedPoint, "Can only proceed if no fixpoint has been reached yet.");
    STORM_LOG_ASSERT(steps > 0, "Can only perform positive number of steps.");

    auto start = std::chrono::high_resolution_clock::now();
    auto timeOfLastMessage = start;
    uint64_t iterations = 0;
    bool refined = true;
    while (refined && iterations < steps) {
        refined = refiner->refine(mode);

        ++iterations;

        auto now = std::chrono::high_resolution_clock::now();
        auto durationSinceLastMessage = std::chrono::duration_cast<std::chrono::seconds>(now - timeOfLastMessage).count();
        if (static_cast<uint64_t>(durationSinceLastMessage) >= showProgressDelay || verboseProgress) {
            auto durationSinceStart = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            STORM_LOG_INFO("State partition after " << iterations << " iterations (" << durationSinceStart << "ms) has "
                                                    << refiner->getStatePartition().getNumberOfBlocks() << " blocks.");
            timeOfLastMessage = std::chrono::high_resolution_clock::now();
        }
    }

    return !refined;
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
bool BisimulationDecomposition<DdType, ValueType, ExportValueType>::getReachedFixedPoint() const {
    return this->refiner->getStatus() == Status::FixedPoint;
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::Model<ExportValueType>> BisimulationDecomposition<DdType, ValueType, ExportValueType>::getQuotient(
    storm::dd::bisimulation::QuotientFormat const& quotientFormat) const {
    std::shared_ptr<storm::models::Model<ExportValueType>> quotient;
    if (this->refiner->getStatus() == Status::FixedPoint) {
        STORM_LOG_INFO("Starting full quotient extraction.");
        QuotientExtractor<DdType, ValueType, ExportValueType> extractor(quotientFormat, bisimulationOptions);
        quotient = extractor.extract(model, refiner->getStatePartition(), preservationInformation);
    } else {
        STORM_LOG_THROW(model.getType() == storm::models::ModelType::Dtmc || model.getType() == storm::models::ModelType::Mdp,
                        storm::exceptions::InvalidOperationException, "Can only extract partial quotient for discrete-time models.");

        STORM_LOG_INFO("Starting partial quotient extraction.");
        if (!partialQuotientExtractor) {
            partialQuotientExtractor = std::make_unique<bisimulation::PartialQuotientExtractor<DdType, ValueType, ExportValueType>>(model, quotientFormat);
        }

        quotient = partialQuotientExtractor->extract(refiner->getStatePartition(), preservationInformation);
    }

    STORM_LOG_INFO("Quotient extraction done.");
    return quotient;
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
void BisimulationDecomposition<DdType, ValueType, ExportValueType>::refineWrtRewardModels() {
    for (auto const& rewardModelName : this->preservationInformation.getRewardModelNames()) {
        auto const& rewardModel = this->model.getRewardModel(rewardModelName);
        refiner->refineWrtRewardModel(rewardModel);
    }
}

template class BisimulationDecomposition<storm::dd::DdType::CUDD, double>;

template class BisimulationDecomposition<storm::dd::DdType::Sylvan, double>;
template class BisimulationDecomposition<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class BisimulationDecomposition<storm::dd::DdType::Sylvan, storm::RationalNumber, double>;
template class BisimulationDecomposition<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace dd
}  // namespace storm
