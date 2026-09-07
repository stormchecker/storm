#pragma once

#include "storm-dft/storage/elements/DFTGate.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * AND gate.
 * Fails if all children have failed.
 */
template<typename ValueType>
class DFTAnd : public DFTGate<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param children Children.
     */
    DFTAnd(size_t id, std::string const& name, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTGate<ValueType>(id, name, children) {
        // Intentionally empty
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTAnd<ValueType>(this->id(), this->name(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::AND;
    }

    bool isStaticElement() const override {
        return true;
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        if (state.isOperational(this->mId)) {
            for (auto const& child : this->children()) {
                if (!state.hasFailed(child->id())) {
                    return;
                }
            }
            // All children have failed
            this->fail(state, queues);
        }
    }

    void checkFailsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(this->hasFailsafeChild(state), "No failsafe child.");
        if (state.isOperational(this->mId)) {
            this->failsafe(state, queues);
            this->childrenDontCare(queues);
        }
    }
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
