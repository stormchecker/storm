#pragma once

#include <set>
#include <string>

namespace storm {
namespace jani {

enum class ModelFeature { Arrays, DerivedOperators, Functions, StateExitRewards, MultiObjectiveProperties, TrigonometricFunctions };

std::string toString(ModelFeature const& modelFeature);

class ModelFeatures {
   public:
    std::string toString() const;

    bool hasArrays() const;
    bool hasFunctions() const;
    bool hasDerivedOperators() const;
    bool hasStateExitRewards() const;
    bool hasMultiObjectiveProperties() const;
    bool hasTrigonometricFunctions() const;

    // Returns true, if no model feature is enabled.
    bool empty() const;
    std::set<ModelFeature> const& asSet() const;

    ModelFeatures& add(ModelFeature const& modelFeature);
    void remove(ModelFeature const& modelFeature);

   private:
    std::set<ModelFeature> features;
};

ModelFeatures getAllKnownModelFeatures();

}  // namespace jani
}  // namespace storm
