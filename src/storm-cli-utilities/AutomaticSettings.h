#pragma once

#include <cstdint>

#include "storm/utility/Engine.h"

namespace storm {

namespace jani {
class Model;
class Property;
}  // namespace jani

namespace utility {
class AutomaticSettings {
   public:
    AutomaticSettings();

    /*!
     * Predicts "good" settings for the provided model checking query
     */
    void predict(storm::jani::Model const& model, storm::jani::Property const& property);

    /*!
     * Predicts "good" settings for the provided model checking query
     * @param stateEstimate A hint that gives a (rough) estimate for the number of states.
     */
    void predict(storm::jani::Model const& model, storm::jani::Property const& property, uint64_t stateEstimate);

    /// Retrieve "good" settings after calling predict.
    storm::utility::Engine getEngine() const;
    bool enableBisimulation() const;
    bool enableExact() const;

   private:
    // some popular configs
    void sparse();
    void hybrid();
    void dd();
    void exact();
    void ddbisim();

    storm::utility::Engine engine;
    bool useBisimulation;
    bool useExact;
};

}  // namespace utility
}  // namespace storm