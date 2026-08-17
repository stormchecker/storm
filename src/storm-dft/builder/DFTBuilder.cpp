#include "storm-dft/builder/DFTBuilder.h"

#include <algorithm>

#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/OrderDFTElementsById.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace builder {

template<typename ValueType>
DFTBuilder<ValueType>::DFTBuilder() : mNextId(0) {
    // Intentionally left empty
}

template<>
DFTBuilder<double>::DFTBuilder() : mNextId(0) {
    // Intentionally left empty
}

template<typename ValueType>
storm::dft::storage::DFT<ValueType> DFTBuilder<ValueType>::build() {
    STORM_LOG_THROW(!mTopLevelName.empty(), storm::exceptions::WrongFormatException, "No top level element defined.");

    // Build parent/child connections for gates
    for (auto& gateChildrenPair : mChildNames) {
        STORM_LOG_ASSERT(gateChildrenPair.first->isGate(), "Element " << *gateChildrenPair.first << " with children is not a gate.");
        DFTGatePointer gate = std::static_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType>>(gateChildrenPair.first);
        for (std::string const& childName : gateChildrenPair.second) {
            auto itFind = mElements.find(childName);
            STORM_LOG_THROW(itFind != mElements.end(), storm::exceptions::WrongFormatException,
                            "Child '" << childName << "' for gate '" << gate->name() << "' not found.");
            DFTElementPointer childElement = itFind->second;
            if (childElement->isRestriction()) {
                STORM_LOG_WARN("Restriction '" << childName << "' is ignored as input for gate '" << gate->name() << "', because restrictions have no output.");
            } else if (childElement->isDependency()) {
                STORM_LOG_WARN("Dependency '" << childName << "' is ignored as input for gate '" << gate->name() << "', because dependencies have no output.");
            } else {
                gate->addChild(childElement);
                childElement->addParent(gate);
            }
        }
    }

    // Build connections for restrictions
    for (auto& restrictionChildrenPair : mRestrictionChildNames) {
        DFTRestrictionPointer restriction = restrictionChildrenPair.first;
        for (std::string const& childName : restrictionChildrenPair.second) {
            auto itFind = mElements.find(childName);
            STORM_LOG_THROW(itFind != mElements.end(), storm::exceptions::WrongFormatException,
                            "Child '" << childName << "' for restriction '" << restriction->name() << "' not found.");
            DFTElementPointer childElement = itFind->second;
            STORM_LOG_THROW(childElement->isGate() || childElement->isBasicElement(), storm::exceptions::WrongFormatException,
                            "Child '" << childElement->name() << "' of restriction '" << restriction->name() << "' must be gate or BE.");
            restriction->addChild(childElement);
            childElement->addRestriction(restriction);
        }
    }

    // Build connections for dependencies
    for (auto& dependencyChildrenPair : mDependencyChildNames) {
        DFTDependencyPointer dependency = dependencyChildrenPair.first;
        bool triggerElement = true;
        for (std::string const& childName : dependencyChildrenPair.second) {
            auto itFind = mElements.find(childName);
            STORM_LOG_THROW(itFind != mElements.end(), storm::exceptions::WrongFormatException,
                            "Child '" << childName << "' for dependency '" << dependency->name() << "' not found.");
            DFTElementPointer childElement = itFind->second;
            if (triggerElement) {
                triggerElement = false;
                STORM_LOG_THROW(childElement->isGate() || childElement->isBasicElement(), storm::exceptions::WrongFormatException,
                                "Trigger element '" << childName << "' of dependency '" << dependency->name() << "' must be gate or BE.");
                dependency->setTriggerElement(std::static_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType>>(childElement));
                childElement->addOutgoingDependency(dependency);
            } else {
                STORM_LOG_THROW(childElement->isBasicElement(), storm::exceptions::WrongFormatException,
                                "Dependent element '" << childName << "' of dependency '" << dependency->name() << "' must be BE.");
                DFTBEPointer dependentBE = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType>>(childElement);
                dependency->addDependentEvent(dependentBE);
                dependentBE->addIngoingDependency(dependency);
            }
        }
    }

    // Sort elements topologically
    DFTElementVector elements = sortTopological();
    // Set ids according to order
    size_t id = 0;
    for (DFTElementPointer e : elements) {
        e->setId(id++);
    }
    // Compute rank
    computeRank(mElements[mTopLevelName]);  // Start with top level element
    for (auto& elem : mElements) {
        computeRank(elem.second);
    }

    // Create DFT
    storm::dft::storage::DFT<ValueType> dft(elements, mElements[mTopLevelName]);

    // Set default BE order from topological order
    std::vector<size_t> beOrder;
    for (auto elem : mElements) {
        if (elem.second->isBasicElement()) {
            beOrder.push_back(elem.second->id());
        }
    }
    std::sort(beOrder.begin(), beOrder.end());
    dft.setBEOrder(beOrder);

    // Set layout info
    for (auto& elem : mElements) {
        if (mLayoutInfo.count(elem.first) > 0) {
            // Use given layout info
            dft.setElementLayoutInfo(elem.second->id(), mLayoutInfo.at(elem.first));
        } else {
            // Set default layout
            dft.setElementLayoutInfo(elem.second->id(), storm::dft::storage::DFTLayoutInfo());
        }
    }

    return dft;
}

