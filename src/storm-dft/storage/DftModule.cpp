#include "storm-dft/storage/DftModule.h"

#include "storm-dft/builder/DFTBuilder.h"
#include "storm-dft/storage/DFT.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace storage {

DftModule::DftModule(size_t representative, std::set<size_t> const& elements) : representative(representative), elements(elements) {
    // Assertion cannot be guaranteed as spare module currently only contain the corresponding BEs and SPAREs
    // STORM_LOG_ASSERT(std::find(this->elements.begin(), this->elements.end(), representative) != this->elements.end(),
    //                 "Representative " + std::to_string(representative) + " must be contained in module.");
}

template<typename ValueType>
std::string DftModule::toString(storm::dft::storage::DFT<ValueType> const& dft) const {
    std::stringstream stream;
    stream << "[" << dft.getElement(representative)->name() << "] = {";
    if (!elements.empty()) {
        auto it = elements.begin();
        STORM_LOG_ASSERT(it != elements.end(), "Element not found.");
        stream << dft.getElement(*it)->name();
        ++it;
        while (it != elements.end()) {
            stream << ", " << dft.getElement(*it)->name();
            ++it;
        }
    }
    stream << "}";
    return stream.str();
}
DftIndependentModule::DftIndependentModule(size_t representative, std::set<size_t> const& elements, std::set<DftIndependentModule> const& submodules,
                                           bool staticElements, bool fullyStatic, bool singleBE)
    : DftModule(representative, elements), staticElements(staticElements), fullyStatic(fullyStatic), singleBE(singleBE), submodules(submodules) {
    STORM_LOG_ASSERT(std::find(this->elements.begin(), this->elements.end(), representative) != this->elements.end(),
                     "Representative " + std::to_string(representative) + " must be contained in module.");
    STORM_LOG_ASSERT(!singleBE || (submodules.empty() && elements.size() == 1), "Module " + std::to_string(representative) + " is not a single BE.");
}

std::set<size_t> DftIndependentModule::getAllElements() const {
    std::set<size_t> allElements = elements;
    for (auto const& submodule : submodules) {
        allElements.merge(submodule.getAllElements());
    }
    return allElements;
}

template<typename ValueType>
storm::dft::storage::DFT<ValueType> DftIndependentModule::getSubtree(storm::dft::storage::DFT<ValueType> const& dft) const {
    storm::dft::builder::DFTBuilder<ValueType> builder;
    std::unordered_set<std::string> depInConflict;
    for (auto const id : getAllElements()) {
        auto const tmpElement = dft.getElement(id);
        builder.cloneElement(tmpElement);
        // Remember dependency conflict
        if (tmpElement->isDependency() && dft.isDependencyInConflict(tmpElement->id())) {
            depInConflict.insert(tmpElement->name());
        }
    }
    builder.setTopLevel(dft.getElement(getRepresentative())->name());
    auto subdft = builder.build();
    // Update dependency conflicts
    for (size_t id : subdft.getDependencies()) {
        // Set dependencies not in conflict
        if (depInConflict.find(subdft.getElement(id)->name()) == depInConflict.end()) {
            subdft.setDependencyNotInConflict(id);
        }
    }
    return subdft;
}

template<typename ValueType>
std::string DftIndependentModule::toString(storm::dft::storage::DFT<ValueType> const& dft, std::string const& indentation) const {
    std::stringstream stream;
    stream << indentation << DftModule::toString(dft);
    std::string subIndentation = indentation + "  ";
    for (DftIndependentModule const& submodule : submodules) {
        stream << "\n" << subIndentation << "Sub-module " << submodule.toString(dft, subIndentation);
    }
    return stream.str();
}

// Explicitly instantiate functions
template std::string DftModule::toString(storm::dft::storage::DFT<double> const&) const;
template std::string DftModule::toString(storm::dft::storage::DFT<storm::RationalFunction> const&) const;

template storm::dft::storage::DFT<double> DftIndependentModule::getSubtree(storm::dft::storage::DFT<double> const&) const;
template storm::dft::storage::DFT<storm::RationalFunction> DftIndependentModule::getSubtree(storm::dft::storage::DFT<storm::RationalFunction> const&) const;
template std::string DftIndependentModule::toString(storm::dft::storage::DFT<double> const&, std::string const&) const;
template std::string DftIndependentModule::toString(storm::dft::storage::DFT<storm::RationalFunction> const&, std::string const&) const;

}  // namespace storage
}  // namespace storm::dft
