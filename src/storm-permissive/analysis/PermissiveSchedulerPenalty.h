#pragma once

#include <cstdint>
#include <unordered_map>

#include "storm/storage/StateActionPair.h"

namespace storm {
namespace ps {

class PermissiveSchedulerPenalties {
    std::unordered_map<storage::StateActionPair, double> mPenalties;

   public:
    double get(uint_fast64_t state, uint_fast64_t action) const {
        return get(storage::StateActionPair(state, action));
    }

    double get(storage::StateActionPair const& sap) const {
        auto it = mPenalties.find(sap);
        if (it == mPenalties.end()) {
            return 1.0;
        } else {
            return it->second;
        }
    }

    void set(uint64_t state, uint64_t action, double penalty) {
        STORM_LOG_ASSERT(penalty >= 1.0, "Penalty too low.");
        if (penalty == 1.0) {
            auto it = mPenalties.find(std::make_pair(state, action));
            if (it != mPenalties.end()) {
                mPenalties.erase(it);
            }
        } else {
            mPenalties.emplace(std::make_pair(state, action), penalty);
        }
    }

    void clear() {
        mPenalties.clear();
    }
};
}  // namespace ps
}  // namespace storm
