#pragma once

#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBase.h"
#include "storm/settings/ArgumentType.h"
#include "storm/settings/ArgumentValidators.h"

#include "storm/exceptions/IllegalArgumentTypeException.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {

/*!
 * This class serves as an API for creating arguments.
 */
class ArgumentBuilder {
   public:
    /*!
     * Creates a string argument with the given parameters.
     *
     * @param name The name of the argument.
     * @param description The description of the argument.
     * @return The builder of the argument.
     */
    static ArgumentBuilder createStringArgument(std::string const& name, std::string const& description) {
        ArgumentBuilder ab(ArgumentType::String, name, description);
        return ab;
    }

    /*!
     * Creates an integer argument with the given parameters.
     *
     * @param name The name of the argument.
     * @param description The description of the argument.
     * @return The builder of the argument.
     */
    static ArgumentBuilder createIntegerArgument(std::string const& name, std::string const& description) {
        ArgumentBuilder ab(ArgumentType::Integer, name, description);
        return ab;
    }

    /*!
     * Creates an unsigned integer argument with the given parameters.
     *
     * @param name The name of the argument.
     * @param description The description of the argument.
     * @return The builder of the argument.
     */
    static ArgumentBuilder createUnsignedIntegerArgument(std::string const& name, std::string const& description) {
        ArgumentBuilder ab(ArgumentType::UnsignedInteger, name, description);
        return ab;
    }

    /*!
     * Creates a double argument with the given parameters.
     *
     * @param name The name of the argument.
     * @param description The description of the argument.
     * @return The builder of the argument.
     */
    static ArgumentBuilder createDoubleArgument(std::string const& name, std::string const& description) {
        ArgumentBuilder ab(ArgumentType::Double, name, description);
        return ab;
    }

    /*!
     * Creates a boolean argument with the given parameters.
     *
     * @param name The name of the argument.
     * @param description The description of the argument.
     * @return The builder of the argument.
     */
    static ArgumentBuilder createBooleanArgument(std::string const& name, std::string const& description) {
        ArgumentBuilder ab(ArgumentType::Boolean, name, description);
        return ab;
    }

    /*!
     * Make the argument optional.
     *
     * @return A reference to the argument builder.
     */
    ArgumentBuilder& makeOptional() {
        this->isOptional = true;
        STORM_LOG_THROW(this->hasDefaultValue, storm::exceptions::IllegalFunctionCallException,
                        "Unable to make argument '" << this->name << "' optional without default value.");
        return *this;
    }

#define PPCAT_NX(A, B) A##B
#define PPCAT(A, B) PPCAT_NX(A, B)
#define MACROaddValidator(funcName, funcType)                                                                       \
    ArgumentBuilder& PPCAT(addValidator, funcName)(std::shared_ptr<ArgumentValidator<funcType>> && validator) {     \
        STORM_LOG_THROW(this->type == ArgumentType::funcName, storm::exceptions::IllegalFunctionCallException,      \
                        "Illegal validation function for argument, because it takes arguments of different type."); \
        (PPCAT(this->validators_, funcName)).emplace_back(validator);                                               \
        return *this;                                                                                               \
    }

    // Add the methods to add validation functions.
    MACROaddValidator(String, std::string) MACROaddValidator(Integer, int_fast64_t) MACROaddValidator(UnsignedInteger, uint_fast64_t)
        MACROaddValidator(Double, double) MACROaddValidator(Boolean, bool)
#define MACROsetDefaultValue(funcName, funcType)                                                                     \
    ArgumentBuilder& PPCAT(setDefaultValue, funcName)(funcType const& defaultValue) {                                \
        STORM_LOG_THROW(this->type == ArgumentType::funcName, storm::exceptions::IllegalFunctionCallException,       \
                        "Illegal default value for argument" << this->name << ", because it is of different type."); \
        PPCAT(this->defaultValue_, funcName) = defaultValue;                                                         \
        this->hasDefaultValue = true;                                                                                \
        return *this;                                                                                                \
    }

        // Add the methods to set a default value.
        MACROsetDefaultValue(String, std::string) MACROsetDefaultValue(Integer, int_fast64_t) MACROsetDefaultValue(UnsignedInteger, uint_fast64_t)
            MACROsetDefaultValue(Double, double) MACROsetDefaultValue(Boolean, bool)

