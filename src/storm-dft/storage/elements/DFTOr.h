#pragma once

#include "storm-dft/storage/elements/DFTGate.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * OR gate.
 * Fails if at least one child has failed.
 */
template<typename ValueType>
class DFTOr : public DFTGate<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param children Children.
     */
    DFTOr(size_t id, std::string const& name, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTGate<ValueType>(id, name, children) {
        // Intentionally empty
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTOr<ValueType>(this->id(), this->name(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::OR;
    }

    bool isStaticElement() const override {
        return true;
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(this->hasFailedChild(state), "No failed child.");
        if (state.isOperational(this->mId)) {
            this->fail(state, queues);
        }
    }

    void checkFailsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        for (auto const& child : this->children()) {
            if (!state.isFailsafe(child->id())) {
                return;
            }
        }
        // All chidren are failsafe
        this->failsafe(state, queues);
    }
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
