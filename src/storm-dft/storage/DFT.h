#pragma once

#include <boost/iterator/counting_iterator.hpp>
#include <list>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "storm-dft/storage/DFTLayoutInfo.h"
#include "storm-dft/storage/DFTStateGenerationInfo.h"
#include "storm-dft/storage/DftModule.h"
#include "storm-dft/storage/DftSymmetries.h"
#include "storm-dft/storage/elements/DFTElements.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm::dft {

// Forward declarations
namespace builder {
template<typename T>
class DFTBuilder;
}  // namespace builder

namespace utility {
class RelevantEvents;
}  // namespace utility

namespace storage {

template<typename ValueType>
struct DFTElementSort {
    bool operator()(std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>> const& a,
                    std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>> const& b) const {
        if (a->rank() == 0 && b->rank() == 0) {
            return a->isConstant();
        } else {
            return a->rank() < b->rank();
        }
    }
};

/**
 * Represents a Dynamic Fault Tree
 */
template<typename ValueType>
class DFT {
    using DFTElementPointer = std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>>;
    using DFTElementCPointer = std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const>;
    using DFTElementVector = std::vector<DFTElementPointer>;
    using DFTGatePointer = std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType>>;
    using DFTGateVector = std::vector<DFTGatePointer>;
    using DFTStatePointer = std::shared_ptr<storm::dft::storage::DFTState<ValueType>>;

   private:
    DFTElementVector mElements;
    size_t mNrOfBEs;
    size_t mNrOfSpares;
    size_t mNrRepresentatives;
    size_t mTopLevelIndex;
    size_t mStateVectorSize;
    size_t mMaxSpareChildCount;
    std::map<size_t, storm::dft::storage::DftModule> mModules;
    std::vector<size_t> mBEs;
    std::vector<size_t> mDependencies;
    std::map<size_t, size_t> mRepresentants;  // id element -> id representative
    std::map<size_t, DFTLayoutInfo> mLayoutInfo;
    mutable std::vector<size_t> mRelevantEvents;
    std::map<size_t, bool> mDependencyInConflict;

   public:
    DFT(DFTElementVector const& elements, DFTElementPointer const& tle);

    DFTStateGenerationInfo buildStateGenerationInfo(storm::dft::storage::DftSymmetries const& symmetries) const;

    size_t generateStateInfo(DFTStateGenerationInfo& generationInfo, size_t id, storm::storage::BitVector& visited, size_t stateIndex) const;

    size_t performStateGenerationInfoDFS(DFTStateGenerationInfo& generationInfo, std::queue<size_t>& visitQueue, storm::storage::BitVector& visited,
                                         size_t stateIndex) const;

    DFT<ValueType> optimize() const;

    size_t stateBitVectorSize() const {
        // Ensure multiple of 64
        return (mStateVectorSize / 64 + (mStateVectorSize % 64 != 0)) * 64;
    }

    size_t nrElements() const {
        return mElements.size();
    }

    size_t nrBasicElements() const {
        return mNrOfBEs;
    }

    size_t nrDynamicElements() const;

    size_t nrStaticElements() const;

    size_t getTopLevelIndex() const {
        return mTopLevelIndex;
    }

    storm::dft::storage::elements::DFTElementType getTopLevelType() const {
        return getTopLevelElement()->type();
    }

    size_t getMaxSpareChildCount() const {
        return mMaxSpareChildCount;
    }

    std::vector<size_t> getSpareIndices() const {
        std::vector<size_t> indices;
        for (auto const& elem : mElements) {
            if (elem->isSpareGate()) {
                indices.push_back(elem->id());
            }
        }
        return indices;
    }

    storm::dft::storage::DftModule const& module(size_t representativeId) const {
        STORM_LOG_ASSERT(mModules.count(representativeId) > 0, "Representative not found.");
        return mModules.at(representativeId);
    }

    std::vector<storm::dft::storage::DftModule> getSpareModules() const {
        std::vector<storm::dft::storage::DftModule> spareModules;
        for (auto const& pair : mModules) {
            if (pair.first != mTopLevelIndex) {
                spareModules.push_back(pair.second);
            }
        }
        return spareModules;
    }

    bool isDependencyInConflict(size_t id) const {
        STORM_LOG_ASSERT(isDependency(id), "Not a dependency.");
        return mDependencyInConflict.at(id);
    }

