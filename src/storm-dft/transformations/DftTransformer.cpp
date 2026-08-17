#include "storm-dft/transformations/DftTransformer.h"

#include "storm-dft/builder/DFTBuilder.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace transformations {

template<typename ValueType>
bool DftTransformer<ValueType>::hasUniqueFailedBE(storm::dft::storage::DFT<ValueType> const &dft) {
    // Count number of constant failed BE or BE with constant probability distribution (as they can also be immediately failed).
    // Return false if this number exceeds 1.
    size_t noConstFailed = 0;
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        if (element->isBasicElement()) {
            auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element);
            if (be->beType() == storm::dft::storage::elements::BEType::CONSTANT) {
                auto beConst = std::static_pointer_cast<storm::dft::storage::elements::BEConst<ValueType> const>(be);
                if (beConst->canFail()) {
                    ++noConstFailed;
                    if (noConstFailed > 1) {
                        return false;
                    }
                }
            } else if (be->beType() == storm::dft::storage::elements::BEType::PROBABILITY) {
                ++noConstFailed;
                if (noConstFailed > 1) {
                    return false;
                }
            }
        }
    }
    return true;
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> DftTransformer<ValueType>::transformUniqueFailedBE(storm::dft::storage::DFT<ValueType> const &dft) {
    storm::dft::builder::DFTBuilder<ValueType> builder;
    std::vector<std::string> failedBEs;

    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::BE: {
                auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element);
                switch (be->beType()) {
                    case storm::dft::storage::elements::BEType::CONSTANT: {
                        // Remember constant failed BEs for later
                        auto beConst = std::static_pointer_cast<storm::dft::storage::elements::BEConst<ValueType> const>(be);
                        if (beConst->canFail()) {
                            STORM_LOG_TRACE("Transform " << *beConst << " to failsafe BE.");
                            failedBEs.push_back(beConst->name());
                        }
                        // All original constant BEs are set to failsafe, failed BEs are later triggered by a new element
                        builder.addBasicElementConst(beConst->name(), false);
                        break;
                    }
                    case storm::dft::storage::elements::BEType::PROBABILITY: {
                        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                        "BE with constant probability distribution are not supported and need to be transformed before.");
                        break;
                    }
                    default:
                        // Clone other types of BEs
                        builder.cloneElement(element);
                        break;
                }
                break;
            }
            default:
                // Clone other elements
                builder.cloneElement(element);
                break;
        }
    }
    // At this point the DFT is an exact copy of the original, except for all constant failure probabilities being 0

    // Introduce new constantly failed BE and FDEPs to trigger all failures
    if (!failedBEs.empty()) {
        STORM_LOG_TRACE("Add unique constant failed BE 'Unique_Constant_Failure'");
        builder.addBasicElementConst("Unique_Constant_Failure", true);
        failedBEs.insert(failedBEs.begin(), "Unique_Constant_Failure");
        STORM_LOG_TRACE("Add FDEP 'Failure_Trigger'");
        builder.addPdep("Failure_Trigger", failedBEs, storm::utility::one<ValueType>());
    }

    builder.setTopLevel(dft.getTopLevelElement()->name());
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(builder.build());
}

