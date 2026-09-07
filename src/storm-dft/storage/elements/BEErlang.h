#pragma once

#include "storm-dft/storage/elements/DFTBE.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * BE with Erlang failure distribution.
 */
template<typename ValueType>
class BEErlang : public DFTBE<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param failureRate Active failure rate.
     * @param phases Number of phases (also called the shape).
     * @param dormancyFactor Dormancy factor for rate.
     */
    BEErlang(size_t id, std::string const& name, ValueType failureRate, unsigned phases, ValueType dormancyFactor)
        : DFTBE<ValueType>(id, name), mActiveFailureRate(failureRate), mPassiveFailureRate(dormancyFactor * failureRate), mPhases(phases) {
        // Intentionally empty
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(
            new BEErlang<ValueType>(this->id(), this->name(), this->activeFailureRate(), this->phases(), this->dormancyFactor()));
    }

    storm::dft::storage::elements::BEType beType() const override {
        return storm::dft::storage::elements::BEType::ERLANG;
    }

    bool canFail() const override {
        STORM_LOG_ASSERT(!storm::utility::isZero(this->activeFailureRate()), "BE ERLANG should have failure rate > 0.");
        return true;
    }

    /*!
     * Return failure probability in active state.
     * @return Active failure probability.
     */
    ValueType const& activeFailureRate() const {
        return mActiveFailureRate;
    }

    /*!
     * Return failure probability in passive state.
     * @return Passive failure probability.
     */
    ValueType const& passiveFailureRate() const {
        return mPassiveFailureRate;
    }

    /*!
     * Return number of phases (also called the shape).
     * @return Number of phases.
     */
    unsigned phases() const {
        return mPhases;
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

    std::string distributionString() const override {
        std::stringstream stream;
        stream << "erlang " << this->phases() << " phases with " << this->activeFailureRate() << ", " << this->passiveFailureRate();
        return stream.str();
    }

   private:
    ValueType mActiveFailureRate;
    ValueType mPassiveFailureRate;
    unsigned mPhases;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
