#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>

namespace storm {
namespace expressions {
class ExpressionManager;
class BaseType;

class Type {
   public:
    friend bool operator<(storm::expressions::Type const& type1, storm::expressions::Type const& type2);

    Type();

    /*!
     * Constructs a new type of the given manager with the given encapsulated type.
     *
     * @param manager The manager responsible for this type.
     * @param innerType The encapsulated type.
     */
    Type(std::shared_ptr<ExpressionManager const> const& manager, std::shared_ptr<BaseType> const& innerType);

    /*!
     * Checks whether two types are the same.
     *
     * @other The type to compare with.
     * @return True iff the types are the same.
     */
    bool operator==(Type const& other) const;

    /*!
     * Retrieves the bit mask of the type.
     *
     * @return The bit mask of the type.
     */
    uint64_t getMask() const;

    /*!
     * Retrieves a string representation of the type.
     *
     * @return A string representation of the type.
     */
    std::string getStringRepresentation() const;

    /*!
     * Checks whether this type is a boolean type.
     *
     * @return True iff the type is a boolean one.
     */
    bool isBooleanType() const;

    /*!
     * Checks whether this type is an integral type.
     *
     * @return True iff the type is a integral one.
     */
    bool isIntegerType() const;

    /*!
     * Checks whether this type is a bitvector type.
     *
     * @return True iff the type is a bitvector one.
     */
    bool isBitVectorType() const;

    /*!
     * Checks whether this type is a rational type.
     *
     * @return True iff the type is a rational one.
     */
    bool isRationalType() const;

    /*!
     * Checks whether this type is a numerical type.
     *
     * @return True iff the type is a numerical one.
     */
    bool isNumericalType() const;

    /*!
     * Checks whether this type is an array type.
     *
     * @return True iff the type is an array.
     */
    bool isArrayType() const;

    /*!
     * Checks whether this type is a transcendental number type.
     *
     * @return True iff the type is a transcendental number.
     */
    bool isTranscendentalNumberType() const;

    /*!
     * Checks whether this type is a string type.
     *
     * @return True iff the type is string.
     */
    bool isStringType() const;

    /*!
     * Retrieves the bit width of the type, provided that it is a bitvector type.
     *
     * @return The bit width of the bitvector type.
     */
    std::size_t getWidth() const;

    /*!
     * Retrieves the element type of the type, provided that it is an Array type.
     *
     * @return The bit width of the bitvector type.
     */
    Type getElementType() const;

    /*!
     * Retrieves the manager of the type.
     *
     * @return The manager of the type.
     */
    storm::expressions::ExpressionManager const& getManager() const;

    // Functions that, given the input types, produce the output type of the corresponding function application.
    Type plusMinusTimes(Type const& other) const;
    Type minus() const;
    Type divide(Type const& other) const;
    Type modulo(Type const& other) const;
    Type logarithm(Type const& other) const;
    Type power(Type const& other, bool allowIntegerType = false) const;
    Type trigonometric() const;
    Type logicalConnective(Type const& other) const;
    Type logicalConnective() const;
    Type numericalComparison(Type const& other) const;
    Type ite(Type const& thenType, Type const& elseType) const;
    Type floorCeil() const;
    Type minimumMaximum(Type const& other) const;

   private:
    // The manager responsible for the type.
    std::shared_ptr<ExpressionManager const> manager;

    // The encapsulated type.
    std::shared_ptr<BaseType> innerType;
};

std::ostream& operator<<(std::ostream& stream, Type const& type);

bool operator<(storm::expressions::Type const& type1, storm::expressions::Type const& type2);

class BaseType {
   public:
    BaseType();
    virtual ~BaseType() = default;

    /*!
     * Retrieves the mask that is associated with this type.
     *
     * @return The mask associated with this type.
     */
    virtual uint64_t getMask() const = 0;

    /*!
     * Checks whether two types are actually the same.
     *
     * @param other The type to compare with.
     * @return True iff the types are the same.
     */
    virtual bool operator==(BaseType const& other) const;

    /*!
     * Returns a string representation of the type.
     *
     * @return A string representation of the type.
     */
    virtual std::string getStringRepresentation() const = 0;

    virtual bool isErrorType() const;
    virtual bool isBooleanType() const;
    virtual bool isIntegerType() const;
    virtual bool isBitVectorType() const;
    virtual bool isRationalType() const;
    virtual bool isArrayType() const;
    virtual bool isTranscendentalNumberType() const;
    virtual bool isStringType() const;
};

class BooleanType : public BaseType {
   public:
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isBooleanType() const override;

   private:
    static const uint64_t mask = (1ull << 56);
};

class IntegerType : public BaseType {
   public:
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isIntegerType() const override;

   private:
    static const uint64_t mask = (1ull << 58);
};

class BitVectorType : public BaseType {
   public:
    /*!
     * Creates a new bounded bitvector type with the given bit width.
     *
     * @param width The bit width of the type.
     */
    BitVectorType(std::size_t width);

    /*!
     * Retrieves the bit width of the bounded type.
     *
     * @return The bit width of the bounded type.
     */
    std::size_t getWidth() const;

    virtual bool operator==(BaseType const& other) const override;
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isIntegerType() const override;
    virtual bool isBitVectorType() const override;

   private:
    static const uint64_t mask = (1ull << 57);

    // The bit width of the type.
    std::size_t width;
};

class RationalType : public BaseType {
   public:
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isRationalType() const override;

   private:
    static const uint64_t mask = (1ull << 59);
};

class ArrayType : public BaseType {
   public:
    ArrayType(Type elementType);

    Type getElementType() const;

    virtual bool operator==(BaseType const& other) const override;
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isArrayType() const override;

   private:
    static const uint64_t mask = (1ull << 55);

    // The type of the array elements (can again be of type array).
    Type elementType;
};

class TranscendentalNumberType : public BaseType {
   public:
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isTranscendentalNumberType() const override;

   private:
    static const uint64_t mask = (1ull << 60);
};

class StringType : public BaseType {
   public:
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isStringType() const override;

   private:
    static const uint64_t mask = (1ull << 61);
};

class ErrorType : public BaseType {
   public:
    virtual uint64_t getMask() const override;
    virtual std::string getStringRepresentation() const override;
    virtual bool isErrorType() const override;

   private:
    static const uint64_t mask = 0;
};

bool operator<(BaseType const& first, BaseType const& second);
}  // namespace expressions
}  // namespace storm

namespace std {
// Provide a hashing operator, so we can put types in unordered collections.
template<>
struct hash<storm::expressions::Type> {
    std::size_t operator()(storm::expressions::Type const& type) const {
        return std::hash<uint64_t>()(type.getMask());
    }
};
}  // namespace std
