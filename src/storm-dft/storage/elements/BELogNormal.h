#pragma once

#include "storm-dft/storage/elements/DFTBE.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * BE with log-normal failure distribution.
 */
template<typename ValueType>
class BELogNormal : public DFTBE<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param mean Mean value (also called expected value).
     * @param standardDeviation Standard deviation.
     */
    BELogNormal(size_t id, std::string const& name, ValueType mean, ValueType standardDeviation)
        : DFTBE<ValueType>(id, name), mMean(mean), mStdDev(standardDeviation) {
        // Intentionally empty
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new BELogNormal<ValueType>(this->id(), this->name(), this->mean(), this->standardDeviation()));
    }

    storm::dft::storage::elements::BEType beType() const override {
        return storm::dft::storage::elements::BEType::LOGNORMAL;
    }

    bool canFail() const override {
        return true;
    }

    /*!
     * Return mean value parameter.
     * @return Mean value.
     */
    ValueType const& mean() const {
        return mMean;
    }

    /*!
     * Return standard deviation parameter.
     * @return Standard deviation.
     */
    ValueType const& standardDeviation() const {
        return mStdDev;
    }

    ValueType getUnreliability(ValueType time) const override;

    std::string distributionString() const override {
        std::stringstream stream;
        stream << "log-normal " << this->mean() << ", " << this->standardDeviation();
        return stream.str();
    }

   private:
    ValueType mMean;
    ValueType mStdDev;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
