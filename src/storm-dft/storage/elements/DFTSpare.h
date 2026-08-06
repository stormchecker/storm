#pragma once

#include "DFTGate.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * SPARE gate.
 */
template<typename ValueType>
class DFTSpare : public DFTGate<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param children Children.
     */
    DFTSpare(size_t id, std::string const& name, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTGate<ValueType>(id, name, children) {
        // Intentionally left empty.
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTSpare<ValueType>(this->id(), this->name(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::SPARE;
    }

    bool isSpareGate() const override {
        return true;
    }

    void fail(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        DFTGate<ValueType>::fail(state, queues);
        state.finalizeUses(this->mId);
    }

    void failsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        DFTGate<ValueType>::failsafe(state, queues);
        state.finalizeUses(this->mId);
    }

    bool checkDontCareAnymore(storm::dft::storage::DFTState<ValueType>& state,
                              storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        // Check children as claiming shared children might make a difference
        for (std::shared_ptr<DFTElement<ValueType>> const& child : this->children()) {
            if (state.isOperational(child->id())) {
                // Check if operational child is shared by another SPARE
                for (std::shared_ptr<DFTGate<ValueType>> const& parent : child->parents()) {
                    if (parent->id() != this->id() && parent->isSpareGate()) {
                        // Child could be claimed by another SPARE -> do not set to DC yet
                        return false;
                    }
                }
            }
        }
        // Perform regular check
        if (DFTGate<ValueType>::checkDontCareAnymore(state, queues)) {
            state.finalizeUses(this->mId);
            return true;
        }
        return false;
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        if (state.isOperational(this->mId)) {
            size_t uses = state.uses(this->mId);
            if (!state.isOperational(uses)) {
                bool claimingSuccessful = state.claimNew(this->mId, uses, this->children());
                if (!claimingSuccessful) {
                    this->fail(state, queues);
                }
            }
        }
    }

    void checkFailsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        if (state.isOperational(this->mId)) {
            if (state.isFailsafe(state.uses(this->mId))) {
                this->failsafe(state, queues);
                this->childrenDontCare(queues);
            }
        }
    }

    std::vector<size_t> independentSubDft(bool blockParents, bool sparesAsLeaves = false) const override {
        auto prelRes = DFTElement<ValueType>::independentSubDft(blockParents);
        if (prelRes.empty()) {
            // No elements (especially not this->id) in the prelimanry result, so we know already that it is not a subdft.
            return prelRes;
        }
        std::set<size_t> unit(prelRes.begin(), prelRes.end());
        std::vector<size_t> pids = this->parentIds();
        if (!sparesAsLeaves) {
            for (auto const& child : this->children()) {
                child->extendSubDft(unit, pids, blockParents, sparesAsLeaves);
                if (unit.empty()) {
                    // Parent in the subdft, ie it is *not* a subdft
                    break;
                }
            }
        }
        return std::vector<size_t>(unit.begin(), unit.end());
    }

    void extendSubDft(std::set<size_t>& elemsInSubtree, std::vector<size_t> const& parentsOfSubRoot, bool blockParents, bool sparesAsLeaves) const override {
        if (elemsInSubtree.count(this->id()) > 0) {
            return;
        }
        DFTElement<ValueType>::extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
        if (elemsInSubtree.empty()) {
            // Parent in the subdft, ie it is *not* a subdft
            return;
        }
        if (!sparesAsLeaves) {
            for (auto const& child : this->children()) {
                child->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
                if (elemsInSubtree.empty()) {
                    // Parent in the subdft, ie it is *not* a subdft
                    return;
                }
            }
        }
    }
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
