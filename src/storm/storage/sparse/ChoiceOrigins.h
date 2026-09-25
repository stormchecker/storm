#pragma once

#include <string>
#include <vector>
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/storage/BitVector.h"

#include "storm/adapters/JsonForward.h"
#include "storm/adapters/RationalNumberForward.h"

namespace storm {
namespace storage {
namespace sparse {

class PrismChoiceOrigins;
class JaniChoiceOrigins;

/*!
 * This class represents the origin of the choices of a model in terms of the input model specification
 * (e.g., the Prism commands that induced the choice).
 */
class ChoiceOrigins {
   public:
    typedef storm::json<storm::RationalNumber> Json;

    virtual ~ChoiceOrigins() = default;

    virtual bool isPrismChoiceOrigins() const;
    virtual bool isJaniChoiceOrigins() const;

    PrismChoiceOrigins& asPrismChoiceOrigins();
    PrismChoiceOrigins const& asPrismChoiceOrigins() const;

    JaniChoiceOrigins& asJaniChoiceOrigins();
    JaniChoiceOrigins const& asJaniChoiceOrigins() const;

    /*
     * Returns a unique identifier of the origin of the given choice which can be used to e.g. check whether two choices have the same origin
     */
    uint_fast64_t getIdentifier(uint_fast64_t choiceIndex) const;

    /*!
     * @return Returns the mapping from choice indices to their identifiers.
     */
    std::vector<uint_fast64_t> const& getIdentifiers() const;

    /*
     * Returns the number of considered identifier.
     * This can be used to, e.g., loop over all identifiers.
     */
    virtual uint_fast64_t getNumberOfIdentifiers() const = 0;

    /*
     * Returns the number of considered choice indices.
     */
    uint_fast64_t getNumberOfChoices() const;

    /*
     * Returns the identifier that is used for choices without an origin in the input specification
     * E.g., Selfloops introduced on deadlock states
     */
    static uint_fast64_t getIdentifierForChoicesWithNoOrigin();

    /*
     * Returns the information for the given choice origin identifier as a (human readable) string
     */
    std::string const& getIdentifierInfo(uint_fast64_t identifier) const;

    /*
     * Returns the choice origin information as a (human readable) string.
     */
    std::string const& getChoiceInfo(uint_fast64_t choiceIndex) const;

    /*
     * Returns the information for the given choice origin identifier as a (human readable) string
     */
    Json const& getIdentifierAsJson(uint_fast64_t identifier) const;

    /*
     * Returns the choice origin information as a (human readable) string.
     */
    Json const& getChoiceAsJson(uint_fast64_t choiceIndex) const;

    /*
     * Derive new choice origins from this by selecting the given choices.
     */
    std::shared_ptr<ChoiceOrigins> selectChoices(storm::storage::BitVector const& selectedChoices) const;

    /*
     * Removes origin information of the given choice.
     */
    void clearOriginOfChoice(uint_fast64_t choiceIndex);

    /*
     * Derive new choice origins from this by selecting the given choices.
     * If an invalid choice index is selected, the corresponding choice will get the identifier for choices with no origin.
     */
    std::shared_ptr<ChoiceOrigins> selectChoices(std::vector<uint_fast64_t> const& selectedChoiceIndices) const;

    storm::models::sparse::ChoiceLabeling toChoiceLabeling() const;

    virtual std::size_t hash() const = 0;

   protected:
    ChoiceOrigins(std::vector<uint_fast64_t> const& indexToIdentifierMapping);
    ChoiceOrigins(std::vector<uint_fast64_t>&& indexToIdentifierMapping);

    /*
     * Returns a copy of this object where the mapping of choice indices to origin identifiers is replaced by the given one.
     */
    virtual std::shared_ptr<ChoiceOrigins> cloneWithNewIndexToIdentifierMapping(std::vector<uint_fast64_t>&& indexToIdentifierMapping) const = 0;

    /*
     * Computes the identifier infos (i.e., human readable strings representing the choice origins).
     */
    virtual void computeIdentifierInfos() const = 0;

    /*
     * Computes the identifier infos (i.e., human readable strings representing the choice origins).
     */
    virtual void computeIdentifierJson() const = 0;

    std::vector<uint_fast64_t> indexToIdentifier;

    // cached identifier infos might be empty if identifiers have not been generated yet.
    mutable std::vector<std::string> identifierToInfo;

    // cached identifier infos might be empty if identifiers have not been generated yet.
    mutable std::vector<Json> identifierToJson;
};
}  // namespace sparse
}  // namespace storage
}  // namespace storm