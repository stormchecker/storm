#pragma once

#include <type_traits>
#include <unordered_set>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"  // clash for likely() macro between Ginac and Sylvan
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/formula/Formula.h>
#pragma clang diagnostic pop

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/sparse/Dtmc.h"

namespace storm {
namespace analysis {

template<typename ValueType, typename Enable = void>
struct ConstraintType {
    typedef void* val;
};

template<typename ValueType>
struct ConstraintType<ValueType, typename std::enable_if<std::is_same<storm::RationalFunction, ValueType>::value>::type> {
    typedef carl::Formula<typename storm::RationalFunction::PolyType::PolyType> val;
};

/**
 * Class to collect constraints on parametric Markov chains.
 */
template<typename ValueType>
class ConstraintCollector {
   private:
    // A set of constraints that says that the DTMC actually has valid probability distributions in all states.
    std::unordered_set<typename ConstraintType<ValueType>::val> wellformedConstraintSet;

    // A set of constraints that makes sure that the underlying graph of the model does not change depending
    // on the parameter values.
    std::unordered_set<typename ConstraintType<ValueType>::val> graphPreservingConstraintSet;

    // A set of variables
    std::set<storm::RationalFunctionVariable> variableSet;

    void wellformedRequiresNonNegativeEntries(std::vector<ValueType> const&);

   public:
    /*!
     * Constructs a constraint collector for the given Model. The constraints are built and ready for
     * retrieval after the construction.
     *
     * @param model The Model for which to create the constraints.
     */
    ConstraintCollector(storm::models::sparse::Model<ValueType> const& model);

    /*!
     * Returns the set of wellformed-ness constraints.
     *
     * @return The set of wellformed-ness constraints.
     */
    std::unordered_set<typename ConstraintType<ValueType>::val> const& getWellformedConstraints() const;

    /*!
     * Returns the set of graph-preserving constraints.
     *
     * @return The set of graph-preserving constraints.
     */
    std::unordered_set<typename ConstraintType<ValueType>::val> const& getGraphPreservingConstraints() const;

    /*!
     * Returns the set of variables in the model
     * @return
     */
    std::set<storm::RationalFunctionVariable> const& getVariables() const;

    /*!
     * Constructs the constraints for the given Model.
     *
     * @param model The DTMC for which to create the constraints.
     */
    void process(storm::models::sparse::Model<ValueType> const& model);

    /*!
     * Constructs the constraints for the given Model by calling the process method.
     *
     * @param model The Model for which to create the constraints.
     */
    void operator()(storm::models::sparse::Model<ValueType> const& model);
};

}  // namespace analysis
}  // namespace storm
