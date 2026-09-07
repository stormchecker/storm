#include "Player.h"

#include <ostream>

namespace storm {
namespace prism {
Player::Player(std::string const& playerName, std::unordered_set<std::string> const& controlledModules,
               std::unordered_set<std::string> const& controlledActions, std::string const& filename, uint_fast32_t lineNumber)
    : LocatedInformation(filename, lineNumber), playerName(playerName), controlledModules(controlledModules), controlledActions(controlledActions) {
    // Nothing to do here.
}

std::string const& Player::getName() const {
    return this->playerName;
}

std::unordered_set<std::string> const& Player::getModules() const {
    return this->controlledModules;
}

std::unordered_set<std::string> const& Player::getActions() const {
    return this->controlledActions;
}

std::ostream& operator<<(std::ostream& stream, Player const& player) {
    stream << "player";
    if (player.getName() != "") {
        stream << " " << player.getName();
    }
    bool firstElement = true;
    for (auto const& module : player.getModules()) {
        if (firstElement) {
            firstElement = false;
        } else {
            stream << ",";
        }
        stream << "\n\t" << module;
    }
    for (auto const& action : player.getActions()) {
        if (firstElement) {
            firstElement = false;
        } else {
            stream << ",";
        }
        stream << "\n\t[" << action << "]";
    }
    stream << "\nendplayer\n";
    return stream;
}
}  // namespace prism
}  // namespace storm
