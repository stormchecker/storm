#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "storm/logic/Formula.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm::pomdp::transformer {

template<typename ValueType>
class RewardBoundUnfolder {
   public:
    struct ReturnType {
        std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
        std::shared_ptr<storm::logic::Formula> formula;
    };

    struct UnfoldingOptions {
        /// Allows to define a levelWidth for each dimension.
        /// If a non-zero levelWidth is given for a dimension, the unfolding in that dimension is performed according to the given width
        /// A fresh reward assignment will be introduced that indicates how often a transition exceeds the given levelWidth
        /// The returned formula will then have a rewardBound in terms of the new level reward.
        /// Assume a dimension i. Let c=levelWidth[i]!=0 and let t be the reward bound threshold (i.e. either <=t or >t).
        /// A transition from epoch a=e[i] with reward b=r[i] leads to epoch (a-b) mod c and yields level reward ceil((b-a)/c).
        /// The initial epoch is -t mod c (Note: in C++ this is (-t)%c+c because % is not the modulos for negative numerators).
        /// The output formula will have reward bound threshold ceil(t/c), where t is the original threshold.
        /// Note that if c=1, no unfolding will be performed.
        /// The case levelWidth[i]=0 is special: this means that the dimension is unfolded until past the threshold.
        /// No level reward is introduced in this case and the bound dimension is removed from the formula.
        /// levelWidth.size() <= i is treated equivalently to levelWidth[i]=0.
        std::vector<uint64_t> levelWidths{};

        /// The reward models that will be preserved in the unfolding.
        std::set<std::string> preservedRewardModels{};
    };

    /**
     * Unfolds reward-bounded dimensions into the model state space and returns an equivalent unbounded formula.
     *
     * Zero level widths unfold a dimension completely and remove its bound. Positive level widths retain a compact
     * epoch representation and introduce a corresponding level reward model.
     */
    static ReturnType transform(storm::models::sparse::Model<ValueType> const& model, storm::logic::Formula const& formula,
                                UnfoldingOptions const& options = {});
};

}  // namespace storm::pomdp::transformer
