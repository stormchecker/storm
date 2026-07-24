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

#include "storm/settings/ArgumentBase.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/ArgumentUnificationException.h"
#include "storm/settings/ArgumentType.h"

namespace storm {
namespace settings {

template<typename ValueType>
class ArgumentValidator;

/*!
 * This class subclasses the argument base to actually implement the pure virtual functions. This construction
 * is necessary so that it becomes easy to store a vector of arguments later despite variing template types, by
 * keeping a vector of pointers to the base class.
 */
template<typename T>
class Argument : public ArgumentBase {
   public:
    /*!
     * Creates a new argument with the given parameters.
     *
     * @param name The name of the argument.
     * @param description The description of the argument.
     * @param validators A vector of validators that are to be executed upon assigning a value to this argument.
     * @param isOptional A flag indicating whether the argument is optional.
     */
    Argument(std::string const& name, std::string const& description, std::vector<std::shared_ptr<ArgumentValidator<T>>> const& validators);

    /*!
     * Creates a new argument with the given parameters.
     *
     * @param name The name of the argument.
     * @param description The description of the argument.
     * @param validators A vector of validators that are to be executed upon assigning a value to this argument.
     * @param isOptional A flag indicating whether the argument is optional.
     */
    Argument(std::string const& name, std::string const& description, std::vector<std::shared_ptr<ArgumentValidator<T>>> const& validators, bool isOptional,
             T defaultValue);

    virtual bool getIsOptional() const override;

    bool setFromStringValue(std::string const& fromStringValue) override;

    bool setFromTypeValue(T const& newValue, bool hasBeenSet = true);

    virtual ArgumentType getType() const override;

    /*!
     * Checks whether the given argument is compatible with the current one. If not, an exception is thrown.
     *
     * @param other The other argument with which to check compatibility.
     * @return True iff the given argument is compatible with the current one.
     */
    template<typename S>
    bool isCompatibleWith(Argument<S> const& other) const {
        STORM_LOG_THROW(this->getType() == other.getType(), storm::exceptions::ArgumentUnificationException,
                        "Unable to unify the arguments " << this->getName() << " and " << other.getName() << ", because they have different types.");
        STORM_LOG_THROW(this->getIsOptional() == other.getIsOptional(), storm::exceptions::ArgumentUnificationException,
                        "Unable to unify the arguments '" << this->getName() << "' and '" << other.getName()
                                                          << "', because one of them is optional and the other one is not.");
        STORM_LOG_THROW(this->getHasDefaultValue() == other.getHasDefaultValue(), storm::exceptions::ArgumentUnificationException,
                        "Unable to unify the arguments " << this->getName() << " and " << other.getName()
                                                         << ", because one of them has a default value and the other one does not.");
        return true;
    }

    /*!
     * Retrieves the value of the argument if any has been set. Otherwise, an exception is thrown.
     *
     * @return The value of the argument.
     */
    T const& getArgumentValue() const;

    virtual bool getHasDefaultValue() const override;

    void setFromDefaultValue() override;

    virtual bool wasSetFromDefaultValue() const override;

    virtual std::string getValueAsString() const override;

    virtual int_fast64_t getValueAsInteger() const override;

    virtual uint_fast64_t getValueAsUnsignedInteger() const override;

    virtual double getValueAsDouble() const override;

    virtual bool getValueAsBoolean() const override;

    virtual void printToStream(std::ostream& out) const override;

   private:
    // The value of the argument (in case it has been set).
    T argumentValue;

    // The type of the argument.
    ArgumentType argumentType;

    // The validation functions that were registered for this argument.
    std::vector<std::shared_ptr<ArgumentValidator<T>>> validators;

    // A flag indicating whether this argument is optional.
    bool isOptional;

    // The default value for the argument (in case one has been provided).
    T defaultValue;

    // A flag indicating whether a default value has been provided.
    bool hasDefaultValue;

    // A flag indicating whether the argument was set from the default value.
    bool wasSetFromDefaultValueFlag;

    /*!
     * Sets the default value of the argument to the provided value.
     *
     * @param newDefault The new default value of the argument.
     */
    void setDefaultValue(T const& newDefault);

    /*!
     * Applies all validation functions to the given value and therefore checks the validity of a value for this
     * argument.
     *
     * @param value The value that is to be validated.
     * @return True iff the value passed all validation functions successfully.
     */
    bool validate(T const& value) const;
};
}  // namespace settings
}  // namespace storm
