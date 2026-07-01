#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

#include "storm-config.h"

#include "storm/utility/macros.h"

namespace storm {
namespace storage {

template<typename Compare = std::less<uint64_t>>
class ConsecutiveUint64DynamicPriorityQueue {
   public:
    typedef uint64_t T;
    typedef std::vector<T> Container;

   private:
    Container container;
    Compare compare;

    std::vector<uint64_t> positions;

   public:
    explicit ConsecutiveUint64DynamicPriorityQueue(uint64_t numberOfIntegers, Compare const& compare)
        : container(numberOfIntegers), compare(compare), positions(numberOfIntegers) {
        std::iota(container.begin(), container.end(), 0);
        std::make_heap(container.begin(), container.end(), compare);
        updatePositions();
    }

    void increase(uint64_t element) {
        uint64_t position = positions[element];
        if (position >= container.size()) {
            return;
        }

        uint64_t parentPosition = (position - 1) / 2;
        while (position > 0 && compare(container[parentPosition], container[position])) {
            std::swap(positions[container[parentPosition]], positions[container[position]]);
            std::swap(container[parentPosition], container[position]);

            position = parentPosition;
            parentPosition = (position - 1) / 2;
        }

        STORM_LOG_ASSERT(std::is_heap(container.begin(), container.end(), compare), "Heap structure lost.");
    }

    bool contains(uint64_t element) const {
        return positions[element] < container.size();
    }

    bool empty() const {
        return container.empty();
    }

    std::size_t size() const {
        return container.size();
    }

    const T& top() const {
        return container.front();
    }

    void push(uint64_t const& item) {
        container.emplace_back(item);
        std::push_heap(container.begin(), container.end(), compare);
    }

    void pop() {
        if (container.size() > 1) {
            // Swap max element to back.
            std::swap(positions[container.front()], positions[container.back()]);
            std::swap(container.front(), container.back());
            container.pop_back();

            // Sift down the element from the top.
            uint64_t positionToSift = 0;
            uint64_t child = 2 * positionToSift + 1;

            while (child < container.size()) {
                if (child + 1 < container.size()) {
                    // Figure out larger child.
                    child = compare(container[child], container[child + 1]) ? child + 1 : child;

                    // Check if we need to sift down.
                    if (compare(container[positionToSift], container[child])) {
                        std::swap(positions[container[positionToSift]], positions[container[child]]);
                        std::swap(container[positionToSift], container[child]);

                        positionToSift = child;
                        child = 2 * positionToSift + 1;
                    } else {
                        break;
                    }
                } else if (compare(container[positionToSift], container[child])) {
                    std::swap(positions[container[positionToSift]], positions[container[child]]);
                    std::swap(container[positionToSift], container[child]);

                    positionToSift = child;
                    child = 2 * positionToSift + 1;
                } else {
                    break;
                }
            }

        } else {
            container.pop_back();
        }

        STORM_LOG_ASSERT(std::is_heap(container.begin(), container.end(), compare), "Heap structure lost.");
    }

    T popTop() {
        T item = top();
        pop();
        return item;
    }

   private:
    bool checkPositions() const {
        uint64_t position = 0;
        for (auto const& e : container) {
            if (positions[e] != position) {
                return false;
            }
            ++position;
        }
        return true;
    }

    void updatePositions() {
        uint64_t position = 0;
        for (auto const& e : container) {
            positions[e] = position;
            ++position;
        }
    }
};
}  // namespace storage
}  // namespace storm
