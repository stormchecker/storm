#pragma once

#include <memory>

#include "storm/storage/Scheduler.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename ValueType>
struct CvarComputationResult {
    ValueType value;
    std::unique_ptr<storm::storage::Scheduler<ValueType>> scheduler;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
