#pragma once

#include "storm-dft/storage/elements/DFTGate.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Priority AND (PAND) gate.
 * Fails if all children fail in order from first to last child.
 * If a child fails before its left sibling, the PAND becomes failsafe.
 * For inclusive PAND<= gates, simultaneous failures are allowed.
 * For exclusive PAND< gates, simultaneous failure make the gate failsafe.
 */
template<typename ValueType>
class DFTPand : public DFTGate<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param inclusive If true, simultaneous failures are allowed.
     * parame children Children.
     */
    DFTPand(size_t id, std::string const& name, bool inclusive, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTGate<ValueType>(id, name, children), inclusive(inclusive) {
        // Intentionally left empty.
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTPand<ValueType>(this->id(), this->name(), this->isInclusive(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::PAND;
    }

    std::string typestring() const override {
        return this->isInclusive() ? "PAND (incl)" : "PAND (excl)";
    }

    /*!
     * Return whether the PAND is inclusive.
     * @return True iff PAND is inclusive.
     */
    bool isInclusive() const {
        return inclusive;
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(isInclusive(), "Exclusive PAND not supported.");
        if (state.isOperational(this->mId)) {
            bool childOperationalBefore = false;
            for (auto const& child : this->children()) {
                if (!state.hasFailed(child->id())) {
                    childOperationalBefore = true;
                } else if (childOperationalBefore && state.hasFailed(child->id())) {
                    // Child failed before sibling -> failsafe
                    this->failsafe(state, queues);
                    this->childrenDontCare(queues);
                    return;
                }
            }
            if (!childOperationalBefore) {
                // All children have failed
                this->fail(state, queues);
            }
        }
    }

    void checkFailsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(isInclusive(), "Exclusive PAND not supported.");
        STORM_LOG_ASSERT(this->hasFailsafeChild(state), "No failsafe child.");
        if (state.isOperational(this->mId)) {
            this->failsafe(state, queues);
            this->childrenDontCare(queues);
        }
    }

   protected:
    bool inclusive;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
