#include "storm/storage/expressions/Type.h"

#include <cmath>
#include <sstream>

#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {

BaseType::BaseType() {
    // Intentionally left empty.
}

bool BaseType::operator==(BaseType const& other) const {
    return this->getMask() == other.getMask();
}

bool BaseType::isErrorType() const {
    return false;
}

bool BaseType::isBooleanType() const {
    return false;
}

bool BooleanType::isBooleanType() const {
    return true;
}

bool BaseType::isIntegerType() const {
    return false;
}

bool IntegerType::isIntegerType() const {
    return true;
}

bool BitVectorType::isIntegerType() const {
    return true;
}

bool BaseType::isBitVectorType() const {
    return false;
}

bool BitVectorType::isBitVectorType() const {
    return true;
}

bool BaseType::isRationalType() const {
    return false;
}

bool RationalType::isRationalType() const {
    return true;
}

bool BaseType::isArrayType() const {
    return false;
}

bool ArrayType::isArrayType() const {
    return true;
}

bool BaseType::isTranscendentalNumberType() const {
    return false;
}

bool TranscendentalNumberType::isTranscendentalNumberType() const {
    return true;
}

bool BaseType::isStringType() const {
    return false;
}

bool StringType::isStringType() const {
    return true;
}

uint64_t BooleanType::getMask() const {
    return BooleanType::mask;
}

std::string BooleanType::getStringRepresentation() const {
    return "bool";
}

uint64_t IntegerType::getMask() const {
    return IntegerType::mask;
}

std::string IntegerType::getStringRepresentation() const {
    return "int";
}

BitVectorType::BitVectorType(std::size_t width) : width(width) {
    // Intentionally left empty.
}

uint64_t BitVectorType::getMask() const {
    return BitVectorType::mask;
}

std::string BitVectorType::getStringRepresentation() const {
    return "bv[" + std::to_string(width) + "]";
}

std::size_t BitVectorType::getWidth() const {
    return width;
}

bool BitVectorType::operator==(BaseType const& other) const {
    return BaseType::operator==(other) && this->width == static_cast<BitVectorType const&>(other).getWidth();
}

uint64_t RationalType::getMask() const {
    return RationalType::mask;
}

std::string RationalType::getStringRepresentation() const {
    return "rational";
}

ArrayType::ArrayType(Type elementType) : elementType(elementType) {
    // Intentionally left empty
}

Type ArrayType::getElementType() const {
    return elementType;
}

bool ArrayType::operator==(BaseType const& other) const {
    return BaseType::operator==(other) && this->elementType == static_cast<ArrayType const&>(other).getElementType();
}

uint64_t ArrayType::getMask() const {
    return ArrayType::mask;
}

std::string ArrayType::getStringRepresentation() const {
    return "array[" + elementType.getStringRepresentation() + "]";
}

uint64_t TranscendentalNumberType::getMask() const {
    return TranscendentalNumberType::mask;
}

std::string TranscendentalNumberType::getStringRepresentation() const {
    return "transcendental";
}

uint64_t StringType::getMask() const {
    return StringType::mask;
}

std::string StringType::getStringRepresentation() const {
    return "string";
}

bool operator<(BaseType const& first, BaseType const& second) {
    if (first.getMask() < second.getMask()) {
        return true;
    }
    if (first.isBitVectorType() && second.isBitVectorType()) {
        return static_cast<BitVectorType const&>(first).getWidth() < static_cast<BitVectorType const&>(second).getWidth();
    }
    if (first.isArrayType() && second.isArrayType()) {
        return static_cast<ArrayType const&>(first).getElementType() < static_cast<ArrayType const&>(second).getElementType();
    }
    return false;
}

Type::Type() : manager(nullptr), innerType(nullptr) {
    // Intentionally left empty.
}

Type::Type(std::shared_ptr<ExpressionManager const> const& manager, std::shared_ptr<BaseType> const& innerType) : manager(manager), innerType(innerType) {
    // Intentionally left empty.
}

bool Type::operator==(Type const& other) const {
    return *this->innerType == *other.innerType;
}

uint64_t Type::getMask() const {
    return this->innerType->getMask();
}

bool Type::isBooleanType() const {
    return this->innerType->isBooleanType();
}

bool Type::isIntegerType() const {
    return this->innerType->isIntegerType();
}

bool Type::isBitVectorType() const {
    return this->innerType->isBitVectorType();
}

