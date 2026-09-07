#pragma once

#include "storm-dft/storage/elements/DFTElement.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Abstract base class for basic events (BEs) in DFTs.
 * BEs are atomic and not further subdivided.
 */
template<typename ValueType>
class DFTBE : public DFTElement<ValueType> {
   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     */
    DFTBE(size_t id, std::string const& name) : DFTElement<ValueType>(id, name) {
        // Intentionally empty
    }

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::BE;
    }

    /*!
     * Get type of BE (constant, exponential, etc.).
     * @return BE type.
     */
    virtual storm::dft::storage::elements::BEType beType() const = 0;

    bool isBasicElement() const override {
        return true;
    }

    bool isStaticElement() const override {
        return true;
    }

    size_t nrChildren() const override {
        return 0;
    }

    /*!
     * Return the unreliability of the BE up to the given time point.
     * Computes the cumulative distribution function F(x) for time x.
     * Note that the computation assumes the BE is always active.
     *
     * @return Cumulative failure probability.
     */
    virtual ValueType getUnreliability(ValueType time) const = 0;

    /*!
     * Return whether the BE can fail.
     * @return True iff BE is not failsafe.
     */
    virtual bool canFail() const = 0;

    /*!
     * Add dependency which can trigger this BE.
     * @param dependency Ingoing dependency.
     */
    void addIngoingDependency(std::shared_ptr<DFTDependency<ValueType>> const& dependency) {
        STORM_LOG_ASSERT(dependency->containsDependentEvent(this->id()), "Dependency " << *dependency << " has no dependent BE " << *this << ".");
        STORM_LOG_ASSERT(std::find(mIngoingDependencies.begin(), mIngoingDependencies.end(), dependency) == mIngoingDependencies.end(),
                         "Ingoing Dependency " << dependency << " already present.");
        mIngoingDependencies.push_back(dependency);
    }

    /*!
     * Return whether the BE has ingoing dependencies.
     * @return True iff BE can be triggered by dependencies.
     */
    bool hasIngoingDependencies() const {
        return !mIngoingDependencies.empty();
    }

    /*!
     * Return the number of ingoing dependencies.
     * @return Number of ingoing dependencies.
     */
    size_t nrIngoingDependencies() const {
        return mIngoingDependencies.size();
    }

    /*!
     * Return ingoing dependencies.
     * @return List of dependencies which can trigger this BE.
     */
    std::vector<std::shared_ptr<DFTDependency<ValueType>>> const& ingoingDependencies() const {
        return mIngoingDependencies;
    }

    void extendSubDft(std::set<size_t>& elemsInSubtree, std::vector<size_t> const& parentsOfSubRoot, bool blockParents, bool sparesAsLeaves) const override;

    bool checkDontCareAnymore(storm::dft::storage::DFTState<ValueType>& state,
                              storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        if (DFTElement<ValueType>::checkDontCareAnymore(state, queues)) {
            state.beNoLongerFailable(this->id());
            return true;
        }
        return false;
    }

    /*!
     * Print information about failure distribution to string.
     * @return Distribution information.
     */
    virtual std::string distributionString() const = 0;

    virtual std::string toString() const override {
        std::stringstream stream;
        stream << "{" << this->name() << "} BE(" << this->distributionString() << ")";
        return stream.str();
    }

   private:
    std::vector<std::shared_ptr<DFTDependency<ValueType>>> mIngoingDependencies;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
