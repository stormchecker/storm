#pragma once

#include <string>

#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace models {
namespace sparse {

class StateAnnotation {
   public:
    virtual ~StateAnnotation() = default;

    virtual std::string getStateInfo(storm::storage::sparse::state_type const& state) const = 0;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
