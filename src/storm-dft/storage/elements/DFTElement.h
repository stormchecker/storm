#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "storm-dft/storage/DFTState.h"
#include "storm-dft/storage/DFTStateSpaceGenerationQueues.h"
#include "storm-dft/storage/elements/DFTElementType.h"
#include "storm/utility/constants.h"

namespace storm::dft {
namespace storage {
namespace elements {

using std::size_t;

// Forward declarations
template<typename ValueType>
class DFTGate;

template<typename ValueType>
class DFTDependency;

template<typename ValueType>
class DFTRestriction;

/*!
 * Abstract base class for DFT elements.
 * It is the most general class.
 */
template<typename ValueType>
class DFTElement {
    using DFTGatePointer = std::shared_ptr<DFTGate<ValueType>>;
    using DFTGateVector = std::vector<DFTGatePointer>;
    using DFTDependencyPointer = std::shared_ptr<DFTDependency<ValueType>>;
    using DFTDependencyVector = std::vector<DFTDependencyPointer>;
    using DFTRestrictionPointer = std::shared_ptr<DFTRestriction<ValueType>>;
    using DFTRestrictionVector = std::vector<DFTRestrictionPointer>;

   public:
    /*!
     * Constructor.
     * @param id Id.
     * @param name Name.
     */
    DFTElement(size_t id, std::string const& name) : mId(id), mName(name), mRank(-1), mRelevant(false), mAllowDC(true) {
        // Intentionally left empty.
    }

    /*!
     * Create a shallow copy of the element.
     * Only data (id, name, etc.) is copied but not the connections (parents, children, etc.).
     * @return Shallow copy of element.
     */
    virtual std::shared_ptr<DFTElement<ValueType>> clone() const = 0;

    /*!
     * Destructor.
     */
    virtual ~DFTElement() = default;

    /*!
     * Get id.
     * @return Id.
     */
    virtual size_t id() const {
        return mId;
    }

    /*!
     * Set id.
     * @param id Id.
     */
    virtual void setId(size_t id) {
        this->mId = id;
    }

    /*!
     * Get name.
     * @return Name.
     */
    virtual std::string const& name() const {
        return mName;
    }

    /*!
     * Get type.
     * @return Type.
     */
    virtual storm::dft::storage::elements::DFTElementType type() const = 0;

    /*!
     * Get type as string.
     * @return String with type information.
     */
    virtual std::string typestring() const {
        return storm::dft::storage::elements::toString(this->type());
    }

    /*!
     * Get rank.
     * @return Rank.
     */
    virtual size_t rank() const {
        return mRank;
    }

    /*!
     * Set rank.
     * @param rank Rank.
     */
    virtual void setRank(size_t rank) {
        this->mRank = rank;
    }

    /*!
     * Get whether the element is relevant.
     * Relevant elements are for example not set to Don't Care and their status is stored as a label in the generated Markov Chain.
     * @return True iff the element is relevant.
     */
    virtual bool isRelevant() const {
        return mRelevant;
    }

    /*!
     * Set the relevancy of the element.
     * @param relevant If true, the element is relevant.
     */
    virtual void setRelevance(bool relevant) const {
        this->mRelevant = relevant;
    }

    /*!
     * Set whether Don't Care propagation is allowed for this element.
     * @param allowDC If true, the element is allowed to be set to Don't Care.
     */
    virtual void setAllowDC(bool allowDC) const {
        this->mAllowDC = allowDC;
    }

    /*!
     * Checks whether the element is a basic element.
     * @return True iff element is a BE.
     */
    virtual bool isBasicElement() const {
        return false;
    }

    /*!
     * Check whether the element is a gate.
     * @return True iff element is a gate.
     */
    virtual bool isGate() const {
        return false;
    }

    /*!
     * Check whether the element is static, ie a BE or a static gate (AND, OR, VOT).
     * @return True iff element is static.
     */
    virtual bool isStaticElement() const {
        return false;
    }

    /*!
     * Check whether the element is a SPARE gate.
     * @return True iff element is a SPARE gate.
     */
    virtual bool isSpareGate() const {
        return false;
    }

    /*!
     * Check whether the element is a dependency.
     * @return True iff element is a dependency.
     */
    virtual bool isDependency() const {
        return false;
    }

    /*!
     * Check whether the element is a restriction.
     * @return True iff element is a restriction.
     */
    virtual bool isRestriction() const {
        return false;
    }

    /*!
     * Get number of children.
     * @return Nr of children.
     */
    virtual std::size_t nrChildren() const = 0;

    /*!
     * Return whether the element has parents.
     * @return True iff at least one parent exists.
     */
    bool hasParents() const {
        return !mParents.empty();
    }

    /*!
     * Return the number of parents.
     * @return Number of parents.
     */
    size_t nrParents() const {
        return mParents.size();
    }

    /*!
     * Get parents.
     * @return Parents.
     */
    DFTGateVector const& parents() const {
        return mParents;
    }

