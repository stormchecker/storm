#include "DftModularizer.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace utility {

template<typename ValueType>
storm::dft::storage::DftIndependentModule DftModularizer<ValueType>::computeModules(storm::dft::storage::DFT<ValueType> const &dft) {
    // Initialize data structures
    // dfsCounters/elementInfos must not be cleared because they are either not initialized or were cleared in a previous call of computeModules()
    for (auto const &id : dft.getAllIds()) {
        dfsCounters[id] = DfsCounter{};
        modInfos[id] = ModularizationInfo{};
    }
    lastDate = 0;

    // First depth first search of the LTA/DR algorithm.
    populateDfsCounters(dft.getTopLevelElement());

    // Second depth first search of the LTA/DR algorithm.
    obtainModules(dft.getTopLevelElement());
    auto &topModInfo{modInfos.at(dft.getTopLevelElement()->id())};
    STORM_LOG_ASSERT(topModInfo.isModule, "Top element should form module.");
    STORM_LOG_ASSERT(topModInfo.elements.empty(), "Top module does not exist.");
    STORM_LOG_ASSERT(topModInfo.submodules.size() == 1, "Top element should form unique module.");
    // Get top module
    storm::dft::storage::DftIndependentModule topModule = *topModInfo.submodules.begin();

    // Free some space
    dfsCounters.clear();
    modInfos.clear();

    return topModule;
}

template<typename ValueType>
void DftModularizer<ValueType>::populateDfsCounters(DFTElementCPointer const element) {
    auto &counter{dfsCounters.at(element->id())};

    ++lastDate;
    if (counter.firstVisit == 0) {
        // parent was never visited before
        // as 0 can never be a valid firstVisit
        counter.firstVisit = lastDate;

        // Recursively visit children
        for (auto const &child : getChildren(element)) {
            populateDfsCounters(child);
        }
        ++lastDate;
        counter.secondVisit = lastDate;
    }
    if (counter.secondVisit == 0) {
        // Will set lastDate before secondDate -> encountered cycle
        STORM_LOG_WARN("Modularizer encountered a cycle containing the element " << *element << ".");
        // The algorithm still terminates as the children have not been visited again.
    }
    counter.lastVisit = lastDate;
}

template<typename ValueType>
void DftModularizer<ValueType>::obtainModules(DFTElementCPointer const element) {
    auto &counter{dfsCounters.at(element->id())};
    auto &modInfo{modInfos.at(element->id())};

    if (counter.minFirstVisit == 0) {
        // element was never visited before as min can never be 0
        // Add current element
        modInfo.elements.insert(element->id());

        // minFirstVisit <= secondVisit
        counter.minFirstVisit = counter.secondVisit;
        // maxLastVisit >= firstVisit
        counter.maxLastVisit = counter.firstVisit;
        for (auto const &child : getChildren(element)) {
            obtainModules(child);

            // Set min/max visit times
            auto const &childCounter{dfsCounters.at(child->id())};
            counter.minFirstVisit = std::min({counter.minFirstVisit, childCounter.firstVisit, childCounter.minFirstVisit});
            counter.maxLastVisit = std::max({counter.maxLastVisit, childCounter.lastVisit, childCounter.maxLastVisit});

            // Fill information
            auto const &childInfo{modInfos.at(child->id())};
            // Only insert elements and submodules once
            modInfo.elements.insert(childInfo.elements.begin(), childInfo.elements.end());
            modInfo.submodules.insert(childInfo.submodules.begin(), childInfo.submodules.end());
            modInfo.fullyStatic = modInfo.fullyStatic && childInfo.fullyStatic;
            if (!childInfo.isStatic && !childInfo.isModule) {
                modInfo.isStatic = false;
            }
        }

        if (!element->isStaticElement()) {
            modInfo.isStatic = false;
            modInfo.fullyStatic = false;
        }

        if (counter.firstVisit < counter.minFirstVisit && counter.maxLastVisit < counter.secondVisit) {
            // Create new module
            storm::dft::storage::DftIndependentModule module(element->id(), modInfo.elements, modInfo.submodules, modInfo.isStatic, modInfo.fullyStatic,
                                                             element->isBasicElement());
            // Update information
            modInfo.elements = {};
            modInfo.submodules = {module};
            modInfo.isModule = true;
        }
    }
}

template<typename ValueType>
std::vector<typename DftModularizer<ValueType>::DFTElementCPointer> DftModularizer<ValueType>::getChildren(DFTElementCPointer const element) {
    std::vector<DFTElementCPointer> children{};
    if (element->isDependency()) {
        auto const dependency{std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType> const>(element)};
        auto const triggerElement{std::static_pointer_cast<storm::dft::storage::elements::DFTElement<ValueType> const>(dependency->triggerEvent())};
        children.push_back(triggerElement);
        children.insert(children.end(), dependency->dependentEvents().begin(), dependency->dependentEvents().end());
    } else if (element->isGate() || element->isRestriction()) {
        auto const parent{std::static_pointer_cast<storm::dft::storage::elements::DFTChildren<ValueType> const>(element)};
        children.insert(children.end(), parent->children().begin(), parent->children().end());
    } else {
        STORM_LOG_ASSERT(element->isBasicElement(), "Element " << *element << " has invalid type.");
    }

    // For each child we also compute the dependencies/restrictions affecting each child
    // These "affecting elements" also act as children of the given element
    // That way, we model dependencies/restrictions affecting a child as having a dummy input to the given element
    std::vector<DFTElementCPointer> affectingElements{};
    for (auto const &child : children) {
        if (child->isBasicElement()) {
            // Add ingoing dependencies
            auto const be{std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(child)};
            for (auto const &ingoingDependency : be->ingoingDependencies()) {
                if (ingoingDependency->id() != element->id() && !containsElement(affectingElements, ingoingDependency)) {
                    affectingElements.push_back(ingoingDependency);
                }
            }
        }

        for (auto const &restriction : child->restrictions()) {
            if (restriction->id() != element->id() && !containsElement(affectingElements, restriction)) {
                affectingElements.push_back(restriction);
            }
        }

        for (auto const &outgoingDependency : child->outgoingDependencies()) {
            if (outgoingDependency->id() != element->id() && !containsElement(affectingElements, outgoingDependency)) {
                affectingElements.push_back(outgoingDependency);
            }
        }
    }

    children.insert(children.end(), affectingElements.begin(), affectingElements.end());
    return children;
}

template<typename ValueType>
bool DftModularizer<ValueType>::containsElement(std::vector<DFTElementCPointer> const &list, DFTElementCPointer const element) {
    return std::find_if(list.begin(), list.end(), [&element](auto const &elem) { return element->id() == elem->id(); }) != list.end();
}

template<typename ValueType>
std::vector<typename DftModularizer<ValueType>::DFTElementCPointer> DftModularizer<ValueType>::getAffectingElements(DFTElementCPointer const element) {
    std::vector<DFTElementCPointer> affectingElements{};

    if (element->isBasicElement()) {
        auto const be{std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element)};
        auto const &dependencies{be->ingoingDependencies()};
        affectingElements.insert(affectingElements.end(), dependencies.begin(), dependencies.end());
    }

    auto const &restrictions{element->restrictions()};
    affectingElements.insert(affectingElements.end(), restrictions.begin(), restrictions.end());

    auto const &dependencies{element->outgoingDependencies()};
    affectingElements.insert(affectingElements.end(), dependencies.begin(), dependencies.end());

    return affectingElements;
}

// Explicitly instantiate the class.
template class DftModularizer<double>;
template class DftModularizer<RationalFunction>;

}  // namespace utility
}  // namespace storm::dft
