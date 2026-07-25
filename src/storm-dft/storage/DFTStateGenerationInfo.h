#pragma once

#include <bit>

#include "storm/utility/macros.h"

namespace storm::dft {
namespace storage {

class DFTStateGenerationInfo {
   private:
    const size_t mUsageInfoBits;
    const size_t stateIndexSize;
    std::map<size_t, size_t> mSpareUsageIndex;                          // id spare -> index first bit in state
    std::map<size_t, size_t> mSpareActivationIndex;                     // id spare representative -> index in state
    std::vector<size_t> mIdToStateIndex;                                // id -> index first bit in state
    std::vector<size_t> mImmediateFailedBEs;                            // list of BEs which are immediately failed
    std::map<size_t, std::vector<size_t>> mSeqRestrictionPreElements;   // id -> list of restriction pre elements
    std::map<size_t, std::vector<size_t>> mSeqRestrictionPostElements;  // id -> list of restriction post elements
    std::map<size_t, std::vector<size_t>> mMutexRestrictionElements;    // id -> list of elements in the same mutexes
    std::vector<std::pair<size_t, std::vector<size_t>>>
        mSymmetries;  // pair (length of symmetry group, vector indicating the starting points of the symmetry groups)

   public:
    DFTStateGenerationInfo(size_t nrElements, size_t nrOfSpares, size_t nrRepresentatives, size_t maxSpareChildCount)
        : mUsageInfoBits(getUsageInfoBits(maxSpareChildCount)),
          stateIndexSize(getStateVectorSize(nrElements, nrOfSpares, nrRepresentatives, maxSpareChildCount)),
          mIdToStateIndex(nrElements) {
        STORM_LOG_ASSERT(maxSpareChildCount < std::pow(2, mUsageInfoBits), "Bit length incorrect.");
    }

    /*!
     * Get number of bits required to store claiming information for spares in binary format.
     * @param maxSpareChildCount Maximal number of children of a spare.
     * @return Number of bits required to store claiming information.
     */
    static size_t getUsageInfoBits(size_t maxSpareChildCount) {
        return std::bit_width(maxSpareChildCount);
    }

    /*!
     * Get length of BitVector capturing DFT state.
     * @param nrElements Number of DFT elements.
     * @param nrOfSpares Number of Spares (needed for claiming).
     * @param nrRepresentatives Number of representatives (needed for activation).
     * @param maxSpareChildCount Maximal number of children of a spare.
     * @return Length of required BitVector.
     */
    static size_t getStateVectorSize(size_t nrElements, size_t nrOfSpares, size_t nrRepresentatives, size_t maxSpareChildCount) {
        return nrElements * 2 + nrOfSpares * getUsageInfoBits(maxSpareChildCount) + nrRepresentatives;
    }

    size_t usageInfoBits() const {
        return mUsageInfoBits;
    }

    void addStateIndex(size_t id, size_t index) {
        STORM_LOG_ASSERT(id < mIdToStateIndex.size(), "Id invalid.");
        STORM_LOG_ASSERT(index < stateIndexSize, "Index invalid");
        mIdToStateIndex[id] = index;
    }

    void addImmediateFailedBE(size_t id) {
        mImmediateFailedBEs.push_back(id);
    }

    std::vector<size_t> const& immediateFailedBE() const {
        return mImmediateFailedBEs;
    }

    void setRestrictionPreElements(size_t id, std::vector<size_t> const& elems) {
        mSeqRestrictionPreElements[id] = elems;
    }

    void setRestrictionPostElements(size_t id, std::vector<size_t> const& elems) {
        mSeqRestrictionPostElements[id] = elems;
    }

    void setMutexElements(size_t id, std::vector<size_t> const& elems) {
        mMutexRestrictionElements[id] = elems;
    }

    std::vector<size_t> const& seqRestrictionPreElements(size_t index) const {
        STORM_LOG_ASSERT(mSeqRestrictionPreElements.count(index) > 0, "Index invalid.");
        return mSeqRestrictionPreElements.at(index);
    }

    std::vector<size_t> const& seqRestrictionPostElements(size_t index) const {
        STORM_LOG_ASSERT(mSeqRestrictionPostElements.count(index) > 0, "Index invalid.");
        return mSeqRestrictionPostElements.at(index);
    }

    std::vector<size_t> const& mutexRestrictionElements(size_t index) const {
        STORM_LOG_ASSERT(mMutexRestrictionElements.count(index) > 0, "Index invalid.");
        return mMutexRestrictionElements.at(index);
    }

    void addSpareActivationIndex(size_t id, size_t index) {
        STORM_LOG_ASSERT(index < stateIndexSize, "Index invalid");
        mSpareActivationIndex[id] = index;
    }

    void addSpareUsageIndex(size_t id, size_t index) {
        STORM_LOG_ASSERT(index < stateIndexSize, "Index invalid");
        mSpareUsageIndex[id] = index;
    }