        /*!
         * Builds an argument based on the information that was added to the builder object.
         *
         * @return The resulting argument.
         */
        std::shared_ptr<ArgumentBase> build() {
        STORM_LOG_THROW(!this->hasBeenBuilt, storm::exceptions::IllegalFunctionCallException,
                        "Cannot rebuild argument with builder that was already used to build an argument.");
        this->hasBeenBuilt = true;
        switch (this->type) {
            case ArgumentType::String: {
                if (this->hasDefaultValue) {
                    return std::shared_ptr<ArgumentBase>(
                        new Argument<std::string>(this->name, this->description, this->validators_String, this->isOptional, this->defaultValue_String));
                } else {
                    return std::shared_ptr<ArgumentBase>(new Argument<std::string>(this->name, this->description, this->validators_String));
                }
                break;
            }
            case ArgumentType::Integer:
                if (this->hasDefaultValue) {
                    return std::shared_ptr<ArgumentBase>(
                        new Argument<int_fast64_t>(this->name, this->description, this->validators_Integer, this->isOptional, this->defaultValue_Integer));
                } else {
                    return std::shared_ptr<ArgumentBase>(new Argument<int_fast64_t>(this->name, this->description, this->validators_Integer));
                }
                break;
            case ArgumentType::UnsignedInteger:
                if (this->hasDefaultValue) {
                    return std::shared_ptr<ArgumentBase>(new Argument<uint_fast64_t>(this->name, this->description, this->validators_UnsignedInteger,
                                                                                     this->isOptional, this->defaultValue_UnsignedInteger));
                } else {
                    return std::shared_ptr<ArgumentBase>(new Argument<uint_fast64_t>(this->name, this->description, this->validators_UnsignedInteger));
                }
                break;
            case ArgumentType::Double:
                if (this->hasDefaultValue) {
                    return std::shared_ptr<ArgumentBase>(
                        new Argument<double>(this->name, this->description, this->validators_Double, this->isOptional, this->defaultValue_Double));
                } else {
                    return std::shared_ptr<ArgumentBase>(new Argument<double>(this->name, this->description, this->validators_Double));
                }
                break;
            case ArgumentType::Boolean:
                if (this->hasDefaultValue) {
                    return std::shared_ptr<ArgumentBase>(
                        new Argument<bool>(this->name, this->description, this->validators_Boolean, this->isOptional, this->defaultValue_Boolean));
                } else {
                    return std::shared_ptr<ArgumentBase>(new Argument<bool>(this->name, this->description, this->validators_Boolean));
                }
                break;
        }
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentTypeException, "Argument has illegal type.");
    }

   private:
    /*!
     * Creates an argument builder for an argument with the given parameters.
     *
     * @param type The type of the argument.
     * @param name The name of the argument.
     * @param description The description of the argument.
     */
    ArgumentBuilder(ArgumentType type, std::string const& name, std::string const& description)
        : hasBeenBuilt(false),
          type(type),
          name(name),
          description(description),
          isOptional(false),
          hasDefaultValue(false),
          defaultValue_String(),
          defaultValue_Integer(),
          defaultValue_UnsignedInteger(),
          defaultValue_Double(),
          defaultValue_Boolean() {
        // Intentionally left empty.
    }

    // A flag that stores whether an argument has been built using this builder.
    bool hasBeenBuilt;

    // The type of the argument.
    ArgumentType type;

    // The name of te argument.
    std::string name;

    // The description of the argument.
    std::string description;

    // A flag indicating whether the argument is optional.
    bool isOptional;

    // A flag that stores whether the argument has a default value.
    bool hasDefaultValue;

    // The default value of the argument separated by its type.
    std::string defaultValue_String;
    int_fast64_t defaultValue_Integer;
    uint_fast64_t defaultValue_UnsignedInteger;
    double defaultValue_Double;
    bool defaultValue_Boolean;

    // The validation functions separated by their type.
    std::vector<std::shared_ptr<ArgumentValidator<std::string>>> validators_String;
    std::vector<std::shared_ptr<ArgumentValidator<int_fast64_t>>> validators_Integer;
    std::vector<std::shared_ptr<ArgumentValidator<uint_fast64_t>>> validators_UnsignedInteger;
    std::vector<std::shared_ptr<ArgumentValidator<double>>> validators_Double;
    std::vector<std::shared_ptr<ArgumentValidator<bool>>> validators_Boolean;
};
}  // namespace settings
}  // namespace storm
