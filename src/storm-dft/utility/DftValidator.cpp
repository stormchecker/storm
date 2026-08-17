#include "DftValidator.h"

#include <algorithm>

#include "storm-dft/transformations/DftTransformer.h"

namespace storm::dft {
namespace utility {

template<typename ValueType>
bool DftValidator<ValueType>::isDftWellFormed(storm::dft::storage::DFT<ValueType> const& dft, std::ostream& stream) {
    // 1. DFT is acyclic
    // was already checked in DFTBuilder::topologicalVisit()

    // 2. Exactly the BEs have no children
    // was already checked in DFTBuilder::addGate()

    // 3. Valid threshold for VOT
    // was already checked in DFTBuilder::addVotingGate()

    // 4. TLE is neither SEQ nor PDEP
    auto const& tle = dft.getTopLevelElement();
    if (tle->isDependency()) {
        stream << "Top level element " << *tle << " should not be a dependency.";
        return false;
    }
    if (tle->isRestriction()) {
        stream << "Top level element " << *tle << " should not be a restriction.";
        return false;
    }
    // 5. SEQ and PDEP have no parents
    // was already checked in DFTBuilder::build()

    // 6. SEQ and PDEP are at least binary
    // was already checked in DFTBuilder::addRestriction() and DFTBuilder::addDependency()
    return true;
}

template<typename ValueType>
bool DftValidator<ValueType>::isDftValidForMarkovianAnalysis(storm::dft::storage::DFT<ValueType> const& dft, std::ostream& stream) {
    if (!storm::dft::utility::DftValidator<ValueType>::isDftWellFormed(dft, stream)) {
        // DFT is not even well-formed
        // Output of isDftWellFormed() is enough
        return false;
    }

    // 7. Restriction to exponential distributions
    if (!storm::dft::transformations::DftTransformer<ValueType>::hasOnlyExponentialDistributions(dft)) {
        stream << "DFT has BE distributions which are neither exponential nor constant failed/failsafe.";
        return false;
    }

    // 8. Independence of spare modules
    auto spareModules = dft.getSpareModules();
    for (auto module1 = spareModules.begin(); module1 != spareModules.end(); ++module1) {
        auto const& module1Elements = module1->getElements();
        if (!module1Elements.empty()) {
            // If module is empty, it overlaps with the top module. Potential modeling issues are checked separately in hasPotentialModelingIssues().
            for (auto module2 = std::next(module1); module2 != spareModules.end(); ++module2) {
                // Check that spare modules do not overlap
                auto const& module2Elements = module2->getElements();
                std::vector<size_t> intersection;
                std::set_intersection(module1Elements.begin(), module1Elements.end(), module2Elements.begin(), module2Elements.end(),
                                      std::back_inserter(intersection));
                if (!intersection.empty()) {
                    stream << "Spare modules of '" << dft.getElement(module1->getRepresentative())->name() << "' and '"
                           << dft.getElement(module2->getRepresentative())->name() << "' should not overlap.";
                    return false;
                }
            }
        }
    }

    // Collect primary modules (used for 9. and 10.)
    std::vector<size_t> primaryModuleIds;
    for (size_t spareId : dft.getSpareIndices()) {
        primaryModuleIds.push_back(dft.getGate(spareId)->children()[0]->id());
    }

    // 9. No constant failed events in primary module
    for (size_t primaryModuleId : primaryModuleIds) {
        for (size_t elementId : dft.module(primaryModuleId).getElements()) {
            if (dft.isBasicElement(elementId)) {
                auto const& be = dft.getBasicElement(elementId);
                if (be->beType() == storm::dft::storage::elements::BEType::CONSTANT) {
                    auto beConst = std::static_pointer_cast<storm::dft::storage::elements::BEConst<ValueType> const>(be);
                    if (beConst->canFail()) {
                        stream << "Spare module of '" << dft.getElement(primaryModuleId)->name() << "' contains a constant failed BE '" << beConst->name()
                               << "'.";
                        return false;
                    }
                }
            }
        }
    }

    // 10. No sharing of primary modules
    for (auto module1It = primaryModuleIds.begin(); module1It != primaryModuleIds.end(); ++module1It) {
        for (auto module2It = std::next(module1It); module2It != primaryModuleIds.end(); ++module2It) {
            if (*module1It == *module2It) {
                stream << "Module of '" << dft.getElement(*module1It)->name() << "' is shared primary module of multiple SPAREs.";
                return false;
            }
        }
    }

    // 11. Dependent events of PDEPs are BEs
    // was already checked in DFTBuilder::build()

    // 12. Unique constant failed BE
    if (!storm::dft::transformations::DftTransformer<ValueType>::hasUniqueFailedBE(dft)) {
        stream << "DFT has more than one constant failed BE.";
        return false;
    }

    // Dependencies must be binary
    if (storm::dft::transformations::DftTransformer<ValueType>::hasNonBinaryDependency(dft)) {
        stream << "DFT has dependency with more than one dependent event.";
        return false;
    }

    return true;
}

template<typename ValueType>
bool DftValidator<ValueType>::hasPotentialModelingIssues(storm::dft::storage::DFT<ValueType> const& dft, std::ostream& stream) {
    bool potentialIssues = false;

    // Warn if spare module is connected to top level module.
    // In this case, the spare module is activated from the beginning.
    for (auto spareModule : dft.getSpareModules()) {
        if (spareModule.getElements().empty()) {
            stream << "Elements of spare module '" << dft.getElement(spareModule.getRepresentative())->name()
                   << "' also contained in top module. All elements of this spare module will be activated from the beginning on.";
            potentialIssues = true;
        }
    }

    // Warn if spare module also contains the parent SPARE gate.
    // If the SPARE gate has no module path to the top level element, the SPARE gate is never activated in this case.
    for (size_t spareId : dft.getSpareIndices()) {
        auto spare = dft.getGate(spareId);
        for (auto const& child : spare->children()) {
            auto module = dft.module(child->id());
            auto const& moduleElements = module.getElements();
            if (std::find(moduleElements.begin(), moduleElements.end(), spare->id()) != moduleElements.end()) {
                stream << "Spare module '" << dft.getElement(module.getRepresentative())->name() << "' also contains the parent SPARE-gate '" << spare->name()
                       << "'. This can prevent proper activation of the spare module. Consider introducing dependencies to properly separate the spare module "
                       << "from the SPARE-gate.";
                potentialIssues = true;
            }
        }
    }

    return potentialIssues;
}

// Explicitly instantiate the class.
template class DftValidator<double>;
template class DftValidator<RationalFunction>;

}  // namespace utility
}  // namespace storm::dft