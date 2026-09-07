#include "SimpleValuation.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/functional/hash.hpp>

#include "storm/adapters/JsonAdapter.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Variable.h"
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

bool SimpleValuation::getBooleanValue(Variable const& booleanVariable) const {
    return booleanValues[booleanVariable.getOffset()];
}

int_fast64_t SimpleValuation::getIntegerValue(Variable const& integerVariable) const {
    return integerValues[integerVariable.getOffset()];
}

int_fast64_t SimpleValuation::getBitVectorValue(Variable const& bitVectorVariable) const {
    return integerValues[bitVectorVariable.getOffset()];
}

double SimpleValuation::getRationalValue(Variable const& rationalVariable) const {
    return rationalValues[rationalVariable.getOffset()];
}

void SimpleValuation::setBooleanValue(Variable const& booleanVariable, bool value) {
    booleanValues[booleanVariable.getOffset()] = value;
}

void SimpleValuation::setIntegerValue(Variable const& integerVariable, int_fast64_t value) {
    integerValues[integerVariable.getOffset()] = value;
}

void SimpleValuation::setBitVectorValue(Variable const& bitVectorVariable, int_fast64_t value) {
    integerValues[bitVectorVariable.getOffset()] = value;
}

void SimpleValuation::setRationalValue(Variable const& rationalVariable, double value) {
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
            allVariables.insert(e.first);
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
