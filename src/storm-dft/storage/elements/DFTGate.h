#pragma once

#include "storm-dft/storage/elements/DFTChildren.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Abstract base class for gates.
 */
template<typename ValueType>
class DFTGate : public DFTChildren<ValueType> {
    using DFTElementPointer = std::shared_ptr<DFTElement<ValueType>>;
    using DFTElementVector = std::vector<DFTElementPointer>;

   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param children Children.
     */
    DFTGate(size_t id, std::string const& name, DFTElementVector const& children) : DFTChildren<ValueType>(id, name, children) {
        // Intentionally left empty.
    }

    /*!
     * Destructor
     */
    virtual ~DFTGate() = default;

    virtual bool isGate() const override {
        return true;
    }

    virtual void extendSpareModule(std::set<size_t>& elementsInSpareModule) const override {
        if (!this->isSpareGate()) {
            DFTElement<ValueType>::extendSpareModule(elementsInSpareModule);
            for (auto const& child : this->children()) {
                if (elementsInSpareModule.count(child->id()) == 0) {
                    elementsInSpareModule.insert(child->id());
                    child->extendSpareModule(elementsInSpareModule);
                }
            }
        }
    }

    virtual bool checkDontCareAnymore(storm::dft::storage::DFTState<ValueType>& state,
                                      storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        if (DFTElement<ValueType>::checkDontCareAnymore(state, queues)) {
            childrenDontCare(queues);
            return true;
        }
        return false;
    }

   protected:
    void fail(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        for (std::shared_ptr<DFTGate> parent : this->mParents) {
            if (state.isOperational(parent->id())) {
                queues.propagateFailure(parent);
            }
        }
        for (std::shared_ptr<DFTRestriction<ValueType>> restr : this->mRestrictions) {
            queues.checkRestrictionLater(restr);
        }
        state.setFailed(this->mId);
        this->childrenDontCare(queues);
    }

    void failsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const override {
        for (std::shared_ptr<DFTGate> parent : this->mParents) {
            if (state.isOperational(parent->id())) {
                queues.propagateFailsafe(parent);
            }
        }
        state.setFailsafe(this->mId);
        this->childrenDontCare(queues);
    }

    /*!
     * Propagate Don't Care to children.
     * @param queues Propagation queues.
     */
    void childrenDontCare(storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const {
        queues.propagateDontCare(this->children());
    }
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
