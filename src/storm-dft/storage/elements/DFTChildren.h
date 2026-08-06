#pragma once

#include "DFTElement.h"

namespace storm::dft {
namespace storage {
namespace elements {

/*!
 * Abstract base class for a DFT element with children.
 */
template<typename ValueType>
class DFTChildren : public DFTElement<ValueType> {
    using DFTElementPointer = std::shared_ptr<DFTElement<ValueType>>;
    using DFTElementVector = std::vector<DFTElementPointer>;

   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     * @param children Children.
     */
    DFTChildren(size_t id, std::string const& name, DFTElementVector const& children) : DFTElement<ValueType>(id, name), mChildren(children) {
        // Intentionally left empty.
    }

    /*!
     * Destructor.
     */
    virtual ~DFTChildren() = default;

    /*!
     * Add child.
     * @param element Element.
     */
    void addChild(DFTElementPointer element) {
        mChildren.push_back(element);
    }

    /*!
     * Get children.
     * @return Children.
     */
    DFTElementVector const& children() const {
        return mChildren;
    }

    size_t nrChildren() const override {
        return mChildren.size();
    }

    /*!
     * Check whether the given element is contained in the list of children.
     * @param id Id of element to search for.
     * @return True iff element was found in list of children.
     */
    bool containsChild(size_t id) {
        auto it = std::find_if(this->mChildren.begin(), this->mChildren.end(), [&id](DFTElementPointer element) -> bool { return element->id() == id; });
        return it != this->mChildren.end();
    }

    virtual std::vector<size_t> independentUnit() const override {
        std::set<size_t> unit = {this->mId};
        for (auto const& child : mChildren) {
            child->extendUnit(unit);
        }
        return std::vector<size_t>(unit.begin(), unit.end());
    }

    virtual void extendUnit(std::set<size_t>& unit) const override {
        DFTElement<ValueType>::extendUnit(unit);
        for (auto const& child : mChildren) {
            child->extendUnit(unit);
        }
    }

    virtual std::vector<size_t> independentSubDft(bool blockParents, bool sparesAsLeaves = false) const override {
        auto prelRes = DFTElement<ValueType>::independentSubDft(blockParents);
        if (prelRes.empty()) {
            // No elements (especially not this->id) in the prelimanry result, so we know already that it is not a subdft.
            return prelRes;
        }
        std::set<size_t> unit(prelRes.begin(), prelRes.end());
        std::vector<size_t> pids = this->parentIds();
        for (auto const& child : mChildren) {
            child->extendSubDft(unit, pids, blockParents, sparesAsLeaves);
            if (unit.empty()) {
                // Parent in the subdft, ie it is *not* a subdft
                break;
            }
        }
        return std::vector<size_t>(unit.begin(), unit.end());
    }

    virtual void extendSubDft(std::set<size_t>& elemsInSubtree, std::vector<size_t> const& parentsOfSubRoot, bool blockParents,
                              bool sparesAsLeaves) const override {
        if (elemsInSubtree.count(this->id()) > 0) {
            return;
        }
        DFTElement<ValueType>::extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
        if (elemsInSubtree.empty()) {
            // Parent in the subdft, ie it is *not* a subdft
            return;
        }
        for (auto const& child : mChildren) {
            child->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
            if (elemsInSubtree.empty()) {
                // Parent in the subdft, ie it is *not* a subdft
                return;
            }
        }
    }

    /*!
     * Check failed status.
     * @param state Current state of DFT.
     * @param queues Propagation queue for failed.
     */
    virtual void checkFails(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const = 0;

    /*!
     * Check failsafe status.
     * @param state Current state of DFT.
     * @param queues Propagation queue for failsafe.
     */
    virtual void checkFailsafe(storm::dft::storage::DFTState<ValueType>& state,
                               storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const = 0;

    virtual std::string toString() const override {
        std::stringstream stream;
        stream << "{" << this->name() << "} " << this->typestring() << "(";
        typename DFTElementVector::const_iterator it = mChildren.begin();
        stream << (*it)->name();
        ++it;
        while (it != mChildren.end()) {
            stream << ", " << (*it)->name();
            ++it;
        }
        stream << ")";
        return stream.str();
    }

   protected:
    /*!
     * Check whether it has a failsafe child.
     * @param state Current state of DFT.
     * @return True iff failsafe child exists.
     */
    bool hasFailsafeChild(storm::dft::storage::DFTState<ValueType>& state) const {
        for (auto const& child : mChildren) {
            if (state.isFailsafe(child->id())) {
                return true;
            }
        }
        return false;
    }

    /*!
     * Check whether it has a failed child.
     * @param state Current state of DFT.
     * @return True iff failed child exists.
     */
    bool hasFailedChild(storm::dft::storage::DFTState<ValueType>& state) const {
        for (auto const& child : mChildren) {
            if (state.hasFailed(child->id())) {
                return true;
            }
        }
        return false;
    }

    virtual void fail(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const = 0;

    virtual void failsafe(storm::dft::storage::DFTState<ValueType>& state, storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const = 0;

   private:
    DFTElementVector mChildren;
};

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
