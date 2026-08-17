#include "FDEPConflictFinder.h"

#include "storm-dft/modelchecker/DFTASFChecker.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace utility {

template<>
std::vector<std::pair<uint64_t, uint64_t>> FDEPConflictFinder<double>::getDependencyConflicts(storm::dft::storage::DFT<double> const& dft, bool useSMT,
                                                                                              uint_fast64_t timeout) {
    std::shared_ptr<storm::dft::modelchecker::DFTASFChecker> smtChecker = nullptr;
    if (useSMT) {
        storm::dft::modelchecker::DFTASFChecker checker(dft);
        smtChecker = std::make_shared<storm::dft::modelchecker::DFTASFChecker>(checker);
        smtChecker->toSolver();
    }

    std::vector<bool> dynamicBehavior = getDynamicBehavior(dft);

    std::vector<std::pair<uint64_t, uint64_t>> res;
    uint64_t dep1Index;
    uint64_t dep2Index;
    for (size_t i = 0; i < dft.getDependencies().size(); ++i) {
        dep1Index = dft.getDependencies().at(i);
        for (size_t j = i + 1; j < dft.getDependencies().size(); ++j) {
            dep2Index = dft.getDependencies().at(j);
            if (dynamicBehavior[dep1Index] && dynamicBehavior[dep2Index]) {
                if (useSMT) {  // if an SMT solver is to be used
                    if (dft.getDependency(dep1Index)->triggerEvent() == dft.getDependency(dep2Index)->triggerEvent()) {
                        STORM_LOG_TRACE("Conflict between " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name()
                                                            << ": Same trigger");
                        res.emplace_back(std::pair<uint64_t, uint64_t>(dep1Index, dep2Index));
                    } else {
                        switch (smtChecker->checkDependencyConflict(dep1Index, dep2Index, timeout)) {
                            case storm::solver::SmtSolver::CheckResult::Sat:
                                STORM_LOG_TRACE("Conflict between " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name());
                                res.emplace_back(std::pair<uint64_t, uint64_t>(dep1Index, dep2Index));
                                break;
                            case storm::solver::SmtSolver::CheckResult::Unknown:
                                STORM_LOG_TRACE("Unknown: Conflict between " << dft.getElement(dep1Index)->name() << " and "
                                                                             << dft.getElement(dep2Index)->name());
                                res.emplace_back(std::pair<uint64_t, uint64_t>(dep1Index, dep2Index));
                                break;
                            default:
                                STORM_LOG_TRACE("No conflict between " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name());
                                break;
                        }
                    }
                } else {
                    STORM_LOG_TRACE("Conflict between " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name());
                    res.emplace_back(std::pair<uint64_t, uint64_t>(dep1Index, dep2Index));
                }
            } else {
                STORM_LOG_TRACE("Static behavior: No conflict between " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name());
                break;
            }
        }
    }
    return res;
}

template<>
std::vector<std::pair<uint64_t, uint64_t>> FDEPConflictFinder<storm::RationalFunction>::getDependencyConflicts(
    storm::dft::storage::DFT<storm::RationalFunction> const& dft, bool useSMT, uint_fast64_t timeout) {
    if (useSMT) {
        STORM_LOG_WARN("SMT encoding for rational functions is not supported");
    }

    std::vector<bool> dynamicBehavior = getDynamicBehavior(dft);

    std::vector<std::pair<uint64_t, uint64_t>> res;
    uint64_t dep1Index;
    uint64_t dep2Index;
    for (size_t i = 0; i < dft.getDependencies().size(); ++i) {
        dep1Index = dft.getDependencies().at(i);
        for (size_t j = i + 1; j < dft.getDependencies().size(); ++j) {
            dep2Index = dft.getDependencies().at(j);
            if (dynamicBehavior[dep1Index] && dynamicBehavior[dep2Index]) {
                STORM_LOG_TRACE("Conflict between " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name());
                res.emplace_back(std::pair<uint64_t, uint64_t>(dep1Index, dep2Index));
            } else {
                STORM_LOG_TRACE("Static behavior: No conflict between " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name());
                break;
            }
        }
    }
    return res;
}

