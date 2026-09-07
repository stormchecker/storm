#pragma once

#include <boost/optional.hpp>
#include <type_traits>

#include "storm/models/ModelRepresentation.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/dd/Bdd.h"

namespace storm {
namespace modelchecker {
namespace helper {

/*!
 * Helper class for solving a model checking query.
 * @tparam VT The value type of a single value.
 * @tparam ModelRepresentation The used kind of model representation
 */
template<typename VT, storm::models::ModelRepresentation ModelRepresentation>
class ModelCheckerHelper {
   public:
    typedef VT ValueType;

    ModelCheckerHelper() = default;
    virtual ~ModelCheckerHelper() = default;

    /*!
     * Identifies a subset of the model states
     */
    using StateSet = typename std::conditional<ModelRepresentation == storm::models::ModelRepresentation::Sparse, storm::storage::BitVector,
                                               storm::dd::Bdd<storm::models::GetDdType<ModelRepresentation>::ddType>>::type;

    /*!
     * Sets relevant states.
     * If relevant states are set, it is assumed that the model checking result is only relevant for the given states.
     * In this case, an arbitrary result can be set to non-relevant states.
     */
    void setRelevantStates(StateSet const& relevantStates);

    /*!
     * Clears the relevant states.
     * If no relevant states are set, it is assumed that a result is required for all (initial- and non-initial) states.
     */
    void clearRelevantStates();

    /*!
     * @return true if there are relevant states set.
     * If relevant states are set, it is assumed that the model checking result is only relevant for the given states.
     * In this case, an arbitrary result can be set to non-relevant states.
     */
    bool hasRelevantStates() const;

    /*!
     * @return relevant states (if there are any) or boost::none (otherwise).
     * If relevant states are set, it is assumed that the model checking result is only relevant for the given states.
     * In this case, an arbitrary result can be set to non-relevant states.
     */
    boost::optional<StateSet> const& getOptionalRelevantStates() const;

    /*!
     * @pre Relevant states have to be set before calling this.
     * @return the relevant states. Should only be called if there are any.
     * If relevant states are set, it is assumed that the model checking result is only relevant for the given states.
     * In this case, an arbitrary result can be set to non-relevant states.
     *
     */
    StateSet const& getRelevantStates() const;

   private:
    boost::optional<StateSet> _relevantStates;
};
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm