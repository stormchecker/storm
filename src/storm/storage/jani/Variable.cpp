#include "storm/storage/jani/Variable.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"

namespace storm {
namespace jani {

Variable::Variable(std::string const& name, JaniType const& type, storm::expressions::Variable const& variable, storm::expressions::Expression const& init,
                   bool transient)
    : name(name), type(type.clone()), variable(variable), init(init), transient(transient) {
    // Intentionally left empty
}

Variable::Variable(std::string const& name, JaniType const& type, storm::expressions::Variable const& variable)
    : name(name), type(type.clone()), variable(variable), init(), transient(false) {
    // Intentionally left empty
}

Variable::~Variable() {
    // Intentionally left empty.
}

std::unique_ptr<Variable> Variable::clone() const {
    return std::make_unique<Variable>(name, *type, variable, init, transient);
}

storm::expressions::Variable const& Variable::getExpressionVariable() const {
    return variable;
}

void Variable::setExpressionVariable(storm::expressions::Variable const& newVariable) {
    variable = newVariable;
}

std::string const& Variable::getName() const {
    return name;
}

void Variable::setName(std::string const& newName) {
    name = newName;
}

bool Variable::isTransient() const {
    return transient;
}

bool Variable::hasInitExpression() const {
    return init.isInitialized();
}

storm::expressions::Expression const& Variable::getInitExpression() const {
    STORM_LOG_ASSERT(hasInitExpression(), "Tried to get the init expression of a variable that doesn't have any.");
    return this->init;
}

void Variable::setInitExpression(storm::expressions::Expression const& initialExpression) {
    this->init = initialExpression;
}

void Variable::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                          bool const substituteTranscendentalNumbers) {
    if (this->hasInitExpression()) {
        this->setInitExpression(substituteJaniExpression(this->getInitExpression(), substitution, substituteTranscendentalNumbers));
    }
    type->substitute(substitution, substituteTranscendentalNumbers);
}

JaniType& Variable::getType() {
    return *type;
}

JaniType const& Variable::getType() const {
    return *type;
}

storm::expressions::Expression Variable::getRangeExpression() const {
    storm::expressions::Expression range;

    if (getType().isBoundedType()) {
        auto const& boundedType = getType().asBoundedType();

        if (boundedType.hasLowerBound()) {
            range = boundedType.getLowerBound() <= this->getExpressionVariable();
        }
        if (boundedType.hasUpperBound()) {
            if (range.isInitialized()) {
                range = range && this->getExpressionVariable() <= boundedType.getUpperBound();
            } else {
                range = this->getExpressionVariable() <= boundedType.getUpperBound();
            }
        }
    }
    return range;
}

std::shared_ptr<Variable> Variable::makeVariable(std::string const& name, JaniType const& type, storm::expressions::Variable const& variable,
                                                 boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    if (initValue) {
        return std::make_shared<Variable>(name, type, variable, initValue.get(), transient);
    } else {
        STORM_LOG_ASSERT(!transient, "Expecting variable without init value to be not a transient variable.");
        return std::make_shared<Variable>(name, type, variable);
    }
}

std::shared_ptr<Variable> Variable::makeBasicTypeVariable(std::string const& name, BasicType::Type const& type, storm::expressions::Variable const& variable,
                                                          boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    return makeVariable(name, BasicType(type), variable, initValue, transient);
}

std::shared_ptr<Variable> Variable::makeBooleanVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                        boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    return makeVariable(name, BasicType(BasicType::Type::Bool), variable, initValue, transient);
}

std::shared_ptr<Variable> Variable::makeIntegerVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                        boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    return makeVariable(name, BasicType(BasicType::Type::Int), variable, initValue, transient);
}

std::shared_ptr<Variable> Variable::makeRealVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                     boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    return makeVariable(name, BasicType(BasicType::Type::Real), variable, initValue, transient);
}

std::shared_ptr<Variable> Variable::makeBoundedVariable(std::string const& name, BoundedType::BaseType const& type,
                                                        storm::expressions::Variable const& variable,
                                                        boost::optional<storm::expressions::Expression> const& initValue, bool transient,
                                                        boost::optional<storm::expressions::Expression> const& lowerBound,
                                                        boost::optional<storm::expressions::Expression> const& upperBound) {
    return makeVariable(name,
                        storm::jani::BoundedType(type, lowerBound ? lowerBound.get() : storm::expressions::Expression(),
                                                 upperBound ? upperBound.get() : storm::expressions::Expression()),
                        variable, initValue, transient);
}

std::shared_ptr<Variable> Variable::makeBoundedIntegerVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                               boost::optional<storm::expressions::Expression> const& initValue, bool transient,
                                                               boost::optional<storm::expressions::Expression> const& lowerBound,
                                                               boost::optional<storm::expressions::Expression> const& upperBound) {
    return makeBoundedVariable(name, BoundedType::BaseType::Int, variable, initValue, transient, lowerBound, upperBound);
}

std::shared_ptr<Variable> Variable::makeBoundedRealVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                            boost::optional<storm::expressions::Expression> const& initValue, bool transient,
                                                            boost::optional<storm::expressions::Expression> const& lowerBound,
                                                            boost::optional<storm::expressions::Expression> const& upperBound) {
    return makeBoundedVariable(name, BoundedType::BaseType::Real, variable, initValue, transient, lowerBound, upperBound);
}

std::shared_ptr<Variable> Variable::makeArrayVariable(std::string const& name, JaniType const& baseType, storm::expressions::Variable const& variable,
                                                      boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    return makeVariable(name, ArrayType(baseType), variable, initValue, transient);
}

std::shared_ptr<Variable> Variable::makeClockVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                      boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    return makeVariable(name, ClockType(), variable, initValue, transient);
}

std::shared_ptr<Variable> Variable::makeContinuousVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                           boost::optional<storm::expressions::Expression> const& initValue, bool transient) {
    return makeVariable(name, ContinuousType(), variable, initValue, transient);
}

bool operator==(Variable const& lhs, Variable const& rhs) {
    return lhs.getExpressionVariable() == rhs.getExpressionVariable();
}

bool operator!=(Variable const& lhs, Variable const& rhs) {
    return !(lhs == rhs);
}
}  // namespace jani
}  // namespace storm