    void setDependencyNotInConflict(size_t id) {
        STORM_LOG_ASSERT(isDependency(id), "Not a dependency.");
        mDependencyInConflict.at(id) = false;
    }

    std::vector<size_t> const& getDependencies() const {
        return mDependencies;
    }

    std::vector<size_t> nonColdBEs() const {
        std::vector<size_t> result;
        for (DFTElementPointer elem : mElements) {
            if (elem->isBasicElement()) {
                std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType>> be =
                    std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType>>(elem);
                if (be->canFail()) {
                    switch (be->beType()) {
                        case storm::dft::storage::elements::BEType::CONSTANT:
                            result.push_back(be->id());
                            break;
                        case storm::dft::storage::elements::BEType::EXPONENTIAL: {
                            auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<ValueType>>(be);
                            if (!beExp->isColdBasicElement()) {
                                result.push_back(be->id());
                            }
                            break;
                        }
                        default:
                            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "BE type '" << be->beType() << "' is not supported.");
                    }
                }
            }
        }
        return result;
    }

    /**
     *  Get a pointer to an element in the DFT
     *  @param index The id of the element
     */
    DFTElementCPointer getElement(size_t index) const {
        STORM_LOG_ASSERT(index < nrElements(), "Index invalid.");
        return mElements[index];
    }

    bool isBasicElement(size_t index) const {
        return getElement(index)->isBasicElement();
    }

    bool isGate(size_t index) const {
        return getElement(index)->isGate();
    }

    bool isDependency(size_t index) const {
        return getElement(index)->isDependency();
    }

    bool isRestriction(size_t index) const {
        return getElement(index)->isRestriction();
    }

    std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const> getBasicElement(size_t index) const {
        STORM_LOG_ASSERT(isBasicElement(index), "Element is no BE.");
        return std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(mElements[index]);
    }

    DFTElementCPointer getTopLevelElement() const {
        return getElement(getTopLevelIndex());
    }

    std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType> const> getGate(size_t index) const {
        STORM_LOG_ASSERT(isGate(index), "Element is no gate.");
        return std::static_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType> const>(mElements[index]);
    }

    std::shared_ptr<storm::dft::storage::elements::DFTDependency<ValueType> const> getDependency(size_t index) const {
        STORM_LOG_ASSERT(isDependency(index), "Element is no dependency.");
        return std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType> const>(mElements[index]);
    }

    std::shared_ptr<storm::dft::storage::elements::DFTRestriction<ValueType> const> getRestriction(size_t index) const {
        STORM_LOG_ASSERT(isRestriction(index), "Element is no restriction.");
        return std::static_pointer_cast<storm::dft::storage::elements::DFTRestriction<ValueType> const>(mElements[index]);
    }

    /*!
     * Set order of BEs.
     * The same order of BEs is returned from getBasicElements().
     *
     * @param bes List of BE ids.
     */
    void setBEOrder(std::vector<size_t> const& bes) {
        STORM_LOG_ASSERT(bes.size() == nrBasicElements(), "BEs must be the same size.");
        STORM_LOG_ASSERT(std::all_of(bes.begin(), bes.end(), [this](size_t id) { return this->getElement(id)->isBasicElement(); }),
                         "All elements must be BEs.");
        this->mBEs = bes;
    }

