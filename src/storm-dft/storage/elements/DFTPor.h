#pragma once

#include "storm-dft/storage/elements/DFTGate.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Priority OR (POR) gate.
 * Fails if the leftmost child fails before all other children.
 * If a child fails before the leftmost child, the POR becomes failsafe.
 * For inclusive POR<= gates, simultaneous failures are allowed.
 * For exclusive POR< gates, simultaneous failures make the gate failsafe.
 */
template<typename ValueType>
class DFTPor : public DFTGate<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param inclusive If true, simultaneous failures are allowed.
     * parame children Children.
     */
    DFTPor(size_t id, std::string const& name, bool inclusive, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTGate<ValueType>(id, name, children), inclusive(inclusive) {
        // Intentionally left empty.
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTPor<ValueType>(this->id(), this->name(), this->isInclusive(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::POR;
    }

    std::string typestring() const override {
        return this->isInclusive() ? "POR (incl)" : "POR (excl)";
    }

    /*!
     * Return whether the PAND is inclusive.
     * @return True iff PAND is inclusive.
     */
    bool isInclusive() const {
        return inclusive;
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(isInclusive(), "Exclusive POR not supported.");
        if (state.isOperational(this->mId)) {
            auto childIter = this->children().begin();
            if (state.hasFailed((*childIter)->id())) {
                // First child has failed before others
                this->fail(state, queues);
                return;
            }
            // Iterate over other children
            for (; childIter != this->children().end(); ++childIter) {
                if (state.hasFailed((*childIter)->id())) {
                    // Child has failed before first child
                    this->failsafe(state, queues);
                    this->childrenDontCare(queues);
                }
            }
        }
    }

    void checkFailsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(isInclusive(), "Exclusive POR not supported.");
        // If first child is not failsafe, it could still fail.
        if (state.isFailsafe(this->children().front()->id())) {
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
