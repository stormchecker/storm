#pragma once

#include "storm-dft/storage/elements/DFTRestriction.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Mutex restriction (MUTEX).
 * Only one of the children can fail.
 * A child which has failed prevents the failure of all other children.
 */
template<typename ValueType>
class DFTMutex : public DFTRestriction<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param children Children.
     */
    DFTMutex(size_t id, std::string const& name, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTRestriction<ValueType>(id, name, children) {
        // Intentionally left empty.
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTMutex<ValueType>(this->id(), this->name(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::MUTEX;
    }

    bool isMutex() const override {
        return true;
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(queues.failurePropagationDone(), "Failure propagation not finished.");
        bool childFailed = false;
        for (auto const& child : this->children()) {
            if (state.hasFailed(child->id())) {
                if (childFailed) {
                    // Two children have failed
                    this->fail(state, queues);
                    return;
                } else {
                    childFailed = true;
                }
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
