#include "storm/expressions/SimpleValuation.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/functional/hash.hpp>

#include "storm/adapters/JsonAdapter.h"

#include "storm/expressions/ExpressionManager.h"
#include "storm/expressions/Variable.h"

#include "storm/exceptions/InvalidTypeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
SimpleValuation::SimpleValuation() : Valuation(nullptr) {
    // Intentionally left empty.
}

SimpleValuation::SimpleValuation(std::shared_ptr<storm::expressions::ExpressionManager const> const& manager)
    : Valuation(manager),
      booleanValues(this->getManager().getNumberOfBooleanVariables()),
      integerValues(this->getManager().getNumberOfIntegerVariables() + this->getManager().getNumberOfBitVectorVariables()),
      rationalValues(this->getManager().getNumberOfRationalVariables()) {
    // Intentionally left empty.
}

SimpleValuation::SimpleValuation(SimpleValuation const& other) : Valuation(other.getManagerAsSharedPtr()) {
    if (this != &other) {
        booleanValues = other.booleanValues;
        integerValues = other.integerValues;
        rationalValues = other.rationalValues;
    }
}

SimpleValuation& SimpleValuation::operator=(SimpleValuation const& other) {
    if (this != &other) {
        this->setManager(other.getManagerAsSharedPtr());
        booleanValues = other.booleanValues;
        integerValues = other.integerValues;
        rationalValues = other.rationalValues;
    }
    return *this;
}

SimpleValuation::SimpleValuation(SimpleValuation&& other) : Valuation(other.getManagerAsSharedPtr()) {
    if (this != &other) {
        booleanValues = std::move(other.booleanValues);
        integerValues = std::move(other.integerValues);
        rationalValues = std::move(other.rationalValues);
    }
}

SimpleValuation& SimpleValuation::operator=(SimpleValuation&& other) {
    if (this != &other) {
        this->setManager(other.getManagerAsSharedPtr());
        booleanValues = std::move(other.booleanValues);
        integerValues = std::move(other.integerValues);
        rationalValues = std::move(other.rationalValues);
    }
    return *this;
}

bool SimpleValuation::operator==(SimpleValuation const& other) const {
    return this->getManager() == other.getManager() && booleanValues == other.booleanValues && integerValues == other.integerValues &&
           rationalValues == other.rationalValues;
}

bool SimpleValuation::contains(Variable const& variable) const {
    // A variable belongs to this valuation only if it is managed by the same manager and if it was already
    // known to that manager when this valuation was created. The latter is checked via the offset of the
    // variable: offsets within a type group are assigned consecutively and are never reused, so an offset
    // that is out of bounds belongs to a variable declared after the creation of this valuation.
    std::shared_ptr<ExpressionManager const> const& manager = this->getManagerAsSharedPtr();
    if (!manager || manager.get() != &variable.getManager()) {
        return false;
    }
    if (variable.hasBooleanType()) {
        return variable.getOffset() < booleanValues.size();
    } else if (variable.hasIntegerType() || variable.hasBitVectorType()) {
        // Integer and bit vector variables share the same storage and hence the same offset space.
        return variable.getOffset() < integerValues.size();
    } else if (variable.hasRationalType()) {
        return variable.getOffset() < rationalValues.size();
    }
    // Arrays and strings are not stored by valuations of this type.
    return false;
}

bool SimpleValuation::getBooleanValue(Variable const& booleanVariable) const {
    STORM_LOG_ASSERT(contains(booleanVariable), "The valuation does not contain a value for variable '" << booleanVariable.getName() << "'.");
    return booleanValues[booleanVariable.getOffset()];
}

int_fast64_t SimpleValuation::getIntegerValue(Variable const& integerVariable) const {
    STORM_LOG_ASSERT(contains(integerVariable), "The valuation does not contain a value for variable '" << integerVariable.getName() << "'.");
    return integerValues[integerVariable.getOffset()];
}

int_fast64_t SimpleValuation::getBitVectorValue(Variable const& bitVectorVariable) const {
    STORM_LOG_ASSERT(contains(bitVectorVariable), "The valuation does not contain a value for variable '" << bitVectorVariable.getName() << "'.");
    return integerValues[bitVectorVariable.getOffset()];
}

double SimpleValuation::getRationalValue(Variable const& rationalVariable) const {
    STORM_LOG_ASSERT(contains(rationalVariable), "The valuation does not contain a value for variable '" << rationalVariable.getName() << "'.");
    return rationalValues[rationalVariable.getOffset()];
}

void SimpleValuation::setBooleanValue(Variable const& booleanVariable, bool value) {
    STORM_LOG_ASSERT(contains(booleanVariable), "The valuation does not contain a value for variable '" << booleanVariable.getName() << "'.");
    booleanValues[booleanVariable.getOffset()] = value;
}

