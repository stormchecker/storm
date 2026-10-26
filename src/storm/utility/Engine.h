#pragma once

#include <ostream>
#include <vector>

#include "storm/builder/BuilderType.h"
#include "storm/dd/DdType.h"
#include "storm/models/ModelType.h"

namespace storm {

// Forward Declarations
namespace jani {
class Property;
}

namespace logic {
class Formula;
}
namespace modelchecker {
template<typename FormulaType, typename ValueType>
class CheckTask;
}
namespace languages {
class SymbolicModelDescription;
}

namespace utility {

/// An enumeration of all engines.
enum class Engine {
    // The last one should always be 'Unknown' to make sure that the getEngines() method below works.
    Sparse,
    Hybrid,
    Dd,
    DdSparse,
    Exploration,
    AbstractionRefinement,
    Automatic,
    Unknown
};

/*!
 * Returns a list of all available engines (excluding Unknown)
 */
std::vector<Engine> getEngines();

/*!
 * Returns a string representation of the given engine.
 */
std::string toString(Engine const& engine);

/*!
 * Writes the string representation of the given engine to the given stream
 */
std::ostream& operator<<(std::ostream& os, Engine const& engine);

/*!
 * Parses the string representation of an engine and returns the corresponding engine.
 * If the engine is not found, we print an error and return Engine::Unknown.
 */
Engine engineFromString(std::string const& engineStr);

/*!
 * Returns the builder type used for the given engine.
 */
storm::builder::BuilderType getBuilderType(storm::utility::Engine const& engine);

/*!
 * Returns false if the given model description and one of the given properties can certainly not be handled by the given engine.
 * Notice that the set of handable model checking queries is only overapproximated, i.e. if this returns true,
 * the query could still be not supported by the engine. This behavior is due to the fact that we sometimes need
 * to actually build the model in order to decide whether it is supported.
 */
template<typename ValueType>
bool canHandle(storm::utility::Engine const& engine, std::vector<storm::jani::Property> const& properties,
               storm::languages::SymbolicModelDescription const& modelDescription);
}  // namespace utility
}  // namespace storm