
#pragma once

#include <cstdint>

namespace storm {
namespace storage {
class SchedulerClass {
   public:
    enum class MemoryPattern { Arbitrary, GoalMemory, Counter };

    SchedulerClass();

    bool isDeterministic() const;
    bool isMemoryBounded() const;
    uint64_t getMemoryStates() const;
    MemoryPattern getMemoryPattern() const;
    bool isPositional() const;

    SchedulerClass& setIsDeterministic(bool value = true);
    SchedulerClass& setMemoryStates(uint64_t value);
    SchedulerClass& unsetMemoryStates();
    SchedulerClass& setMemoryPattern(MemoryPattern const& pattern);
    SchedulerClass& setPositional();

   private:
    bool deterministic;
    uint64_t memorystates;
    MemoryPattern memoryPattern;
};
}  // namespace storage
}  // namespace storm
