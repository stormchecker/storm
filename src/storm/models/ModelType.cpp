#include "ModelType.h"

#include "storm/exceptions/InvalidTypeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace models {

ModelType getModelType(std::string const& type) {
    if (type == "DTMC") {
        return ModelType::Dtmc;
    } else if (type == "CTMC") {
        return ModelType::Ctmc;
    } else if (type == "MDP") {
        return ModelType::Mdp;
    } else if (type == "Markov Automaton") {
        return ModelType::MarkovAutomaton;
    } else if (type == "S2PG") {
        return ModelType::S2pg;
    } else if (type == "POMDP") {
        return ModelType::Pomdp;
    } else if (type == "SMG") {
        return ModelType::Smg;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Type " << type << "not known.");
    }
}

std::ostream& operator<<(std::ostream& os, ModelType const& type) {
    switch (type) {
        case ModelType::Dtmc:
            os << "DTMC";
            break;
        case ModelType::Ctmc:
            os << "CTMC";
            break;
        case ModelType::Mdp:
            os << "MDP";
            break;
        case ModelType::MarkovAutomaton:
            os << "Markov Automaton";
            break;
        case ModelType::S2pg:
            os << "S2PG";
            break;
        case ModelType::Pomdp:
            os << "POMDP";
            break;
        case ModelType::Smg:
            os << "SMG";
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unknown model type.");
    }
    return os;
}
}  // namespace models
}  // namespace storm
