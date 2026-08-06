#pragma once

#include "storm/storage/Decomposition.h"
#include "storm/storage/StateBlock.h"

namespace storm {
namespace storage {

/*!
 * This class represents a strongly connected component, i.e., a set of states such that every state can reach
 * every other state.
 */
class StronglyConnectedComponent : public StateBlock {
   public:
    StronglyConnectedComponent();
    StronglyConnectedComponent(StronglyConnectedComponent const& other) = default;
    StronglyConnectedComponent(StronglyConnectedComponent&& other) = default;
    StronglyConnectedComponent& operator=(StronglyConnectedComponent const& other) = default;
    StronglyConnectedComponent& operator=(StronglyConnectedComponent&& other) = default;

    /*!
     * Sets whether this SCC is trivial or not.
     *
     * @param trivial A flag indicating whether this SCC is trivial or not.
     */
    void setIsTrivial(bool trivial);

    /*!
     * Retrieves whether this SCC is trivial.
     *
     * @return True iff this SCC is trivial.
     */
    bool isTrivial() const;

   private:
    // Stores whether this SCC is trivial.
    bool isTrivialScc;
};

}  // namespace storage
}  // namespace storm
