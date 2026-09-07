#include "Argument.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/settings/ArgumentTypeInferationHelper.h"
#include "storm/settings/ArgumentValidators.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {

template<typename T>
Argument<T>::Argument(std::string const& name, std::string const& description, std::vector<std::shared_ptr<ArgumentValidator<T>>> const& validators)
    : ArgumentBase(name, description),
      argumentValue(),
      argumentType(inferToEnumType<T>()),
      validators(validators),
      isOptional(false),
      defaultValue(),
      hasDefaultValue(false),
      wasSetFromDefaultValueFlag(false) {
    // Intentionally left empty.
}

template<typename T>
Argument<T>::Argument(std::string const& name, std::string const& description, std::vector<std::shared_ptr<ArgumentValidator<T>>> const& validators,
                      bool isOptional, T defaultValue)
    : ArgumentBase(name, description),
      argumentValue(),
      argumentType(inferToEnumType<T>()),
      validators(validators),
      isOptional(isOptional),
      defaultValue(),
      hasDefaultValue(true),
      wasSetFromDefaultValueFlag(false) {
    this->setDefaultValue(defaultValue);
}

template<typename T>
bool Argument<T>::getIsOptional() const {
    return this->isOptional;
}

template<typename T>
bool Argument<T>::setFromStringValue(std::string const& fromStringValue) {
    bool conversionOk = false;
    T newValue = ArgumentBase::convertFromString<T>(fromStringValue, conversionOk);
    if (!conversionOk) {
        return false;
    }
    return this->setFromTypeValue(newValue);
}

template<typename T>
bool Argument<T>::setFromTypeValue(T const& newValue, bool hasBeenSet) {
    if (!this->validate(newValue)) {
        return false;
    }
    this->argumentValue = newValue;
    this->hasBeenSet = hasBeenSet;
    return true;
}

template<typename T>
ArgumentType Argument<T>::getType() const {
    return this->argumentType;
}

template<typename T>
T const& Argument<T>::getArgumentValue() const {
    STORM_LOG_THROW(this->getHasBeenSet() || this->getHasDefaultValue(), storm::exceptions::IllegalFunctionCallException,
                    "Unable to retrieve value of argument '" << this->getName() << "', because it was neither set nor specifies a default value.");
    if (this->getHasBeenSet()) {
        return this->argumentValue;
    } else {
        return this->defaultValue;
    }
}

template<typename T>
bool Argument<T>::getHasDefaultValue() const {
    return this->hasDefaultValue;
}

template<typename T>
void Argument<T>::setFromDefaultValue() {
    STORM_LOG_THROW(this->hasDefaultValue, storm::exceptions::IllegalFunctionCallException,
                    "Unable to set value from default value, because the argument " << name << " has none.");
    bool result = this->setFromTypeValue(this->defaultValue, false);
    STORM_LOG_THROW(result, storm::exceptions::IllegalArgumentValueException,
                    "Unable to assign default value to argument " << name << ", because it was rejected.");
    this->wasSetFromDefaultValueFlag = true;
}

template<typename T>
bool Argument<T>::wasSetFromDefaultValue() const {
    return wasSetFromDefaultValueFlag;
}

template<typename T>
std::string Argument<T>::getValueAsString() const {
    switch (this->argumentType) {
        case ArgumentType::String:
            return inferToString(ArgumentType::String, this->getArgumentValue());
        case ArgumentType::Boolean: {
            bool iValue = inferToBoolean(ArgumentType::Boolean, this->getArgumentValue());
            if (iValue) {
                return "true";
            } else {
                return "false";
            }
        }
        default:
            return ArgumentBase::convertToString(this->argumentValue);
    }
}

template<typename T>
int_fast64_t Argument<T>::getValueAsInteger() const {
    switch (this->argumentType) {
        case ArgumentType::Integer:
            return inferToInteger(ArgumentType::Integer, this->getArgumentValue());
        default:
            STORM_LOG_THROW(false, storm::exceptions::IllegalFunctionCallException, "Unable to retrieve argument value for " << name << " as integer.");
            break;
    }
}

template<typename T>
uint_fast64_t Argument<T>::getValueAsUnsignedInteger() const {
    switch (this->argumentType) {
        case ArgumentType::UnsignedInteger:
            return inferToUnsignedInteger(ArgumentType::UnsignedInteger, this->getArgumentValue());
        default:
            STORM_LOG_THROW(false, storm::exceptions::IllegalFunctionCallException,
                            "Unable to retrieve argument value for " << name << " as unsigned integer.");
            break;
    }
}

template<typename T>
double Argument<T>::getValueAsDouble() const {
    switch (this->argumentType) {
        case ArgumentType::Double:
            return inferToDouble(ArgumentType::Double, this->getArgumentValue());
        default:
            STORM_LOG_THROW(false, storm::exceptions::IllegalFunctionCallException, "Unable to retrieve argument value for " << name << " as double.");
            break;
    }
}

template<typename T>
bool Argument<T>::getValueAsBoolean() const {
    switch (this->argumentType) {
        case ArgumentType::Boolean:
            return inferToBoolean(ArgumentType::Boolean, this->getArgumentValue());
        default:
            STORM_LOG_THROW(false, storm::exceptions::IllegalFunctionCallException, "Unable to retrieve argument value for " << name << " as Boolean.");
            break;
    }
}

template<typename T>
void Argument<T>::setDefaultValue(T const& newDefault) {
    STORM_LOG_THROW(this->validate(newDefault), storm::exceptions::IllegalArgumentValueException,
                    "The default value for the argument did not pass all validation functions.");
    this->defaultValue = newDefault;
    this->hasDefaultValue = true;
}

template<typename T>
bool Argument<T>::validate(T const& value) const {
    bool result = true;
    for (auto const& validator : validators) {
        result &= validator->isValid(value);
    }
    return result;
}

template<typename T>
void printValue(std::ostream& out, T const& value) {
    out << value;
}

template<>
void printValue(std::ostream& out, std::string const& value) {
    if (value.empty()) {
        out << "empty";
    } else {
        out << value;
    }
}

template<typename T>
void Argument<T>::printToStream(std::ostream& out) const {
    out << std::setw(0) << std::left << "<" << this->getName() << ">";
    if (!this->validators.empty() || this->hasDefaultValue) {
        out << " (";
        bool previousEntry = false;
        if (this->getIsOptional()) {
            out << "optional";
            previousEntry = true;
        }
        if (!this->validators.empty()) {
            if (previousEntry) {
                out << "; ";
            }
            for (uint64_t i = 0; i < this->validators.size(); ++i) {
                out << this->validators[i]->toString();
                if (i + 1 < this->validators.size()) {
                    out << ", ";
                }
            }
            previousEntry = true;
        }

        if (this->hasDefaultValue) {
            if (previousEntry) {
                out << "; ";
            }
            out << "default: ";
            printValue(out, defaultValue);
        }
        out << ")";
    }

    out << ": " << this->getDescription();
}

template class Argument<std::string>;
template class Argument<int_fast64_t>;
template class Argument<uint_fast64_t>;
template class Argument<double>;
template class Argument<bool>;
}  // namespace settings
}  // namespace storm
