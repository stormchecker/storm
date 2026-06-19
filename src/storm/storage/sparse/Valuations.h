#pragma once

#include <cstdint>
#include <string>

#include "storm/adapters/JsonForward.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/utility/NumberTraits.h"

namespace storm {

namespace expressions {
template<typename T>
class ExpressionEvaluator;
}

namespace umb {
struct ValuationClassDescription;
class Valuations;
}  // namespace umb

namespace storage::sparse {

/*!
 * Provides access to valuations of variables for a set of entities (e.g. states / observations).
 * This class serves as a wrapper around the more low-level storm::umb::Valuations class
 */
class Valuations {
   public:
    Valuations(storm::umb::ValuationClassDescription const valuationClassDescription,
               std::shared_ptr<storm::expressions::ExpressionManager const> const& manager = {}, uint64_t const numEntities = 0);
    Valuations(storm::umb::Valuations&& umbValuations);
    Valuations(Valuations const& other);
    Valuations(Valuations&& other);
    ~Valuations();
    Valuations& operator=(Valuations&& other);
    Valuations& operator=(Valuations const& other);

    storm::expressions::ExpressionManager const& getManager() const;
    storm::umb::Valuations const& getUmbValuations() const;
    storm::umb::Valuations& getUmbValuations();

    /*!
     * @return the numer of entities that this object describes
     */
    uint64_t getNumberOfEntities() const;

    /*!
     * @return the variables that this stores valuations for
     */
    std::set<storm::expressions::Variable> getAllVariables() const;

    /*!
     * Returns true iff the variable is relevant for the given entity
     */
    bool entityHasVariable(uint64_t entity, storm::expressions::Variable const& variable) const;

    // optional variants have no value iff either entityHasVariable(entity, variable) is false or the value is of optional type and not set.
    bool getBooleanValue(uint64_t const entity, storm::expressions::Variable const& booleanVariable) const;
    std::optional<bool> getOptionalBooleanValue(uint64_t const entity, storm::expressions::Variable const& booleanVariable) const;
    int64_t getInt64Value(uint64_t const entity, storm::expressions::Variable const& integerVariable) const;
    std::optional<int64_t> getOptionalInt64Value(uint64_t const entity, storm::expressions::Variable const& integerVariable) const;
    double getDoubleValue(uint64_t const entity, storm::expressions::Variable const& doubleVariable) const;
    std::optional<double> getOptionalDoubleValue(uint64_t const entity, storm::expressions::Variable const& doubleVariable) const;
    storm::RationalNumber getRationalValue(uint64_t const entity, storm::expressions::Variable const& rationalVariable) const;
    std::optional<storm::RationalNumber> getOptionalRationalValue(uint64_t const entity, storm::expressions::Variable const& rationalVariable) const;
    std::string getStringValue(uint64_t const entity, storm::expressions::Variable const& stringVariable) const;
    std::optional<std::string> getOptionalStringValue(uint64_t const entity, storm::expressions::Variable const& stringVariable) const;

    /*!
     * Reads the variable values for the given entity and sets them into the given expression evaluator.
     * @tparam RationalValueType The value type of rationals as stored by the evaluator (e.g. double or storm::RationalNumber).
     * @param entity             Entity index; must be less than size().
     * @param evaluator          The expression evaluator to set the variable values into.
     */
    template<typename RationalValueType>
    void setValuesInEvaluator(uint64_t entity, storm::expressions::ExpressionEvaluator<RationalValueType>& evaluator) const;

    /*!
     * Returns a vector of size getNumberOfEntities() such that the i'th entry is the value of the given variable of entity i.
     */
    storm::storage::BitVector getBooleanValues(storm::expressions::Variable const& booleanVariable) const;

    /*!
     * Returns a vector of size getNumberOfEntities() such that the i'th entry is the value of the given variable of entity i.
     */
    std::vector<int64_t> getInt64Values(storm::expressions::Variable const& integerVariable) const;

    /*!
     * Returns a vector of size getNumberOfEntities() such that the i'th entry is the value of the given variable of entity i.
     */
    std::vector<double> getDoubleValues(storm::expressions::Variable const& doubleVariable) const;

    /*!
     * Returns a vector of size getNumberOfEntities() such that the i'th entry is the value of the given variable of entity i.
     */
    std::vector<storm::RationalNumber> getRationalValues(storm::expressions::Variable const& rationalVariable) const;

    /*!
     * Returns a vector of size getNumberOfEntities() such that the i'th entry is the value of the given variable of entity i.
     */
    std::vector<std::string> getStringValues(storm::expressions::Variable const& stringVariable) const;

    /*!
     * Returns a string representation of the valuation.
     *
     * @param selectedVariables If given, only the informations for the variables in this set are processed.
     * @return The string representation.
     */
    std::string toString(uint64_t const entity, bool const pretty = true,
                         std::optional<std::set<storm::expressions::Variable>> const& selectedVariables = {}) const;

    /*!
     * Returns a JSON representation of this valuation
     * @param selectedVariables If given, only the informations for the variables in this set are processed.
     * @return the json representation
     */
    template<typename JsonRationalType = storm::RationalNumber>
    storm::json<JsonRationalType> toJson(uint64_t const entity, std::optional<std::set<storm::expressions::Variable>> const& selectedVariables = {}) const;

    /*!
     * Derive new  valuations from this by selecting the given entities.
     */
    Valuations selectEntities(storm::storage::BitVector const& selectedEntities) const;

    /*!
     * Derive new valuations from this by selecting the given entities.
     * Requires that the selectedEntities are valid indices, i.e., < getNumberOfEntities()
     */
    Valuations selectEntities(std::vector<uint64_t> const& selectedEntities) const;

    std::size_t hash() const;

   private:
    std::unique_ptr<storm::umb::Valuations> umbValuations;
};

}  // namespace storage::sparse
}  // namespace storm