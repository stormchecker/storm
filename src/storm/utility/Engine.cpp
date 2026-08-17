#include "storm/utility/Engine.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/csl/HybridCtmcCslModelChecker.h"
#include "storm/modelchecker/csl/HybridMarkovAutomatonCslModelChecker.h"
#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"
#include "storm/modelchecker/csl/SparseMarkovAutomatonCslModelChecker.h"
#include "storm/modelchecker/exploration/SparseExplorationModelChecker.h"
#include "storm/modelchecker/prctl/HybridDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/HybridMdpPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SymbolicDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SymbolicMdpPrctlModelChecker.h"
#include "storm/modelchecker/rpatl/SparseSmgRpatlModelChecker.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Property.h"
#include "storm/utility/macros.h"

namespace storm {
namespace utility {

// Returns a list of all available engines (excluding Unknown)
std::vector<Engine> getEngines() {
    std::vector<Engine> res;
    for (int i = 0; i != static_cast<int>(Engine::Unknown); ++i) {
        res.push_back(static_cast<Engine>(i));
    }
    return res;
}

std::string toString(Engine const& engine) {
    switch (engine) {
        case Engine::Sparse:
            return "sparse";
        case Engine::Hybrid:
            return "hybrid";
        case Engine::Dd:
            return "dd";
        case Engine::DdSparse:
            return "dd-to-sparse";
        case Engine::Exploration:
            return "expl";
        case Engine::AbstractionRefinement:
            return "abs";
        case Engine::Automatic:
            return "automatic";
        case Engine::Unknown:
            return "UNKNOWN";
        default:
            STORM_LOG_ASSERT(false, "The given engine has no name assigned to it.");
            return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, Engine const& engine) {
    os << toString(engine);
    return os;
}

Engine engineFromString(std::string const& engineStr) {
    for (Engine const& e : getEngines()) {
        if (engineStr == toString(e)) {
            return e;
        }
    }
    if (engineStr == "portfolio") {
        STORM_LOG_WARN("The engine name \"portfolio\" is deprecated. The name of this engine has been changed to \"" << toString(Engine::Automatic) << "\".");
        return Engine::Automatic;
    }
    STORM_LOG_ERROR("The engine '" << engineStr << "' was not found.");
    return Engine::Unknown;
}

storm::builder::BuilderType getBuilderType(Engine const& engine) {
    switch (engine) {
        case Engine::Sparse:
            return storm::builder::BuilderType::Explicit;
        case Engine::Hybrid:
            return storm::builder::BuilderType::Dd;
        case Engine::Dd:
            return storm::builder::BuilderType::Dd;
        case Engine::DdSparse:
            return storm::builder::BuilderType::Dd;
        case Engine::Exploration:
            return storm::builder::BuilderType::Explicit;
        case Engine::AbstractionRefinement:
            return storm::builder::BuilderType::Dd;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The given engine has no builder type to it.");
            return storm::builder::BuilderType::Explicit;
    }
}

template<typename ValueType>
bool canHandle(storm::utility::Engine const& engine, storm::storage::SymbolicModelDescription::ModelType const& modelType,
               storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    // Define types to improve readability
    typedef storm::storage::SymbolicModelDescription::ModelType ModelType;
    // The Dd library does not make much of a difference (in case of exact or parametric models we will switch to sylvan anyway).
    // Therefore, we always use sylvan here
    storm::dd::DdType const ddType = storm::dd::DdType::Sylvan;
    switch (engine) {
        case Engine::Sparse:
        case Engine::DdSparse:
            switch (modelType) {
                case ModelType::DTMC:
                    return storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>>::canHandleStatic(checkTask);
                case ModelType::MDP:
                    return storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>>::canHandleStatic(checkTask);
                case ModelType::CTMC:
                    return storm::modelchecker::SparseCtmcCslModelChecker<storm::models::sparse::Ctmc<ValueType>>::canHandleStatic(checkTask);
                case ModelType::MA:
                    return storm::modelchecker::SparseMarkovAutomatonCslModelChecker<storm::models::sparse::MarkovAutomaton<ValueType>>::canHandleStatic(
                        checkTask);
                case ModelType::POMDP:
                    return false;
                case ModelType::SMG:
                    return storm::modelchecker::SparseSmgRpatlModelChecker<storm::models::sparse::Smg<ValueType>>::canHandleStatic(checkTask);
            }
            break;
        case Engine::Hybrid:
            switch (modelType) {
                case ModelType::DTMC:
                    return storm::modelchecker::HybridDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<ddType, ValueType>>::canHandleStatic(checkTask);
                case ModelType::MDP:
                    return storm::modelchecker::HybridMdpPrctlModelChecker<storm::models::symbolic::Mdp<ddType, ValueType>>::canHandleStatic(checkTask);
                case ModelType::CTMC:
                    return storm::modelchecker::HybridCtmcCslModelChecker<storm::models::symbolic::Ctmc<ddType, ValueType>>::canHandleStatic(checkTask);
                case ModelType::MA:
                    return storm::modelchecker::HybridMarkovAutomatonCslModelChecker<
                        storm::models::symbolic::MarkovAutomaton<ddType, ValueType>>::canHandleStatic(checkTask);
                case ModelType::POMDP:
                case ModelType::SMG:
                    return false;
            }
            break;
        case Engine::Dd:
            switch (modelType) {
                case ModelType::DTMC:
                    return storm::modelchecker::SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<ddType, ValueType>>::canHandleStatic(checkTask);
                case ModelType::MDP:
                    return storm::modelchecker::SymbolicMdpPrctlModelChecker<storm::models::symbolic::Mdp<ddType, ValueType>>::canHandleStatic(checkTask);
                case ModelType::CTMC:
                case ModelType::MA:
                case ModelType::POMDP:
                case ModelType::SMG:
                    return false;
            }
            break;
        case Engine::Exploration:
            if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
                return false;
            }
            switch (modelType) {
                case ModelType::DTMC:
                    return storm::modelchecker::SparseExplorationModelChecker<storm::models::sparse::Dtmc<ValueType>>::canHandleStatic(checkTask);
                case ModelType::MDP:
                    return storm::modelchecker::SparseExplorationModelChecker<storm::models::sparse::Mdp<ValueType>>::canHandleStatic(checkTask);
                case ModelType::CTMC:
                case ModelType::MA:
                case ModelType::POMDP:
                case ModelType::SMG:
                    return false;
            }
            break;
        case Engine::AbstractionRefinement:
            if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
                return false;
            }
            switch (modelType) {
                case ModelType::DTMC:
                case ModelType::MDP:
                    // Since the corresponding model checker is in a separate library, we can only make a very crude over-approximation of what can be handled
                    // at this point.
                    return true;
                case ModelType::CTMC:
                case ModelType::MA:
                case ModelType::POMDP:
                case ModelType::SMG:
                    return false;
            }
            break;
        default:
            STORM_LOG_ERROR("The selected engine " << engine << " is not considered.");
    }
    STORM_LOG_ERROR("The selected combination of engine (" << engine << ") and model type (" << modelType
                                                           << ") does not seem to be supported for this value type.");
    return false;
}

template<>
bool canHandle<storm::RationalFunction>(storm::utility::Engine const& engine, storm::storage::SymbolicModelDescription::ModelType const& modelType,
                                        storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> const& checkTask) {
    // Define types to improve readability
    typedef storm::storage::SymbolicModelDescription::ModelType ModelType;
    // The Dd library does not make much of a difference (in case of exact or parametric models we will switch to sylvan anyway).
    // Therefore, we always use sylvan here
    storm::dd::DdType const ddType = storm::dd::DdType::Sylvan;
    switch (engine) {
        case Engine::Sparse:
        case Engine::DdSparse:
            switch (modelType) {
                case ModelType::DTMC:
                    return storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>>::canHandleStatic(checkTask);
                case ModelType::CTMC:
                    return storm::modelchecker::SparseCtmcCslModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>>::canHandleStatic(checkTask);
                case ModelType::MDP:
                case ModelType::MA:
                case ModelType::POMDP:
                case ModelType::SMG:
                    return false;
            }
            break;
        case Engine::Hybrid:
            switch (modelType) {
                case ModelType::DTMC:
                    return storm::modelchecker::HybridDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<ddType, storm::RationalFunction>>::canHandleStatic(
                        checkTask);
                case ModelType::CTMC:
                    return storm::modelchecker::HybridCtmcCslModelChecker<storm::models::symbolic::Ctmc<ddType, storm::RationalFunction>>::canHandleStatic(
                        checkTask);
                case ModelType::MDP:
                case ModelType::MA:
                case ModelType::POMDP:
                case ModelType::SMG:
                    return false;
            }
            break;
        case Engine::Dd:
            switch (modelType) {
                case ModelType::DTMC:
                    return storm::modelchecker::SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<ddType, storm::RationalFunction>>::canHandleStatic(
                        checkTask);
                case ModelType::MDP:
                case ModelType::CTMC:
                case ModelType::MA:
                case ModelType::POMDP:
                case ModelType::SMG:
                    return false;
            }
            break;
        case Engine::Exploration:
        case Engine::AbstractionRefinement:
            return false;
        default:
            STORM_LOG_ERROR("The selected engine" << engine << " is not considered.");
    }
    STORM_LOG_ERROR("The selected combination of engine (" << engine << ") and model type (" << modelType
                                                           << ") does not seem to be supported for this value type.");
    return false;
}

template<typename ValueType>
bool canHandle(storm::utility::Engine const& engine, std::vector<storm::jani::Property> const& properties,
               storm::storage::SymbolicModelDescription const& modelDescription) {
    auto canHandleFormula = [&engine, &modelDescription](auto const& formula, bool onlyInitialStatesRelevant) {
        auto task = storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(formula, onlyInitialStatesRelevant);
        bool result = canHandle(engine, modelDescription.getModelType(), task);
        STORM_LOG_INFO_COND(result, "Engine " << engine << " can not handle formula '" << task.getFormula() << "' on models of type "
                                              << modelDescription.getModelType() << ".");
        return result;
    };
    auto canHandleProperty = [&canHandleFormula](auto const& property) {
        if (property.getFilter().isDefault()) {
            return canHandleFormula(*property.getRawFormula(), true);
        } else {
            return canHandleFormula(*property.getRawFormula(), false) && canHandleFormula(*property.getFilter().getStatesFormula(), false);
        }
    };
    return std::all_of(properties.begin(), properties.end(), canHandleProperty) &&
           storm::builder::canHandle<ValueType>(getBuilderType(engine), modelDescription, properties);
}

// explicit template instantiations.
template bool canHandle<double>(storm::utility::Engine const&, std::vector<storm::jani::Property> const&, storm::storage::SymbolicModelDescription const&);
template bool canHandle<storm::RationalNumber>(storm::utility::Engine const&, std::vector<storm::jani::Property> const&,
                                               storm::storage::SymbolicModelDescription const&);
template bool canHandle<storm::RationalFunction>(storm::utility::Engine const&, std::vector<storm::jani::Property> const&,
                                                 storm::storage::SymbolicModelDescription const&);

}  // namespace utility
}  // namespace storm