template<typename ValueType>
std::vector<bool> FDEPConflictFinder<ValueType>::getDynamicBehavior(storm::dft::storage::DFT<ValueType> const& dft) {
    std::vector<bool> dynamicBehaviorVector(dft.nrElements(), false);

    std::queue<std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const>> elementQueue;

    // deal with all dynamic elements
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        auto element = dft.getElement(i);
        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::PAND:
            case storm::dft::storage::elements::DFTElementType::POR:
            case storm::dft::storage::elements::DFTElementType::MUTEX: {
                auto gate = std::static_pointer_cast<storm::dft::storage::elements::DFTChildren<ValueType> const>(element);
                dynamicBehaviorVector[gate->id()] = true;
                for (auto const& child : gate->children()) {
                    // only enqueue static children
                    if (!dynamicBehaviorVector.at(child->id())) {
                        elementQueue.push(child);
                    }
                }
                break;
            }
                // TODO different cases
            case storm::dft::storage::elements::DFTElementType::SPARE: {
                auto spare = std::static_pointer_cast<storm::dft::storage::elements::DFTSpare<ValueType> const>(element);

                // Iterate over all children (representatives of spare modules)
                for (auto const& child : spare->children()) {
                    // Case 1: Shared Module
                    // If child only has one parent, it is this SPARE -> nothing to check
                    if (child->nrParents() > 1) {
                        // TODO make more efficient by directly setting ALL spares which share a module to be dynamic
                        for (auto const& parent : child->parents()) {
                            if (parent->isSpareGate() and parent->id() != spare->id()) {
                                dynamicBehaviorVector[spare->id()] = true;
                                break;  // inner loop
                            }
                        }
                    }
                    // Case 2: Triggering outside events
                    // If the SPARE was already detected to have dynamic behavior, do not proceed
                    if (!dynamicBehaviorVector[spare->id()]) {
                        for (auto const& memberID : dft.module(child->id()).getElements()) {
                            // Iterate over all members of the module child represents
                            auto member = dft.getElement(memberID);
                            for (auto const& dep : member->outgoingDependencies()) {
                                // If the member has outgoing dependencies, check if those trigger something outside the module
                                for (auto const& depEvent : dep->dependentEvents()) {
                                    // If a dependent event is not found in the module, SPARE is dynamic
                                    auto const& childModuleElements = dft.module(child->id()).getElements();
                                    if (std::find(childModuleElements.begin(), childModuleElements.end(), depEvent->id()) == childModuleElements.end()) {
                                        dynamicBehaviorVector[spare->id()] = true;
                                        break;  // depEvent-loop
                                    }
                                }
                                if (dynamicBehaviorVector[spare->id()]) {
                                    break;
                                }  // dependency-loop
                            }
                            if (dynamicBehaviorVector[spare->id()]) {
                                break;
                            }  // module-loop
                        }
                    }
                    if (dynamicBehaviorVector[spare->id()]) {
                        break;
                    }  // child-loop
                }
                // if during the computation, dynamic behavior was detected, add children to queue
                if (dynamicBehaviorVector[spare->id()]) {
                    for (auto const& child : spare->children()) {
                        // only enqueue static children
                        if (!dynamicBehaviorVector.at(child->id())) {
                            elementQueue.push(child);
                        }
                    }
                }
                break;
            }
            case storm::dft::storage::elements::DFTElementType::SEQ: {
                auto seq = std::static_pointer_cast<storm::dft::storage::elements::DFTSeq<ValueType> const>(element);
                // A SEQ only has dynamic behavior if not all children are BEs
                if (!seq->allChildrenBEs()) {
                    dynamicBehaviorVector[seq->id()] = true;
                    for (auto const& child : seq->children()) {
                        // only enqueue static children
                        if (!dynamicBehaviorVector.at(child->id())) {
                            elementQueue.push(child);
                        }
                    }
                }
                break;
            }
            default: {
                break;
            }
        }
    }
    // propagate dynamic behavior
    while (!elementQueue.empty()) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> currentElement = elementQueue.front();
        elementQueue.pop();
        switch (currentElement->type()) {
            // Static Gates
            case storm::dft::storage::elements::DFTElementType::AND:
            case storm::dft::storage::elements::DFTElementType::OR:
            case storm::dft::storage::elements::DFTElementType::VOT: {
                // check all parents and if one has dynamic behavior, propagate it
                dynamicBehaviorVector[currentElement->id()] = true;
                auto gate = std::static_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType> const>(currentElement);
                for (auto const& child : gate->children()) {
                    // only enqueue static children
                    if (!dynamicBehaviorVector.at(child->id())) {
                        elementQueue.push(child);
                    }
                }
                break;
            }
                // BEs
            case storm::dft::storage::elements::DFTElementType::BE: {
                auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(currentElement);
                dynamicBehaviorVector[be->id()] = true;
                // add all ingoing dependencies to queue
                for (auto const& dep : be->ingoingDependencies()) {
                    if (!dynamicBehaviorVector.at(dep->id())) {
                        elementQueue.push(dep);
                    }
                }
                break;
            }
            case storm::dft::storage::elements::DFTElementType::PDEP: {
                auto dep = std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType> const>(currentElement);
                dynamicBehaviorVector[dep->id()] = true;
                // add all ingoing dependencies to queue
                auto trigger = dep->triggerEvent();
                if (!dynamicBehaviorVector.at(trigger->id())) {
                    elementQueue.push(trigger);
                }
                break;
            }
            default:
                break;
        }
    }
    return dynamicBehaviorVector;
}

template class FDEPConflictFinder<double>;
template class FDEPConflictFinder<storm::RationalFunction>;

}  // namespace utility
}  // namespace storm::dft
