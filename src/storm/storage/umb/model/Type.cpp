#include "storm/storage/umb/model/Type.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

namespace storm::umb {

bool isBooleanType(Type const type) {
    return type == Type::Bool;
}

bool isDiscreteNumericType(Type const type) {
    using enum Type;
    switch (type) {
        case Int:
        case IntInterval:
        case Uint:
        case UintInterval:
            return true;
        default:
            return false;
    }
}

bool isContinuousNumericType(Type const type) {
    using enum Type;
    switch (type) {
        case Double:
        case DoubleInterval:
        case Rational:
        case RationalInterval:
            return true;
        default:
            return false;
    }
}

bool isNumericType(Type const type) {
    return isDiscreteNumericType(type) || isContinuousNumericType(type);
}

bool isIntervalType(Type const type) {
    using enum Type;
    switch (type) {
        case IntInterval:
        case UintInterval:
        case DoubleInterval:
        case RationalInterval:
            return true;
        default:
            return false;
    }
}

bool isStringType(Type const type) {
    return type == Type::String;
}

uint64_t defaultBitSize(Type const type) {
    using enum Type;
    switch (type) {
        case Bool:
            return 1;
        case Int:
        case Uint:
        case Double:
        case String:
            return 64;
        case IntInterval:
        case UintInterval:
        case DoubleInterval:
        case Rational:
            return 128;
        case RationalInterval:
            return 256;
        default:
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type " << toString(type) << ".");
    }
    return std::numeric_limits<uint64_t>::max();
}

std::string toString(Type const type) {
    storm::SerializedEnum<TypeDeclaration> e(type);
    return std::string(e.toString());
}

uint64_t SizedType::bitSize() const {
    return size.value_or(defaultBitSize(type));
}

std::string SizedType::toString() const {
    return std::string(type.toString()) + "[" + std::to_string(bitSize()) + "]";
}

}  // namespace storm::umb