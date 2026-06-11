#include "storm/storage/sparse/Valuations.h"

#include <boost/algorithm/string/join.hpp>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/umb/model/Valuations.h"

namespace storm::storage::sparse {

Valuations::Valuations(storm::umb::ValuationClassDescription const umbValuationDescription,
                       std::shared_ptr<storm::expressions::ExpressionManager const> const& manager, uint64_t const numEntities)
    : umbValuations(std::make_unique<storm::umb::Valuations>(umbValuationDescription, manager)) {
    umbValuations->resize(numEntities);
    // Intentionally empty
}

Valuations::Valuations(storm::umb::Valuations&& umbValuations) : umbValuations(std::make_unique<storm::umb::Valuations>(std::move(umbValuations))) {
    // Intentionally empty
}

// The following need to be defined in the .cpp file since the umb::Valuations type definition must be complete. This allows forward-declaring the
// umb::Valuations type in the header file.
Valuations::~Valuations() = default;
Valuations::Valuations(Valuations&& other) = default;
Valuations& Valuations::operator=(Valuations&& other) = default;

Valuations::Valuations(Valuations const& other) {
    if (other.umbValuations) {
        umbValuations = std::make_unique<storm::umb::Valuations>(*other.umbValuations);
    }
}

Valuations& Valuations::operator=(Valuations const& other) {
    if (this != &other) {
        if (other.umbValuations) {
            umbValuations = std::make_unique<storm::umb::Valuations>(*other.umbValuations);
        } else {
            umbValuations.reset();
        }
    }
    return *this;
}

storm::expressions::ExpressionManager const& Valuations::getManager() const {
    return umbValuations->getManager();
}

storm::umb::Valuations const& Valuations::getUmbValuations() const {
    return *umbValuations;
}
storm::umb::Valuations& Valuations::getUmbValuations() {
    return *umbValuations;
}

uint64_t Valuations::getNumberOfEntities() const {
    return umbValuations->size();
}

std::set<storm::expressions::Variable> Valuations::getAllVariables() const {
    return umbValuations->getAllVariables();
}

bool Valuations::entityHasVariable(uint64_t entity, const storm::expressions::Variable& variable) const {
    return umbValuations->entityHasVariable(entity, variable);
}

bool Valuations::getBooleanValue(uint64_t const entity, storm::expressions::Variable const& booleanVariable) const {
    return umbValuations->readValue<bool>(entity, booleanVariable);
}

std::optional<bool> Valuations::getOptionalBooleanValue(uint64_t const entity, storm::expressions::Variable const& booleanVariable) const {
    if (!entityHasVariable(entity, booleanVariable)) {
        return std::nullopt;
    }
    std::optional<bool> result;
    umbValuations->readCallback<std::nullopt_t, bool>(entity, booleanVariable, [&result](auto, auto const&, auto&& value) {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            result = value;
        }
        // std::nullopt_t means the optional variable's presence bit is unset → result stays std::nullopt
    });
    return result;
}

int64_t Valuations::getInt64Value(uint64_t const entity, storm::expressions::Variable const& integerVariable) const {
    return umbValuations->readValue<int64_t>(entity, integerVariable);
}

std::optional<int64_t> Valuations::getOptionalInt64Value(uint64_t const entity, storm::expressions::Variable const& integerVariable) const {
    if (!entityHasVariable(entity, integerVariable)) {
        return std::nullopt;
    }
    std::optional<int64_t> result;
    umbValuations->readCallback<std::nullopt_t, int64_t>(entity, integerVariable, [&result](auto, auto const&, auto&& value) {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            result = value;
        }
    });
    return result;
}

double Valuations::getDoubleValue(uint64_t const entity, storm::expressions::Variable const& doubleVariable) const {
    return umbValuations->readValue<double>(entity, doubleVariable);
}

std::optional<double> Valuations::getOptionalDoubleValue(uint64_t const entity, storm::expressions::Variable const& doubleVariable) const {
    if (!entityHasVariable(entity, doubleVariable)) {
        return std::nullopt;
    }
    std::optional<double> result;
    umbValuations->readCallback<std::nullopt_t, double>(entity, doubleVariable, [&result](auto, auto const&, auto&& value) {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<T, double>) {
            result = value;
        }
    });
    return result;
}

storm::RationalNumber Valuations::getRationalValue(uint64_t const entity, storm::expressions::Variable const& rationalVariable) const {
    return umbValuations->readValue<storm::RationalNumber>(entity, rationalVariable);
}

std::optional<storm::RationalNumber> Valuations::getOptionalRationalValue(uint64_t const entity, storm::expressions::Variable const& rationalVariable) const {
    if (!entityHasVariable(entity, rationalVariable)) {
        return std::nullopt;
    }
    std::optional<storm::RationalNumber> result;
    umbValuations->readCallback<std::nullopt_t, storm::RationalNumber>(entity, rationalVariable, [&result](auto, auto const&, auto&& value) {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<T, storm::RationalNumber>) {
            result = std::move(value);
        }
    });
    return result;
}

std::string Valuations::getStringValue(uint64_t const entity, storm::expressions::Variable const& stringVariable) const {
    return umbValuations->readValue<std::string>(entity, stringVariable);
}

std::optional<std::string> Valuations::getOptionalStringValue(uint64_t const entity, storm::expressions::Variable const& stringVariable) const {
    if (!entityHasVariable(entity, stringVariable)) {
        return std::nullopt;
    }
    std::optional<std::string> result;
    umbValuations->readCallback<std::nullopt_t, std::string>(entity, stringVariable, [&result](auto, auto const&, auto&& value) {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
            result = std::move(value);
        }
    });
    return result;
}

template<typename RationalValueType>
void Valuations::setValuesInEvaluator(uint64_t entity, storm::expressions::ExpressionEvaluator<RationalValueType>& evaluator) const {
    umbValuations->setValuesInEvaluator(entity, evaluator);
}
template void Valuations::setValuesInEvaluator(uint64_t entity, storm::expressions::ExpressionEvaluator<double>& evaluator) const;
template void Valuations::setValuesInEvaluator(uint64_t entity, storm::expressions::ExpressionEvaluator<storm::RationalNumber>& evaluator) const;

storm::storage::BitVector Valuations::getBooleanValues(storm::expressions::Variable const& booleanVariable) const {
    STORM_LOG_ASSERT(booleanVariable.hasBooleanType(), "Variable " << booleanVariable.getName() << " is not of boolean type.");
    storm::storage::BitVector result(getNumberOfEntities(), false);
    umbValuations->readCallback<bool>(booleanVariable, [&result](auto const entity, auto, bool value) {
        if (value) {
            result.set(entity);
        }
    });
    return result;
}

std::vector<int64_t> Valuations::getInt64Values(storm::expressions::Variable const& integerVariable) const {
    STORM_LOG_ASSERT(integerVariable.hasIntegerType(), "Variable " << integerVariable.getName() << " is not of integer type.");
    std::vector<int64_t> result;
    result.reserve(getNumberOfEntities());
    umbValuations->readCallback<int64_t>(integerVariable, [&result](auto const entity, auto, int64_t value) {
        STORM_LOG_ASSERT(entity == result.size(), "entities processed in unexpected order.");
        result.push_back(value);
    });
    return result;
}

std::vector<double> Valuations::getDoubleValues(storm::expressions::Variable const& doubleVariable) const {
    STORM_LOG_ASSERT(doubleVariable.hasRationalType(), "Variable " << doubleVariable.getName() << " is not of rational type.");
    std::vector<double> result;
    result.reserve(getNumberOfEntities());
    umbValuations->readCallback<double>(doubleVariable, [&result](auto const entity, auto, double value) {
        STORM_LOG_ASSERT(entity == result.size(), "entities processed in unexpected order.");
        result.push_back(value);
    });
    return result;
}

std::vector<storm::RationalNumber> Valuations::getRationalValues(storm::expressions::Variable const& rationalVariable) const {
    STORM_LOG_ASSERT(rationalVariable.hasRationalType(), "Variable " << rationalVariable.getName() << " is not of rational type.");
    std::vector<storm::RationalNumber> result;
    result.reserve(getNumberOfEntities());
    umbValuations->readCallback<storm::RationalNumber>(rationalVariable, [&result](auto const entity, auto, storm::RationalNumber value) {
        STORM_LOG_ASSERT(entity == result.size(), "entities processed in unexpected order.");
        result.push_back(std::move(value));
    });
    return result;
}

std::vector<std::string> Valuations::getStringValues(storm::expressions::Variable const& stringVariable) const {
    STORM_LOG_ASSERT(stringVariable.hasStringType(), "Variable " << stringVariable.getName() << " is not of rational type.");
    std::vector<std::string> result;
    result.reserve(getNumberOfEntities());
    umbValuations->readCallback<std::string>(stringVariable, [&result](auto const entity, auto, std::string&& value) {
        STORM_LOG_ASSERT(entity == result.size(), "entities processed in unexpected order.");
        result.push_back(std::move(value));
    });
    return result;
}

std::string Valuations::toString(uint64_t const entity, bool const pretty,
                                 std::optional<std::set<storm::expressions::Variable>> const& selectedVariables) const {
    std::vector<std::string> assignments;
    umbValuations->readCallback(entity, [pretty, &selectedVariables, &assignments](auto, auto const& var, auto&& value) {
        if (selectedVariables && !selectedVariables->contains(var)) {
            return;
        }
        using ValueType = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<ValueType, std::nullopt_t>) {
            assignments.push_back(pretty ? (var.getName() + "=none") : "none");
        } else if constexpr (std::is_same_v<ValueType, bool>) {
            if (pretty) {
                assignments.push_back(value ? "" : "!" + var.getName());
            } else {
                assignments.push_back(value ? "true" : "false");
            }
        } else {
            std::stringstream stream;
            if (pretty) {
                stream << var.getName() << "=";
            }
            stream << value;
            assignments.push_back(stream.str());
        }
    });
    return "[" + boost::join(assignments, pretty ? "\t& " : "\t") + "]";
}

template<typename JsonRationalType>
storm::json<JsonRationalType> Valuations::toJson(uint64_t const entity, std::optional<std::set<storm::expressions::Variable>> const& selectedVariables) const {
    storm::json<JsonRationalType> result;
    umbValuations->readCallback<bool, uint64_t, int64_t, double, std::string>(entity, [&selectedVariables, &result](auto, auto const& var, auto&& value) {
        if (selectedVariables && !selectedVariables->contains(var)) {
            return;
        }
        result[var.getName()] = value;
    });
    return result;
}

Valuations Valuations::selectEntities(storm::storage::BitVector const& selectedEntities) const {
    return Valuations(umbValuations->selectEntities(selectedEntities));
}

Valuations Valuations::selectEntities(std::vector<uint64_t> const& selectedEntities) const {
    return Valuations(umbValuations->selectEntities(selectedEntities));
}

std::size_t Valuations::hash() const {
    return umbValuations->hash();
}

template storm::json<double> Valuations::toJson<double>(uint64_t const, std::optional<std::set<storm::expressions::Variable>> const&) const;
template storm::json<storm::RationalNumber> Valuations::toJson<storm::RationalNumber>(uint64_t const,
                                                                                      std::optional<std::set<storm::expressions::Variable>> const&) const;

}  // namespace storm::storage::sparse
