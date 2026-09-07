#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <vector>

#include "storm/storage/expressions/Expression.h"

namespace storm::gbar {
namespace abstraction {

class RefinementCommand {
   public:
    /*!
     * Creates a new refinement command for the given player 1 choice.
     */
    RefinementCommand(uint64_t referencedPlayer1Choice, std::vector<storm::expressions::Expression> const& predicates);

    /*!
     * Creates a new refinement command for all player 1 choices.
     */
    RefinementCommand(std::vector<storm::expressions::Expression> const& predicates);

    /// Access to the details of this refinement commands.
    bool refersToPlayer1Choice() const;
    uint64_t getReferencedPlayer1Choice() const;
    std::vector<storm::expressions::Expression> const& getPredicates() const;

   private:
    boost::optional<uint64_t> referencedPlayer1Choice;
    std::vector<storm::expressions::Expression> predicates;
};

}  // namespace abstraction
}  // namespace storm::gbar
