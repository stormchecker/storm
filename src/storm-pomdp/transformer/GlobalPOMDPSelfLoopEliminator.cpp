#include "storm-pomdp/transformer/GlobalPOMDPSelfLoopEliminator.h"

#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/storage/BitVector.h"
#include "storm/transformer/ChoiceSelector.h"

namespace storm {
namespace transformer {

template<typename ValueType>
bool GlobalPOMDPSelfLoopEliminator<ValueType>::preservesFormula(storm::logic::Formula const& formula) const {
    if (formula.isOperatorFormula() && formula.asOperatorFormula().hasOptimalityType()) {
        bool maxProb = formula.isProbabilityOperatorFormula() && storm::solver::maximize(formula.asOperatorFormula().getOptimalityType());
        bool minRew = formula.isRewardOperatorFormula() && storm::solver::minimize(formula.asOperatorFormula().getOptimalityType());

        auto const& subformula = formula.asOperatorFormula().getSubformula();
        if (subformula.isEventuallyFormula()) {
            if (subformula.asEventuallyFormula().getSubformula().isInFragment(storm::logic::propositional())) {
                return maxProb || minRew;
            }
        } else if (subformula.isUntilFormula()) {
            if (subformula.asUntilFormula().getLeftSubformula().isInFragment(storm::logic::propositional()) &&
                subformula.asUntilFormula().getRightSubformula().isInFragment(storm::logic::propositional())) {
                return maxProb;
            }
        }
    }
    return false;
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> GlobalPOMDPSelfLoopEliminator<ValueType>::transform() const {
    uint64_t nrStates = pomdp.getNumberOfStates();

    // For each observation, get the choice indices that have a selfloop at every state with that observation
    std::vector<storm::storage::BitVector> observationSelfLoopMasks(pomdp.getNrObservations());
    for (uint64_t state = 0; state < nrStates; ++state) {
        uint32_t observation = pomdp.getObservation(state);
        STORM_LOG_ASSERT(pomdp.getNumberOfChoices(state) != 0, "State with no choices found.");

        STORM_LOG_ASSERT(observation < observationSelfLoopMasks.size(),
                         "Observation index (" << observation << ") should be less than number of observations (" << observationSelfLoopMasks.size() << "). ");
        auto& observationSelfLoopMask = observationSelfLoopMasks[observation];
        // If we see this observation for the first time, add the corresponding amount of bits to the observation mask

        if (observationSelfLoopMask.size() == 0) {
            observationSelfLoopMask.resize(pomdp.getNumberOfChoices(state), true);
        }
        STORM_LOG_ASSERT(observationSelfLoopMask.size() == pomdp.getNumberOfChoices(state),
                         "State " << state << " has " << pomdp.getNumberOfChoices(state)
                                  << " actions, different from other state(s) with same observation, which have " << observationSelfLoopMask.size()
                                  << " actions.");

        // Iterate over all set bits in observationSelfLoopMask
        // We use `getNextSetIndex` (instead of the more concise `for each`-syntax) to safely unset bits in the BitVector while iterating over it.
        for (uint64_t localChoiceIndex = observationSelfLoopMask.getNextSetIndex(0); localChoiceIndex < observationSelfLoopMask.size();
             localChoiceIndex = observationSelfLoopMask.getNextSetIndex(localChoiceIndex + 1)) {
            // We just look at the first entry.
            auto row = pomdp.getTransitionMatrix().getRow(state, localChoiceIndex);
            STORM_LOG_ASSERT(row.getNumberOfEntries() > 0, "Unexpected empty row.");
            if (row.getNumberOfEntries() > 1 || row.begin()->getColumn() != state) {
                observationSelfLoopMask.set(localChoiceIndex, false);
            }
        }
    }

    // For each observation, make sure that we keep at least one choice
    for (auto& obsMask : observationSelfLoopMasks) {
        if (obsMask.size() > 0 && obsMask.full()) {
            obsMask.set(0, false);
        }
    }

    // Finally create a filter that is 1 for every choice we can keep.
    storm::storage::BitVector filter(pomdp.getNumberOfChoices(), true);
    uint64_t offset = 0;
    for (uint64_t state = 0; state < nrStates; ++state) {
        storm::storage::BitVector& observationSelfLoopMask = observationSelfLoopMasks[pomdp.getObservation(state)];
        STORM_LOG_ASSERT(!observationSelfLoopMask.full(), "Observation self-loop mask is full.");
        for (auto const localChoiceIndex : observationSelfLoopMask) {
            filter.set(offset + localChoiceIndex, false);
        }
        offset += pomdp.getNumberOfChoices(state);
    }

    // We do not consider rewards right now, since this reduction only preserves maximizing probabilities and minimizing expected rewards, anyway.
    // In both cases, the reward at a selfloop has no effect (assuming not reaching goal means infinite reward)

    STORM_LOG_INFO("Selfloop reduction eliminates " << (filter.size() - filter.getNumberOfSetBits()) << " choices.");

    ChoiceSelector<ValueType> cs(pomdp);
    auto res = cs.transform(filter)->template as<storm::models::sparse::Pomdp<ValueType>>();

    // The transformation preserves canonicity.
    res->setIsCanonic(pomdp.isCanonic());
    return res;
}

template class GlobalPOMDPSelfLoopEliminator<double>;
template class GlobalPOMDPSelfLoopEliminator<storm::RationalNumber>;
}  // namespace transformer
}  // namespace storm