    size_t getStateIndex(size_t id) const {
        STORM_LOG_ASSERT(id < mIdToStateIndex.size(), "Id invalid.");
        return mIdToStateIndex[id];
    }

    size_t getSpareUsageIndex(size_t id) const {
        STORM_LOG_ASSERT(mSpareUsageIndex.count(id) > 0, "Id invalid.");
        return mSpareUsageIndex.at(id);
    }

    size_t getSpareActivationIndex(size_t id) const {
        STORM_LOG_ASSERT(mSpareActivationIndex.count(id) > 0, "Id invalid.");
        return mSpareActivationIndex.at(id);
    }

    void addSymmetry(size_t length, std::vector<size_t>& startingIndices) {
        mSymmetries.push_back(std::make_pair(length, startingIndices));
    }

    /**
     * Generate more symmetries by combining two symmetries
     */
    void generateSymmetries() {
        // Iterate over possible children
        for (size_t i = 0; i < mSymmetries.size(); ++i) {
            size_t childStart = mSymmetries[i].second[0];
            size_t childLength = mSymmetries[i].first;
            // Iterate over possible parents
            for (size_t j = i + 1; j < mSymmetries.size(); ++j) {
                size_t parentStart = mSymmetries[j].second[0];
                size_t parentLength = mSymmetries[j].first;
                // Check if child lies in parent
                if (parentStart <= childStart && childStart + childLength < parentStart + parentLength) {
                    // We add the symmetry of the child to all symmetric elements in the parent
                    std::vector<std::vector<size_t>> newSymmetries;
                    // Start iteration at 1, because symmetry for child at 0 is already included
                    for (size_t index = 1; index < mSymmetries[j].second.size(); ++index) {
                        std::vector<size_t> newStarts;
                        // Apply child symmetry to all symmetric elements of parent
                        for (size_t symmetryStarts : mSymmetries[i].second) {
                            // Get symmetric element by applying the bijection
                            size_t symmetryOffset = symmetryStarts - parentStart;
                            newStarts.push_back(mSymmetries[j].second[index] + symmetryOffset);
                        }
                        newSymmetries.push_back(newStarts);
                    }
                    // Insert new symmetry after child
                    for (size_t index = 0; index < newSymmetries.size(); ++index) {
                        mSymmetries.insert(mSymmetries.begin() + i + 1 + index, std::make_pair(childLength, newSymmetries[index]));
                    }
                    i += newSymmetries.size();
                    break;
                }
            }
        }
    }

    void checkSymmetries() {
        for (auto pair : mSymmetries) {
            STORM_LOG_ASSERT(pair.first > 0, "Empty symmetry.");
            STORM_LOG_ASSERT(pair.first < stateIndexSize, "Symmetry too long.");
            for (size_t index : pair.second) {
                STORM_LOG_ASSERT(index < stateIndexSize, "Symmetry starting point " << index << " invalid.");
                STORM_LOG_ASSERT(index + pair.first < stateIndexSize, "Symmetry ending point " << index << " invalid.");
            }
        }
    }

    size_t getSymmetrySize() const {
        return mSymmetries.size();
    }

    bool hasSymmetries() const {
        return !mSymmetries.empty();
    }

    size_t getSymmetryLength(size_t pos) const {
        STORM_LOG_ASSERT(pos < mSymmetries.size(), "Pos invalid.");
        return mSymmetries[pos].first;
    }

    std::vector<size_t> const& getSymmetryIndices(size_t pos) const {
        STORM_LOG_ASSERT(pos < mSymmetries.size(), "Pos invalid.");
        return mSymmetries[pos].second;
    }

    friend std::ostream& operator<<(std::ostream& os, DFTStateGenerationInfo const& info) {
        os << "StateGenerationInfo:\n";
        os << "Length of state vector: " << info.stateIndexSize << '\n';
        os << "Id to state index:\n";
        for (size_t id = 0; id < info.mIdToStateIndex.size(); ++id) {
            os << id << " -> " << info.getStateIndex(id) << '\n';
        }
        os << "Spare usage index with usage InfoBits of size " << info.mUsageInfoBits << ":\n";
        for (auto pair : info.mSpareUsageIndex) {
            os << pair.first << " -> " << pair.second << '\n';
        }
        os << "Spare activation index:\n";
        for (auto pair : info.mSpareActivationIndex) {
            os << pair.first << " -> " << pair.second << '\n';
        }
        os << "Symmetries:\n";
        for (auto pair : info.mSymmetries) {
            os << "Length: " << pair.first << ", starting indices: ";
            for (size_t index : pair.second) {
                os << index << ", ";
            }
            os << '\n';
        }
        return os;
    }
};

}  // namespace storage
}  // namespace storm::dft
