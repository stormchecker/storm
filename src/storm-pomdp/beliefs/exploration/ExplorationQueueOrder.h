#pragma once
namespace storm::pomdp::beliefs {
/** Order used to select the next discovered belief for exploration. */
enum class ExplorationQueueOrder { Unordered, FIFO, LIFO };
}  // namespace storm::pomdp::beliefs
