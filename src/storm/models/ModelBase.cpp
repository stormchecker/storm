#include "storm/models/ModelBase.h"

#include <initializer_list>

namespace storm {
namespace models {
ModelType ModelBase::getType() const {
    return modelType;
}

bool ModelBase::isSparseModel() const {
    return false;
}

bool ModelBase::isSymbolicModel() const {
    return false;
}

std::optional<storm::dd::DdType> ModelBase::getDdType() const {
    return std::nullopt;
}

bool ModelBase::supportsUncertainty() const {
    return false;
}

bool ModelBase::isOfType(storm::models::ModelType const& modelType) const {
    return this->getType() == modelType;
}

bool ModelBase::isNondeterministicModel() const {
    for (auto const& type :
         {storm::models::ModelType::Mdp, storm::models::ModelType::Pomdp, storm::models::ModelType::S2pg, storm::models::ModelType::MarkovAutomaton}) {
        if (this->isOfType(type)) {
            return true;
        }
    }
    return false;
}

bool ModelBase::isDiscreteTimeModel() const {
    for (auto const& type : {storm::models::ModelType::Dtmc, storm::models::ModelType::Mdp, storm::models::ModelType::Pomdp, storm::models::ModelType::S2pg}) {
        if (this->isOfType(type)) {
            return true;
        }
    }
    return false;
}

bool ModelBase::isPartiallyObservable() const {
    return false;
}

bool ModelBase::supportsParameters() const {
    return false;
}

bool ModelBase::hasParameters() const {
    return false;
}

bool ModelBase::isExact() const {
    return false;
}
}  // namespace models
}  // namespace storm
