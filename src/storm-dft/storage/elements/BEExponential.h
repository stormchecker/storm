#pragma once

#include "storm-dft/storage/elements/DFTBE.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * BE with exponential failure distribution.
 */
template<typename ValueType>
class BEExponential : public DFTBE<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param failureRate Active failure rate.
     * @param dormancyFactor Dormancy factor.
     * @param transient True iff the BE experiences transient failures.
     */
    BEExponential(size_t id, std::string const& name, ValueType failureRate, ValueType dormancyFactor, bool transient = false)
        : DFTBE<ValueType>(id, name), mActiveFailureRate(failureRate), mPassiveFailureRate(dormancyFactor * failureRate), mTransient(transient) {
        STORM_LOG_ASSERT(!storm::utility::isZero<ValueType>(failureRate), "Exponential failure rate should not be zero.");
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(
            new BEExponential<ValueType>(this->id(), this->name(), this->activeFailureRate(), this->dormancyFactor(), this->isTransient()));
    }

    storm::dft::storage::elements::BEType beType() const override {
        return storm::dft::storage::elements::BEType::EXPONENTIAL;
    }

    /*!
     * Return failure rate in active state.
     * @return Active failure rate.
     */
    ValueType const& activeFailureRate() const {
        return mActiveFailureRate;
    }

    /*!
     * Return failure rate in passive state.
     * @return Passive failure rate.
     */
    ValueType const& passiveFailureRate() const {
        return mPassiveFailureRate;
    }

    /*!
     * Return dormancy factor given by passive_failure_rate / active_failure_rate.
     * @return Dormancy factor.
     */
    ValueType dormancyFactor() const {
        STORM_LOG_ASSERT(!storm::utility::isZero<ValueType>(this->activeFailureRate()), "Active failure rate should not be zero.");
        return this->passiveFailureRate() / this->activeFailureRate();
    }

    ValueType getUnreliability(ValueType time) const override;

    /*!
     * Return whether the BE experiences transient failures.
     * @return True iff BE is transient.
     */
    bool isTransient() const {
        return mTransient;
    }

    bool canFail() const override {
        STORM_LOG_ASSERT(!storm::utility::isZero(this->activeFailureRate()), "BE EXP should have failure rate > 0.");
        return true;
    }

    /*!
     * Return whether the BE is a cold BE, i.e., passive failure rate = 0.
     * @return True iff BE is cold BE.
     */
    bool isColdBasicElement() const {
        return storm::utility::isZero(this->passiveFailureRate());
    }

    std::string distributionString() const override {
        std::stringstream stream;
        stream << "exp " << this->activeFailureRate() << ", " << this->passiveFailureRate();
        return stream.str();
    }

   private:
    ValueType mActiveFailureRate;
    ValueType mPassiveFailureRate;
    bool mTransient;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
