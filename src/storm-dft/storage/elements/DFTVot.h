#pragma once

#include "DFTGate.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * VOT gate with threshold k.
 * Fails if k children have failed.
 */
template<typename ValueType>
class DFTVot : public DFTGate<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param threshold Threshold k, nr of failed children needed for failure.
     * @param children Children.
     */
    DFTVot(size_t id, std::string const& name, unsigned threshold, std::vector<std::shared_ptr<DFTElement<ValueType>>> const& children = {})
        : DFTGate<ValueType>(id, name, children), mThreshold(threshold) {
        STORM_LOG_ASSERT(mThreshold > 1, "Should use OR gate instead of VOT1.");
        // k=n cannot be checked as children might be added later
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTVot<ValueType>(this->id(), this->name(), this->threshold(), {}));
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::VOT;
    }

    bool isStaticElement() const override {
        return true;
    }

    /*!
     * Get the threshold k.
     * @return Threshold.
     */
    unsigned threshold() const {
        return mThreshold;
    }

    std::string typestring() const override {
        return storm::dft::storage::elements::toString(this->type()) + " (" + std::to_string(mThreshold) + ")";
    }

    void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        if (state.isOperational(this->mId)) {
            unsigned nrFailedChildren = 0;
            for (auto const& child : this->children()) {
                if (state.hasFailed(child->id())) {
                    ++nrFailedChildren;
                    if (nrFailedChildren >= mThreshold) {
                        this->fail(state, queues);
                        return;
                    }
                }
            }
        }
    }

    void checkFailsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        STORM_LOG_ASSERT(this->hasFailsafeChild(state), "No failsafe child.");
        if (state.isOperational(this->mId)) {
            unsigned nrFailsafeChildren = 0;
            for (auto const& child : this->children()) {
                if (state.isFailsafe(child->id())) {
                    ++nrFailsafeChildren;
                    if (nrFailsafeChildren > this->nrChildren() - mThreshold) {
                        this->failsafe(state, queues);
                        this->childrenDontCare(queues);
                        return;
                    }
                }
            }
        }
    }

    bool isTypeEqualTo(DFTElement<ValueType> const& other) const override {
        if (!DFTElement<ValueType>::isTypeEqualTo(other)) {
            return false;
        }
        auto& otherVOT = static_cast<DFTVot<ValueType> const&>(other);
        return this->threshold() == otherVOT.threshold();
    }

   private:
    unsigned mThreshold;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
