#include "SymmetryFinder.h"

#include "storm-dft/storage/DFTIsomorphism.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/vector.h"

namespace storm::dft {
namespace utility {

template<typename ValueType>
storm::dft::storage::DftSymmetries SymmetryFinder<ValueType>::findSymmetries(storm::dft::storage::DFT<ValueType> const& dft) {
    // Colour the DFT elements to find candidates for symmetries
    storm::dft::storage::DFTColouring<ValueType> colouring(dft);

    std::vector<size_t> subDftIndices(storm::utility::vector::buildVectorForRange(size_t(0), dft.nrElements()));
    storm::dft::storage::BijectionCandidates<ValueType> completeCategories = colouring.colourSubdft(subDftIndices);
    std::map<size_t, std::vector<std::vector<size_t>>> res;

    // Find symmetries for gates
    for (auto const& colourClass : completeCategories.gateCandidates) {
        findSymmetriesHelper(dft, colourClass.second, colouring, res);
    }

    // Find symmetries for BEs
    for (auto const& colourClass : completeCategories.beCandidates) {
        findSymmetriesHelper(dft, colourClass.second, colouring, res);
    }

    return storm::dft::storage::DftSymmetries(res);
}

template<typename ValueType>
std::map<size_t, size_t> SymmetryFinder<ValueType>::findBijection(storm::dft::storage::DFT<ValueType> const& dft, size_t index1, size_t index2,
                                                                  storm::dft::storage::DFTColouring<ValueType> const& colouring, bool sparesAsLeaves) {
    STORM_LOG_TRACE("Considering ids " << index1 << ", " << index2 << " for isomorphism.");
    bool sharedSpareMode = false;
    std::map<size_t, size_t> bijection;

    if (dft.getElement(index1)->isRelevant() || dft.getElement(index2)->isRelevant()) {
        // Relevant events need to be uniquely identified and cannot be symmetric.
        return {};
    }

    if (dft.isBasicElement(index1)) {
        if (!dft.isBasicElement(index2)) {
            return {};
        }
        if (colouring.hasSameColour(index1, index2)) {
            bijection[index1] = index2;
            return bijection;
        } else {
            return {};
        }
    }

    STORM_LOG_ASSERT(dft.isGate(index1), "Element is no gate.");
    STORM_LOG_ASSERT(dft.isGate(index2), "Element is no gate.");
    std::vector<size_t> isubdft1 = dft.getGate(index1)->independentSubDft(false);
    std::vector<size_t> isubdft2 = dft.getGate(index2)->independentSubDft(false);
    if (isubdft1.empty() || isubdft2.empty() || isubdft1.size() != isubdft2.size()) {
        if (isubdft1.empty() && isubdft2.empty() && sparesAsLeaves) {
            // Check again for shared spares
            sharedSpareMode = true;
            isubdft1 = dft.getGate(index1)->independentSubDft(false, true);
            isubdft2 = dft.getGate(index2)->independentSubDft(false, true);
            if (isubdft1.empty() || isubdft2.empty() || isubdft1.size() != isubdft2.size()) {
                return {};
            }
        } else {
            return {};
        }
    }
    STORM_LOG_TRACE("Checking subdfts from " << index1 << ", " << index2 << " for isomorphism.");
    auto LHS = colouring.colourSubdft(isubdft1);
    auto RHS = colouring.colourSubdft(isubdft2);
    auto IsoCheck = storm::dft::storage::DFTIsomorphismCheck<ValueType>(LHS, RHS, dft);

    while (IsoCheck.findNextIsomorphism()) {
        bijection = IsoCheck.getIsomorphism();
        if (sharedSpareMode) {
            bool bijectionSpareCompatible = true;
            for (size_t elementId : isubdft1) {
                if (dft.getElement(elementId)->isSpareGate()) {
                    auto spareLeft = std::static_pointer_cast<storm::dft::storage::elements::DFTSpare<ValueType> const>(dft.getElement(elementId));
                    auto spareRight =
                        std::static_pointer_cast<storm::dft::storage::elements::DFTSpare<ValueType> const>(dft.getElement(bijection.at(elementId)));

                    if (spareLeft->nrChildren() != spareRight->nrChildren()) {
                        bijectionSpareCompatible = false;
                        break;
                    }
                    // Check bijection for spare children
                    for (size_t i = 0; i < spareLeft->nrChildren(); ++i) {
                        size_t childLeftId = spareLeft->children().at(i)->id();
                        size_t childRightId = spareRight->children().at(i)->id();

                        STORM_LOG_ASSERT(bijection.count(childLeftId) == 0, "Child already part of bijection.");
                        if (childLeftId == childRightId) {
                            // Ignore shared child
                            continue;
                        }

                        // TODO generalize for more than one parent
                        if (spareLeft->children().at(i)->nrParents() != 1 || spareRight->children().at(i)->nrParents() != 1) {
                            bijectionSpareCompatible = false;
                            break;
                        }

                        std::map<size_t, size_t> tmpBijection = findBijection(dft, childLeftId, childRightId, colouring, false);
                        if (!tmpBijection.empty()) {
                            bijection.insert(tmpBijection.begin(), tmpBijection.end());
                        } else {
                            bijectionSpareCompatible = false;
                            break;
                        }
                    }
                    if (!bijectionSpareCompatible) {
                        break;
                    }
                }
            }
            if (bijectionSpareCompatible) {
                return bijection;
            }
        } else {
            return bijection;
        }
    }  // end while
    return {};
}

template<typename ValueType>
void SymmetryFinder<ValueType>::findSymmetriesHelper(storm::dft::storage::DFT<ValueType> const& dft, std::vector<size_t> const& candidates,
                                                     storm::dft::storage::DFTColouring<ValueType> const& colouring,
                                                     std::map<size_t, std::vector<std::vector<size_t>>>& result) {
    if (candidates.size() <= 0) {
        return;
    }

    std::set<size_t> foundEqClassFor;
    for (auto it1 = candidates.cbegin(); it1 != candidates.cend(); ++it1) {
        std::vector<std::vector<size_t>> symClass;
        if (foundEqClassFor.count(*it1) > 0) {
            // This item is already in a class.
            continue;
        }
        auto elem1 = dft.getElement(*it1);
        if (!elem1->hasOnlyStaticParents()) {
            continue;
        }
        if (hasSeqRestriction(elem1)) {
            continue;
        }

        std::tuple<std::vector<size_t>, std::vector<size_t>, std::vector<size_t>, std::vector<size_t>> influencedElem1Ids = getInfluencedIds(dft, *it1);
        auto it2 = it1;
        for (++it2; it2 != candidates.cend(); ++it2) {
            auto elem2 = dft.getElement(*it2);
            if (!elem2->hasOnlyStaticParents()) {
                continue;
            }
            if (hasSeqRestriction(elem2)) {
                continue;
            }

            if (influencedElem1Ids == getInfluencedIds(dft, *it2)) {
                std::map<size_t, size_t> bijection = findBijection(dft, *it1, *it2, colouring, true);
                if (!bijection.empty()) {
                    STORM_LOG_TRACE("Subdfts are symmetric");
                    foundEqClassFor.insert(*it2);
                    if (symClass.empty()) {
                        for (auto const& i : bijection) {
                            symClass.push_back(std::vector<size_t>({i.first}));
                        }
                    }
                    auto symClassIt = symClass.begin();
                    for (auto const& i : bijection) {
                        symClassIt->emplace_back(i.second);
                        ++symClassIt;
                    }
                }
            }
        }

        if (!symClass.empty()) {
            result.emplace(*it1, symClass);
        }
    }
}

template<typename ValueType>
bool SymmetryFinder<ValueType>::hasSeqRestriction(std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> elem) {
    for (auto const& restr : elem->restrictions()) {
        if (restr->isSeqEnforcer()) {
            return true;
        }
    }
    return false;
}

template<typename ValueType>
std::tuple<std::vector<size_t>, std::vector<size_t>, std::vector<size_t>, std::vector<size_t>> SymmetryFinder<ValueType>::getInfluencedIds(
    storm::dft::storage::DFT<ValueType> const& dft, size_t index) {
    // Parents
    std::vector<size_t> parents = dft.getElement(index)->parentIds();
    std::sort(parents.begin(), parents.end());
    // Ingoing dependencies
    std::vector<size_t> ingoingDeps;
    if (dft.isBasicElement(index)) {
        for (auto const& dep : dft.getBasicElement(index)->ingoingDependencies()) {
            ingoingDeps.push_back(dep->id());
        }
        std::sort(ingoingDeps.begin(), ingoingDeps.end());
    }
    // Outgoing dependencies
    std::vector<size_t> outgoingDeps;
    for (auto const& dep : dft.getElement(index)->outgoingDependencies()) {
        outgoingDeps.push_back(dep->id());
    }
    std::sort(outgoingDeps.begin(), outgoingDeps.end());
    std::vector<size_t> restrictions;
    for (auto const& restr : dft.getElement(index)->restrictions()) {
        restrictions.push_back(restr->id());
    }
    return std::make_tuple(parents, ingoingDeps, outgoingDeps, restrictions);
}

template class SymmetryFinder<double>;
template class SymmetryFinder<storm::RationalFunction>;

}  // namespace utility
}  // namespace storm::dft
