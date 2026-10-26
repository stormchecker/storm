#pragma once

#include <cstdint>
#include <ostream>

#include "storm/storage/scheduler/ExplicitGameStrategy.h"

namespace storm {
namespace storage {

class ExplicitGameStrategyPair {
   public:
    ExplicitGameStrategyPair(uint64_t numberOfPlayer1States, uint64_t numberOfPlayer2States);
    ExplicitGameStrategyPair(ExplicitGameStrategy&& player1Strategy, ExplicitGameStrategy&& player2Strategy);

    ExplicitGameStrategy& getPlayer1Strategy();
    ExplicitGameStrategy const& getPlayer1Strategy() const;
    ExplicitGameStrategy& getPlayer2Strategy();
    ExplicitGameStrategy const& getPlayer2Strategy() const;

    uint64_t getNumberOfUndefinedPlayer1States() const;
    uint64_t getNumberOfUndefinedPlayer2States() const;

   private:
    ExplicitGameStrategy player1Strategy;
    ExplicitGameStrategy player2Strategy;
};

std::ostream& operator<<(std::ostream& out, ExplicitGameStrategyPair const& strategyPair);

}  // namespace storage
}  // namespace storm
