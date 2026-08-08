#include "storm/storage/jani/ModelFeatures.h"

#include "storm/utility/macros.h"

namespace storm {
namespace jani {

std::string toString(ModelFeature const& modelFeature) {
    switch (modelFeature) {
        case ModelFeature::Arrays:
            return "arrays";
        case ModelFeature::DerivedOperators:
            return "derived-operators";
        case ModelFeature::Functions:
            return "functions";
        case ModelFeature::StateExitRewards:
            return "state-exit-rewards";
        case ModelFeature::MultiObjectiveProperties:
            return "multi-objective-properties";
        case ModelFeature::TrigonometricFunctions:
            return "trigonometric-functions";
    }
    STORM_LOG_ASSERT(false, "Unhandled model feature.");
    return "Unhandled-feature";
}

std::string ModelFeatures::toString() const {
    std::string res = "[";
    bool first = true;
    for (auto const& f : features) {
        if (!first) {
            res += ", ";
        }
        res += "\"" + storm::jani::toString(f) + "\"";
        first = false;
    }
    res += "]";
    return res;
}

bool ModelFeatures::hasArrays() const {
    return features.count(ModelFeature::Arrays) > 0;
}

bool ModelFeatures::hasDerivedOperators() const {
    return features.count(ModelFeature::DerivedOperators) > 0;
}

bool ModelFeatures::hasFunctions() const {
    return features.count(ModelFeature::Functions) > 0;
}

bool ModelFeatures::hasStateExitRewards() const {
    return features.count(ModelFeature::StateExitRewards) > 0;
}

bool ModelFeatures::hasMultiObjectiveProperties() const {
    return features.count(ModelFeature::MultiObjectiveProperties) > 0;
}

bool ModelFeatures::hasTrigonometricFunctions() const {
    return features.count(ModelFeature::TrigonometricFunctions) > 0;
}

std::set<ModelFeature> const& ModelFeatures::asSet() const {
    return features;
}

bool ModelFeatures::empty() const {
    return features.empty();
}

ModelFeatures& ModelFeatures::add(ModelFeature const& modelFeature) {
    features.insert(modelFeature);
    return *this;
}

void ModelFeatures::remove(ModelFeature const& modelFeature) {
    features.erase(modelFeature);
}

ModelFeatures getAllKnownModelFeatures() {
    return ModelFeatures()
        .add(ModelFeature::Arrays)
        .add(ModelFeature::DerivedOperators)
        .add(ModelFeature::Functions)
        .add(ModelFeature::StateExitRewards)
        .add(ModelFeature::MultiObjectiveProperties)
        .add(ModelFeature::TrigonometricFunctions);
}
}  // namespace jani
}  // namespace storm
