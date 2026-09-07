#include "ModelType.h"

namespace storm {
namespace jani {

std::ostream& operator<<(std::ostream& stream, ModelType const& type) {
    return stream << to_string(type);
}

std::string to_string(ModelType const& type) {
    switch (type) {
        case ModelType::UNDEFINED:
            return "undefined";
        case ModelType::LTS:
            return "lts";
        case ModelType::DTMC:
            return "dtmc";
        case ModelType::CTMC:
            return "ctmc";
        case ModelType::MDP:
            return "mdp";
        case ModelType::CTMDP:
            return "ctmdp";
        case ModelType::MA:
            return "ma";
        case ModelType::TA:
            return "ta";
        case ModelType::PTA:
            return "pta";
        case ModelType::STA:
            return "sta";
        case ModelType::HA:
            return "ha";
        case ModelType::PHA:
            return "pha";
        case ModelType::SHA:
            return "sha";
    }
    return "invalid";
}

ModelType getModelType(std::string const& input) {
    if (input == "lts") {
        return ModelType::LTS;
    }
    if (input == "dtmc") {
        return ModelType::DTMC;
    }
    if (input == "ctmc") {
        return ModelType::CTMC;
    }
    if (input == "mdp") {
        return ModelType::MDP;
    }
    if (input == "ctmdp") {
        return ModelType::CTMDP;
    }
    if (input == "ma") {
        return ModelType::MA;
    }
    if (input == "ta") {
        return ModelType::TA;
    }
    if (input == "pta") {
        return ModelType::PTA;
    }
    if (input == "sta") {
        return ModelType::STA;
    }
    if (input == "ha") {
        return ModelType::HA;
    }
    if (input == "pha") {
        return ModelType::PHA;
    }
    if (input == "sha") {
        return ModelType::SHA;
    }
    return ModelType::UNDEFINED;
}

bool isDeterministicModel(ModelType const& modelType) {
    if (modelType == ModelType::DTMC || modelType == ModelType::CTMC) {
        return true;
    }
    return false;
}

bool isDiscreteTimeModel(ModelType const& modelType) {
    if (modelType == ModelType::DTMC || modelType == ModelType::MDP) {
        return true;
    }
    return false;
}

}  // namespace jani
}  // namespace storm
