#pragma once

#include <memory>
#include <string>

#include "storm/languages/prism/Program.h"
#include "storm/storage/BoostTypes.h"
#include "storm/storage/sparse/ChoiceOrigins.h"

namespace storm {
namespace storage {
namespace sparse {

/*!
 * This class represents for each choice the set of prism commands that induced the choice
 */
class PrismChoiceOrigins : public ChoiceOrigins {
   public:
    typedef storm::storage::FlatSet<uint_fast64_t> CommandSet;

    /*!
     * Creates a new representation of the choice indices to their origin in the prism program
     * @param prismProgram The associated prism program
     * @param indexToIdentifierMapping maps a choice index to the internally used identifier of the choice origin
     * @param identifierToCommandSetMapping maps an origin identifier to the set of global indices of the corresponding prism commands
     */
    PrismChoiceOrigins(std::shared_ptr<storm::prism::Program const> const& prismProgram, std::vector<uint_fast64_t> const& indexToIdentifierMapping,
                       std::vector<CommandSet> const& identifierToCommandSetMapping);
    PrismChoiceOrigins(std::shared_ptr<storm::prism::Program const> const& prismProgram, std::vector<uint_fast64_t>&& indexToIdentifierMapping,
                       std::vector<CommandSet>&& identifierToCommandSetMapping);

    virtual ~PrismChoiceOrigins() = default;

    virtual bool isPrismChoiceOrigins() const override;

    /*
     * Returns the number of identifier that are used by this object.
     * This can be used to, e.g., loop over all identifiers.
     */
    virtual uint_fast64_t getNumberOfIdentifiers() const override;

    /*
     * Returns the prism program associated with this
     */
    storm::prism::Program const& getProgram() const;

    /*
     * Returns the set of prism commands that induced the choice with the given index.
     * The command set is represented by a set of global command indices
     */
    CommandSet const& getCommandSet(uint_fast64_t choiceIndex) const;

    std::size_t hash() const override;

   protected:
    /*
     * Returns a copy of this object where the mapping of choice indices to origin identifiers is replaced by the given one.
     */
    virtual std::shared_ptr<ChoiceOrigins> cloneWithNewIndexToIdentifierMapping(std::vector<uint_fast64_t>&& indexToIdentifierMapping) const override;

    /*
     * Computes the identifier infos (i.e., human readable strings representing the choice origins).
     */
    virtual void computeIdentifierInfos() const override;

    /*
     * Computes the identifier infos as json (i.e., a machine readable representation of the choice origins).
     */
    virtual void computeIdentifierJson() const override;

    std::shared_ptr<storm::prism::Program const> program;
    std::vector<CommandSet> identifierToCommandSet;
};
}  // namespace sparse
}  // namespace storage
}  // namespace storm
