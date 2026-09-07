#pragma once

#include "storm-dft/storage/elements/DFTBE.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * BE which is either constant failed or constant failsafe.
 * The BE is either always failed (from the beginning) or can never fail (failsafe).
 */
template<typename ValueType>
class BEConst : public DFTBE<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param failed True iff the const BE is failed, otherwise it is failsafe.
     */
    BEConst(size_t id, std::string const& name, bool failed) : DFTBE<ValueType>(id, name), mFailed(failed) {
        // Intentionally empty
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new BEConst<ValueType>(this->id(), this->name(), this->failed()));
    }

    storm::dft::storage::elements::BEType beType() const override {
        return storm::dft::storage::elements::BEType::CONSTANT;
    }

    /*!
     * Return whether the BE has failed.
     * @return True iff the BE is const failed.
     */
    bool failed() const {
        return mFailed;
    }

    bool canFail() const override {
        return this->failed();
    }

    ValueType getUnreliability(ValueType time) const override;

    std::string distributionString() const override {
        std::stringstream stream;
        stream << "const " << (this->failed() ? "failed" : "failsafe");
        return stream.str();
    }

   private:
    bool mFailed;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
