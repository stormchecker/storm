#pragma once

#include "storm/storage/expressions/BaseExpression.h"
#include "storm/storage/expressions/RationalLiteralExpression.h"

namespace storm {
namespace expressions {
class TranscendentalNumberLiteralExpression : public BaseExpression {
   public:
    /*!
     * @brief Enum class to represent the supported TranscendentalNumbers
     */
    enum class TranscendentalNumber {
        PI,  // π
        E    // EulerNumber
    };
    /*!
     * Creates a unary expression with the given return type and operand.
     *
     * @param manager The manager responsible for this expression.
     * @param value The constant value assigned to the variable
     */
    TranscendentalNumberLiteralExpression(ExpressionManager const& manager, TranscendentalNumber const& value);

    // Instantiate constructors and assignments with their default implementations.
    TranscendentalNumberLiteralExpression(TranscendentalNumberLiteralExpression const& other) = default;
    TranscendentalNumberLiteralExpression& operator=(TranscendentalNumberLiteralExpression const& other) = delete;
    TranscendentalNumberLiteralExpression(TranscendentalNumberLiteralExpression&&) = default;
    TranscendentalNumberLiteralExpression& operator=(TranscendentalNumberLiteralExpression&&) = delete;
    virtual ~TranscendentalNumberLiteralExpression() = default;

    // Override base class methods.
    virtual double evaluateAsDouble(Valuation const* valuation = nullptr) const override;
    virtual bool isLiteral() const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;

    /*!
     * @brief Getter for the constant value stored by the object
     * @return A reference to the stored TranscendentalNumber
     */
    TranscendentalNumber const& getTranscendentalNumber() const;

    /*!
     * @brief Get the Transcendental number as string, like in the Jani file (single character)
     * @return A string representing the transcendental number
     */
    std::string asString() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The operand of the unary expression.
    const TranscendentalNumber value;
};
}  // namespace expressions
}  // namespace storm
