#include "storm-pomdp/beliefs/exploration/ExplorationQueue.h"

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::beliefs {

ExplorationQueue::ExplorationQueue(ExplorationQueueOrder const order) : order(order) {
    // Intentionally left empty.
}

void ExplorationQueue::changeOrder(ExplorationQueueOrder const newOrder) {
    STORM_LOG_ASSERT((order == ExplorationQueueOrder::Unordered) ? queue.empty() : (contents.size() == queue.size()), "inconsistent queue state");
    if (newOrder == order) {
        return;  // nothing to do.
    }
    if (newOrder == ExplorationQueueOrder::Unordered) {
        queue.clear();  // just keep the contents, drop the queue order
    } else if (order == ExplorationQueueOrder::Unordered) {
        // since newOrder != order, the newOrder will need the queue
        for (auto const id : contents) {
            queue.push_back(id);
        }
    }
    order = newOrder;
    STORM_LOG_ASSERT((order == ExplorationQueueOrder::Unordered) ? queue.empty() : (contents.size() == queue.size()), "inconsistent queue state");
}

bool ExplorationQueue::hasNext() const {
    STORM_LOG_ASSERT((order == ExplorationQueueOrder::Unordered) ? queue.empty() : (contents.size() == queue.size()), "inconsistent queue state");
    return !contents.empty();
}

bool ExplorationQueue::push(BeliefId const id) {
    if (contents.insert(id).second) {
        if (order != ExplorationQueueOrder::Unordered) {
            queue.push_back(id);
        }
        return true;
    }
    return false;
}

BeliefId ExplorationQueue::popNext() {
    STORM_LOG_ASSERT(hasNext(), "Trying to pop from empty queue.");
    if (order == ExplorationQueueOrder::Unordered) {
        BeliefId const id = *contents.begin();
        contents.erase(contents.begin());
        return id;
    } else {
        BeliefId id;
        if (order == ExplorationQueueOrder::LIFO) {
            id = queue.back();
            queue.pop_back();
        } else {
            id = queue.front();
            queue.pop_front();
        }
        STORM_LOG_ASSERT(contents.count(id) == 1, "Queue contains belief that is not in contents.");
        contents.erase(id);
        return id;
    }
}

std::unordered_set<storm::pomdp::beliefs::BeliefId> ExplorationQueue::getContents() const {
    return contents;
}
}  // namespace storm::pomdp::beliefs