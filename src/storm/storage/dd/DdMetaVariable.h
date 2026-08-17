#pragma once

#include <vector>

#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace dd {

template<DdType LibraryType>
class DdManager;

template<DdType LibraryType, typename ValueTpe>
class Add;

// An enumeration for all legal types of meta variables.
enum class MetaVariableType { Bool, Int, BitVector };
// An enum that expresses the relation of two meta variables relative to each other.
enum class MetaVariablePosition { Above, Below };

// Declare DdMetaVariable class so we can then specialize it for the different DD types.
template<DdType LibraryType>
class DdMetaVariable {
   public:
    friend class DdManager<LibraryType>;
    friend class Bdd<LibraryType>;

    template<DdType LibraryTypePrime, typename ValueTypePrime>
    friend class Add;

    template<DdType LibraryTypePrime, typename ValueTypePrime>
    friend class AddIterator;

    /*!
     * Retrieves the name of the meta variable.
     *
     * @return The name of the variable.
     */
    std::string const& getName() const;

    /*!
     * Retrieves the type of the meta variable.
     *
     * @return The type of the meta variable.
     */
    MetaVariableType getType() const;

    /*!
     * Retrieves the lowest value of the range of the variable.
     *
     * @return The lowest value of the range of the variable.
     */
    int_fast64_t getLow() const;

    /*!
     * Retrieves whether the variable has an upper bound.
     *
     * @return True iff the variable has an upper bound.
     */
    bool hasHigh() const;

    /*!
     * Retrieves the highest value of the range of the variable.
     *
     * @return The highest value of the range of the variable.
     */
    int_fast64_t getHigh() const;

    /*!
     * Retrieves whether the meta variable can represent the given value.
     *
     * @param value The value to check for legality.
     * @return True iff the value is legal.
     */
    bool canRepresent(int_fast64_t value) const;

    /*!
     * Retrieves the number of DD variables for this meta variable.
     *
     * @return The number of DD variables for this meta variable.
     */
    std::size_t getNumberOfDdVariables() const;

    /*!
     * Retrieves the cube of all variables that encode this meta variable.
     *
     * @return The cube of all variables that encode this meta variable.
     */
    Bdd<LibraryType> const& getCube() const;

    /*!
     * Retrieves the highest index of all DD variables belonging to this meta variable.
     */
    uint64_t getLowestIndex() const;

    /*!
     * Retrieves the highest level of all DD variables belonging to this meta variable.
     */
    uint64_t getHighestLevel() const;

    /*!
     * Retrieves the indices of the DD variables associated with this meta variable.
     *
     * @param sortedByLevel If true, the indices are sorted by their level.
     */
    std::vector<uint64_t> getIndices(bool sortedByLevel = true) const;

    /*!
     * Retrieves the indices and levels of the DD variables associated with this meta variable.
     */
    std::vector<std::pair<uint64_t, uint64_t>> getIndicesAndLevels() const;

   private:
    /*!
     * Creates an integer meta variable with the given name and range bounds.
     *
     * @param name The name of the meta variable.
     * @param low The lowest value of the range of the variable.
     * @param high The highest value of the range of the variable.
     * @param ddVariables The vector of variables used to encode this variable.
     */
    DdMetaVariable(std::string const& name, int_fast64_t low, int_fast64_t high, std::vector<Bdd<LibraryType>> const& ddVariables);

    /*!
     * Creates a boolean meta variable with the given name.
     * @param type The type of the meta variable.
     * @param name The name of the meta variable.
     * @param ddVariables The vector of variables used to encode this variable.
     */
    DdMetaVariable(MetaVariableType const& type, std::string const& name, std::vector<Bdd<LibraryType>> const& ddVariables);

    /*!
     * Precomputes the lowest index of any DD variable associated with this meta variable.
     */
    void precomputeLowestIndex();

    /*!
     * Retrieves the variables used to encode the meta variable.
     *
     * @return A vector of variables used to encode the meta variable.
     */
    std::vector<Bdd<LibraryType>> const& getDdVariables() const;

    /*!
     * Creates the cube for this meta variable from the DD variables.
     */
    void createCube();

    // The name of the meta variable.
    std::string name;

    // The type of the variable.
    MetaVariableType type;

    // The lowest value of the range of the variable.
    int_fast64_t low;

    // The highest value of the range of the variable.
    boost::optional<int_fast64_t> high;

    // The vector of variables that are used to encode the meta variable.
    std::vector<Bdd<LibraryType>> ddVariables;

    // The cube consisting of all variables that encode the meta variable.
    Bdd<LibraryType> cube;

    // The lowest index of any DD variable of this meta variable.
    uint64_t lowestIndex;
};
}  // namespace dd
}  // namespace storm