    /*!
     * Add parent.
     * @param parent Parent.
     */
    void addParent(DFTGatePointer const& parent) {
        if (std::find(this->parents().begin(), this->parents().end(), parent) == this->parents().end()) {
            // Parent does not exist yet
            mParents.push_back(parent);
        }
    }

    /*!
     * Return Ids of parents.
     * @return Parent ids.
     */
    std::vector<size_t> parentIds() const {
        std::vector<size_t> ids;
        for (auto parent : this->parents()) {
            ids.push_back(parent->id());
        }
        return ids;
    }

    /*!
     * Check whether the element has only static gates as parents.
     * @return True iff all parents are static gates.
     */
    bool hasOnlyStaticParents() const {
        for (auto const& parent : this->parents()) {
            if (!parent->isStaticElement()) {
                return false;
            }
        }
        return true;
    }

    /*!
     * Return whether the element has restrictions.
     * @return True iff at least one restriction exists.
     */
    bool hasRestrictions() const {
        return !mRestrictions.empty();
    }

    /*!
     * Return the number of restrictions.
     * @return Number of restrictions.
     */
    size_t nrRestrictions() const {
        return mRestrictions.size();
    }

    /*!
     * Get restrictions.
     * @return Restrictions.
     */
    DFTRestrictionVector const& restrictions() const {
        return mRestrictions;
    }

    /*!
     * Add restriction.
     * @param restrictions Restriction.
     */
    void addRestriction(DFTRestrictionPointer const& restriction) {
        if (std::find(this->restrictions().begin(), this->restrictions().end(), restriction) == this->restrictions().end()) {
            // Restriction does not exist yet
            mRestrictions.push_back(restriction);
        }
    }

    /*!
     * Return whether the element has outgoing dependencies.
     * @return True iff at least one restriction exists.
     */
    bool hasOutgoingDependencies() const {
        return !mOutgoingDependencies.empty();
    }

    /*!
     * Return the number of outgoing dependencies.
     * @return Number of outgoing dependencies.
     */
    size_t nrOutgoingDependencies() const {
        return mOutgoingDependencies.size();
    }

    /*!
     * Get outgoing dependencies.
     * @return Outgoing dependencies.
     */
    DFTDependencyVector const& outgoingDependencies() const {
        return mOutgoingDependencies;
    }

    /*!
     * Add outgoing dependency.
     * @param outgoingDependency Outgoing dependency.
     */
    void addOutgoingDependency(DFTDependencyPointer const& outgoingDependency) {
        STORM_LOG_ASSERT(outgoingDependency->triggerEvent()->id() == this->id(), "Ids do not match.");
        if (std::find(this->outgoingDependencies().begin(), this->outgoingDependencies().end(), outgoingDependency) == this->outgoingDependencies().end()) {
            // Outgoing dependency does not exist yet
            mOutgoingDependencies.push_back(outgoingDependency);
        }
    }

    virtual void extendSpareModule(std::set<size_t>& elementsInModule) const;

    virtual bool checkDontCareAnymore(storm::dft::storage::DFTState<ValueType>& state,
                                      storm::dft::storage::DFTStateSpaceGenerationQueues<ValueType>& queues) const;

    /**
     *  Computes the independent unit of this element, that is, all elements which are direct or indirect successors of an element.
     */
    virtual std::vector<size_t> independentUnit() const;

    /**
     *  Helper to independent unit computation
     *  @see independentUnit
     */
    virtual void extendUnit(std::set<size_t>& unit) const;

    /**
     *  Computes independent subtrees starting with this element (this), that is, all elements (x) which are connected to either
     *  - one of the children of the element,
     *  - a probabilistic dependency
     *  such that there exists a  path from x to a child of this does not go through this.
     */
    virtual std::vector<size_t> independentSubDft(bool blockParents, bool sparesAsLeaves = false) const;

    /**
     * Helper to the independent subtree computation
     * @see independentSubDft
     */
    virtual void extendSubDft(std::set<size_t>& elemsInSubtree, std::vector<size_t> const& parentsOfSubRoot, bool blockParents, bool sparesAsLeaves) const;

    /*!
     * Check whether two elements have the same type.
     * @param other Other element.
     * @return True iff this and other have the same type.
     */
    virtual bool isTypeEqualTo(DFTElement<ValueType> const& other) const {
        return type() == other.type();
    }

    /*!
     * Print information about element to string.
     * @return Element information.
     */
    virtual std::string toString() const = 0;

   protected:
    std::size_t mId;
    std::string mName;
    std::size_t mRank;
    DFTGateVector mParents;
    DFTDependencyVector mOutgoingDependencies;
    DFTRestrictionVector mRestrictions;
    mutable bool mRelevant;  // Must be mutable to allow changes later on. TODO: avoid mutable
    mutable bool mAllowDC;   // Must be mutable to allow changes later on. TODO: avoid mutable
};

template<typename ValueType>
inline std::ostream& operator<<(std::ostream& os, DFTElement<ValueType> const& element) {
    return os << element.toString();
}

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
