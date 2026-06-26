#include "storm/generator/VariableInformation.h"

#include "storm/storage/jani/Model.h"
#include "storm/storage/prism/Program.h"

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/eliminator/ArrayEliminator.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/macros.h"

#include <cmath>

namespace storm {
namespace generator {

BooleanVariableInformation::BooleanVariableInformation(storm::expressions::Variable const& variable, uint_fast64_t bitOffset, bool global, bool observable)
    : variable(variable), bitOffset(bitOffset), global(global), observable(observable) {
    // Intentionally left empty.
}

IntegerVariableInformation::IntegerVariableInformation(storm::expressions::Variable const& variable, int_fast64_t lowerBound, int_fast64_t upperBound,
                                                       uint_fast64_t bitOffset, uint_fast64_t bitWidth, bool global, bool observable,
                                                       bool forceOutOfBoundsCheck)
    : variable(variable),
      lowerBound(lowerBound),
      upperBound(upperBound),
      bitOffset(bitOffset),
      bitWidth(bitWidth),
      global(global),
      observable(observable),
      forceOutOfBoundsCheck(forceOutOfBoundsCheck) {
    // Intentionally left empty.
}

LocationVariableInformation::LocationVariableInformation(storm::expressions::Variable const& variable, uint64_t highestValue, uint_fast64_t bitOffset,
                                                         uint_fast64_t bitWidth, bool observable)
    : variable(variable), highestValue(highestValue), bitOffset(bitOffset), bitWidth(bitWidth), observable(observable) {
    // Intentionally left empty.
}

ObservationLabelInformation::ObservationLabelInformation(storm::expressions::Variable const& variable) : variable(variable) {
    // Intentionally left empty.
}

/*!
 * Small helper function that sets unspecified lower/upper bounds for an integer variable based on the provided reservedBitsForUnboundedVariables and returns
 * the number of bits required to represent the final variable range
 * @pre If has[Lower,Upper]Bound is true, [lower,upper]Bound must be set to the corresponding bound.
 * @post lowerBound and upperBound are set to the considered bound for this variable
 * @param hasLowerBound shall be true iff there is a lower bound given
 * @param lowerBound a reference to the lower bound value
 * @param hasUpperBound shall be true iff there is an upper bound given
 * @param upperBound a reference to the upper bound
 * @param reservedBitsForUnboundedVariables the number of bits that shall be used to represent unbounded variables
 * @return the number of bits required to represent the final variable range
 */
uint64_t getBitWidthLowerUpperBound(bool const& hasLowerBound, int64_t& lowerBound, bool const& hasUpperBound, int64_t& upperBound,
                                    uint64_t const& reservedBitsForUnboundedVariables) {
    if (hasLowerBound) {
        if (hasUpperBound) {
            STORM_LOG_THROW(lowerBound <= upperBound, storm::exceptions::WrongFormatException, "Lower bound must not be above upper bound");
            // We do not have to set any bounds in this case.
            // Return the number of bits required to store all the values between lower and upper bound
            return static_cast<uint64_t>(std::ceil(std::log2(upperBound - lowerBound + 1)));
        } else {
            // We only have a lower bound. Find the largest upper bound we can store with the given number of bits.
            upperBound = lowerBound + ((1ll << reservedBitsForUnboundedVariables) - 1);
        }
    } else {
        if (hasUpperBound) {
            // We only have an upper bound. Find the smallest lower bound we can store with the given number of bits
            lowerBound = upperBound - ((1ll << reservedBitsForUnboundedVariables) - 1);
        } else {
            // We neither have a lower nor an upper bound. Take the usual n-bit integer values for lower/upper bounds
            lowerBound = -(1ll << (reservedBitsForUnboundedVariables - 1));     // = -2^(reservedBits-1)
            upperBound = (1ll << (reservedBitsForUnboundedVariables - 1)) - 1;  // = 2^(reservedBits-1) - 1
        }
    }
    // If we reach this point, it means that the variable is unbounded.
    // Lets check for potential overflows.
    STORM_LOG_THROW(lowerBound <= upperBound, storm::exceptions::WrongFormatException,
                    "Lower bound must not be above upper bound. Has there been an integer over-/underflow?");
    // By choice of the lower/upper bound, the number of reserved bits must coincide with the bitwidth
    STORM_LOG_ASSERT(reservedBitsForUnboundedVariables == static_cast<uint64_t>(std::ceil(std::log2(upperBound - lowerBound + 1))),
                     "Unexpected bitwidth for unbounded variable.");
    return reservedBitsForUnboundedVariables;
}

VariableInformation::VariableInformation(storm::prism::Program const& program, uint64_t reservedBitsForUnboundedVariables, bool outOfBoundsState)
    : totalBitOffset(0) {
    if (outOfBoundsState) {
        outOfBoundsBit.emplace(program.getManager().declareBooleanVariable("_OutOfBoundsBit"), totalBitOffset, true, false);
        ++totalBitOffset;
    }
    for (auto const& booleanVariable : program.getGlobalBooleanVariables()) {
        booleanVariables.emplace_back(booleanVariable.getExpressionVariable(), totalBitOffset, true, booleanVariable.isObservable());
        ++totalBitOffset;
    }
    for (auto const& integerVariable : program.getGlobalIntegerVariables()) {
        int64_t lowerBound, upperBound;
        if (integerVariable.hasLowerBoundExpression()) {
            lowerBound = integerVariable.getLowerBoundExpression().evaluateAsInt();
        }
        if (integerVariable.hasUpperBoundExpression()) {
            upperBound = integerVariable.getUpperBoundExpression().evaluateAsInt();
        }
        uint64_t bitwidth = getBitWidthLowerUpperBound(integerVariable.hasLowerBoundExpression(), lowerBound, integerVariable.hasUpperBoundExpression(),
                                                       upperBound, reservedBitsForUnboundedVariables);
        integerVariables.emplace_back(integerVariable.getExpressionVariable(), lowerBound, upperBound, totalBitOffset, bitwidth, true,
                                      integerVariable.isObservable(), !integerVariable.hasLowerBoundExpression() || !integerVariable.hasUpperBoundExpression());
        totalBitOffset += bitwidth;
    }
    for (auto const& module : program.getModules()) {
        for (auto const& booleanVariable : module.getBooleanVariables()) {
            booleanVariables.emplace_back(booleanVariable.getExpressionVariable(), totalBitOffset, false, booleanVariable.isObservable());
            ++totalBitOffset;
        }
        for (auto const& integerVariable : module.getIntegerVariables()) {
            int64_t lowerBound, upperBound;
            if (integerVariable.hasLowerBoundExpression()) {
                lowerBound = integerVariable.getLowerBoundExpression().evaluateAsInt();
            }
            if (integerVariable.hasUpperBoundExpression()) {
                upperBound = integerVariable.getUpperBoundExpression().evaluateAsInt();
            }
            uint64_t bitwidth = getBitWidthLowerUpperBound(integerVariable.hasLowerBoundExpression(), lowerBound, integerVariable.hasUpperBoundExpression(),
                                                           upperBound, reservedBitsForUnboundedVariables);
            integerVariables.emplace_back(integerVariable.getExpressionVariable(), lowerBound, upperBound, totalBitOffset, bitwidth, false,
                                          integerVariable.isObservable(),
                                          !integerVariable.hasLowerBoundExpression() || !integerVariable.hasUpperBoundExpression());
            totalBitOffset += bitwidth;
        }
    }
    for (auto const& oblab : program.getObservationLabels()) {
        storm::expressions::Variable obVar;
        if (program.getManager().hasVariable(oblab.getName())) {
            obVar = program.getManager().getVariable(oblab.getName());
            auto const& obPredicate = oblab.getStatePredicateExpression();
            STORM_LOG_THROW(obPredicate.isVariable() && obPredicate.getBaseExpression().asVariableExpression().getVariable() == obVar, storm::exceptions::NotSupportedException, "Observation valuations for label '" << oblab << " is not supported since a variable '" << oblab.getName() << "' is already known and the expression '" << oblab.getStatePredicateExpression() << "' is not equal to it.");
        } else {
            obVar = program.getManager().declareVariable(oblab.getName(), oblab.getStatePredicateExpression().getType());
        }
        observationLabels.emplace_back(obVar);
    }

    sortVariables();
}

VariableInformation::VariableInformation(storm::jani::Model const& model,
                                         std::vector<std::reference_wrapper<storm::jani::Automaton const>> const& parallelAutomata,
                                         uint64_t reservedBitsForUnboundedVariables, bool outOfBoundsState)
    : totalBitOffset(0) {
    // Check that the model does not contain non-transient real variables.
    STORM_LOG_THROW(!model.getGlobalVariables().containsNonTransientRealVariables(), storm::exceptions::InvalidArgumentException,
                    "Cannot build model from JANI model that contains global non-transient real variables.");
    for (auto const& automaton : model.getAutomata()) {
        STORM_LOG_THROW(!automaton.getVariables().containsNonTransientRealVariables(), storm::exceptions::InvalidArgumentException,
                        "Cannot build model from JANI model that contains non-transient real variables in automaton '" << automaton.getName() << "'.");
    }

    if (outOfBoundsState) {
        std::string outOfBoundsVarName = "_OutOfBoundsBit";
        while (model.getManager().hasVariable(outOfBoundsVarName)) {
            outOfBoundsVarName += "_";
        }
        outOfBoundsBit.emplace(model.getManager().declareBooleanVariable(outOfBoundsVarName), totalBitOffset, true, false);
        ++totalBitOffset;
    }

    createVariablesForVariableSet(model.getGlobalVariables(), reservedBitsForUnboundedVariables, true);

    for (auto const& automatonRef : parallelAutomata) {
        createVariablesForAutomaton(automatonRef.get(), reservedBitsForUnboundedVariables);
    }

    sortVariables();
}

void VariableInformation::registerArrayVariableReplacements(storm::jani::ArrayEliminatorData const& arrayEliminatorData) {
    arrayVariableToElementInformations.clear();
    // Find for each replaced array variable the corresponding references in this variable information
    for (auto const& arrayVariable : arrayEliminatorData.eliminatedArrayVariables) {
        if (!arrayVariable->isTransient()) {
            auto findRes = arrayEliminatorData.replacements.find(arrayVariable->getExpressionVariable());
            STORM_LOG_ASSERT(findRes != arrayEliminatorData.replacements.end(), "No replacement for array variable.");
            auto const& replacements = findRes->second;
            auto const& innerType = arrayVariable->getType().asArrayType().getBaseTypeRecursive();
            if (innerType.isBasicType() && innerType.asBasicType().isBooleanType()) {
                auto replInfo = convertArrayReplacement(replacements, booleanVariables);
                this->arrayVariableToElementInformations.emplace(arrayVariable->getExpressionVariable(), std::move(replInfo));
            } else if ((innerType.isBasicType() && innerType.asBasicType().isIntegerType()) ||
                       (innerType.isBoundedType() && innerType.asBoundedType().isIntegerType())) {
                auto replInfo = convertArrayReplacement(replacements, integerVariables);
                this->arrayVariableToElementInformations.emplace(arrayVariable->getExpressionVariable(), std::move(replInfo));
            } else {
                STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled type of base variable.");
            }
        }
    }
}

BooleanVariableInformation const& VariableInformation::getBooleanArrayVariableReplacement(storm::expressions::Variable const& arrayVariable,
                                                                                          std::vector<uint64_t> const& arrayIndexVector) const {
    return booleanVariables[arrayVariableToElementInformations.at(arrayVariable).getVariableInformationIndex(arrayIndexVector)];
}

IntegerVariableInformation const& VariableInformation::getIntegerArrayVariableReplacement(storm::expressions::Variable const& arrayVariable,
                                                                                          std::vector<uint64_t> const& arrayIndexVector) const {
    return integerVariables[arrayVariableToElementInformations.at(arrayVariable).getVariableInformationIndex(arrayIndexVector)];
}

void VariableInformation::createVariablesForAutomaton(storm::jani::Automaton const& automaton, uint64_t reservedBitsForUnboundedVariables) {
    uint_fast64_t bitwidth = static_cast<uint_fast64_t>(std::ceil(std::log2(automaton.getNumberOfLocations())));
    locationVariables.emplace_back(automaton.getLocationExpressionVariable(), automaton.getNumberOfLocations() - 1, totalBitOffset, bitwidth, true);
    totalBitOffset += bitwidth;

    createVariablesForVariableSet(automaton.getVariables(), reservedBitsForUnboundedVariables, false);
}

void VariableInformation::createVariablesForVariableSet(storm::jani::VariableSet const& variableSet, uint64_t reservedBitsForUnboundedVariables, bool global) {
    for (auto const& variable : variableSet.getBooleanVariables()) {
        if (!variable.isTransient()) {
            booleanVariables.emplace_back(variable.getExpressionVariable(), totalBitOffset, global, true);
            ++totalBitOffset;
        }
    }
    for (auto const& variable : variableSet.getBoundedIntegerVariables()) {
        if (!variable.isTransient()) {
            int64_t lowerBound, upperBound;
            auto const& type = variable.getType().asBoundedType();
            STORM_LOG_ASSERT(type.hasLowerBound() || type.hasUpperBound(), "Bounded integer variable has neither a lower nor an upper bound.");
            if (type.hasLowerBound()) {
                lowerBound = type.getLowerBound().evaluateAsInt();
            }
            if (type.hasUpperBound()) {
                upperBound = type.getUpperBound().evaluateAsInt();
            }
            uint64_t bitwidth =
                getBitWidthLowerUpperBound(type.hasLowerBound(), lowerBound, type.hasUpperBound(), upperBound, reservedBitsForUnboundedVariables);
            integerVariables.emplace_back(variable.getExpressionVariable(), lowerBound, upperBound, totalBitOffset, bitwidth, global, true,
                                          !type.hasLowerBound() || !type.hasUpperBound());
            totalBitOffset += bitwidth;
        }
    }
    for (auto const& variable : variableSet.getUnboundedIntegerVariables()) {
        if (!variable.isTransient()) {
            int64_t lowerBound, upperBound;
            uint64_t bitwidth = getBitWidthLowerUpperBound(false, lowerBound, false, upperBound, reservedBitsForUnboundedVariables);
            integerVariables.emplace_back(variable.getExpressionVariable(), lowerBound, upperBound, totalBitOffset, bitwidth, global, true, true);
            totalBitOffset += reservedBitsForUnboundedVariables;
        }
    }
}

uint_fast64_t VariableInformation::getTotalBitOffset(bool roundTo64Bit) const {
    uint_fast64_t result = totalBitOffset;
    if (roundTo64Bit & ((result & ((1ull << 6) - 1)) != 0)) {
        result = ((result >> 6) + 1) << 6;
    }
    return result;
}

bool VariableInformation::hasOutOfBoundsBit() const {
    return outOfBoundsBit.has_value();
}

uint64_t VariableInformation::getOutOfBoundsBit() const {
    assert(hasOutOfBoundsBit());
    return outOfBoundsBit->bitOffset;
}

void VariableInformation::sortVariables() {
    // Sort the variables so we can make some assumptions when iterating over them (in the next-state generators).
    std::sort(booleanVariables.begin(), booleanVariables.end(),
              [](BooleanVariableInformation const& a, BooleanVariableInformation const& b) { return a.variable < b.variable; });
    std::sort(integerVariables.begin(), integerVariables.end(),
              [](IntegerVariableInformation const& a, IntegerVariableInformation const& b) { return a.variable < b.variable; });
}
}  // namespace generator
}  // namespace storm
