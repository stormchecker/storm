#include "storm-dft/storage/elements/DFTElement.h"

#include <set>

#include "storm-dft/storage/elements/DFTElements.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace storage {
namespace elements {

template<typename ValueType>
bool DFTElement<ValueType>::checkDontCareAnymore(storm::dft::storage::DFTState<ValueType>& state,
                                                 storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const {
    if (!this->mAllowDC) {
        return false;
    }

    if (state.dontCare(mId)) {
        return false;
    }

    // Check that no outgoing dependencies can be triggered anymore
    // Notice that n-ary dependencies are supported via rewriting them during build-time
    for (DFTDependencyPointer dependency : mOutgoingDependencies) {
        STORM_LOG_ASSERT(dependency->dependentEvents().size() == 1, "Expected exactly one dependent event.");
        if (state.isOperational(dependency->dependentEvents()[0]->id()) && state.isOperational(dependency->triggerEvent()->id())) {
            return false;
        }
    }

    bool hasParentSpare = false;

    // Check that no parent can fail anymore
    for (DFTGatePointer const& parent : mParents) {
        if (state.isOperational(parent->id())) {
            return false;
        }
        if (parent->isSpareGate()) {
            hasParentSpare = true;
        }
    }

    if (!mRestrictions.empty() && state.isEventRelevantInRestriction(mId)) {
        return false;
    }

    state.setDontCare(mId);
    if (hasParentSpare) {
        // Activate child for consistency in failed spares
        state.activate(mId);
    }
    return true;
}

template<typename ValueType>
void DFTElement<ValueType>::extendSpareModule(std::set<size_t>& elementsInModule) const {
    for (auto const& parent : mParents) {
        if (elementsInModule.count(parent->id()) == 0 && !parent->isSpareGate()) {
            elementsInModule.insert(parent->id());
            parent->extendSpareModule(elementsInModule);
        }
    }
}

template<typename ValueType>
std::vector<size_t> DFTElement<ValueType>::independentUnit() const {
    std::vector<size_t> res;
    res.push_back(this->id());
    // Extend for pdeps.
    return res;
}

template<typename ValueType>
void DFTElement<ValueType>::extendUnit(std::set<size_t>& unit) const {
    unit.insert(mId);
}

template<typename ValueType>
std::vector<size_t> DFTElement<ValueType>::independentSubDft(bool blockParents, bool sparesAsLeaves) const {
    std::vector<size_t> res;
    res.push_back(this->id());
    return res;
}

template<typename ValueType>
void DFTElement<ValueType>::extendSubDft(std::set<size_t>& elemsInSubtree, std::vector<size_t> const& parentsOfSubRoot, bool blockParents,
                                         bool sparesAsLeaves) const {
    if (elemsInSubtree.count(this->id()) > 0) {
        return;
    }
    if (std::find(parentsOfSubRoot.begin(), parentsOfSubRoot.end(), mId) != parentsOfSubRoot.end()) {
        // This is a parent of the suspected root, thus it is not a subdft.
        elemsInSubtree.clear();
        return;
    }
    elemsInSubtree.insert(mId);
    for (auto const& parent : mParents) {
        if (blockParents && std::find(parentsOfSubRoot.begin(), parentsOfSubRoot.end(), parent->id()) != parentsOfSubRoot.end()) {
            continue;
        }
        parent->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
        if (elemsInSubtree.empty()) {
            return;
        }
    }
    for (auto const& dep : mOutgoingDependencies) {
        dep->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
        if (elemsInSubtree.empty()) {
            return;
        }
    }

    for (auto const& restr : mRestrictions) {
        restr->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
        if (elemsInSubtree.empty()) {
            return;
        }
    }
}

// Explicitly instantiate the class.
template class DFTElement<double>;
template class DFTElement<RationalFunction>;

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
