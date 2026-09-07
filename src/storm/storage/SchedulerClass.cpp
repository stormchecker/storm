#include "SchedulerClass.h"

#include "storm/utility/macros.h"

namespace storm {
namespace storage {
SchedulerClass::SchedulerClass() : deterministic(false), memorystates(0), memoryPattern(MemoryPattern::Arbitrary) {
    // Intentionally left empty
}

bool SchedulerClass::isDeterministic() const {
    return deterministic;
}

bool SchedulerClass::isMemoryBounded() const {
    return memorystates > 0;
}

uint64_t SchedulerClass::getMemoryStates() const {
    STORM_LOG_ASSERT(isMemoryBounded(), "Tried to retrieve the number of memory states although it is not bounded.");
    return memorystates;
}

SchedulerClass::MemoryPattern SchedulerClass::getMemoryPattern() const {
    return memoryPattern;
}

bool SchedulerClass::isPositional() const {
    return getMemoryStates() == 1 && getMemoryPattern() == MemoryPattern::Arbitrary;
}

SchedulerClass& SchedulerClass::setIsDeterministic(bool value) {
    deterministic = value;
    return *this;
}

SchedulerClass& SchedulerClass::setMemoryStates(uint64_t value) {
    STORM_LOG_ASSERT(value > 0, "Can not set the number of memory states to zero.");
    memorystates = value;
    return *this;
}

SchedulerClass& SchedulerClass::unsetMemoryStates() {
    memorystates = 0;
    return *this;
}

SchedulerClass& SchedulerClass::setMemoryPattern(MemoryPattern const& pattern) {
    memoryPattern = pattern;
    return *this;
}

SchedulerClass& SchedulerClass::setPositional() {
    setMemoryPattern(MemoryPattern::Arbitrary);
    setMemoryStates(1);
    return *this;
}

}  // namespace storage
}  // namespace storm
