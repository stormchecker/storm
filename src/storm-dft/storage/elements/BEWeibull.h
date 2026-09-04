#pragma once

#include "DFTBE.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * BE with Weibull failure distribution.
 */
template<typename ValueType>
class BEWeibull : public DFTBE<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param shape Shape parameter.
     * @param rate Failure rate (also called scale parameter).
     */
    BEWeibull(size_t id, std::string const& name, ValueType shape, ValueType rate) : DFTBE<ValueType>(id, name), mShape(shape), mRate(rate) {
        // Intentionally empty
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new BEWeibull<ValueType>(this->id(), this->name(), this->shape(), this->rate()));
    }

    storm::dft::storage::elements::BEType beType() const override {
        return storm::dft::storage::elements::BEType::WEIBULL;
    }

    bool canFail() const override {
        STORM_LOG_ASSERT(!storm::numbers::isZero(this->rate()), "BE WEIBULL should have rate > 0.");
        STORM_LOG_ASSERT(!storm::numbers::isZero(this->shape()), "BE WEIBULL should have shape > 0.");
        return true;
    }

    /*!
     * Return shape parameter.
     * @return Shape.
     */
    ValueType const& shape() const {
        return mShape;
    }

    /*!
     * Return failure rate (also called scale parameter).
     * @return Rate.
     */
    ValueType const& rate() const {
        return mRate;
    }

    ValueType getUnreliability(ValueType time) const override;

    std::string distributionString() const override {
        std::stringstream stream;
        stream << "weibull " << this->rate() << ", " << this->shape();
        return stream.str();
    }

   private:
    ValueType mShape;
    ValueType mRate;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
