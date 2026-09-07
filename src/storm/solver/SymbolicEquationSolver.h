#pragma once

#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace solver {

template<storm::dd::DdType DdType, typename ValueType = double>
class SymbolicEquationSolver {
   public:
    SymbolicEquationSolver() = default;
    SymbolicEquationSolver(storm::dd::Bdd<DdType> const& allRows);
    virtual ~SymbolicEquationSolver() = default;

    virtual void setLowerBounds(storm::dd::Add<DdType, ValueType> const& lowerBounds);
    virtual void setLowerBound(ValueType const& lowerBound);
    virtual void setUpperBounds(storm::dd::Add<DdType, ValueType> const& upperBounds);
    virtual void setUpperBound(ValueType const& lowerBound);
    virtual void setBounds(ValueType const& lowerBound, ValueType const& upperBound);
    virtual void setBounds(storm::dd::Add<DdType, ValueType> const& lowerBounds, storm::dd::Add<DdType, ValueType> const& upperBounds);

    bool hasLowerBound() const;
    ValueType const& getLowerBound() const;
    bool hasLowerBounds() const;
    storm::dd::Add<DdType, ValueType> const& getLowerBounds() const;
    bool hasUpperBound() const;
    ValueType const& getUpperBound() const;
    bool hasUpperBounds() const;
    storm::dd::Add<DdType, ValueType> const& getUpperBounds() const;

    /*!
     * Retrieves a vector of lower bounds for all values (if any lower bounds are known).
     */
    storm::dd::Add<DdType, ValueType> getLowerBoundsVector() const;

    /*!
     * Retrieves a vector of upper bounds for all values (if any lower bounds are known).
     */
    storm::dd::Add<DdType, ValueType> getUpperBoundsVector() const;

   protected:
    storm::dd::DdManager<DdType>& getDdManager() const;

    void setAllRows(storm::dd::Bdd<DdType> const& allRows);
    storm::dd::Bdd<DdType> const& getAllRows() const;

    // The relevant rows to this equation solver.
    storm::dd::Bdd<DdType> allRows;

   private:
    // Lower bounds (if given).
    boost::optional<storm::dd::Add<DdType, ValueType>> lowerBounds;
    boost::optional<ValueType> lowerBound;

    // Upper bounds (if given).
    boost::optional<storm::dd::Add<DdType, ValueType>> upperBounds;
    boost::optional<ValueType> upperBound;
};

}  // namespace solver
}  // namespace storm