void SimpleValuation::setIntegerValue(Variable const& integerVariable, int_fast64_t value) {
    STORM_LOG_ASSERT(contains(integerVariable), "The valuation does not contain a value for variable '" << integerVariable.getName() << "'.");
    integerValues[integerVariable.getOffset()] = value;
}

void SimpleValuation::setBitVectorValue(Variable const& bitVectorVariable, int_fast64_t value) {
    STORM_LOG_ASSERT(contains(bitVectorVariable), "The valuation does not contain a value for variable '" << bitVectorVariable.getName() << "'.");
    integerValues[bitVectorVariable.getOffset()] = value;
}

void SimpleValuation::setRationalValue(Variable const& rationalVariable, double value) {
    STORM_LOG_ASSERT(contains(rationalVariable), "The valuation does not contain a value for variable '" << rationalVariable.getName() << "'.");
    rationalValues[rationalVariable.getOffset()] = value;
}

std::string SimpleValuation::toPrettyString(std::set<storm::expressions::Variable> const& selectedVariables) const {
    std::vector<std::string> assignments;
    for (auto const& variable : selectedVariables) {
        std::stringstream stream;
        stream << variable.getName() << "=";
        if (variable.hasBooleanType()) {
            stream << std::boolalpha << this->getBooleanValue(variable) << std::noboolalpha;
        } else if (variable.hasIntegerType()) {
            stream << this->getIntegerValue(variable);
        } else if (variable.hasRationalType()) {
            stream << this->getRationalValue(variable);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unexpected variable type.");
        }
        assignments.push_back(stream.str());
    }
    return "[" + boost::join(assignments, ", ") + "]";
}

std::string SimpleValuation::toString(bool pretty) const {
    if (pretty) {
        std::set<storm::expressions::Variable> allVariables;
        for (auto const& e : getManager()) {
            // Skip variables that were declared after this valuation was created, as there is no value for them.
            if (contains(e.first)) {
                allVariables.insert(e.first);
            }
        }
        return toPrettyString(allVariables);
    } else {
        std::stringstream sstr;
        sstr << "[\n";
        sstr << getManager() << '\n';
        if (!booleanValues.empty()) {
            for (auto element : booleanValues) {
                sstr << element << " ";
            }
            sstr << '\n';
        }
        if (!integerValues.empty()) {
            for (auto const& element : integerValues) {
                sstr << element << " ";
            }
            sstr << '\n';
        }
        if (!rationalValues.empty()) {
            for (auto const& element : rationalValues) {
                sstr << element << " ";
            }
            sstr << '\n';
        }
        sstr << "]";
        return sstr.str();
    }
}

storm::json<storm::RationalNumber> SimpleValuation::toJson() const {
    storm::json<storm::RationalNumber> result;
    for (auto const& variable : getManager()) {
        // Skip variables that were declared after this valuation was created, as there is no value for them.
        if (!contains(variable.first)) {
            continue;
        }
        if (variable.second.isBooleanType()) {
            result[variable.first.getName()] = this->getBooleanValue(variable.first);
        } else if (variable.second.isIntegerType()) {
            result[variable.first.getName()] = this->getIntegerValue(variable.first);
        } else if (variable.second.isRationalType()) {
            result[variable.first.getName()] = storm::utility::convertNumber<storm::RationalNumber>(this->getRationalValue(variable.first));
        } else {
            STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unexpected variable type.");
        }
    }
    return result;
}

std::ostream& operator<<(std::ostream& out, SimpleValuation const& valuation) {
    out << valuation.toString(false) << '\n';
    return out;
}

std::size_t SimpleValuationPointerHash::operator()(SimpleValuation* valuation) const {
    size_t seed = std::hash<std::vector<bool>>()(valuation->booleanValues);
    boost::hash_combine(seed, valuation->integerValues);
    boost::hash_combine(seed, valuation->rationalValues);
    return seed;
}

bool SimpleValuationPointerCompare::operator()(SimpleValuation* SimpleValuation1, SimpleValuation* SimpleValuation2) const {
    return *SimpleValuation1 == *SimpleValuation2;
}

bool SimpleValuationPointerLess::operator()(SimpleValuation* SimpleValuation1, SimpleValuation* SimpleValuation2) const {
    return SimpleValuation1->booleanValues < SimpleValuation2->booleanValues || SimpleValuation1->integerValues < SimpleValuation2->integerValues ||
           SimpleValuation1->rationalValues < SimpleValuation2->rationalValues;
}
}  // namespace expressions
}  // namespace storm
