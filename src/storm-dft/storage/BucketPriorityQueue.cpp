#include "storm-dft/storage/BucketPriorityQueue.h"

#include <cmath>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace storage {

template<typename PriorityType>
BucketPriorityQueue<PriorityType>::BucketPriorityQueue(size_t nrBuckets, double lowerValue, double ratio, bool higher)
    : buckets(nrBuckets), currentBucket(nrBuckets), nrUnsortedItems(0), lowerValue(lowerValue), higher(higher), logBase(std::log(ratio)), nrBuckets(nrBuckets) {
    compare = ([](PriorityTypePointer a, PriorityTypePointer b) { return *a < *b; });
}

template<typename PriorityType>
void BucketPriorityQueue<PriorityType>::fix() {
    if (currentBucket < nrBuckets && nrUnsortedItems > buckets[currentBucket].size() / 10) {
        // Sort current bucket
        std::make_heap(buckets[currentBucket].begin(), buckets[currentBucket].end(), compare);
        nrUnsortedItems = 0;
    }
}

template<typename PriorityType>
bool BucketPriorityQueue<PriorityType>::empty() const {
    return currentBucket == nrBuckets && immediateBucket.empty();
}

template<typename PriorityType>
size_t BucketPriorityQueue<PriorityType>::size() const {
    size_t size = immediateBucket.size();
    for (size_t i = currentBucket; currentBucket < nrBuckets; ++i) {
        size += buckets[i].size();
    }
    return size;
}

template<typename PriorityType>
typename BucketPriorityQueue<PriorityType>::PriorityTypePointer const& BucketPriorityQueue<PriorityType>::top() const {
    if (!immediateBucket.empty()) {
        return immediateBucket.back();
    }
    STORM_LOG_ASSERT(!empty(), "BucketPriorityQueue is empty.");
    return buckets[currentBucket].front();
}

template<typename PriorityType>
void BucketPriorityQueue<PriorityType>::push(PriorityTypePointer const& item) {
    if (item->isExpand()) {
        immediateBucket.push_back(item);
        return;
    }
    size_t bucket = getBucket(item->getPriority());
    if (bucket < currentBucket) {
        currentBucket = bucket;
        nrUnsortedItems = 0;
    }
    buckets[bucket].push_back(item);
    if (bucket == currentBucket) {
        // Inserted in first bucket
        if (AUTOSORT) {
            std::push_heap(buckets[currentBucket].begin(), buckets[currentBucket].end(), compare);
        } else {
            ++nrUnsortedItems;
        }
    }
}

template<typename PriorityType>
void BucketPriorityQueue<PriorityType>::update(PriorityTypePointer const& item, double oldPriority) {
    STORM_LOG_ASSERT(!item->isExpand(), "Item is marked for expansion.");
    size_t newBucket = getBucket(item->getPriority());
    size_t oldBucket = getBucket(oldPriority);

    if (oldBucket == newBucket) {
        if (currentBucket < newBucket) {
            // No change as the bucket is not sorted yet
        } else {
            if (AUTOSORT) {
                // Sort first bucket
                fix();
            } else {
                ++nrUnsortedItems;
            }
        }
    } else {
        // Move to new bucket
        STORM_LOG_ASSERT(newBucket < oldBucket, "Will update item " << item->getId() << " from " << oldBucket << " to higher bucket " << newBucket);
        if (newBucket < currentBucket) {
            currentBucket = newBucket;
            nrUnsortedItems = 0;
        }
        // Remove old entry by swap-and-pop
        if (buckets[oldBucket].size() >= 2) {
            // Find old index by linear search
            // Notice: using a map to remember index was not efficient
            size_t oldIndex = 0;
            for (; oldIndex < buckets[oldBucket].size(); ++oldIndex) {
                if (buckets[oldBucket][oldIndex]->getId() == item->getId()) {
                    break;
                }
            }
            STORM_LOG_ASSERT(oldIndex < buckets[oldBucket].size(), "Id " << item->getId() << " not found");
            std::iter_swap(buckets[oldBucket].begin() + oldIndex, buckets[oldBucket].end() - 1);
        }
        buckets[oldBucket].pop_back();
        // Insert new element
        buckets[newBucket].push_back(item);
        if (newBucket == currentBucket) {
            if (AUTOSORT) {
                // Sort in first bucket
                std::push_heap(buckets[currentBucket].begin(), buckets[currentBucket].end(), compare);
            } else {
                ++nrUnsortedItems;
            }
        }
    }
}

