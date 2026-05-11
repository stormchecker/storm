#include "storm/builder/BuilderType.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/builder/DdJaniModelBuilder.h"
#include "storm/builder/DdPrismModelBuilder.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/generator/JaniNextStateGenerator.h"
#include "storm/generator/PrismNextStateGenerator.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/jani/ModelFeatures.h"
#include "storm/utility/macros.h"

namespace storm {
namespace builder {

storm::jani::ModelFeatures getSupportedJaniFeatures(BuilderType const& builderType) {
    // The supported jani features should be independent of the ValueType and Dd type. We just take sylvan and double for all.
    storm::dd::DdType const ddType = storm::dd::DdType::Sylvan;
    typedef double ValueType;
    switch (builderType) {
        case BuilderType::Explicit:
            return storm::generator::JaniNextStateGenerator<ValueType>::getSupportedJaniFeatures();
        case BuilderType::Dd:
            return storm::builder::DdJaniModelBuilder<ddType, ValueType>::getSupportedJaniFeatures();
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected builder type.");
}

template<typename ValueType>
bool canHandle(BuilderType const& builderType, storm::storage::SymbolicModelDescription const& modelDescription,
               storm::OptionalRef<std::vector<storm::jani::Property> const> properties) {
    storm::dd::DdType const ddType = storm::dd::DdType::Sylvan;
    if (!modelDescription.hasModel()) {
        // If there is no model to be build, we assume that the task of obtaining a model is either not required or can be accomplished somehow.
        return true;
    }
    STORM_LOG_THROW(modelDescription.isPrismProgram() || modelDescription.isJaniModel(), storm::exceptions::UnexpectedException,
                    "The model is neither PRISM nor Jani which is not expected.");
    switch (builderType) {
        case BuilderType::Explicit:
            if (modelDescription.isPrismProgram()) {
                return storm::generator::PrismNextStateGenerator<ValueType>::canHandle(modelDescription.asPrismProgram());
            } else {
                return storm::generator::JaniNextStateGenerator<ValueType>::canHandle(modelDescription.asJaniModel());
            }
        case BuilderType::Dd:
            if (modelDescription.isPrismProgram()) {
                return storm::builder::DdPrismModelBuilder<ddType, ValueType>::canHandle(modelDescription.asPrismProgram());
            } else {
                return storm::builder::DdJaniModelBuilder<ddType, ValueType>::canHandle(modelDescription.asJaniModel(), properties);
            }
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled builderType.");
    return false;
}

template bool canHandle<double>(BuilderType const& builderType, storm::storage::SymbolicModelDescription const& modelDescription,
                                storm::OptionalRef<std::vector<storm::jani::Property> const> properties);
template bool canHandle<storm::RationalNumber>(BuilderType const& builderType, storm::storage::SymbolicModelDescription const& modelDescription,
                                               storm::OptionalRef<std::vector<storm::jani::Property> const> properties);
template bool canHandle<storm::RationalFunction>(BuilderType const& builderType, storm::storage::SymbolicModelDescription const& modelDescription,
                                                 storm::OptionalRef<std::vector<storm::jani::Property> const> properties);

}  // namespace builder
}  // namespace storm