template<typename ValueType>
bool DftTransformer<ValueType>::hasNonBinaryDependency(storm::dft::storage::DFT<ValueType> const &dft) {
    if (dft.getDependencies().empty()) {
        return false;
    }
    for (size_t dependencyId : dft.getDependencies()) {
        if (dft.getDependency(dependencyId)->dependentEvents().size() > 1) {
            return true;
        }
    }
    return false;
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> DftTransformer<ValueType>::transformBinaryDependencies(storm::dft::storage::DFT<ValueType> const &dft) {
    storm::dft::builder::DFTBuilder<ValueType> builder;

    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::PDEP: {
                auto dep = std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType> const>(element);
                if (dep->dependentEvents().size() == 1) {
                    // Already binary dependency -> simply clone element
                    builder.cloneElement(dep);
                } else {
                    if (!storm::utility::isOne(dep->probability())) {
                        // PDEP with probability < 1
                        STORM_LOG_TRACE("Transform " << *element);
                        // Introduce additional element to first capture the probabilistic dependency
                        std::string nameAdditional = dep->name() + "_additional";
                        STORM_LOG_TRACE("Add auxiliary BE " << nameAdditional);
                        builder.addBasicElementConst(nameAdditional, false);
                        STORM_LOG_TRACE("Add PDEP " << dep->name() << "_pdep");
                        // First consider probabilistic dependency
                        builder.addPdep(dep->name() + "_pdep", {dep->triggerEvent()->name(), nameAdditional}, dep->probability());
                        // Then consider dependencies to the children if probabilistic dependency failed
                        for (size_t j = 0; j < dep->dependentEvents().size(); ++j) {
                            std::string nameDep = dep->name() + "_" + std::to_string(j);
                            std::string dependentName = dep->dependentEvents()[j]->name();
                            STORM_LOG_TRACE("Add FDEP " << nameDep << " for " << dependentName);
                            builder.addPdep(nameDep, {nameAdditional, dependentName}, storm::utility::one<ValueType>());
                        }
                    } else {
                        // FDEP -> add explicit dependencies for each dependent event
                        STORM_LOG_TRACE("Transform " << *element);
                        for (size_t j = 0; j < dep->dependentEvents().size(); ++j) {
                            std::string nameDep = dep->name() + "_" + std::to_string(j);
                            std::string dependentName = dep->dependentEvents()[j]->name();
                            STORM_LOG_TRACE("Add FDEP " << nameDep << " for " << dependentName);
                            builder.addPdep(nameDep, {dep->triggerEvent()->name(), dependentName}, storm::utility::one<ValueType>());
                        }
                    }
                }
                break;
            }
            default:
                // Clone other elements
                builder.cloneElement(element);
                break;
        }
    }

    builder.setTopLevel(dft.getTopLevelElement()->name());
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(builder.build());
}

template<typename ValueType>
bool DftTransformer<ValueType>::hasOnlyExponentialDistributions(storm::dft::storage::DFT<ValueType> const &dft) {
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        if (element->isBasicElement()) {
            auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element);
            if (be->beType() != storm::dft::storage::elements::BEType::EXPONENTIAL && be->beType() != storm::dft::storage::elements::BEType::CONSTANT) {
                return false;
            }
        }
    }
    return true;
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> DftTransformer<ValueType>::transformExponentialDistributions(
    storm::dft::storage::DFT<ValueType> const &dft) {
    storm::dft::builder::DFTBuilder<ValueType> builder;

    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::BE: {
                auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element);
                switch (be->beType()) {
                    case storm::dft::storage::elements::BEType::PROBABILITY: {
                        STORM_LOG_TRACE("Replace " << *be << " with constant failsafe BE and PDEP.");
                        auto beProb = std::static_pointer_cast<storm::dft::storage::elements::BEProbability<ValueType> const>(be);
                        // Model BE with constant probability by PDEP
                        // Introduce constant failed element as trigger
                        std::string triggerName = "constantBeTrigger_" + beProb->name();
                        builder.addBasicElementConst(triggerName, true);
                        // Add constant failsafe element for original BE
                        builder.addBasicElementConst(beProb->name(), false);
                        // Add PDEP in which the probability corresponds to the failure probability of the original BE
                        builder.addPdep(beProb->name() + "_pdep", {triggerName, beProb->name()}, beProb->activeFailureProbability());
                        break;
                    }
                    case storm::dft::storage::elements::BEType::ERLANG: {
                        STORM_LOG_TRACE("Replace " << *be << " with multiple BE and SEQ.");
                        auto beErlang = std::static_pointer_cast<storm::dft::storage::elements::BEErlang<ValueType> const>(be);
                        // Model BE with Erlang distribution by using SEQ over BEs instead.
                        std::vector<std::string> childNames;
                        builder.addBasicElementExponential(beErlang->name(), beErlang->activeFailureRate(), beErlang->dormancyFactor());
                        // For each phase a BE is added
                        for (size_t j = 0; j < beErlang->phases() - 1; ++j) {
                            std::string beName = beErlang->name() + "_" + std::to_string(j);
                            childNames.push_back(beName);
                            builder.addBasicElementExponential(beName, beErlang->activeFailureRate(), beErlang->dormancyFactor());
                        }
                        childNames.push_back(beErlang->name());
                        // SEQ ensures the ordered failure.
                        builder.addSequenceEnforcer(beErlang->name() + "_seq", childNames);
                        break;
                    }
                    default:
                        // Clone other types of BEs
                        builder.cloneElement(be);
                        break;
                }
                break;
            }
            default:
                // Clone other elements
                builder.cloneElement(element);
                break;
        }
    }

    builder.setTopLevel(dft.getTopLevelElement()->name());
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(builder.build());
}

// Explicitly instantiate the class.
template class DftTransformer<double>;
template class DftTransformer<RationalFunction>;

}  // namespace transformations
}  // namespace storm::dft
