#pragma once

#include <unordered_map>
#include <vector>

#include "storm-pomdp/beliefs/utility/types.h"

namespace storm::pomdp::beliefs {

/**
 * Interns beliefs and assigns them consecutive identifiers.
 *
 * IDs are stable for the collector's lifetime. References returned from the collector may be invalidated when new
 * beliefs are inserted.
 */
template<typename BeliefType>
class BeliefCollector {
   public:
    /*!
     * Indicates that this collector gives IDs in a consecutive way, ensuring that all Ids are less than `getNumberOfBeliefIds()`
     */
    static constexpr bool HasConsecutiveIds = true;

    /*!
     * @return true, if either the IDs are equal or the represented beliefs are equal.
     */
    bool isEqual(BeliefId const& first, BeliefId const& second) const;

    /*!
     * @return The number of beliefs that have been collected.
     */
    BeliefId getNumberOfBeliefIds() const;

    /*!
     * @return The belief that corresponds to the given ID.
     * @note the returned belief is a reference to the internal storage which might be invalidated when further beliefs are added.
     */
    BeliefType const& getBeliefFromId(BeliefId const& id) const;

    /*!
     * @return The ID of the given belief.
     */
    BeliefId getIdFromBelief(BeliefType const& belief) const;

    /*!
     * @return true, if the given belief is present in the collector.
     */
    bool containsBelief(BeliefType const& belief) const;

    /*!
     * @return true, if the given ID is present in the collector.
     */
    bool containsId(BeliefId const& id) const;

    /*!
     * @return The Id of the given belief if it is present in the collector. `InvalidBeliefId` otherwise.
     */
    BeliefId getIdOptional(BeliefType const& belief) const;

    /*!
     * Checks if the given belief has already been collected before.
     *  - If yes, the Id is returned.
     *  - If not, the belief is collected and a fresh Id is allocated.
     */
    BeliefId getIdOrAddBelief(BeliefType&& belief);

    /*!
     * Adds a fresh belief and returns its ID.
     * @pre The belief must not be present in this collector.
     */
    BeliefId addBelief(BeliefType&& inputBelief);

   private:
    std::vector<BeliefType> gatheredBeliefs;
    std::vector<std::unordered_map<BeliefType, BeliefId, typename BeliefType::BeliefHash>> beliefToIdMap;
};

}  // namespace storm::pomdp::beliefs