template<typename PriorityType>
typename BucketPriorityQueue<PriorityType>::PriorityTypePointer BucketPriorityQueue<PriorityType>::pop() {
    if (!immediateBucket.empty()) {
        PriorityTypePointer item = immediateBucket.back();
        immediateBucket.pop_back();
        return item;
    }
    STORM_LOG_ASSERT(!empty(), "BucketPriorityQueue is empty.");
    std::pop_heap(buckets[currentBucket].begin(), buckets[currentBucket].end(), compare);
    PriorityTypePointer item = buckets[currentBucket].back();
    buckets[currentBucket].pop_back();
    if (buckets[currentBucket].empty()) {
        // Find next bucket with elements
        for (; currentBucket < nrBuckets; ++currentBucket) {
            if (!buckets[currentBucket].empty()) {
                nrUnsortedItems = buckets[currentBucket].size();
                if (AUTOSORT) {
                    fix();
                }
                break;
            }
        }
    }
    return item;
}

template<typename PriorityType>
size_t BucketPriorityQueue<PriorityType>::getBucket(double priority) const {
    STORM_LOG_ASSERT(priority >= lowerValue, "Priority " << priority << " is too low");

    // For possible values greater 1
    double tmpBucket = std::log(priority - lowerValue) / logBase;
    tmpBucket += buckets.size() / 2;
    if (tmpBucket < 0) {
        tmpBucket = 0;
    }
    size_t newBucket = static_cast<size_t>(tmpBucket);
    // For values ensured to be lower 1
    if (newBucket >= nrBuckets) {
        newBucket = nrBuckets - 1;
    }
    if (!higher) {
        newBucket = nrBuckets - 1 - newBucket;
    }
    STORM_LOG_ASSERT(newBucket < nrBuckets, "Priority " << priority << " is too high");
    return newBucket;
}

template<typename PriorityType>
void BucketPriorityQueue<PriorityType>::print(std::ostream& out) const {
    out << "Bucket priority queue with size " << buckets.size() << ", lower value: " << lowerValue << " and logBase: " << logBase << '\n';
    out << "Immediate bucket: ";
    for (auto item : immediateBucket) {
        out << item->getId() << ", ";
    }
    out << '\n';
    out << "Current bucket (" << currentBucket << ") has " << nrUnsortedItems << " unsorted items\n";
    for (size_t bucket = 0; bucket < buckets.size(); ++bucket) {
        if (!buckets[bucket].empty()) {
            out << "Bucket " << bucket << ":\n";
            for (auto item : buckets[bucket]) {
                out << "\t" << item->getId() << ": " << item->getPriority() << '\n';
            }
        }
    }
}

template<typename PriorityType>
void BucketPriorityQueue<PriorityType>::printSizes(std::ostream& out) const {
    out << "Bucket sizes: " << immediateBucket.size() << " | ";
    for (size_t bucket = 0; bucket < buckets.size(); ++bucket) {
        out << buckets[bucket].size() << " ";
    }
    out << '\n';
}

// Template instantiations
template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristic<double>>;
template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristicDepth<double>>;
template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristicProbability<double>>;
template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristicBoundDifference<double>>;

template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristic<storm::RationalFunction>>;
template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristicDepth<storm::RationalFunction>>;
template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristicProbability<storm::RationalFunction>>;
template class BucketPriorityQueue<storm::dft::builder::DFTExplorationHeuristicBoundDifference<storm::RationalFunction>>;

}  // namespace storage
}  // namespace storm::dft