    /*!
     * Return list of basic elements. The list is ordered according to the initially given order of BEs.
     *
     * @return List of BEs.
     */
    std::vector<std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const>> getBasicElements() const {
        std::vector<std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const>> elements;
        for (size_t id : mBEs) {
            auto element = getElement(id);
            STORM_LOG_ASSERT(element->isBasicElement(), "Element is not a BE.");
            elements.push_back(std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element));
        }
        return elements;
    }

    bool canHaveNondeterminism() const;

    uint64_t maxRank() const;

    std::vector<DFT<ValueType>> topModularisation() const;

    bool isRepresentative(size_t id) const {
        for (auto const& parent : getElement(id)->parents()) {
            if (parent->isSpareGate()) {
                return true;
            }
        }
        return false;
    }

    bool hasRepresentant(size_t id) const {
        return mRepresentants.find(id) != mRepresentants.end();
    }

    size_t getRepresentant(size_t id) const {
        STORM_LOG_ASSERT(hasRepresentant(id), "Element has no representant.");
        return mRepresentants.find(id)->second;
    }

    bool hasFailed(DFTStatePointer const& state) const {
        return state->hasFailed(mTopLevelIndex);
    }

    bool hasFailed(storm::storage::BitVector const& state, DFTStateGenerationInfo const& stateGenerationInfo) const {
        return storm::dft::storage::DFTState<ValueType>::hasFailed(state, stateGenerationInfo.getStateIndex(mTopLevelIndex));
    }

    bool isFailsafe(DFTStatePointer const& state) const {
        return state->isFailsafe(mTopLevelIndex);
    }

    bool isFailsafe(storm::storage::BitVector const& state, DFTStateGenerationInfo const& stateGenerationInfo) const {
        return storm::dft::storage::DFTState<ValueType>::isFailsafe(state, stateGenerationInfo.getStateIndex(mTopLevelIndex));
    }

    /*!
     * Return id of used child for a given spare gate.
     * If no child is used the id of the spare gate is returned.
     *
     * @param state DFT state.
     * @param stateGenerationInfo State generation information.
     * @param id Id of spare gate.
     * @return Id of used child. Id of spare gate if no child is used.
     */
    size_t uses(storm::storage::BitVector const& state, DFTStateGenerationInfo const& stateGenerationInfo, size_t id) const {
        size_t nrUsedChild = storm::dft::storage::DFTState<ValueType>::usesIndex(state, stateGenerationInfo, id);
        if (nrUsedChild == getMaxSpareChildCount()) {
            return id;
        } else {
            return getChild(id, nrUsedChild);
        }
    }

    size_t getChild(size_t spareId, size_t nrUsedChild) const;

    size_t getNrChild(size_t spareId, size_t childId) const;

    std::string getElementsString() const;

    std::string getInfoString() const;

    std::string getModulesString() const;

    std::string getElementsWithStateString(DFTStatePointer const& state) const;

    std::string getStateString(DFTStatePointer const& state) const;

    std::string getStateString(storm::storage::BitVector const& status, DFTStateGenerationInfo const& stateGenerationInfo, size_t id) const;

    std::vector<size_t> immediateFailureCauses(size_t index) const;

    std::vector<size_t> findModularisationRewrite() const;

    void setElementLayoutInfo(size_t id, DFTLayoutInfo const& layoutInfo) {
        mLayoutInfo[id] = layoutInfo;
    }

    DFTLayoutInfo const& getElementLayoutInfo(size_t id) const {
        return mLayoutInfo.at(id);
    }

    void writeStatsToStream(std::ostream& stream) const;

    /*!
     * Get Ids of all elements.
     * @return All element ids.
     */
    std::set<size_t> getAllIds() const;

    /*!
     * Check whether an element with the given name exists.
     * @param name Name of element.
     * @return True iff element with given name exists.
     */
    bool existsName(std::string const& name) const;

    /*!
     * Get id for the given element name.
     * @param name Name of element.
     * @return Index of element.
     */
    size_t getIndex(std::string const& name) const;

    /*!
     * Get all relevant events.
     * @return List of all relevant events.
     */
    std::vector<size_t> const& getRelevantEvents() const;

    /*!
     * Set the relevance flag for all elements according to the given relevant events.
     * @param relevantEvents All elements which should be to relevant. All elements not occurring are set to irrelevant.
     * @param allowDCForRelevant Whether to allow Don't Care propagation for relevant events
     */
    void setRelevantEvents(storm::dft::utility::RelevantEvents const& relevantEvents, bool const allowDCForRelevant) const;

    /*!
     * Get a string containing the list of all relevant events.
     * @return String containing all relevant events.
     */
    std::string getRelevantEventsString() const;

   private:
    bool elementIndicesCorrect() const {
        for (size_t i = 0; i < mElements.size(); ++i) {
            if (mElements[i]->id() != i) {
                return false;
            }
        }
        return true;
    }
};

/*!
 * Get all rate/probability parameters occurring in the DFT.
 * @param dft DFT.
 * @return Set of parameters.
 */
std::set<storm::RationalFunctionVariable> getParameters(DFT<storm::RationalFunction> const& dft);

}  // namespace storage
}  // namespace storm::dft
