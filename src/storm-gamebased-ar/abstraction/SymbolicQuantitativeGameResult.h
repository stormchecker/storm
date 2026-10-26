#pragma once

#include "storm/dd/Add.h"
#include "storm/dd/Bdd.h"
#include "storm/dd/DdType.h"
#include "storm/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type, typename ValueType>
class SymbolicQuantitativeGameResult {
   public:
    SymbolicQuantitativeGameResult() = default;

    SymbolicQuantitativeGameResult(storm::dd::Add<Type, ValueType> const& values);
    SymbolicQuantitativeGameResult(boost::optional<std::pair<ValueType, ValueType>> const& initialStatesRange, storm::dd::Add<Type, ValueType> const& values,
                                   boost::optional<storm::dd::Bdd<Type>> const& player1Strategy, boost::optional<storm::dd::Bdd<Type>> const& player2Strategy);

    bool hasPlayer1Strategy() const;

    storm::dd::Bdd<Type> const& getPlayer1Strategy() const;

    storm::dd::Bdd<Type>& getPlayer1Strategy();

    bool hasPlayer2Strategy() const;

    storm::dd::Bdd<Type> const& getPlayer2Strategy() const;

    storm::dd::Bdd<Type>& getPlayer2Strategy();

    bool hasInitialStatesRange() const;

    std::pair<ValueType, ValueType> const& getInitialStatesRange() const;

    boost::optional<std::pair<ValueType, ValueType>> initialStatesRange;
    storm::dd::Add<Type, ValueType> values;
    boost::optional<storm::dd::Bdd<Type>> player1Strategy;
    boost::optional<storm::dd::Bdd<Type>> player2Strategy;
};

}  // namespace abstraction
}  // namespace storm::gbar
