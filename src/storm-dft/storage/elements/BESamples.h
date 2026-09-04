#pragma once

#include "DFTBE.h"

#include <map>

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * BE where the failure distribution is defined by samples.
 * A sample defines the unreliability at a time point (i.e. the cumulative distribution function F(x)).
 */
template<typename ValueType>
class BESamples : public DFTBE<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param activeSamples Samples defining unreliability in active state for certain time points.
     */
    BESamples(size_t id, std::string const& name, std::map<ValueType, ValueType> activeSamples) : DFTBE<ValueType>(id, name), mActiveSamples(activeSamples) {
        STORM_LOG_ASSERT(activeSamples.size() > 0, "At least one sample should be given.");
        STORM_LOG_ASSERT(this->canFail(), "At least one sample should have a non-zero probability.");
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new BESamples<ValueType>(this->id(), this->name(), this->activeSamples()));
    }

    storm::dft::storage::elements::BEType beType() const override {
        return storm::dft::storage::elements::BEType::SAMPLES;
    }

    /*!
     * Return samples defining unreliability in active state.
     * @return Samples for active state.
     */
    std::map<ValueType, ValueType> const& activeSamples() const {
        return mActiveSamples;
    }

    ValueType getUnreliability(ValueType time) const override;

    bool canFail() const override {
        // At least one sample is not zero
        for (auto const& sample : mActiveSamples) {
            if (!storm::numbers::isZero(sample.second)) {
                return true;
            }
        }
        return false;
    }

    std::string distributionString() const override {
        std::stringstream stream;
        stream << "samples " << this->activeSamples().size();
        return stream.str();
    }

   private:
    std::map<ValueType, ValueType> mActiveSamples;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
