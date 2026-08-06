#pragma once

#include "storm/logic/ProbabilityOperatorFormula.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/transformer/ChoiceSelector.h"

namespace storm {

class Environment;

namespace ps {

class PermissiveScheduler {
   public:
    virtual ~PermissiveScheduler() = default;
};

template<typename RM = storm::models::sparse::StandardRewardModel<double>>
class SubMDPPermissiveScheduler : public PermissiveScheduler {
    storm::models::sparse::Mdp<double, RM> const& mdp;
    storm::storage::BitVector enabledChoices;

   public:
    virtual ~SubMDPPermissiveScheduler() = default;

    SubMDPPermissiveScheduler(SubMDPPermissiveScheduler&&) = default;

    SubMDPPermissiveScheduler(SubMDPPermissiveScheduler const&) = delete;

    SubMDPPermissiveScheduler(storm::models::sparse::Mdp<double, RM> const& refmdp, bool allEnabled)
        : PermissiveScheduler(), mdp(refmdp), enabledChoices(refmdp.getNumberOfChoices(), allEnabled) {
        // Intentionally left empty.
    }

    void disable(uint_fast64_t choiceIndex) {
        STORM_LOG_ASSERT(choiceIndex < enabledChoices.size(), "Invalid choiceIndex.");
        enabledChoices.set(choiceIndex, false);
    }

    storm::models::sparse::Mdp<double, RM> apply() const {
        storm::transformer::ChoiceSelector<double, RM> cs(mdp);
        return *(cs.transform(enabledChoices)->template as<storm::models::sparse::Mdp<double, RM>>());
    }

    template<typename T>
    std::map<uint_fast64_t, T> remapChoiceIndices(std::map<uint_fast64_t, T> const& in) const {
        std::map<uint_fast64_t, T> res;
        uint_fast64_t last = 0;
        uint_fast64_t curr = 0;
        storm::storage::BitVector::const_iterator it = enabledChoices.begin();
        for (auto const& entry : in) {
            curr = entry.first;
            uint_fast64_t diff = last - curr;
            it += diff;
            res[*it] = entry.second;
            last = curr;
        }
        return res;
    }

    template<typename T>
    std::map<uint_fast64_t, T> remapChoiceIndices(std::map<storm::storage::StateActionPair, T> const& in) const {
        std::map<uint_fast64_t, T> res;
        uint_fast64_t last = 0;
        uint_fast64_t curr = 0;
        storm::storage::BitVector::const_iterator it = enabledChoices.begin();
        for (auto const& entry : in) {
            curr = mdp.getChoiceIndex(entry.first);
            uint_fast64_t diff = curr - last;
            it += diff;
            res[*it] = entry.second;
            last = curr;
        }
        return res;
    }
};

template<typename RM = storm::models::sparse::StandardRewardModel<double>>
boost::optional<SubMDPPermissiveScheduler<RM>> computePermissiveSchedulerViaMILP(storm::Environment const& env,
                                                                                 storm::models::sparse::Mdp<double, RM> const& mdp,
                                                                                 storm::logic::ProbabilityOperatorFormula const& safeProp);

template<typename RM>
boost::optional<SubMDPPermissiveScheduler<RM>> computePermissiveSchedulerViaSMT(storm::models::sparse::Mdp<double, RM> const& mdp,
                                                                                storm::logic::ProbabilityOperatorFormula const& safeProp);
}  // namespace ps
}  // namespace storm
