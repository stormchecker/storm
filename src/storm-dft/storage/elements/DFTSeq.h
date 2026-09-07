#pragma once

#include "storm-dft/storage/elements/DFTRestriction.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Sequence enforcer (SEQ).
 * All children can only fail in order from first to last child.
 * A child which has not failed yet prevents the failure of all children to the right of it.
 */
template<typename ValueType>
class DFTSeq : public DFTRestriction<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param children Children.
     */
    DFTSeq(size_t id, std::string const& name, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTRestriction<ValueType>(id, name, children) {
        // Intentionally left empty.
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTSeq<ValueType>(this->id(), this->name(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::SEQ;
    }

    bool isSeqEnforcer() const override {
        return true;
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(queues.failurePropagationDone(), "Failure propagation not finished.");
        bool childOperationalBefore = false;
        for (auto const& child : this->children()) {
            if (!state.hasFailed(child->id())) {
                childOperationalBefore = true;
            } else if (childOperationalBefore && state.hasFailed(child->id())) {
                // Child has failed before left sibling.
                this->fail(state, queues);
                return;
            }
        }
    }

    void checkFailsafe(storm::dft::storage::DFTState<ValueType>&, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>&) const override {}

    bool checkDontCareAnymore(storm::dft::storage::DFTState<ValueType>&, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>&) const override {
        return false;
    }
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
