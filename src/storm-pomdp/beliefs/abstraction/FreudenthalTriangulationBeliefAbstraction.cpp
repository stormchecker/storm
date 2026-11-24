#include "storm-pomdp/beliefs/abstraction/FreudenthalTriangulationBeliefAbstraction.h"
#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm/adapters/RationalNumberAdapter.h"

namespace storm::pomdp::beliefs {

template<typename BeliefType>
FreudenthalTriangulationBeliefAbstraction<BeliefType>::FreudenthalTriangulationBeliefAbstraction(BeliefValueType const& initialResolution,
                                                                                                 FreudenthalTriangulationMode mode)
    : defaultResolution(storm::utility::ceil<BeliefValueType>(initialResolution)), mode(mode) {
    STORM_LOG_ASSERT(defaultResolution > storm::utility::zero<BeliefValueType>(),
                     "Expected that the resolution is a positive integer. Got " << defaultResolution << " instead.");
}

template class FreudenthalTriangulationBeliefAbstraction<Belief<double>>;
template class FreudenthalTriangulationBeliefAbstraction<Belief<storm::RationalNumber>>;

}  // namespace storm::pomdp::beliefs