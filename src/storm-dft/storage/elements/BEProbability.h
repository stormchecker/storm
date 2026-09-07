#pragma once

#include "storm-dft/storage/elements/DFTBE.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * BE with constant (Bernoulli) failure probability distribution.
 */
template<typename ValueType>
class BEProbability : public DFTBE<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param failureProbability Active failure probability.
     * @param dormancyFactor Dormancy factor.
     */
    BEProbability(size_t id, std::string const& name, ValueType failureProbability, ValueType dormancyFactor)
        : DFTBE<ValueType>(id, name), mActiveFailureProbability(failureProbability), mPassiveFailureProbability(dormancyFactor * failureProbability) {
        // Intentionally empty
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(
            new BEProbability<ValueType>(this->id(), this->name(), this->activeFailureProbability(), this->dormancyFactor()));
    }

    storm::dft::storage::elements::BEType beType() const override {
        return storm::dft::storage::elements::BEType::PROBABILITY;
    }

    bool canFail() const override {
        STORM_LOG_ASSERT(!storm::utility::isZero(this->activeFailureProbability()), "BE CONST should have failure probability > 0.");
        return true;
    }

    /*!
     * Return failure probability in active state.
     * @return Active failure probability.
     */
    ValueType const& activeFailureProbability() const {
        return mActiveFailureProbability;
    }

    /*!
     * Return failure probability in passive state.
     * @return Passive failure probability.
     */
    ValueType const& passiveFailureProbability() const {
        return mPassiveFailureProbability;
    }

    /*!
     * Return dormancy factor given by passive_failure_rate / active_failure_rate.
     * @return Dormancy factor.
     */
    ValueType dormancyFactor() const {
        STORM_LOG_ASSERT(!storm::utility::isZero<ValueType>(this->activeFailureProbability()), "Active failure probability should not be zero.");
        return this->passiveFailureProbability() / this->activeFailureProbability();
    }

    ValueType getUnreliability(ValueType time) const override;

    std::string distributionString() const override {
        std::stringstream stream;
        stream << "prob " << this->activeFailureProbability() << ", " << this->passiveFailureProbability();
        return stream.str();
    }

   private:
    ValueType mActiveFailureProbability;
    ValueType mPassiveFailureProbability;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
