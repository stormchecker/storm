#include "PlayerCoalition.h"

#include <ostream>

namespace storm {
namespace logic {

PlayerCoalition::PlayerCoalition(std::vector<std::variant<std::string, storm::storage::PlayerIndex>> const& playerIds) : _playerIds(playerIds) {
    // Intentionally left empty.
}

std::vector<std::variant<std::string, storm::storage::PlayerIndex>> const& PlayerCoalition::getPlayers() const {
    return _playerIds;
}

template<typename T0, typename... Ts>
std::ostream& operator<<(std::ostream& s, std::variant<T0, Ts...> const& v) {
    std::visit([&](auto&& arg) { s << arg; }, v);
    return s;
}

std::ostream& operator<<(std::ostream& stream, PlayerCoalition const& coalition) {
    bool firstItem = true;
    for (auto const& id : coalition._playerIds) {
        if (firstItem) {
            firstItem = false;
        } else {
            stream << ",";
        }
        stream << id;
    }
    return stream;
}
}  // namespace logic
}  // namespace storm
