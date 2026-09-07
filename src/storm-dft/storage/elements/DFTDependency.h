#pragma once

#include "storm-dft/storage/elements/DFTElement.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Dependency gate with probability p.
 * If p=1 the gate is a functional dependency (FDEP), otherwise it is a probabilistic dependency (PDEP).
 *
 * If the trigger event (i.e., the first child) fails, a coin is flipped.
 * With probability p the failure is forwarded to all other children.
 * With probability 1-p the failure is not forwarded and the dependency has no effect.
 *
 * The failure forwarding to the children happens in a strict (non-deterministically chosen) order.
 */
template<typename ValueType>
class DFTDependency : public DFTElement<ValueType> {
    using DFTElementPointer = std::shared_ptr<DFTElement<ValueType>>;
    using DFTElementVector = std::vector<DFTElementPointer>;

   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param probability Probability p of failure forwarding.
     */
    DFTDependency(size_t id, std::string const& name, ValueType probability) : DFTElement<ValueType>(id, name), mProbability(probability) {
        // We cannot assert 0<=p<=1 in general, because ValueType might be RationalFunction.
    }

    std::shared_ptr<DFTElement<ValueType>> clone() const override {
        return std::shared_ptr<DFTElement<ValueType>>(new DFTDependency<ValueType>(this->id(), this->name(), this->probability()));
    }

    /*!
     * Destructor
     */
    virtual ~DFTDependency() = default;

    storm::dft::storage::elements::DFTElementType type() const override {
        return storm::dft::storage::elements::DFTElementType::PDEP;
    }

    std::string typestring() const override {
        return this->isFDEP() ? "FDEP" : "PDEP";
    }

    /*!
     * Get probability of forwarding the failure.
     * @return Probability.
     */
    ValueType const& probability() const {
        return mProbability;
    }

    bool isDependency() const override {
        return true;
    }

    /*!
     * Check whether the dependency is an FDEP, i.e., p=1.
     * @return True iff p=1.
     */
    bool isFDEP() const {
        return storm::utility::isOne(this->probability());
    }

    /*!
     * Get trigger event, i.e., the first child.
     * @return Trigger event.
     */
    DFTElementPointer const& triggerEvent() const {
        STORM_LOG_ASSERT(mTriggerEvent, "Trigger does not exist.");
        return mTriggerEvent;
    }

    /*!
     * Set the trigger event, i.e., the first child.
     * @param triggerEvent Trigger event.
     */
    void setTriggerElement(DFTElementPointer const& triggerEvent) {
        mTriggerEvent = triggerEvent;
    }

    /*!
     * Get dependent events.
     * @return Dependent events.
     */
    DFTElementVector const& dependentEvents() const {
        STORM_LOG_ASSERT(mDependentEvents.size() > 0, "Dependent event does not exists.");
        return mDependentEvents;
    }

    /*!
     * Add dependent event.
     * @param dependentEvent Dependent event.
     */
    void addDependentEvent(DFTElementPointer const& dependentEvent) {
        mDependentEvents.push_back(dependentEvent);
    }

    /*!
     * Check whether the given element is a dependent event.
     * @param id Id of element to search for.
     * @return True iff element was found in dependent events.
     */
    bool containsDependentEvent(size_t id) {
        auto it =
            std::find_if(this->mDependentEvents.begin(), this->mDependentEvents.end(), [&id](DFTElementPointer elem) -> bool { return elem->id() == id; });
        return it != this->mDependentEvents.end();
    }

    virtual size_t nrChildren() const override {
        return 1;
    }

    bool isTypeEqualTo(DFTElement<ValueType> const& other) const override {
        if (!DFTElement<ValueType>::isTypeEqualTo(other)) {
            return false;
        }
        auto& otherDEP = static_cast<DFTDependency<ValueType> const&>(other);
        return this->probability() == otherDEP.probability();
    }

    void extendSpareModule(std::set<size_t>&) const override {
        // Do nothing
    }

    std::vector<size_t> independentUnit() const override {
        std::set<size_t> unit = {this->mId};
        for (auto const& depEv : mDependentEvents) {
            depEv->extendUnit(unit);
            if (unit.count(mTriggerEvent->id()) != 0) {
                return {};
            }
        }
        return std::vector<size_t>(unit.begin(), unit.end());
    }

    void extendSubDft(std::set<size_t>& elemsInSubtree, std::vector<size_t> const& parentsOfSubRoot, bool blockParents, bool sparesAsLeaves) const override {
        if (elemsInSubtree.count(this->id())) {
            return;
        }
        DFTElement<ValueType>::extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
        if (elemsInSubtree.empty()) {
            // Parent in the subdft, ie it is *not* a subdft
            return;
        }
        for (auto const& depEv : mDependentEvents) {
            depEv->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
            if (elemsInSubtree.empty()) {
                return;
            }
        }
        if (elemsInSubtree.empty()) {
            // Parent in the subdft, ie it is *not* a subdft
            return;
        }
        mTriggerEvent->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
    }

    std::string toString() const override {
        std::stringstream stream;
        stream << "{" << this->name() << "} " << this->typestring() << "(" << this->triggerEvent()->name() << " => { ";
        for (auto const& depEv : this->dependentEvents()) {
            stream << depEv->name() << " ";
        }
        stream << "}";
        if (!this->isFDEP()) {
            stream << " with probability " << this->probability();
        }
        return stream.str();
    }

   private:
    ValueType mProbability;
    DFTElementPointer mTriggerEvent;
    DFTElementVector mDependentEvents;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