bool Type::isNumericalType() const {
    return this->isIntegerType() || this->isRationalType() || this->isTranscendentalNumberType();
}

bool Type::isArrayType() const {
    return this->innerType->isArrayType();
}

bool Type::isTranscendentalNumberType() const {
    return this->innerType->isTranscendentalNumberType();
}

bool Type::isStringType() const {
    return this->innerType->isStringType();
}

std::string Type::getStringRepresentation() const {
    return this->innerType->getStringRepresentation();
}

std::size_t Type::getWidth() const {
    return static_cast<BitVectorType const&>(*this->innerType).getWidth();
}

Type Type::getElementType() const {
    return static_cast<ArrayType const&>(*this->innerType).getElementType();
}

bool Type::isRationalType() const {
    return this->innerType->isRationalType();
}

storm::expressions::ExpressionManager const& Type::getManager() const {
    return *manager;
}

Type Type::plusMinusTimes(Type const& other) const {
    STORM_LOG_THROW(this->isNumericalType() && other.isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operands.");
    return std::max(*this, other);
}

Type Type::minus() const {
    STORM_LOG_THROW(this->isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operand.");
    return *this;
}

Type Type::divide(Type const& other) const {
    STORM_LOG_THROW(this->isNumericalType() && other.isNumericalType(), storm::exceptions::InvalidTypeException,
                    "Operator division requires numerical operands, got " << this->isNumericalType() << " and " << other.isNumericalType() << ".");
    STORM_LOG_THROW(!this->isBitVectorType() && !other.isBitVectorType(), storm::exceptions::InvalidTypeException, "Operator requires non-bitvector operands.");
    return std::max(*this, other);
}

Type Type::modulo(Type const& other) const {
    STORM_LOG_THROW(this->isNumericalType() && other.isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operands.");
    return std::max(*this, other);
}

Type Type::logarithm(Type const& other) const {
    STORM_LOG_THROW(this->isNumericalType() && other.isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operands.");
    return this->getManager().getRationalType();
}

Type Type::power(Type const& other, bool allowIntegerType) const {
    STORM_LOG_THROW(this->isNumericalType() && other.isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operands.");
    STORM_LOG_THROW(!this->isBitVectorType() && !other.isBitVectorType(), storm::exceptions::InvalidTypeException, "Operator requires non-bitvector operands.");
    if (allowIntegerType) {
        return std::max(*this, other);
    } else {
        return this->getManager().getRationalType();
    }
}

Type Type::trigonometric() const {
    STORM_LOG_THROW(this->isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operand.");
    return this->getManager().getRationalType();
}

Type Type::logicalConnective(Type const& other) const {
    STORM_LOG_THROW(this->isBooleanType() && other.isBooleanType(), storm::exceptions::InvalidTypeException, "Operator requires boolean operands.");
    return *this;
}

Type Type::logicalConnective() const {
    STORM_LOG_THROW(this->isBooleanType(), storm::exceptions::InvalidTypeException, "Operator requires boolean operand.");
    return *this;
}

Type Type::numericalComparison(Type const& other) const {
    STORM_LOG_THROW(this->isNumericalType() && other.isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operands.");
    return this->getManager().getBooleanType();
}

Type Type::ite(Type const& thenType, Type const& elseType) const {
    STORM_LOG_THROW(this->isBooleanType(), storm::exceptions::InvalidTypeException, "Operator requires boolean condition.");
    if (thenType == elseType) {
        return thenType;
    } else {
        STORM_LOG_THROW(thenType.isNumericalType() && elseType.isNumericalType(), storm::exceptions::InvalidTypeException,
                        "Operator 'ite' requires proper types.");
        return std::max(thenType, elseType);
    }
}

Type Type::floorCeil() const {
    STORM_LOG_THROW(this->isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires rational operand.");
    return this->getManager().getIntegerType();
}

Type Type::minimumMaximum(Type const& other) const {
    STORM_LOG_THROW(this->isNumericalType() && other.isNumericalType(), storm::exceptions::InvalidTypeException, "Operator requires numerical operands.");
    return std::max(*this, other);
}

bool operator<(storm::expressions::Type const& type1, storm::expressions::Type const& type2) {
    return *type1.innerType < *type2.innerType;
}

std::ostream& operator<<(std::ostream& stream, Type const& type) {
    stream << type.getStringRepresentation();
    return stream;
}
}  // namespace expressions
}  // namespace storm
