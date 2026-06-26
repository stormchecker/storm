#ifndef STORM_GENERATOR_VARIABLEINFORMATION_H_
#define STORM_GENERATOR_VARIABLEINFORMATION_H_

#include <unordered_map>
#include <vector>

#include "storm/generator/ArrayVariableReplacementInformation.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace prism {
class Program;
}

namespace jani {
class Model;
class Automaton;
struct ArrayEliminatorData;
class VariableSet;
}  // namespace jani

namespace generator {
// A structure storing information about the boolean variables of the model.
struct BooleanVariableInformation {
    BooleanVariableInformation(storm::expressions::Variable const& variable, uint_fast64_t bitOffset, bool global, bool observable);

    std::string const& getName() const {
        return variable.getName();
    }

    // The boolean variable.
    storm::expressions::Variable variable;

    // Its bit offset in the compressed state.
    uint_fast64_t bitOffset;

    // A flag indicating whether the variable is a global one.
    bool global;

    //
    bool observable;
};

// A structure storing information about the integer variables of the model.
struct IntegerVariableInformation {
    IntegerVariableInformation(storm::expressions::Variable const& variable, int_fast64_t lowerBound, int_fast64_t upperBound, uint_fast64_t bitOffset,
                               uint_fast64_t bitWidth, bool global = false, bool observable = true, bool forceOutOfBoundsCheck = false);

    std::string const& getName() const {
        return variable.getName();
    }

    // The integer variable.
    storm::expressions::Variable variable;

    // The lower bound of its range.
    int_fast64_t lowerBound;

    // The upper bound of its range.
    int_fast64_t upperBound;

    // Its bit offset in the compressed state.
    uint_fast64_t bitOffset;

    // Its bit width in the compressed state.
    uint_fast64_t bitWidth;

    // A flag indicating whether the variable is a global one.
    bool global;

    bool observable;

    // A flag indicating whether an out of bounds check is enforced for this variable.
    bool forceOutOfBoundsCheck;
};

// A structure storing information about the location variables of the model.
struct LocationVariableInformation {
    LocationVariableInformation(storm::expressions::Variable const& variable, uint64_t highestValue, uint_fast64_t bitOffset, uint_fast64_t bitWidth,
                                bool observable);

    // The expression variable for this location.
    storm::expressions::Variable variable;

    // The highest possible location value.
    uint64_t highestValue;

    // Its bit offset in the compressed state.
    uint_fast64_t bitOffset;

    // Its bit width in the compressed state.
    uint_fast64_t bitWidth;

    bool observable;
};

struct ObservationLabelInformation {
    ObservationLabelInformation(storm::expressions::Variable const& variable);

    std::string const& getName() const {
        return variable.getName();
    }

    storm::expressions::Variable variable;

    bool deterministic = true;
};

// A structure storing information about the used variables of the program.
struct VariableInformation {
    VariableInformation(storm::prism::Program const& program, uint64_t reservedBitsForUnboundedVariables, bool outOfBoundsState = false);
    VariableInformation(storm::jani::Model const& model, std::vector<std::reference_wrapper<storm::jani::Automaton const>> const& parallelAutomata,
                        uint64_t reservedBitsForUnboundedVariables, bool outOfBoundsState);

    VariableInformation() = default;
    uint_fast64_t getTotalBitOffset(bool roundTo64Bit = false) const;

    void registerArrayVariableReplacements(storm::jani::ArrayEliminatorData const& arrayEliminatorData);
    BooleanVariableInformation const& getBooleanArrayVariableReplacement(storm::expressions::Variable const& arrayVariable,
                                                                         std::vector<uint64_t> const& arrayIndexVector) const;
    IntegerVariableInformation const& getIntegerArrayVariableReplacement(storm::expressions::Variable const& arrayVariable,
                                                                         std::vector<uint64_t> const& arrayIndexVector) const;

    /// The total bit offset over all variables.
    uint_fast64_t totalBitOffset;

    /// The location variables.
    std::vector<LocationVariableInformation> locationVariables;

    /// The boolean variables.
    std::vector<BooleanVariableInformation> booleanVariables;

    /// The integer variables.
    std::vector<IntegerVariableInformation> integerVariables;

    /// The observation labels
    std::vector<ObservationLabelInformation> observationLabels;

    /// Replacements for each array variable
    std::unordered_map<storm::expressions::Variable, ArrayVariableReplacementInformation> arrayVariableToElementInformations;

    bool hasOutOfBoundsBit() const;

    uint64_t getOutOfBoundsBit() const;

    std::optional<BooleanVariableInformation> outOfBoundsBit;

   private:
    /*!
     * Sorts the variables to establish a known ordering.
     */
    void sortVariables();

    /*!
     * Creates all necessary variables for a JANI automaton.
     */
    void createVariablesForAutomaton(storm::jani::Automaton const& automaton, uint64_t reservedBitsForUnboundedVariables);

    /*!
     * Creates all non-transient variables from the given set
     */
    void createVariablesForVariableSet(storm::jani::VariableSet const& variableSet, uint64_t reservedBitsForUnboundedVariables, bool global);
};

}  // namespace generator
}  // namespace storm

#endif /* STORM_GENERATOR_VARIABLEINFORMATION_H_ */