template<typename ValueType>
void DFTBuilder<ValueType>::addElement(DFTElementPointer element) {
    STORM_LOG_THROW(!nameInUse(element->name()), storm::exceptions::InvalidArgumentException, "Element with name '" << element->name() << "' already exists.");
    STORM_LOG_ASSERT(mNextId == mElements.size(), "Current id " << mNextId << " and number of elements " << mElements.size() << " do not match.");
    element->setId(mNextId++);
    mElements[element->name()] = element;
}

template<typename ValueType>
void DFTBuilder<ValueType>::addBasicElementConst(std::string const& name, bool failed) {
    addElement(std::make_shared<storm::dft::storage::elements::BEConst<ValueType>>(0, name, failed));
}

template<typename ValueType>
void DFTBuilder<ValueType>::addBasicElementProbability(std::string const& name, ValueType probability, ValueType dormancyFactor) {
    // Handle special cases
    if (storm::utility::isZero<ValueType>(probability)) {
        addBasicElementConst(name, false);
    } else if (storm::utility::isOne<ValueType>(probability)) {
        addBasicElementConst(name, true);
    } else {
        STORM_LOG_THROW(isValidProbability(probability), storm::exceptions::WrongFormatException,
                        "Failure probability " << probability << " of BE " << name << " is not within interval [0, 1].");
        STORM_LOG_THROW(isValidProbability(dormancyFactor), storm::exceptions::WrongFormatException,
                        "Dormancy factor " << dormancyFactor << " of BE " << name << " is not within interval [0, 1].");
        addElement(std::make_shared<storm::dft::storage::elements::BEProbability<ValueType>>(0, name, probability, dormancyFactor));
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::addBasicElementExponential(std::string const& name, ValueType rate, ValueType dormancyFactor, bool transient) {
    // Handle special cases
    if (storm::utility::isZero<ValueType>(rate)) {
        addBasicElementConst(name, false);
    } else {
        STORM_LOG_THROW(!storm::utility::isConstant(rate) || storm::utility::isPositive(rate), storm::exceptions::WrongFormatException,
                        "Failure rate " << rate << " of BE " << name << " must be positive.");
        STORM_LOG_THROW(isValidProbability(dormancyFactor), storm::exceptions::WrongFormatException,
                        "Dormancy factor " << dormancyFactor << " of BE " << name << " is not within interval [0, 1].");
        addElement(std::make_shared<storm::dft::storage::elements::BEExponential<ValueType>>(0, name, rate, dormancyFactor, transient));
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::addBasicElementErlang(std::string const& name, ValueType rate, unsigned phases, ValueType dormancyFactor) {
    // Handle special cases
    if (storm::utility::isZero<ValueType>(rate)) {
        addBasicElementConst(name, false);
    } else if (phases == 1) {
        // shape=1 reduces to exponential distribution
        addBasicElementExponential(name, rate, dormancyFactor);
    } else {
        STORM_LOG_THROW(!storm::utility::isConstant(rate) || storm::utility::isPositive(rate), storm::exceptions::WrongFormatException,
                        "Erlang distribution of BE " << name << " requires a positive rate.");
        STORM_LOG_THROW(phases > 0, storm::exceptions::WrongFormatException, "Erlang distribution of BE " << name << " requires a positive number of phases.");
        STORM_LOG_THROW(isValidProbability(dormancyFactor), storm::exceptions::WrongFormatException,
                        "Dormancy factor " << dormancyFactor << " of BE " << name << " is not within interval [0, 1].");
        addElement(std::make_shared<storm::dft::storage::elements::BEErlang<ValueType>>(0, name, rate, phases, dormancyFactor));
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::addBasicElementWeibull(std::string const& name, ValueType shape, ValueType rate) {
    STORM_LOG_THROW(!storm::utility::isConstant(rate) || storm::utility::isPositive(rate), storm::exceptions::WrongFormatException,
                    "Weibull distribution of BE " << name << " requires a positive scale.");
    STORM_LOG_THROW(!storm::utility::isConstant(shape) || storm::utility::isPositive(shape), storm::exceptions::WrongFormatException,
                    "Weibull distribution of BE " << name << " requires a positive shape.");

    // Handle special cases
    if (storm::utility::isOne<ValueType>(shape)) {
        // shape=1 reduces to exponential distribution with rate 1/lambda
        addBasicElementExponential(name, storm::utility::one<ValueType>() / rate, storm::utility::one<ValueType>());
    } else {
        addElement(std::make_shared<storm::dft::storage::elements::BEWeibull<ValueType>>(0, name, shape, rate));
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::addBasicElementLogNormal(std::string const& name, ValueType mean, ValueType standardDeviation) {
    STORM_LOG_THROW(!storm::utility::isConstant(standardDeviation) || storm::utility::isPositive(standardDeviation), storm::exceptions::WrongFormatException,
                    "Log-normal distribution of BE " << name << " requires a positive standard deviation.");
    addElement(std::make_shared<storm::dft::storage::elements::BELogNormal<ValueType>>(0, name, mean, standardDeviation));
}

template<typename ValueType>
void DFTBuilder<ValueType>::addBasicElementSamples(std::string const& name, std::map<ValueType, ValueType> const& activeSamples) {
    // Check if it can fail
    bool canFail = false;
    for (auto const& sample : activeSamples) {
        // At least one sample is not zero
        if (!storm::utility::isZero(sample.second)) {
            canFail = true;
            break;
        }
    }
    if (!canFail) {
        // Add constant failed BE instead
        addBasicElementConst(name, false);
    } else {
        addElement(std::make_shared<storm::dft::storage::elements::BESamples<ValueType>>(0, name, activeSamples));
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::addGate(DFTGatePointer gate, std::vector<std::string> const& children) {
    STORM_LOG_THROW(children.size() > 0, storm::exceptions::WrongFormatException, "No children given for gate " << gate << ".");
    mChildNames[gate] = children;
    addElement(gate);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addAndGate(std::string const& name, std::vector<std::string> const& children) {
    addGate(std::make_shared<storm::dft::storage::elements::DFTAnd<ValueType>>(0, name), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addOrGate(std::string const& name, std::vector<std::string> const& children) {
    addGate(std::make_shared<storm::dft::storage::elements::DFTOr<ValueType>>(0, name), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addVotingGate(std::string const& name, unsigned threshold, std::vector<std::string> const& children) {
    STORM_LOG_THROW(threshold <= children.size(), storm::exceptions::WrongFormatException,
                    "Voting gate " << name << " has threshold " << threshold << " higher than the number of children " << children.size() << ".");
    // Handle special cases
    if (children.size() == threshold) {
        addAndGate(name, children);
    } else if (threshold == 1) {
        addOrGate(name, children);
    } else {
        addGate(std::make_shared<storm::dft::storage::elements::DFTVot<ValueType>>(0, name, threshold), children);
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::addPandGate(std::string const& name, std::vector<std::string> const& children, bool inclusive) {
    addGate(std::make_shared<storm::dft::storage::elements::DFTPand<ValueType>>(0, name, inclusive), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addPorGate(std::string const& name, std::vector<std::string> const& children, bool inclusive) {
    addGate(std::make_shared<storm::dft::storage::elements::DFTPor<ValueType>>(0, name, inclusive), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addSpareGate(std::string const& name, std::vector<std::string> const& children) {
    addGate(std::make_shared<storm::dft::storage::elements::DFTSpare<ValueType>>(0, name), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addDependency(DFTDependencyPointer dependency, std::vector<std::string> const& children) {
    STORM_LOG_THROW(children.size() > 1, storm::exceptions::WrongFormatException, "Dependency " << dependency->name() << " requires at least two children.");
    if (storm::utility::isZero(dependency->probability())) {
        STORM_LOG_WARN("Dependency " << dependency->name() << " with probability 0 is superfluous and will not be added.");
    } else {
        STORM_LOG_THROW(isValidProbability(dependency->probability()), storm::exceptions::WrongFormatException,
                        "Probability " << dependency->probability() << " of PDEP " << *dependency << " is not within interval [0, 1].");
        mDependencyChildNames[dependency] = children;
        addElement(dependency);
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::addPdep(std::string const& name, std::vector<std::string> const& children, ValueType probability) {
    addDependency(std::make_shared<storm::dft::storage::elements::DFTDependency<ValueType>>(0, name, probability), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addRestriction(DFTRestrictionPointer restriction, std::vector<std::string> const& children) {
    STORM_LOG_THROW(children.size() > 1, storm::exceptions::WrongFormatException, "Restriction " << restriction->name() << " requires at least two children.");
    mRestrictionChildNames[restriction] = children;
    addElement(restriction);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addSequenceEnforcer(std::string const& name, std::vector<std::string> const& children) {
    addRestriction(std::make_shared<storm::dft::storage::elements::DFTSeq<ValueType>>(0, name), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::addMutex(std::string const& name, std::vector<std::string> const& children) {
    addRestriction(std::make_shared<storm::dft::storage::elements::DFTMutex<ValueType>>(0, name), children);
}

template<typename ValueType>
void DFTBuilder<ValueType>::setTopLevel(std::string const& tle) {
    STORM_LOG_THROW(mTopLevelName.empty(), storm::exceptions::WrongFormatException, "Top level element was already set.");
    STORM_LOG_THROW(nameInUse(tle), storm::exceptions::InvalidArgumentException, "Element with name '" << tle << "' not known.");
    mTopLevelName = tle;
}

template<typename ValueType>
void DFTBuilder<ValueType>::addLayoutInfo(std::string const& name, double x, double y) {
    STORM_LOG_THROW(nameInUse(name), storm::exceptions::InvalidArgumentException, "Element with name '" << name << "' not found.");
    mLayoutInfo[name] = storm::dft::storage::DFTLayoutInfo(x, y);
}

template<typename ValueType>
void DFTBuilder<ValueType>::cloneElement(DFTElementCPointer element) {
    switch (element->type()) {
        case storm::dft::storage::elements::DFTElementType::BE:
            addElement(element->clone());
            break;
        case storm::dft::storage::elements::DFTElementType::AND:
        case storm::dft::storage::elements::DFTElementType::OR:
        case storm::dft::storage::elements::DFTElementType::VOT:
        case storm::dft::storage::elements::DFTElementType::PAND:
        case storm::dft::storage::elements::DFTElementType::POR:
        case storm::dft::storage::elements::DFTElementType::SPARE:
        case storm::dft::storage::elements::DFTElementType::SEQ:
        case storm::dft::storage::elements::DFTElementType::MUTEX: {
            auto elemWithChildren = std::static_pointer_cast<storm::dft::storage::elements::DFTChildren<ValueType> const>(element);
            std::vector<std::string> children{};
            for (DFTElementPointer const& elem : elemWithChildren->children()) {
                children.push_back(elem->name());
            }
            cloneElementWithNewChildren(elemWithChildren, children);
            break;
        }
        case storm::dft::storage::elements::DFTElementType::PDEP: {
            auto dependency = std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType> const>(element);
            std::vector<std::string> children = {dependency->triggerEvent()->name()};
            for (auto const& depEv : dependency->dependentEvents()) {
                children.push_back(depEv->name());
            }
            addDependency(std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType>>(dependency->clone()), children);
            break;
        }
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "DFT element type '" << element->type() << "' not known.");
            break;
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::cloneElementWithNewChildren(DFTChildrenCPointer elemWithChildren, std::vector<std::string> const& children) {
    switch (elemWithChildren->type()) {
        case storm::dft::storage::elements::DFTElementType::AND:
        case storm::dft::storage::elements::DFTElementType::OR:
        case storm::dft::storage::elements::DFTElementType::VOT:
        case storm::dft::storage::elements::DFTElementType::PAND:
        case storm::dft::storage::elements::DFTElementType::POR:
        case storm::dft::storage::elements::DFTElementType::SPARE:
            addGate(std::static_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType>>(elemWithChildren->clone()), children);
            break;
        case storm::dft::storage::elements::DFTElementType::SEQ:
        case storm::dft::storage::elements::DFTElementType::MUTEX:
            addRestriction(std::static_pointer_cast<storm::dft::storage::elements::DFTRestriction<ValueType>>(elemWithChildren->clone()), children);
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "DFT element type '" << elemWithChildren->type() << "' not known.");
            break;
    }
}

template<typename ValueType>
void DFTBuilder<ValueType>::topologicalVisit(DFTElementPointer const& element,
                                             std::map<DFTElementPointer, topoSortColour, storm::dft::storage::OrderElementsById<ValueType>>& visited,
                                             DFTElementVector& visitedElements) {
    STORM_LOG_THROW(visited[element] != topoSortColour::GREY, storm::exceptions::WrongFormatException, "DFT is cyclic.");
    if (visited[element] == topoSortColour::WHITE) {
        // Mark as currently visiting
        visited[element] = topoSortColour::GREY;
        // Element was not visited before
        if (element->isGate()) {
            for (DFTElementPointer const& child : std::static_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType>>(element)->children()) {
                // Recursively visit all children
                topologicalVisit(child, visited, visitedElements);
            }
        }
        // TODO: restrictions and dependencies have no parents, so this can be done more efficiently.
        else if (element->isRestriction()) {
            for (DFTElementPointer const& child : std::static_pointer_cast<storm::dft::storage::elements::DFTRestriction<ValueType>>(element)->children()) {
                // Recursively visit all children
                topologicalVisit(child, visited, visitedElements);
            }
        } else if (element->isDependency()) {
            for (DFTElementPointer const& child :
                 std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType>>(element)->dependentEvents()) {
                // Recursively visit all dependent children
                topologicalVisit(child, visited, visitedElements);
            }
            // Visit trigger element
            topologicalVisit(std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType>>(element)->triggerEvent(), visited,
                             visitedElements);
        } else {
            STORM_LOG_ASSERT(element->isBasicElement(), "Unknown element type " << element->type() << " for " << *element);
        }
        // Mark as completely visited
        visited[element] = topoSortColour::BLACK;
        // Children have all been visited before -> add element to list
        visitedElements.push_back(element);
    }
}

template<typename ValueType>
bool DFTBuilder<ValueType>::nameInUse(std::string const& name) const {
    return mElements.find(name) != mElements.end();
}

template<typename ValueType>
bool DFTBuilder<ValueType>::isValidProbability(ValueType value) const {
    if (!storm::utility::isConstant(value)) {
        // Do not check further if value is non-constant rational function
        return true;
    } else {
        return storm::utility::isBetween(storm::utility::zero<ValueType>(), value, storm::utility::one<ValueType>());
    }
}

template<typename ValueType>
std::vector<std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>>> DFTBuilder<ValueType>::sortTopological() {
    // Prepare map
    std::map<DFTElementPointer, topoSortColour, storm::dft::storage::OrderElementsById<ValueType>> visited;
    for (auto const& e : mElements) {
        visited.insert(std::make_pair(e.second, topoSortColour::WHITE));
    }

    DFTElementVector visitedElements;  // Return argument
    // Start from top level element
    topologicalVisit(mElements[mTopLevelName], visited, visitedElements);
    for (auto const& e : visited) {
        // Visit all elements to account for restrictions/dependencies/etc. not directly reachable from top level element
        topologicalVisit(e.first, visited, visitedElements);
    }
    return visitedElements;
}

template<typename ValueType>
size_t DFTBuilder<ValueType>::computeRank(DFTElementPointer const& elem) {
    if (elem->rank() == static_cast<decltype(elem->rank())>(-1)) {
        // Compute rank
        if (elem->isBasicElement() || elem->isDependency() || elem->isRestriction()) {
            // Rank is 0 for BEs/dependencies/restrictions
            elem->setRank(0);
        } else {
            // Rank is maximal rank of children + 1
            DFTGatePointer gate = std::static_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType>>(elem);
            size_t maxRank = 0;
            for (DFTElementPointer const& child : gate->children()) {
                maxRank = std::max(maxRank, computeRank(child));
            }
            elem->setRank(maxRank + 1);
        }
    }

    return elem->rank();
}

// Explicitly instantiate the class.
template class DFTBuilder<double>;
template class DFTBuilder<RationalFunction>;

}  // namespace builder
}  // namespace storm::dft
