#pragma once

#include <deque>
#include <unordered_set>

#include "storm-pomdp/beliefs/exploration/ExplorationQueueOrder.h"
#include "storm-pomdp/beliefs/utility/types.h"

namespace storm::pomdp::beliefs {

class ExplorationQueue {
   public:
    ExplorationQueue(ExplorationQueueOrder const order = ExplorationQueueOrder::Unordered);

    /*!
     * Changes the order in which the queue processes the elements.
     * @note if the queue is currently non-empty, all elements will remain in the queue. However, if there is more than one element in the queue, we do not give
     * any guarantees about the order in which they will be processed.
     * @param newOrder
     */
    void changeOrder(ExplorationQueueOrder const newOrder);

    /*!
     * @return true if the queue is not empty.
     */
    bool hasNext() const;

    /*!
     * Adds the given belief id to the queue.
     */
    bool push(BeliefId const id);

    /*!
     * Removes and returns the next belief id from the queue.
     */
    BeliefId popNext();

    /*!
     * @return the contents of the queue
     */
    std::unordered_set<storm::pomdp::beliefs::BeliefId> getContents() const;

   private:
    /// The order in which to process the elements.
    ExplorationQueueOrder order;

    /// The set of belief ids in the queue.
    std::unordered_set<storm::pomdp::beliefs::BeliefId> contents;

    /// the order in which elements were inserted (empty if unordered)
    std::deque<storm::pomdp::beliefs::BeliefId> queue;
};

}  // namespace storm::pomdp::beliefs
